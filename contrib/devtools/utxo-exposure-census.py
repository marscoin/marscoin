#!/usr/bin/env python3
# Copyright (c) 2026 The Marscoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Classify the UTXO set by public-key exposure (quantum work order MQ-31).

Reads a snapshot written by `marscoin-cli dumptxoutset <path>` and reports,
for each output class, how many coins and how much value it holds, broken down
by age. The classes say whether spending the output reveals a public key that
is already visible on chain:

  exposed  P2PK, bare multisig, P2TR: the public key is in the output itself.
  hashed   P2PKH, P2WPKH, P2SH, P2WSH: only a hash is visible until spent.
  pq       P2WPQH (witness v2): hash-based post-quantum key.
  other    Unspendable (OP_RETURN) or non-standard scripts.

Hashed outputs become exposed once their key has been revealed by an earlier
spend from the same address. Finding those needs a scan of spent inputs and is
not done here.

Usage: utxo-exposure-census.py <snapshot> [--tip-height N] [--json]
"""

import argparse
import json
import struct
import sys
from collections import defaultdict

COIN = 100_000_000
SNAPSHOT_MAGIC = b"utxo\xff"
BLOCKS_PER_YEAR = 365.25 * 24 * 3600 / 123  # Marscoin targets 123-second blocks
AGE_BUCKETS = [(1, "< 1 year"), (3, "1-3 years"), (5, "3-5 years"), (10, "5-10 years"), (float("inf"), "10+ years")]


class Reader:
    def __init__(self, f):
        self.f = f

    def read(self, n):
        data = self.f.read(n)
        if len(data) != n:
            raise EOFError("unexpected end of snapshot")
        return data

    def compact_size(self):
        n = self.read(1)[0]
        if n < 253:
            return n
        return struct.unpack("<" + {253: "H", 254: "I", 255: "Q"}[n], self.read({253: 2, 254: 4, 255: 8}[n]))[0]

    def varint(self):
        """Bitcoin Core's MSB base-128 VARINT (serialize.h)."""
        n = 0
        while True:
            b = self.read(1)[0]
            n = (n << 7) | (b & 0x7F)
            if b & 0x80:
                n += 1
            else:
                return n


def decompress_amount(x):
    """Inverse of CompressAmount (compressor.cpp)."""
    if x == 0:
        return 0
    x -= 1
    e = x % 10
    x //= 10
    if e < 9:
        d = (x % 9) + 1
        x //= 9
        n = x * 10 + d
    else:
        n = x + 1
    return n * 10**e


def read_script(r):
    """Read a compressed script (compressor.h). Returns (kind, script_bytes_or_None)."""
    size = r.varint()
    if size == 0:
        r.read(20)
        return "p2pkh", None
    if size == 1:
        r.read(20)
        return "p2sh", None
    if size in (2, 3, 4, 5):
        r.read(32)
        return "p2pk", None
    return None, r.read(size - 6)


def classify_raw(script):
    n = len(script)
    if n > 0 and script[0] == 0x6A:
        return "op_return"
    if n == 22 and script[0] == 0x00 and script[1] == 0x14:
        return "p2wpkh"
    if n == 34 and script[0] == 0x00 and script[1] == 0x20:
        return "p2wsh"
    if n == 34 and script[0] == 0x51 and script[1] == 0x20:
        return "p2tr"
    if n == 34 and script[0] == 0x52 and script[1] == 0x20:
        return "p2wpqh"
    if n in (35, 67) and script[-1] == 0xAC and script[0] in (33, 65):
        return "p2pk"
    if n >= 37 and script[-1] == 0xAE and 0x51 <= script[0] <= 0x60:
        return "bare_multisig"
    if n == 25 and script[:3] == b"\x76\xa9\x14" and script[-2:] == b"\x88\xac":
        return "p2pkh"
    if n == 23 and script[:2] == b"\xa9\x14" and script[-1] == 0x87:
        return "p2sh"
    return "nonstandard"


EXPOSURE = {
    "p2pk": "exposed", "bare_multisig": "exposed", "p2tr": "exposed",
    "p2pkh": "hashed", "p2wpkh": "hashed", "p2sh": "hashed", "p2wsh": "hashed",
    "p2wpqh": "pq",
    "op_return": "other", "nonstandard": "other",
}


