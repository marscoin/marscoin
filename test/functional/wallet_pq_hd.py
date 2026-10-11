#!/usr/bin/env python3
# Copyright (c) 2026 The Marscoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test seed-derived post-quantum keys (PQ HD v1, doc/quantum-pq-key-derivation-v1.md).

- New descriptor wallets get active wpq() descriptors for receiving and change.
- Their addresses follow the specification, checked against the independent
  reference implementation in contrib/devtools/pq-hd-vectors.py.
- A wallet restored from its wpq() descriptors alone finds and spends its
  P2WPQH coins, also after a restart.
- No public output and no encrypted wallet file contains a PQ HD node.
- A locked wallet hands out keys derived earlier but derives no new ones.
- A wallet without wpq() descriptors gets them on the first getnewpqaddress.
"""
import importlib.util
from decimal import Decimal
from pathlib import Path

from test_framework.segwit_addr import CHARSET, Encoding, bech32_verify_checksum, convertbits
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)

PASSPHRASE = "pq hd passphrase"
SPHINCS_SHA2_128S_SIG_SIZE = 7856


def load_reference():
    path = Path(__file__).resolve().parents[2] / "contrib" / "devtools" / "pq-hd-vectors.py"
    spec = importlib.util.spec_from_file_location("pq_hd_reference", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def decode_bech32m(text, hrp):
    """Bech32m decoding without the 90-character limit of addresses."""
    pos = text.rfind("1")
    assert_equal(text[:pos], hrp)
    data = [CHARSET.find(c) for c in text[pos + 1:]]
    assert -1 not in data
    assert_equal(bech32_verify_checksum(hrp, data), Encoding.BECH32M)
    return bytes(convertbits(data[:-6], 5, 8, False))


class WalletPQHDTest(BitcoinTestFramework):
    def add_options(self, parser):
        self.add_wallet_options(parser, legacy=False)

    def set_test_params(self):
        self.num_nodes = 1
        # Restores rescan with block filters, which must follow the lookahead too.
        self.extra_args = [["-blockfilterindex=1"]]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def wpq_descriptors(self, wallet, private=False):
        return [d for d in wallet.listdescriptors(private)["descriptors"] if d["desc"].startswith("wpq(")]

    def read_wallet_file(self, name):
        node = self.nodes[0]
        node.unloadwallet(name)
        # Without a wallets/ directory (as in the cached chain), wallets live in the data directory.
        wallets_dir = node.wallets_path if node.wallets_path.is_dir() else node.chain_path
        data = (wallets_dir / name / self.wallet_data_filename).read_bytes()
        node.loadwallet(name)
        return data

    def run_test(self):
        self.ref = load_reference()
        node = self.nodes[0]
        self.funder = node.get_wallet_rpc(self.default_wallet_name)

        self.test_new_wallet()
        self.test_restore_and_spend()
        self.test_encryption()
        self.test_missing_descriptors()

    def test_new_wallet(self):
        self.log.info("New wallets have active wpq() descriptors that reveal no node")
        node = self.nodes[0]
        node.createwallet("pq")
        wallet = node.get_wallet_rpc("pq")
        descs = self.wpq_descriptors(wallet)
        assert_equal(len(descs), 2)
        assert_equal(sorted(d["internal"] for d in descs), [False, True])
        for d in descs:
            assert_equal(d["active"], True)
            assert "mpqid1" in d["desc"] and "mpqprv" not in d["desc"]

        self.log.info("Addresses follow the specification")
        roots = set()
        for d in self.wpq_descriptors(wallet, private=True):
            key, *path = d["desc"].split("#")[0][len("wpq("):-1].split("/")
            # Regtest uses coin type 1; receiving and change are chains 0h and 1h.
            assert_equal(path, ["1h", "0h", f"{int(d['internal'])}h", "*h"])
            roots.add(decode_bech32m(key, "mpqprv"))
        # Both chains hang off one root.
        assert_equal(len(roots), 1)
        node_bytes = roots.pop()
        root = (node_bytes[:32], node_bytes[32:])
        for index in range(2):
            result = wallet.getnewpqaddress()
            expected = self.ref.key_vector(root, f"m/1h/0h/0h/{index}h")
            assert_equal(result["pubkey"], expected["pubkey"])
            assert_equal(result["program"], expected["program"])
            assert_equal(result["parameter_set"], "SLH-DSA-SHA2-128s")
            assert result["address"].startswith("bcrt1z")
            assert_equal(wallet.getaddressinfo(result["address"])["ismine"], True)

    def test_restore_and_spend(self):
        self.log.info("A wallet restored from its wpq() descriptors alone finds and spends its PQ coins")
        node = self.nodes[0]
        node.createwallet("source")
        source = node.get_wallet_rpc("source")
        # Pay two addresses in separate blocks, so the restore must extend its lookahead in between.
        amounts = [Decimal("10"), Decimal("5")]
        for amount in amounts:
            address = source.getnewpqaddress()["address"]
            self.funder.sendtoaddress(address, amount)
            self.generate(node, 1)
        assert_equal(source.getbalances()["mine"]["trusted"], sum(amounts))

        node.createwallet("restored", blank=True)
        restored = node.get_wallet_rpc("restored")
        requests = [{"desc": d["desc"], "timestamp": 0, "active": True, "internal": d["internal"]}
                    for d in self.wpq_descriptors(source, private=True)]
        assert_equal(len(requests), 2)
        with node.assert_debug_log(["fast variant using block filters"]):
            results = restored.importdescriptors(requests)
        for result in results:
            assert_equal(result["success"], True)
            # The PQ HD node counts as the descriptor's private key.
            assert_equal(result.get("warnings", []), ["Range not given, using default keypool range"])
        assert_equal(restored.getbalances()["mine"]["trusted"], sum(amounts))

        self.log.info("The restored wallet signs its P2WPQH inputs")
        destination = self.funder.getnewaddress()
        txid = restored.sendtoaddress(destination, Decimal("14"))
        tx = restored.gettransaction(txid=txid, verbose=True)["decoded"]
        assert_equal(len(tx["vin"]), 2)
        # A wallet with only PQ keys sends its change to a PQ address.
        assert any(vout["scriptPubKey"]["address"].startswith("bcrt1z") for vout in tx["vout"])
        for vin in tx["vin"]:
            witness = vin["txinwitness"]
            assert_equal(len(witness), 3)
            assert len(witness[0]) // 2 >= SPHINCS_SHA2_128S_SIG_SIZE
        self.generate(node, 1)
        assert_equal(self.funder.gettransaction(txid)["confirmations"], 1)

        # The change went to the restored wallet's PQ change chain, which the source shares.
        change = restored.getbalances()["mine"]["trusted"]
        assert Decimal("0") < change < Decimal("1")
        assert_equal(source.getbalances()["mine"]["trusted"], change)

        self.log.info("PQ coins and keys survive a restart")
        self.restart_node(0)
        for name in (self.default_wallet_name, "pq", "source", "restored"):
            if name not in node.listwallets():
                node.loadwallet(name)
        # RPC handles from before the restart carry the old cookie.
        self.funder = node.get_wallet_rpc(self.default_wallet_name)
        source = node.get_wallet_rpc("source")
        restored = node.get_wallet_rpc("restored")
        assert_equal(restored.getbalances()["mine"]["trusted"], change)
        address = source.getnewpqaddress()["address"]
        self.funder.sendtoaddress(address, Decimal("1"))
        self.generate(node, 1)
        assert_equal(restored.getbalances()["mine"]["trusted"], change + 1)
        assert_equal(source.getbalances()["mine"]["trusted"], change + 1)

        self.log.info("Public wpq() descriptors can't be imported")
        node.createwallet("public", blank=True)
        public = node.get_wallet_rpc("public")
        result = public.importdescriptors([{"desc": self.wpq_descriptors(source)[0]["desc"], "timestamp": "now"}])[0]
        assert_equal(result["success"], False)
        assert "without its mpqprv node" in result["error"]["message"]

    def test_encryption(self):
        self.log.info("An encrypted wallet keeps its PQ HD node encrypted")
        node = self.nodes[0]
        node.createwallet("enc")
        wallet = node.get_wallet_rpc("enc")
        first = wallet.getnewpqaddress()
        priv = self.wpq_descriptors(wallet, private=True)[0]["desc"]
        node_bytes = decode_bech32m(priv.split("#")[0][len("wpq("):].split("/")[0], "mpqprv")
        assert node_bytes in self.read_wallet_file("enc")

        wallet.encryptwallet(PASSPHRASE)
        data = self.read_wallet_file("enc")
        assert node_bytes not in data, "plaintext PQ HD node left in encrypted wallet file"
        assert bytes.fromhex(first["pubkey"]) in data
        assert_raises_rpc_error(-13, "walletpassphrase", wallet.listdescriptors, True)

        self.log.info("A locked wallet hands out keys derived earlier, then needs unlocking")
        # The test framework uses -keypool=1: one key is derived ahead.
        wallet.getnewpqaddress()
        assert_raises_rpc_error(-13, "walletpassphrase", wallet.getnewpqaddress)

        self.funder.sendtoaddress(first["address"], Decimal("2"))
        self.generate(node, 1)
        assert_raises_rpc_error(-13, "walletpassphrase", wallet.sendtoaddress, self.funder.getnewaddress(), Decimal("1"))
        wallet.walletpassphrase(PASSPHRASE, 600)
        wallet.getnewpqaddress()
        wallet.sendtoaddress(self.funder.getnewaddress(), Decimal("1"))
        self.generate(node, 1)
        wallet.walletlock()

    def test_missing_descriptors(self):
        self.log.info("A wallet without wpq() descriptors gets them on the first getnewpqaddress")
        node = self.nodes[0]
        node.createwallet("old", blank=True)
        old = node.get_wallet_rpc("old")
        assert_equal(self.wpq_descriptors(old), [])
        result = old.getnewpqaddress()
        assert_equal(len(self.wpq_descriptors(old)), 2)
        assert_equal(old.getaddressinfo(result["address"])["ismine"], True)
        assert "Back it up again" in result["warning"]
        assert "warning" not in old.getnewpqaddress()

        node.createwallet("oldenc", blank=True, passphrase=PASSPHRASE)
        oldenc = node.get_wallet_rpc("oldenc")
        assert_raises_rpc_error(-13, "walletpassphrase", oldenc.getnewpqaddress)
        oldenc.walletpassphrase(PASSPHRASE, 600)
        oldenc.getnewpqaddress()
        assert_equal(len(self.wpq_descriptors(oldenc)), 2)

        node.createwallet("watchonly", disable_private_keys=True)
        assert_raises_rpc_error(-4, "cannot hold post-quantum keys", node.get_wallet_rpc("watchonly").getnewpqaddress)


if __name__ == '__main__':
    WalletPQHDTest(__file__).main()
