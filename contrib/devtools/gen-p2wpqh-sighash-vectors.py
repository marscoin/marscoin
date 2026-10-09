#!/usr/bin/env python3
# Copyright (c) 2026 The Marscoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Reference implementation of the P2WPQH signature hash and vector generator.

This is an independent implementation of doc/quantum-p2wpqh-sighash-v1.md,
written with only the Python standard library. It writes the test vectors in
src/test/data/p2wpqh_sighash_vectors.json, which the C++ unit tests check
against SignatureHashPQ.

Usage: contrib/devtools/gen-p2wpqh-sighash-vectors.py > src/test/data/p2wpqh_sighash_vectors.json
"""

import hashlib
import json
import struct

SIGHASH_DEFAULT = 0x00
SIGHASH_ALL = 0x01
SIGHASH_NONE = 0x02
SIGHASH_SINGLE = 0x03
SIGHASH_ANYONECANPAY = 0x80

TAG = b"Marscoin/P2WPQH/sighash"
PARAM_SET_SLH_DSA_SHA2_128S = 0x01


def sha256(data):
    return hashlib.sha256(data).digest()


def tagged_hash(tag, msg):
    tag_hash = sha256(tag)
    return sha256(tag_hash + tag_hash + msg)


def compact_size(n):
    if n < 0xfd:
        return bytes([n])
    if n <= 0xffff:
        return b"\xfd" + struct.pack("<H", n)
    if n <= 0xffffffff:
        return b"\xfe" + struct.pack("<I", n)
    return b"\xff" + struct.pack("<Q", n)


def ser_script(script):
    return compact_size(len(script)) + script


def ser_outpoint(txid_hex, n):
    # txid_hex is in display order (as shown by RPCs); serialize in internal order.
    return bytes.fromhex(txid_hex)[::-1] + struct.pack("<I", n)


def ser_txout(amount, script):
    return struct.pack("<q", amount) + ser_script(script)


def ser_tx(tx):
    """Serialize without witness data."""
    out = struct.pack("<I", tx["version"])
    out += compact_size(len(tx["vin"]))
    for txin in tx["vin"]:
        out += ser_outpoint(txin["txid"], txin["vout"]) + ser_script(b"") + struct.pack("<I", txin["sequence"])
    out += compact_size(len(tx["vout"]))
    for txout in tx["vout"]:
        out += ser_txout(txout["amount"], txout["script"])
    out += struct.pack("<I", tx["locktime"])
    return out


def is_valid_hash_type(hash_type):
    return hash_type <= 0x03 or 0x81 <= hash_type <= 0x83


def p2wpqh_sighash(tx, spent, in_pos, hash_type, param_set_id, pubkey):
    """Return the 32-byte sighash, or None if it is undefined."""
    if not is_valid_hash_type(hash_type):
        return None
    output_type = SIGHASH_ALL if hash_type == SIGHASH_DEFAULT else hash_type & 0x03
    anyonecanpay = (hash_type & SIGHASH_ANYONECANPAY) != 0

    msg = bytes([0x00, hash_type])  # epoch, hash type
    msg += struct.pack("<I", tx["version"]) + struct.pack("<I", tx["locktime"])
    if not anyonecanpay:
        msg += sha256(b"".join(ser_outpoint(i["txid"], i["vout"]) for i in tx["vin"]))
        msg += sha256(b"".join(struct.pack("<q", s["amount"]) for s in spent))
        msg += sha256(b"".join(ser_script(s["script"]) for s in spent))
        msg += sha256(b"".join(struct.pack("<I", i["sequence"]) for i in tx["vin"]))
    if output_type == SIGHASH_ALL:
        msg += sha256(b"".join(ser_txout(o["amount"], o["script"]) for o in tx["vout"]))
    msg += bytes([0x00])  # spend_type: no annex
    if anyonecanpay:
        txin = tx["vin"][in_pos]
        msg += ser_outpoint(txin["txid"], txin["vout"])
        msg += ser_txout(spent[in_pos]["amount"], spent[in_pos]["script"])
        msg += struct.pack("<I", txin["sequence"])
    else:
        msg += struct.pack("<I", in_pos)
    if output_type == SIGHASH_SINGLE:
        if in_pos >= len(tx["vout"]):
            return None
        o = tx["vout"][in_pos]
        msg += sha256(ser_txout(o["amount"], o["script"]))
    msg += bytes([param_set_id]) + pubkey
    return tagged_hash(TAG, msg)


def p2wpqh_script(param_set_id, pubkey):
    return bytes([0x52, 0x20]) + sha256(bytes([param_set_id]) + pubkey)


def main():
    pubkey_a = bytes(range(0x00, 0x20))
    pubkey_b = bytes(range(0x20, 0x40))
    p2wpkh = bytes([0x00, 0x14]) + bytes.fromhex("751e76e8199196d454941c45d1b3a323f1433bd6")

    tx = {
        "version": 2,
        "locktime": 500000,
        "vin": [
            {"txid": "aa" * 31 + "01", "vout": 0, "sequence": 0xffffffff},
            {"txid": "bb" * 31 + "02", "vout": 7, "sequence": 0xfffffffd},
            {"txid": "cc" * 31 + "03", "vout": 1, "sequence": 0},
        ],
        "vout": [
            {"amount": 200000000, "script": p2wpkh},
            {"amount": 174000000, "script": p2wpqh_script(PARAM_SET_SLH_DSA_SHA2_128S, pubkey_b)},
        ],
    }
    spent = [
        {"amount": 150000000, "script": p2wpqh_script(PARAM_SET_SLH_DSA_SHA2_128S, pubkey_a)},
        {"amount": 25000000, "script": p2wpkh},
        {"amount": 300000000, "script": p2wpqh_script(PARAM_SET_SLH_DSA_SHA2_128S, pubkey_b)},
    ]

    cases = [
        (0, SIGHASH_DEFAULT, pubkey_a, "input 0, SIGHASH_DEFAULT"),
        (0, SIGHASH_ALL, pubkey_a, "input 0, SIGHASH_ALL"),
        (0, SIGHASH_NONE, pubkey_a, "input 0, SIGHASH_NONE"),
        (0, SIGHASH_SINGLE, pubkey_a, "input 0, SIGHASH_SINGLE"),
        (0, SIGHASH_ALL | SIGHASH_ANYONECANPAY, pubkey_a, "input 0, SIGHASH_ALL|ANYONECANPAY"),
        (0, SIGHASH_NONE | SIGHASH_ANYONECANPAY, pubkey_a, "input 0, SIGHASH_NONE|ANYONECANPAY"),
        (0, SIGHASH_SINGLE | SIGHASH_ANYONECANPAY, pubkey_a, "input 0, SIGHASH_SINGLE|ANYONECANPAY"),
        (2, SIGHASH_DEFAULT, pubkey_b, "input 2, SIGHASH_DEFAULT"),
        (2, SIGHASH_ALL | SIGHASH_ANYONECANPAY, pubkey_b, "input 2, SIGHASH_ALL|ANYONECANPAY"),
        (2, SIGHASH_SINGLE, pubkey_b, "input 2, SIGHASH_SINGLE without a matching output: undefined"),
        (2, SIGHASH_SINGLE | SIGHASH_ANYONECANPAY, pubkey_b, "input 2, SIGHASH_SINGLE|ANYONECANPAY without a matching output: undefined"),
        (0, 0x04, pubkey_a, "input 0, invalid hash type 0x04: undefined"),
        (0, 0x84, pubkey_a, "input 0, invalid hash type 0x84: undefined"),
    ]

    vectors = []
    for in_pos, hash_type, pubkey, comment in cases:
        sighash = p2wpqh_sighash(tx, spent, in_pos, hash_type, PARAM_SET_SLH_DSA_SHA2_128S, pubkey)
        vectors.append({
            "comment": comment,
            "input_index": in_pos,
            "hash_type": hash_type,
            "param_set_id": PARAM_SET_SLH_DSA_SHA2_128S,
            "pubkey": pubkey.hex(),
            "sighash": sighash.hex() if sighash is not None else None,
        })

    out = {
        "comment": "P2WPQH signature hash test vectors (doc/quantum-p2wpqh-sighash-v1.md), "
                   "generated by contrib/devtools/gen-p2wpqh-sighash-vectors.py. "
                   "sighash is the raw SHA256 output in byte order, or null where it is undefined.",
        "tx": ser_tx(tx).hex(),
        "spent_outputs": [{"amount": s["amount"], "scriptPubKey": s["script"].hex()} for s in spent],
        "vectors": vectors,
    }
    print(json.dumps(out, indent=2))


if __name__ == "__main__":
    main()
