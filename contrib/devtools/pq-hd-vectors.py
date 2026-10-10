#!/usr/bin/env python3
# Copyright (c) 2026 The Marscoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Reference implementation and test vectors for PQ HD v1.

This implements doc/quantum-pq-key-derivation-v1.md independently of the C++
code: the hash-only key tree, the HKDF expansion into FIPS 205 key-generation
seeds, a minimal SLH-DSA-SHA2-128s key generation, and P2WPQH addresses.

    contrib/devtools/pq-hd-vectors.py --check-acvp
        Check the SLH-DSA key generation against the NIST ACVP vectors in
        src/test/data/slh_dsa_sha2_128s_acvp.json.

    contrib/devtools/pq-hd-vectors.py > src/test/data/pq_hd_vectors.json
        Write the test vectors.

Key generation is pure Python and takes about a second per key.
"""
import argparse
import hashlib
import hmac
import json
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "test", "functional"))
from test_framework.segwit_addr import Encoding, bech32_encode, convertbits, encode_segwit_address  # noqa: E402

ROOT_HMAC_KEY = b"Marscoin PQ seed"
KEYGEN_INFO = b"Marscoin P2WPQH keygen"
NODE_HRP = "mpqprv"
NODE_ID_TAG = b"Marscoin/PQHD/node-id"
NODE_ID_HRP = "mpqid"
HARDENED = 0x80000000
PARAM_SET_SLH_DSA_SHA2_128S = 0x01
SEED_SIZE = {PARAM_SET_SLH_DSA_SHA2_128S: 16}
ADDRESS_HRP = {107: "mars", 1: "mqt"}  # mainnet, and marsqnet for coin type 1
P2WPQH_WITNESS_VERSION = 2

# SLH-DSA-SHA2-128s (FIPS 205, table 2): n = 16, h' = 9, d = 7, lg_w = 4, so
# w = 16 and len = len1 + len2 = 32 + 3 = 35.
N = 16
HP = 9
D = 7
W = 16
WOTS_LEN = 35

# Address types (FIPS 205, section 4.2).
WOTS_HASH = 0
WOTS_PK = 1
TREE = 2
WOTS_PRF = 5


def adrs_c(layer, tree, addr_type, word1, word2, word3):
    """Compressed 22-byte address of the SHA2 instantiation (FIPS 205, section 11.2)."""
    return bytes([layer]) + tree.to_bytes(8, "big") + bytes([addr_type]) + struct.pack(">III", word1, word2, word3)


class Sha2Category1:
    """PRF, F, H and T_l for security category 1: Trunc_n(SHA-256(PK.seed || 0^(64-n) || ADRSc || M))."""

    def __init__(self, pk_seed):
        self.prefix = hashlib.sha256(pk_seed + bytes(64 - N))

    def __call__(self, adrs, message):
        h = self.prefix.copy()
        h.update(adrs)
        h.update(message)
        return h.digest()[:N]


def wots_pk_gen(hash_fn, sk_seed, layer, keypair):
    """FIPS 205 Algorithm 6 (wots_pkGen) for tree address 0."""
    ends = b""
    for chain in range(WOTS_LEN):
        x = hash_fn(adrs_c(layer, 0, WOTS_PRF, keypair, chain, 0), sk_seed)
        for step in range(W - 1):
            x = hash_fn(adrs_c(layer, 0, WOTS_HASH, keypair, chain, step), x)
        ends += x
    return hash_fn(adrs_c(layer, 0, WOTS_PK, keypair, 0, 0), ends)


def slh_keygen_internal(sk_seed, sk_prf, pk_seed):
    """FIPS 205 Algorithm 18: PK.root is the root of the top XMSS tree (Algorithm 9)."""
    assert len(sk_seed) == len(sk_prf) == len(pk_seed) == N
    hash_fn = Sha2Category1(pk_seed)
    layer = D - 1
    nodes = [wots_pk_gen(hash_fn, sk_seed, layer, i) for i in range(1 << HP)]
    for height in range(1, HP + 1):
        nodes = [hash_fn(adrs_c(layer, 0, TREE, 0, height, i), nodes[2 * i] + nodes[2 * i + 1])
                 for i in range(len(nodes) // 2)]
    pk = pk_seed + nodes[0]
    return sk_seed + sk_prf + pk, pk


def root_from_seed(seed):
    """Root node: HMAC-SHA512(key="Marscoin PQ seed", data=seed)."""
    if not 16 <= len(seed) <= 64:
        return None
    i = hmac.new(ROOT_HMAC_KEY, seed, hashlib.sha512).digest()
    return i[:32], i[32:]


def derive_child(node, index):
    """Hardened child: HMAC-SHA512(key=chaincode, data=0x00 || key || ser32(index))."""
    if not index & HARDENED:
        return None
    key, chaincode = node
    i = hmac.new(chaincode, b"\x00" + key + index.to_bytes(4, "big"), hashlib.sha512).digest()
    return i[:32], i[32:]


def parse_path(path):
    parts = path.split("/")
    assert parts[0] == "m"
    indices = []
    for part in parts[1:]:
        hardened = part[-1:] in ("h", "'")
        index = int(part[:-1] if hardened else part)
        indices.append(index | HARDENED if hardened else index)
    return indices


def derive_path(node, path):
    for index in parse_path(path):
        node = derive_child(node, index)
        if node is None:
            return None
    return node


def keygen_seeds(node, param_set):
    """HKDF-Expand-SHA512 (RFC 5869) with PRK = key || chaincode and
    info = "Marscoin P2WPQH keygen" || param_set, split into SK.seed, SK.prf, PK.seed."""
    n = SEED_SIZE[param_set]
    prk = node[0] + node[1]
    info = KEYGEN_INFO + bytes([param_set])
    okm, block, counter = b"", b"", 1
    while len(okm) < 3 * n:
        block = hmac.new(prk, block + info + bytes([counter]), hashlib.sha512).digest()
        okm += block
        counter += 1
    return okm[:n], okm[n:2 * n], okm[2 * n:3 * n]


def encode_node(node):
    return bech32_encode(Encoding.BECH32M, NODE_HRP, convertbits(node[0] + node[1], 8, 5))


def node_id(node):
    """Tagged hash (BIP340 style) of key || chaincode."""
    tag = hashlib.sha256(NODE_ID_TAG).digest()
    return hashlib.sha256(tag + tag + node[0] + node[1]).digest()


def encode_node_id(identifier):
    return bech32_encode(Encoding.BECH32M, NODE_ID_HRP, convertbits(identifier, 8, 5))


def key_vector(root, path):
    leaf = derive_path(root, path)
    sk_seed, sk_prf, pk_seed = keygen_seeds(leaf, PARAM_SET_SLH_DSA_SHA2_128S)
    _, pubkey = slh_keygen_internal(sk_seed, sk_prf, pk_seed)
    program = hashlib.sha256(bytes([PARAM_SET_SLH_DSA_SHA2_128S]) + pubkey).digest()
    coin_type = parse_path(path)[0] & ~HARDENED
    return {
        "path": path,
        "key": leaf[0].hex(),
        "chaincode": leaf[1].hex(),
        "parameter_set": PARAM_SET_SLH_DSA_SHA2_128S,
        "sk_seed": sk_seed.hex(),
        "sk_prf": sk_prf.hex(),
        "pk_seed": pk_seed.hex(),
        "pubkey": pubkey.hex(),
        "program": program.hex(),
        "address": encode_segwit_address(ADDRESS_HRP[coin_type], P2WPQH_WITNESS_VERSION, program),
    }


SEEDS = [
    {
        "comment": "BIP32 test vector 1 seed (16 bytes)",
        "seed": "000102030405060708090a0b0c0d0e0f",
        "paths": ["m/107h/0h/0h/0h", "m/107h/0h/0h/1h", "m/107h/0h/1h/0h", "m/107h/1h/0h/0h", "m/1h/0h/0h/0h"],
    },
    {
        "comment": "BIP32 test vector 2 seed (64 bytes)",
        "seed": "fffcf9f6f3f0edeae7e4e1dedbd8d5d2cfccc9c6c3c0bdbab7b4b1aeaba8a5a2"
                "9f9c999693908d8a8784817e7b7875726f6c696663605d5a5754514e4b484542",
        "paths": ["m/107h/0h/0h/0h", "m/1h/0h/0h/0h"],
    },
    {
        "comment": "BIP39 seed of the mnemonic 'abandon abandon abandon abandon abandon abandon abandon abandon "
                   "abandon abandon abandon about' with passphrase 'TREZOR' (BIP39 test vector 1)",
        "seed": "c55257c360c07c72029aebc1b53c05ed0362ada38ead3e3e9efa3708e53495531f09a6987599d18264c1e1c92f2cf141"
                "630c7a3c4ab7c81b2f001698e7463b04",
        "paths": ["m/107h/0h/0h/0h", "m/107h/0h/1h/0h"],
    },
]


def make_vectors():
    vectors = []
    for entry in SEEDS:
        root = root_from_seed(bytes.fromhex(entry["seed"]))
        vectors.append({
            "comment": entry["comment"],
            "seed": entry["seed"],
            "root": {
                "key": root[0].hex(),
                "chaincode": root[1].hex(),
                "encoded": encode_node(root),
                "id": node_id(root).hex(),
                "id_encoded": encode_node_id(node_id(root)),
            },
            "keys": [key_vector(root, path) for path in entry["paths"]],
        })

    root = root_from_seed(bytes.fromhex(SEEDS[0]["seed"]))
    payload = convertbits(root[0] + root[1], 8, 5)
    valid = encode_node(root)
    bad_checksum = valid[:-1] + ("q" if valid[-1] != "q" else "p")
    valid_id = encode_node_id(node_id(root))
    bad_id_checksum = valid_id[:-1] + ("q" if valid_id[-1] != "q" else "p")
    return {
        "comment": "PQ HD v1 test vectors (doc/quantum-pq-key-derivation-v1.md), "
                   "generated by contrib/devtools/pq-hd-vectors.py",
        "vectors": vectors,
        "invalid_seeds": [(bytes(range(15))).hex(), (bytes(range(65))).hex()],
        "invalid_paths": [
            {"seed": SEEDS[0]["seed"], "path": "m/107h/0h/0h/0", "comment": "non-hardened index"},
            {"seed": SEEDS[0]["seed"], "path": "m/107/0h/0h/0h", "comment": "non-hardened index"},
        ],
        "invalid_encodings": [
            {"encoded": bech32_encode(Encoding.BECH32, NODE_HRP, payload), "comment": "Bech32 instead of Bech32m"},
            {"encoded": bech32_encode(Encoding.BECH32M, "mpqpub", payload), "comment": "wrong prefix"},
            {"encoded": bech32_encode(Encoding.BECH32M, NODE_HRP, convertbits((root[0] + root[1])[:63], 8, 5)),
             "comment": "63-byte payload"},
            {"encoded": bad_checksum, "comment": "bad checksum"},
            {"encoded": valid.upper()[:10] + valid[10:], "comment": "mixed case"},
            {"encoded": encode_node_id(node_id(root)), "comment": "an identifier is not a node"},
        ],
        "invalid_id_encodings": [
            {"encoded": bad_id_checksum, "comment": "bad checksum"},
            {"encoded": bech32_encode(Encoding.BECH32, NODE_ID_HRP, convertbits(node_id(root), 8, 5)),
             "comment": "Bech32 instead of Bech32m"},
            {"encoded": bech32_encode(Encoding.BECH32M, NODE_ID_HRP, convertbits(node_id(root)[:31], 8, 5)),
             "comment": "31-byte identifier"},
            {"encoded": valid, "comment": "a node is not an identifier"},
        ],
    }


def check_acvp():
    path = os.path.join(os.path.dirname(__file__), "..", "..", "src", "test", "data", "slh_dsa_sha2_128s_acvp.json")
    with open(path, encoding="utf8") as f:
        cases = [c for c in json.load(f) if isinstance(c, dict) and c["type"] == "keyGen"]
    for case in cases:
        sk, pk = slh_keygen_internal(bytes.fromhex(case["skSeed"]), bytes.fromhex(case["skPrf"]),
                                     bytes.fromhex(case["pkSeed"]))
        if sk.hex().upper() != case["sk"].upper() or pk.hex().upper() != case["pk"].upper():
            sys.exit(f"ACVP keyGen tcId {case['tcId']}: mismatch")
    print(f"ACVP keyGen: {len(cases)} cases match")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--check-acvp", action="store_true", help="check key generation against NIST ACVP")
    args = parser.parse_args()
    if args.check_acvp:
        check_acvp()
        return
    print(json.dumps(make_vectors(), indent=2))


if __name__ == "__main__":
    main()
