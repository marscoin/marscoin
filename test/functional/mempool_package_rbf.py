#!/usr/bin/env python3
# Copyright (c) 2021-present The Bitcoin Core developers
# Copyright (c) 2026 The Marscoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test that packages never replace mempool transactions.

Upstream's mempool_package_rbf.py tests package replace-by-fee. Marscoin
never replaces mempool transactions (58e16509a2), so a package that
conflicts with the mempool is refused whatever its fees, and the
transactions it conflicts with stay.
"""

from decimal import Decimal

from test_framework.messages import (
    COIN,
    MAX_BIP125_RBF_SEQUENCE,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
)
from test_framework.wallet import (
    DEFAULT_FEE,
    MiniWallet,
)

DEFAULT_CHILD_FEE = DEFAULT_FEE * 4


class PackageNoRBFTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        self.wallet = MiniWallet(self.nodes[0])
        self.generate(self.wallet, 30)
        self.generate(self.nodes[0], 100)
        # Vary nSequence so that conflicting transactions with the same fee
        # are still different transactions.
        self.ctr = 0

        self.test_package_conflicting_with_package()
        self.test_package_conflicting_with_singleton()
        self.test_high_fee_package()
        self.test_zero_fee_parent()
        self.test_child_conflicts_parent_mempool_ancestor()

    def package(self, coin, parent_fee=DEFAULT_FEE, child_fee=DEFAULT_CHILD_FEE, parent_fee_rate=None):
        """A parent spending `coin` and a child spending the parent, both signaling BIP125."""
        self.ctr += 1
        if parent_fee_rate is not None:
            parent = self.wallet.create_self_transfer(fee_rate=parent_fee_rate, fee=0, utxo_to_spend=coin, sequence=MAX_BIP125_RBF_SEQUENCE - self.ctr)
        else:
            parent = self.wallet.create_self_transfer(fee=parent_fee, utxo_to_spend=coin, sequence=MAX_BIP125_RBF_SEQUENCE - self.ctr)
        self.ctr += 1
        child = self.wallet.create_self_transfer(fee=child_fee, utxo_to_spend=parent["new_utxo"], sequence=MAX_BIP125_RBF_SEQUENCE - self.ctr)
        return parent, child

    def assert_refused(self, package_hex, kept, refused):
        node = self.nodes[0]
        result = node.submitpackage(package_hex)
        assert result["package_msg"] != "success"
        mempool = node.getrawmempool()
        for txid in kept:
            assert txid in mempool
        for txid in refused:
            assert txid not in mempool

    def test_package_conflicting_with_package(self):
        self.log.info("A package conflicting with a mempool package is refused")
        node = self.nodes[0]
        coin = self.wallet.get_utxo()
        parent1, child1 = self.package(coin)
        assert_equal(node.submitpackage([parent1["hex"], child1["hex"]])["package_msg"], "success")
        parent2, child2 = self.package(coin, child_fee=DEFAULT_CHILD_FEE * 5)
        self.assert_refused([parent2["hex"], child2["hex"]], kept=[parent1["txid"], child1["txid"]], refused=[parent2["txid"], child2["txid"]])
        self.generate(node, 1)

    def test_package_conflicting_with_singleton(self):
        self.log.info("A package conflicting with a single mempool transaction is refused")
        node = self.nodes[0]
        coin = self.wallet.get_utxo()
        self.ctr += 1
        single = self.wallet.create_self_transfer(fee=DEFAULT_FEE, utxo_to_spend=coin, sequence=MAX_BIP125_RBF_SEQUENCE - self.ctr)
        node.sendrawtransaction(single["hex"])
        parent, child = self.package(coin, child_fee=DEFAULT_CHILD_FEE * 5)
        self.assert_refused([parent["hex"], child["hex"]], kept=[single["txid"]], refused=[parent["txid"], child["txid"]])
        self.generate(node, 1)

    def test_high_fee_package(self):
        self.log.info("Even a package paying a hundred times the fee is refused")
        node = self.nodes[0]
        coin = self.wallet.get_utxo()
        parent1, child1 = self.package(coin)
        node.submitpackage([parent1["hex"], child1["hex"]])
        parent2, child2 = self.package(coin, parent_fee=DEFAULT_FEE * 100, child_fee=DEFAULT_CHILD_FEE * 100)
        assert_greater_than(parent2["fee"] + child2["fee"], 50 * (parent1["fee"] + child1["fee"]))
        self.assert_refused([parent2["hex"], child2["hex"]], kept=[parent1["txid"], child1["txid"]], refused=[parent2["txid"], child2["txid"]])
        self.generate(node, 1)

    def test_zero_fee_parent(self):
        self.log.info("A package with a 0-fee parent and a high-fee child is refused if it conflicts")
        node = self.nodes[0]
        coin = self.wallet.get_utxo()
        parent1, child1 = self.package(coin)
        node.submitpackage([parent1["hex"], child1["hex"]])
        parent2, child2 = self.package(coin, parent_fee=0, child_fee=DEFAULT_CHILD_FEE * 10)
        self.assert_refused([parent2["hex"], child2["hex"]], kept=[parent1["txid"], child1["txid"]], refused=[parent2["txid"], child2["txid"]])
        self.generate(node, 1)

    def test_child_conflicts_parent_mempool_ancestor(self):
        self.log.info("A package whose child double-spends its parent's mempool ancestor is refused")
        node = self.nodes[0]
        coin = self.wallet.get_utxo()
        self.ctr += 1
        grandparent = self.wallet.create_self_transfer(fee=DEFAULT_FEE, utxo_to_spend=coin, sequence=MAX_BIP125_RBF_SEQUENCE - self.ctr)
        node.sendrawtransaction(grandparent["hex"])
        self.ctr += 1
        parent = self.wallet.create_self_transfer(fee_rate=Decimal("0.00001000"), utxo_to_spend=grandparent["new_utxo"], sequence=MAX_BIP125_RBF_SEQUENCE - self.ctr)
        # The child also spends the grandparent's coin, which isn't in the package.
        self.ctr += 1
        child = self.wallet.create_self_transfer_multi(
            fee_per_output=int(DEFAULT_CHILD_FEE * COIN),
            utxos_to_spend=[parent["new_utxo"], coin],
            sequence=MAX_BIP125_RBF_SEQUENCE - self.ctr,
        )
        # The parent conflicts with nothing and may be accepted on its own; the
        # child conflicts with the grandparent and is refused.
        self.assert_refused([parent["hex"], child["hex"]], kept=[grandparent["txid"]], refused=[child["txid"]])
        self.generate(node, 1)


if __name__ == "__main__":
    PackageNoRBFTest(__file__).main()
