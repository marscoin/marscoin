#!/usr/bin/env python3
# Copyright (c) 2017-2021 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test transaction upload"""

from test_framework.messages import msg_getdata, CInv, MSG_TX, MSG_WTX
from test_framework.p2p import p2p_lock, P2PDataStore, P2PTxInvStore
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)
from test_framework.wallet import MiniWallet


class P2PNode(P2PDataStore):
    def on_inv(self, msg):
        pass


class P2PLeakTxTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def run_test(self):
        self.gen_node = self.nodes[0]  # The block and tx generating node
        self.miniwallet = MiniWallet(self.gen_node)

        self.test_tx_in_block()
        self.test_tx_not_replaced()
        self.test_notfound_on_unannounced_tx()

    def test_tx_in_block(self):
        self.log.info("Check that a transaction in the last block is uploaded (beneficial for compact block relay)")
        inbound_peer = self.gen_node.add_p2p_connection(P2PNode())

        self.log.debug("Generate transaction and block")
        inbound_peer.last_message.pop("inv", None)
        wtxid = self.miniwallet.send_self_transfer(from_node=self.gen_node)["wtxid"]
        inbound_peer.wait_until(lambda: "inv" in inbound_peer.last_message and inbound_peer.last_message.get("inv").inv[0].hash == int(wtxid, 16))
        want_tx = msg_getdata(inv=inbound_peer.last_message.get("inv").inv)
        self.generate(self.gen_node, 1)

        self.log.debug("Request transaction")
        inbound_peer.last_message.pop("tx", None)
        inbound_peer.send_and_ping(want_tx)
        assert_equal(inbound_peer.last_message.get("tx").tx.getwtxid(), wtxid)

    def test_tx_not_replaced(self):
        # Upstream checks that a replaced transaction is answered with notfound.
        # Marscoin never replaces mempool transactions (58e16509a2), so the
        # conflict is refused and the announced transaction stays available.
        self.gen_node.disconnect_p2ps()
        inbound_peer = self.gen_node.add_p2p_connection(P2PTxInvStore())

        self.log.info("Transaction tx_a is broadcast")
        tx_a = self.miniwallet.send_self_transfer(from_node=self.gen_node)
        inbound_peer.wait_for_broadcast(txns=[tx_a["wtxid"]])

        self.log.info("A conflicting tx_b is refused")
        tx_b = tx_a["tx"]
        tx_b.vout[0].nValue -= 9000
        assert_raises_rpc_error(-26, "bip125-replacement-disallowed", self.gen_node.sendrawtransaction, tx_b.serialize().hex())

        self.log.info("Re-request of tx_a is still answered with the transaction")
        want_tx = msg_getdata()
        want_tx.inv = [CInv(t=MSG_WTX, h=int(tx_a["wtxid"], 16))]
        with p2p_lock:
            inbound_peer.last_message.pop("notfound", None)
            inbound_peer.last_message.pop("tx", None)
        inbound_peer.send_and_ping(want_tx)
        assert_equal(inbound_peer.last_message.get("tx").tx.getwtxid(), tx_a["wtxid"])
        assert "notfound" not in inbound_peer.last_message

    def test_notfound_on_unannounced_tx(self):
        self.log.info("Check that we don't leak txs to inbound peers that we haven't yet announced to")
        self.gen_node.disconnect_p2ps()
        inbound_peer = self.gen_node.add_p2p_connection(P2PNode())  # An "attacking" inbound peer

        MAX_REPEATS = 100
        self.log.info("Running test up to {} times.".format(MAX_REPEATS))
        for i in range(MAX_REPEATS):
            self.log.info('Run repeat {}'.format(i + 1))
            txid = self.miniwallet.send_self_transfer(from_node=self.gen_node)["wtxid"]

            want_tx = msg_getdata()
            want_tx.inv.append(CInv(t=MSG_TX, h=int(txid, 16)))
            with p2p_lock:
                inbound_peer.last_message.pop('notfound', None)
            inbound_peer.send_and_ping(want_tx)

            if inbound_peer.last_message.get('notfound'):
                self.log.debug('tx {} was not yet announced to us.'.format(txid))
                self.log.debug("node has responded with a notfound message. End test.")
                assert_equal(inbound_peer.last_message['notfound'].vec[0].hash, int(txid, 16))
                with p2p_lock:
                    inbound_peer.last_message.pop('notfound')
                break
            else:
                self.log.debug('tx {} was already announced to us. Try test again.'.format(txid))
                assert int(txid, 16) in [inv.hash for inv in inbound_peer.last_message['inv'].inv]


if __name__ == '__main__':
    P2PLeakTxTest(__file__).main()
