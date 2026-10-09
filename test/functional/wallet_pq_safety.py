#!/usr/bin/env python3
# Copyright (c) 2026 The Marscoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test wallet safety guards for post-quantum (witness v2, P2WPQH) outputs.

- Where consensus verifies witness v2 spends, the wallet creates PQ addresses
  and signs PQ inputs.
- Where it does not (PQ inactive), the wallet refuses to create PQ addresses,
  send to them, or sign PQ inputs, because such outputs could be spent by anyone.
- encryptwallet encrypts PQ private keys, generating and signing require an
  unlocked wallet, and no plaintext secret remains in the wallet file.
- Taproot addresses starting with "<hrp>1pq" are valid.

Marscoin regtest currently needs about 2^20 scrypt hashes per block, so this
test does not mine. It signs spends of PQ outputs supplied through prevtxs.
"""
from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework, SkipTest
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)

PQ_ACTIVATION_HEIGHT = 1000
PASSPHRASE = "pq passphrase"
# Taproot output for x-only key 45*G; the address starts with "bcrt1pq"
TAPROOT_HRP1PQ_ADDRESS = "bcrt1pqjfhpf947s6p9639752w3mx66pfxvy27fflvkyu8yvvq3795t93sr98x0j"
SPHINCS_SHA2_128S_SIG_SIZE = 7856
INACTIVE_ADDRESS_ERROR = "Post-quantum addresses are disabled"
INACTIVE_SEND_ERROR = "Cannot send to a post-quantum (witness v2) address"
INACTIVE_SIGN_ERROR = "Post-quantum signing is disabled"


class WalletPQSafetyTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser)

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        # node0: regtest default, PQ active from height 1
        # node1: PQ (and ABWL) activate at PQ_ACTIVATION_HEIGHT
        self.extra_args = [[], [f"-testactivationheight=abwl@{PQ_ACTIVATION_HEIGHT}"]]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self):
        # The nodes follow different activation rules; keep them apart.
        self.setup_nodes()

    @staticmethod
    def pq_prevtx(result, n):
        """A made-up unspent output paying to the PQ address in a getnewpqaddress result."""
        return {"txid": f"{n:02x}" * 32, "vout": 0, "scriptPubKey": "5220" + result["program"], "amount": Decimal("1")}

    @staticmethod
    def spend_of(wallet, prevtx):
        return wallet.createrawtransaction([{"txid": prevtx["txid"], "vout": prevtx["vout"]}], {wallet.getnewaddress(): Decimal("0.99")})

    def assert_signs(self, wallet, prevtx):
        signed = wallet.signrawtransactionwithwallet(self.spend_of(wallet, prevtx), [prevtx])
        assert_equal(signed["complete"], True)
        witness = wallet.decoderawtransaction(signed["hex"])["vin"][0]["txinwitness"]
        # <signature> <parameter set> <public key>
        assert_equal(len(witness), 3)
        assert len(witness[0]) // 2 >= SPHINCS_SHA2_128S_SIG_SIZE
        assert_equal(len(witness[1]) // 2, 1)
        assert_equal(len(witness[2]) // 2, 32)

    def read_wallet_file(self, node, name):
        node.unloadwallet(name)
        data = (node.wallets_path / name / self.wallet_data_filename).read_bytes()
        node.loadwallet(name)
        return data

    @staticmethod
    def find_pq_secret(data, pubkey):
        """Return the 32 secret bytes (SK.seed || SK.prf) stored next to pubkey.

        An unencrypted record holds param_set_id || pubkey || secret key, and a
        SPHINCS+ secret key is SK.seed || SK.prf || pubkey.
        """
        start = data.find(pubkey)
        while start != -1:
            if data[start + 64:start + 96] == pubkey:
                return data[start + 32:start + 64]
            start = data.find(pubkey, start + 1)
        return None

    def run_test(self):
        node0, node1 = self.nodes
        node0.createwallet("pq")
        try:
            node0.get_wallet_rpc("pq").getnewpqaddress()
        except Exception as e:
            if "OQS backend not available" in str(e):
                raise SkipTest("marscoind was built without --enable-pq-oqs-vendor")
            raise

        self.test_taproot_hrp1pq(node0)
        self.test_pq_active(node0)
        self.test_encryption(node0)
        self.test_pq_inactive(node0, node1)

    def test_taproot_hrp1pq(self, node):
        self.log.info("Taproot addresses starting with <hrp>1pq are valid")
        info = node.validateaddress(TAPROOT_HRP1PQ_ADDRESS)
        assert_equal(info["isvalid"], True)
        assert_equal(info["witness_version"], 1)

    def test_pq_active(self, node):
        self.log.info("With PQ active, the wallet creates PQ addresses and signs PQ inputs")
        wallet = node.get_wallet_rpc("pq")
        result = wallet.getnewpqaddress()
        assert result["address"].startswith("bcrt1z")
        assert_equal(wallet.getaddressinfo(result["address"])["ismine"], True)
        self.assert_signs(wallet, self.pq_prevtx(result, 1))

    def test_encryption(self, node):
        self.log.info("encryptwallet encrypts PQ keys already in the wallet")
        node.createwallet("pqenc")
        wallet = node.get_wallet_rpc("pqenc")
        old_key = wallet.getnewpqaddress()
        pubkey = bytes.fromhex(old_key["pubkey"])

        secret = self.find_pq_secret(self.read_wallet_file(node, "pqenc"), pubkey)
        assert secret is not None, "plaintext PQ key not found in unencrypted wallet file"

        wallet.encryptwallet(PASSPHRASE)
        data = self.read_wallet_file(node, "pqenc")
        assert pubkey in data, "PQ public key missing from encrypted wallet file"
        assert secret not in data, "plaintext PQ secret key left in encrypted wallet file"
        assert_equal(wallet.getaddressinfo(old_key["address"])["ismine"], True)

        self.log.info("A locked wallet can neither generate PQ keys nor sign PQ inputs")
        old_prevtx = self.pq_prevtx(old_key, 2)
        assert_raises_rpc_error(-13, "walletpassphrase", wallet.getnewpqaddress)
        assert_raises_rpc_error(-13, "walletpassphrase", wallet.signrawtransactionwithwallet, self.spend_of(wallet, old_prevtx), [old_prevtx])

        self.log.info("After unlocking, keys created before and after encryption sign")
        wallet.walletpassphrase(PASSPHRASE, 600)
        new_key = wallet.getnewpqaddress()
        self.assert_signs(wallet, old_prevtx)
        self.assert_signs(wallet, self.pq_prevtx(new_key, 3))
        wallet.walletlock()
        assert pubkey in self.read_wallet_file(node, "pqenc")

    def test_pq_inactive(self, node0, node1):
        wallet1 = node1.get_wallet_rpc(self.default_wallet_name)
        assert node1.getblockcount() + 1 < PQ_ACTIVATION_HEIGHT

        self.log.info("With PQ inactive, the wallet refuses to create PQ addresses")
        assert_raises_rpc_error(-4, INACTIVE_ADDRESS_ERROR, wallet1.getnewpqaddress)

        self.log.info("With PQ inactive, the wallet refuses to send to PQ addresses")
        pq_address = node0.get_wallet_rpc("pq").getnewpqaddress()["address"]
        assert_raises_rpc_error(-6, INACTIVE_SEND_ERROR, wallet1.sendtoaddress, pq_address, 1)
        assert_raises_rpc_error(-4, INACTIVE_SEND_ERROR, wallet1.walletcreatefundedpsbt, [], [{pq_address: 1}])

        self.log.info("With PQ inactive, the wallet refuses to sign PQ inputs")
        node0.createwallet("pqbackup")
        backup_key = node0.get_wallet_rpc("pqbackup").getnewpqaddress()
        backup_path = node0.datadir_path / "pqbackup.dat"
        node0.get_wallet_rpc("pqbackup").backupwallet(backup_path)
        node1.restorewallet("pqbackup", backup_path)
        restored = node1.get_wallet_rpc("pqbackup")
        prevtx = self.pq_prevtx(backup_key, 4)
        signed = restored.signrawtransactionwithwallet(self.spend_of(restored, prevtx), [prevtx])
        assert_equal(signed["complete"], False)
        assert INACTIVE_SIGN_ERROR in signed["errors"][0]["error"]


if __name__ == '__main__':
    WalletPQSafetyTest(__file__).main()