def age_bucket(height, tip_height):
    if tip_height is None:
        return "unknown"
    years = max(0, tip_height - height) / BLOCKS_PER_YEAR
    for limit, label in AGE_BUCKETS:
        if years < limit:
            return label
    return AGE_BUCKETS[-1][1]


def census(path, tip_height):
    stats = defaultdict(lambda: [0, 0])  # (kind, age) -> [count, value]
    with open(path, "rb") as f:
        r = Reader(f)
        head = r.read(5)
        if head == SNAPSHOT_MAGIC:
            version = struct.unpack("<H", r.read(2))[0]
            network_magic = r.read(4).hex()
            base_hash = r.read(32)[::-1].hex()
        else:  # pre-versioned format: base block hash, then coin count
            version, network_magic = 0, None
            base_hash = (head + r.read(27))[::-1].hex()
        coins_total = struct.unpack("<Q", r.read(8))[0]
        coins_read = 0
        while coins_read < coins_total:
            r.read(32)  # txid
            for _ in range(r.compact_size()):
                r.compact_size()  # output index
                code = r.varint()
                height = code >> 1
                value = decompress_amount(r.varint())
                kind, raw = read_script(r)
                if kind is None:
                    kind = classify_raw(raw)
                bucket = stats[(kind, age_bucket(height, tip_height))]
                bucket[0] += 1
                bucket[1] += value
                coins_read += 1
        trailing = f.read(1)
    return {
        "snapshot_version": version,
        "network_magic": network_magic,
        "base_block_hash": base_hash,
        "coins": coins_total,
        "trailing_data": bool(trailing),
        "stats": stats,
    }


def summarize(result):
    by_kind = defaultdict(lambda: [0, 0])
    by_exposure = defaultdict(lambda: [0, 0])
    by_exposure_age = defaultdict(lambda: [0, 0])
    total_value = 0
    for (kind, age), (count, value) in result["stats"].items():
        for table, key in ((by_kind, kind), (by_exposure, EXPOSURE[kind]), (by_exposure_age, (EXPOSURE[kind], age))):
            table[key][0] += count
            table[key][1] += value
        total_value += value
    return by_kind, by_exposure, by_exposure_age, total_value


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("snapshot")
    parser.add_argument("--tip-height", type=int, help="snapshot height, for age buckets")
    parser.add_argument("--json", action="store_true", help="print machine-readable output")
    args = parser.parse_args()

    result = census(args.snapshot, args.tip_height)
    by_kind, by_exposure, by_exposure_age, total = summarize(result)

    if args.json:
        json.dump({
            "base_block_hash": result["base_block_hash"],
            "coins": result["coins"],
            "total_value": total,
            "by_kind": {k: {"count": c, "value": v} for k, (c, v) in sorted(by_kind.items())},
            "by_exposure": {k: {"count": c, "value": v} for k, (c, v) in sorted(by_exposure.items())},
            "by_exposure_age": {f"{e}|{a}": {"count": c, "value": v} for (e, a), (c, v) in sorted(by_exposure_age.items())},
        }, sys.stdout, indent=2)
        print()
        return

    def pct(v):
        return 100 * v / total if total else 0

    print(f"Snapshot at block {result['base_block_hash']} (format v{result['snapshot_version']}), {result['coins']:,} coins, {total / COIN:,.2f} MARS")
    if result["trailing_data"]:
        print("warning: snapshot has trailing data after the declared coin count")
    print("\nBy exposure:")
    for key in ("exposed", "hashed", "pq", "other"):
        count, value = by_exposure.get(key, (0, 0))
        print(f"  {key:8s} {count:>12,} coins  {value / COIN:>18,.2f} MARS  {pct(value):6.2f}%")
    print("\nBy output type:")
    for kind, (count, value) in sorted(by_kind.items(), key=lambda kv: -kv[1][1]):
        print(f"  {kind:14s} {count:>12,} coins  {value / COIN:>18,.2f} MARS  {pct(value):6.2f}%")
    if args.tip_height is not None:
        print("\nBy exposure and age:")
        for key in ("exposed", "hashed"):
            for _, label in AGE_BUCKETS:
                count, value = by_exposure_age.get((key, label), (0, 0))
                if count:
                    print(f"  {key:8s} {label:11s} {count:>12,} coins  {value / COIN:>18,.2f} MARS  {pct(value):6.2f}%")


if __name__ == "__main__":
    main()
