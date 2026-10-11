#!/usr/bin/env python3
# Copyright (c) 2016-2022 The Bitcoin Core developers
# Copyright (c) 2026 The Marscoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test that the wallet refuses to bump fees.

A fee bump replaces the transaction, and Marscoin never replaces mempool
transactions (58e16509a2). bumpfee and psbtbumpfee used to report success
while the replacement was refused, leaving the original marked as replaced
by a transaction that could never confirm. They now refuse up front, the
wallet is left unchanged, and child-pays-for-parent is the way to speed a
transaction up.
"""

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
)

ERROR = "Fee bumping is not available"


class BumpFeeTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True
        self.extra_args = [["-walletrbf=1"], ["-walletrbf=0"]]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        node = self.nodes[0]
        self.generate(node, 110)
        self.sync_all()

        self.log.info("bumpfee and psbtbumpfee refuse, with or without BIP125 signaling")
        for replaceable in (True, False):
            txid = node.sendtoaddress(address=self.nodes[1].getnewaddress(), amount=1, replaceable=replaceable)
            wallet_txs_before = len(node.listtransactions("*", 1000))
            assert_raises_rpc_error(-32600, ERROR, node.bumpfee, txid)
            assert_raises_rpc_error(-32600, ERROR, node.psbtbumpfee, txid)
            assert_raises_rpc_error(-32600, ERROR, node.bumpfee, txid, {"fee_rate": 100})
            # The wallet is unchanged: the original stays in the mempool and
            # isn't marked as replaced, and no new transaction was recorded.
            assert txid in node.getrawmempool()
            assert "replaced_by_txid" not in node.gettransaction(txid)
            assert_equal(len(node.listtransactions("*", 1000)), wallet_txs_before)

        self.log.info("The error explains how to speed a transaction up instead")
        txid = node.sendtoaddress(address=self.nodes[1].getnewaddress(), amount=1)
        assert_raises_rpc_error(-32600, "child pays for parent", node.bumpfee, txid)

        self.log.info("Child pays for parent works")
        self.sync_mempools()
        parent = node.gettransaction(txid, verbose=True)["decoded"]
        change = next(o for o in parent["vout"] if node.getaddressinfo(o["scriptPubKey"]["address"])["ischange"])
        child = node.send(
            outputs=[{node.getnewaddress(): change["value"] - Decimal("0.01")}],
            inputs=[{"txid": txid, "vout": change["n"]}],
            add_inputs=False,
        )
        assert child["complete"]
        entry = node.getmempoolentry(child["txid"])
        assert_greater_than(entry["fees"]["ancestor"], node.getmempoolentry(txid)["fees"]["base"])
        self.generate(node, 1)
        assert_equal(node.gettransaction(txid)["confirmations"], 1)
        assert_equal(node.gettransaction(child["txid"])["confirmations"], 1)


if __name__ == "__main__":
    BumpFeeTest(__file__).main()
