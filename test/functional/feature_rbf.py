#!/usr/bin/env python3
# Copyright (c) 2014-2022 The Bitcoin Core developers
# Copyright (c) 2026 The Marscoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test that Marscoin never replaces mempool transactions.

Marscoin disabled replace-by-fee (58e16509a2): a transaction that spends an
input already spent by a mempool transaction is rejected, whatever its fee,
its BIP125 signaling or -mempoolfullrbf. Upstream's feature_rbf.py tests the
replacement rules instead; this test checks that every path to a replacement
is refused and that the first-seen transaction stays.
"""

from decimal import Decimal

from test_framework.authproxy import JSONRPCException
from test_framework.messages import (
    MAX_BIP125_RBF_SEQUENCE,
    SEQUENCE_FINAL,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)
from test_framework.wallet import MiniWallet

REJECT = "bip125-replacement-disallowed"


class NoReplaceByFeeTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def run_test(self):
        self.wallet = MiniWallet(self.nodes[0])
        self.generate(self.wallet, 10)
        self.generate(self.nodes[0], 100)

        self.test_doublespend_rejected()
        self.test_bip125_signaling_ignored()
        self.test_descendants_protect_nothing()
        self.test_testmempoolaccept_and_package()
        self.test_relay_from_peer()
        self.test_conflict_after_confirmation()
        self.test_truc_sibling_not_evicted()
        self.test_fullrbf_option_ignored()

    def conflicting(self, utxo, *, fee, sequence=SEQUENCE_FINAL, locktime=0):
        """A transaction spending utxo, paying `fee`. Vary locktime to get a
        different transaction at the same fee."""
        return self.wallet.create_self_transfer(utxo_to_spend=utxo, fee=fee, sequence=sequence, locktime=locktime)

    def assert_in_mempool_only(self, node, present, absent):
        mempool = node.getrawmempool()
        assert present in mempool
        assert absent not in mempool

    def test_doublespend_rejected(self):
        self.log.info("A double spend is rejected at any fee, and the first-seen transaction stays")
        node = self.nodes[0]
        utxo = self.wallet.get_utxo()
        tx1 = self.conflicting(utxo, fee=Decimal("0.0001"))
        node.sendrawtransaction(tx1["hex"])
        for fee in (Decimal("0.0001"), Decimal("0.001"), Decimal("1")):
            tx2 = self.conflicting(utxo, fee=fee, locktime=1)
            assert_raises_rpc_error(-26, REJECT, node.sendrawtransaction, tx2["hex"], 0)
            self.assert_in_mempool_only(node, tx1["txid"], tx2["txid"])
        self.generate(node, 1)

    def test_bip125_signaling_ignored(self):
        self.log.info("Signaling BIP125 replaceability doesn't make a transaction replaceable")
        node = self.nodes[0]
        utxo = self.wallet.get_utxo()
        tx1 = self.conflicting(utxo, fee=Decimal("0.0001"), sequence=MAX_BIP125_RBF_SEQUENCE)
        node.sendrawtransaction(tx1["hex"])
        # getmempoolentry still reports the signal; it doesn't mean the node replaces.
        assert_equal(node.getmempoolentry(tx1["txid"])["bip125-replaceable"], True)
        tx2 = self.conflicting(utxo, fee=Decimal("0.01"), sequence=MAX_BIP125_RBF_SEQUENCE)
        assert_raises_rpc_error(-26, REJECT, node.sendrawtransaction, tx2["hex"], 0)
        self.assert_in_mempool_only(node, tx1["txid"], tx2["txid"])
        self.generate(node, 1)

    def test_descendants_protect_nothing(self):
        self.log.info("A conflict with a transaction that has descendants is rejected; the chain stays")
        node = self.nodes[0]
        utxo = self.wallet.get_utxo()
        parent = self.conflicting(utxo, fee=Decimal("0.0001"), sequence=MAX_BIP125_RBF_SEQUENCE)
        node.sendrawtransaction(parent["hex"])
        child = self.wallet.send_self_transfer(from_node=node, utxo_to_spend=parent["new_utxo"])
        conflict = self.conflicting(utxo, fee=Decimal("0.5"))
        assert_raises_rpc_error(-26, REJECT, node.sendrawtransaction, conflict["hex"], 0)
        mempool = node.getrawmempool()
        assert parent["txid"] in mempool and child["txid"] in mempool
        assert conflict["txid"] not in mempool
        self.generate(node, 1)

    def test_testmempoolaccept_and_package(self):
        self.log.info("testmempoolaccept and submitpackage refuse conflicts too")
        node = self.nodes[0]
        utxo = self.wallet.get_utxo()
        tx1 = self.conflicting(utxo, fee=Decimal("0.0001"))
        node.sendrawtransaction(tx1["hex"])
        tx2 = self.conflicting(utxo, fee=Decimal("0.01"))
        result = node.testmempoolaccept([tx2["hex"]])[0]
        assert_equal(result["allowed"], False)
        assert_equal(result["reject-reason"], REJECT)
        # A package whose parent conflicts with the mempool is refused as well.
        child = self.wallet.create_self_transfer(utxo_to_spend=tx2["new_utxo"], fee=Decimal("0.01"))
        try:
            assert node.submitpackage([tx2["hex"], child["hex"]])["package_msg"] != "success"
        except JSONRPCException:
            pass
        self.assert_in_mempool_only(node, tx1["txid"], tx2["txid"])
        assert child["txid"] not in node.getrawmempool()
        self.generate(node, 1)

    def test_relay_from_peer(self):
        self.log.info("A node that already has the first transaction refuses the conflict, and it doesn't spread")
        node0, node1 = self.nodes
        utxo = self.wallet.get_utxo()
        tx1 = self.conflicting(utxo, fee=Decimal("0.0001"))
        node0.sendrawtransaction(tx1["hex"])
        self.sync_mempools()
        tx2 = self.conflicting(utxo, fee=Decimal("0.01"))
        # Submit the conflict to the second node, which already has tx1.
        assert_raises_rpc_error(-26, REJECT, node1.sendrawtransaction, tx2["hex"], 0)
        for node in self.nodes:
            self.assert_in_mempool_only(node, tx1["txid"], tx2["txid"])
        self.generate(node0, 1)

    def test_conflict_after_confirmation(self):
        self.log.info("Once the first transaction is mined, its conflict is invalid")
        node = self.nodes[0]
        utxo = self.wallet.get_utxo()
        tx1 = self.conflicting(utxo, fee=Decimal("0.0001"))
        node.sendrawtransaction(tx1["hex"])
        self.generate(node, 1)
        tx2 = self.conflicting(utxo, fee=Decimal("0.01"))
        assert_raises_rpc_error(-25, "bad-txns-inputs-missingorspent", node.sendrawtransaction, tx2["hex"], 0)

    def test_truc_sibling_not_evicted(self):
        self.log.info("A TRUC child is not evicted by a higher-fee sibling")
        node = self.nodes[0]
        parent = self.wallet.send_self_transfer_multi(from_node=node, num_outputs=2, version=3)
        child1 = self.wallet.send_self_transfer(from_node=node, utxo_to_spend=parent["new_utxos"][0], version=3)
        # Upstream's TRUC sibling eviction would let this higher-fee child evict child1.
        child2 = self.wallet.create_self_transfer(utxo_to_spend=parent["new_utxos"][1], version=3, fee=Decimal("0.01"))
        assert_raises_rpc_error(-26, "", node.sendrawtransaction, child2["hex"], 0)
        mempool = node.getrawmempool()
        assert child1["txid"] in mempool
        assert child2["txid"] not in mempool
        self.generate(node, 1)

    def test_fullrbf_option_ignored(self):
        self.log.info("-mempoolfullrbf=1 doesn't enable replacements")
        self.restart_node(0, extra_args=["-mempoolfullrbf=1"])
        node = self.nodes[0]
        self.connect_nodes(0, 1)
        utxo = self.wallet.get_utxo()
        tx1 = self.conflicting(utxo, fee=Decimal("0.0001"))
        node.sendrawtransaction(tx1["hex"])
        tx2 = self.conflicting(utxo, fee=Decimal("0.01"))
        assert_raises_rpc_error(-26, REJECT, node.sendrawtransaction, tx2["hex"], 0)
        self.assert_in_mempool_only(node, tx1["txid"], tx2["txid"])


if __name__ == '__main__':
    NoReplaceByFeeTest(__file__).main()
