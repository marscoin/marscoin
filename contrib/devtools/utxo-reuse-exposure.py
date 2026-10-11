#!/usr/bin/env python3
# Copyright (c) 2026 The Marscoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Count hashed outputs whose public key is already revealed (work order MQ-31, part 2).

A P2PKH output only shows a hash of its public key, but that key becomes public
once any transaction reveals it: a spend from the same address, a P2PK or bare
multisig output paying the same key, or a multisig redeem script containing it.
From then on every output at that address is as exposed as a P2PK output.

This tool reads a `dumptxoutset` snapshot and the node's raw block files
(blocks/blk*.dat, XOR-deobfuscated with blocks/xor.dat, merged-mining headers
skipped), collects every revealed public key, and reports how much P2PKH value
in the snapshot sits at an address whose key was revealed at or below the
snapshot height. P2SH outputs count as revealed once their redeem script has
appeared in a spend. Reveal heights are resolved through the node's RPC
interface, and only main-chain blocks at or below the snapshot height count.

Only aggregate numbers are printed.

Usage:
  utxo-reuse-exposure.py <snapshot> --datadir <dir> --tip-height N
      [--rpcport 8332] [--spot-check N] [--json]
"""

import argparse
import base64
import hashlib
import http.client
import importlib.util
import json
import os
import random
import struct
import sys
import time
from collections import defaultdict

_spec = importlib.util.spec_from_file_location(
    "utxo_exposure_census", os.path.join(os.path.dirname(os.path.abspath(__file__)), "utxo-exposure-census.py"))
census = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(census)

MAINNET_MAGIC = bytes.fromhex("fbc0b6db")
VERSION_AUXPOW = 1 << 8
XOR_CHUNK = 8 << 20  # multiple of the 8-byte key, so the key phase stays aligned
MAX_REVEALS_PER_ADDRESS = 4
OP_CHECKSIG = 0xAC
OP_CHECKMULTISIG = 0xAE


def hash160(data):
    return hashlib.new("ripemd160", hashlib.sha256(data).digest()).digest()


def is_pubkey(push):
    n = len(push)
    return (n == 33 and push[0] in (2, 3)) or (n == 65 and push[0] == 4)


def script_pushes(script):
    """Data pushes of a script; stops at the first malformed push."""
    pushes = []
    i, n = 0, len(script)
    while i < n:
        op = script[i]
        i += 1
        if op == 0:
            continue
        if op < 0x4C:
            size = op
        elif op == 0x4C:
            if i + 1 > n:
                break
            size = script[i]
            i += 1
        elif op == 0x4D:
            if i + 2 > n:
                break
            size = script[i] | (script[i + 1] << 8)
            i += 2
        elif op == 0x4E:
            if i + 4 > n:
                break
            size = int.from_bytes(script[i:i + 4], "little")
            i += 4
        else:
            continue
        if i + size > n:
            break
        pushes.append(bytes(script[i:i + size]))
        i += size
    return pushes


# --- snapshot ---------------------------------------------------------------

def load_snapshot(path, tip_height):
    """Return (p2pkh, p2sh, other_stats, totals). p2pkh/p2sh map hash -> [(height, value), ...]."""
    p2pkh = defaultdict(list)
    p2sh = defaultdict(list)
    other = defaultdict(lambda: [0, 0])
    total_coins = total_value = 0
    with open(path, "rb") as f:
        r = census.Reader(f)
        head = r.read(5)
        if head != census.SNAPSHOT_MAGIC:
            raise SystemExit("unsupported snapshot format (expected a versioned snapshot)")
        r.read(2 + 4)
        base_hash = r.read(32)[::-1].hex()
        coins = struct.unpack("<Q", r.read(8))[0]
        read = 0
        while read < coins:
            r.read(32)
            for _ in range(r.compact_size()):
                r.compact_size()
                height = r.varint() >> 1
                value = census.decompress_amount(r.varint())
                size = r.varint()
                if size == 0:
                    p2pkh[r.read(20)].append((height, value))
                elif size == 1:
                    p2sh[r.read(20)].append((height, value))
                elif size in (2, 3, 4, 5):
                    r.read(32)
                    bucket = other[("p2pk", census.age_bucket(height, tip_height))]
                    bucket[0] += 1
                    bucket[1] += value
                else:
                    raw = r.read(size - 6)
                    kind = census.classify_raw(raw)
                    if kind == "p2pkh":
                        p2pkh[bytes(raw[3:23])].append((height, value))
                    elif kind == "p2sh":
                        p2sh[bytes(raw[2:22])].append((height, value))
                    else:
                        bucket = other[(kind, census.age_bucket(height, tip_height))]
                        bucket[0] += 1
                        bucket[1] += value
                total_coins += 1
                total_value += value
                read += 1
    return p2pkh, p2sh, other, (base_hash, total_coins, total_value)


# --- block files ------------------------------------------------------------

def read_block_file(path, key):
    data = bytearray(open(path, "rb").read())
    if key and any(key):
        for start in range(0, len(data), XOR_CHUNK):
            seg = data[start:start + XOR_CHUNK]
            pad = (key * (len(seg) // 8 + 1))[:len(seg)]
            data[start:start + len(seg)] = (int.from_bytes(seg, "little") ^ int.from_bytes(pad, "little")).to_bytes(len(seg), "little")
    return data


def compact(mv, o):
    n = mv[o]
    if n < 253:
        return n, o + 1
    if n == 253:
        return mv[o + 1] | (mv[o + 2] << 8), o + 3
    if n == 254:
        return int.from_bytes(mv[o + 1:o + 5], "little"), o + 5
    return int.from_bytes(mv[o + 1:o + 9], "little"), o + 9


def parse_tx(mv, o, collect):
    """Parse one transaction starting at o; feed scripts to collect(); return the new offset."""
    o += 4
    segwit = mv[o] == 0 and mv[o + 1] != 0
    if segwit:
        o += 2
    nin, o = compact(mv, o)
    for _ in range(nin):
        o += 36
        size, o = compact(mv, o)
        if collect:
            collect("in", mv[o:o + size])
        o += size + 4
    nout, o = compact(mv, o)
    for _ in range(nout):
        o += 8
        size, o = compact(mv, o)
        if collect:
            collect("out", mv[o:o + size])
        o += size
    if segwit:
        for _ in range(nin):
            items = []
            nitems, o = compact(mv, o)
            for _ in range(nitems):
                size, o = compact(mv, o)
                items.append(mv[o:o + size])
                o += size
            if collect and items:
                collect("wit", items)
    return o + 4


def skip_auxpow(mv, o):
    o = parse_tx(mv, o, None)          # parent coinbase (another chain's transaction)
    o += 32                            # hashBlock
    n, o = compact(mv, o)
    o += 32 * n + 4                    # coinbase merkle branch, nIndex
    n, o = compact(mv, o)
    o += 32 * n + 4                    # chain merkle branch, nChainIndex
    return o + 80                      # parent block header


class Scanner:
    def __init__(self, p2pkh, p2sh):
        self.p2pkh = p2pkh
        self.p2sh = p2sh
        self.reveals = defaultdict(set)      # ("pkh"|"sh", hash) -> {block hash, ...}
        self.block_hash = None
        self.blocks = self.txs = self.bad_blocks = self.auxpow_blocks = 0
        self.pubkeys_seen = 0

    def _key(self, pk):
        self.pubkeys_seen += 1
        h = hash160(pk)
        if h in self.p2pkh:
            s = self.reveals[("pkh", h)]
            if len(s) < MAX_REVEALS_PER_ADDRESS:
                s.add(self.block_hash)

    def _redeem(self, script):
        h = hash160(script)
        if h in self.p2sh:
            s = self.reveals[("sh", h)]
            if len(s) < MAX_REVEALS_PER_ADDRESS:
                s.add(self.block_hash)
        if script and script[-1] in (OP_CHECKSIG, OP_CHECKMULTISIG):
            for push in script_pushes(script):
                if is_pubkey(push):
                    self._key(push)

    def collect(self, where, data):
        if where == "in":
            pushes = script_pushes(data)
            for push in pushes:
                if is_pubkey(push):
                    self._key(push)
            if pushes and not is_pubkey(pushes[-1]) and len(pushes[-1]) > 1:
                self._redeem(pushes[-1])
        elif where == "out":
            script = bytes(data)
            n = len(script)
            if (n in (35, 67) and script[-1] == OP_CHECKSIG) or (n >= 37 and script[-1] == OP_CHECKMULTISIG):
                for push in script_pushes(script):
                    if is_pubkey(push):
                        self._key(push)
        else:  # witness stack
            for item in data:
                item = bytes(item)
                if is_pubkey(item):
                    self._key(item)
            last = bytes(data[-1])
            if len(data) > 1 and not is_pubkey(last) and len(last) > 1:
                self._redeem(last)

    def scan_file(self, path, key):
        data = read_block_file(path, key)
        mv = memoryview(data)
        o, n = 0, len(data)
        while o + 8 <= n:
            if mv[o:o + 4] != MAINNET_MAGIC:
                break  # zero padding at the end of a preallocated file
            size = int.from_bytes(mv[o + 4:o + 8], "little")
            start = o + 8
            end = start + size
            if size < 80 or end > n:
                break  # incomplete record (file still being written)
            header = bytes(mv[start:start + 80])
            self.block_hash = hashlib.sha256(hashlib.sha256(header).digest()).digest()  # internal byte order
            try:
                p = start + 80
                if int.from_bytes(header[0:4], "little") & VERSION_AUXPOW:
                    self.auxpow_blocks += 1
                    p = skip_auxpow(mv, p)
                ntx, p = compact(mv, p)
                for _ in range(ntx):
                    p = parse_tx(mv, p, self.collect)
                self.txs += ntx
                if p != end:
                    self.bad_blocks += 1
            except (IndexError, ValueError):
                self.bad_blocks += 1
            self.blocks += 1
            o = end
        mv.release()


# --- RPC --------------------------------------------------------------------

class RPC:
    def __init__(self, datadir, port):
        cookie = open(os.path.join(datadir, ".cookie")).read().strip()
        self.auth = "Basic " + base64.b64encode(cookie.encode()).decode()
        self.port = port

    def batch(self, calls):
        conn = http.client.HTTPConnection("127.0.0.1", self.port, timeout=600)
        body = json.dumps([{"jsonrpc": "1.0", "id": i, "method": m, "params": p} for i, (m, p) in enumerate(calls)])
        conn.request("POST", "/", body, {"Authorization": self.auth, "Content-Type": "application/json"})
        replies = json.loads(conn.getresponse().read())
        conn.close()
        return [x for x in sorted(replies, key=lambda x: x["id"])]


def block_heights(rpc, hashes):
    """Map block hash (internal byte order) -> height for main-chain blocks (others are omitted)."""
    heights = {}
    hashes = list(hashes)
    for i in range(0, len(hashes), 2000):
        chunk = hashes[i:i + 2000]
        for h, reply in zip(chunk, rpc.batch([("getblockheader", [h[::-1].hex(), True]) for h in chunk])):
            res = reply.get("result")
            if res and res.get("confirmations", -1) >= 1:
                heights[h] = res["height"]
    return heights


def spot_check(rpc, scanner, exposed, count):
    """Re-derive a sample of reveals from the node's own decoded blocks."""
    sample = random.Random(1).sample(sorted(exposed), min(count, len(exposed)))
    confirmed = 0
    for key in sample:
        kind, h = key
        block = sorted(scanner.reveals[key])[0][::-1].hex()
        found = False
        for tx in rpc.batch([("getblock", [block, 2])])[0]["result"]["tx"]:
            scripts = [bytes.fromhex(v["scriptSig"]["hex"]) for v in tx["vin"] if "scriptSig" in v]
            scripts += [bytes.fromhex(o["scriptPubKey"]["hex"]) for o in tx["vout"]]
            for v in tx["vin"]:
                scripts += [bytes.fromhex(w) for w in v.get("txinwitness", [])]
            for script in scripts:
                candidates = [script] + script_pushes(script)
                if kind == "pkh" and any(is_pubkey(c) and hash160(c) == h for c in candidates):
                    found = True
                if kind == "sh" and any(hash160(c) == h for c in candidates):
                    found = True
        confirmed += found
    return confirmed, len(sample)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("snapshot")
    parser.add_argument("--datadir", required=True)
    parser.add_argument("--tip-height", type=int, required=True, help="snapshot height")
    parser.add_argument("--rpcport", type=int, default=8332)
    parser.add_argument("--spot-check", type=int, default=0)
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    t0 = time.time()

    p2pkh, p2sh, other, (base_hash, coins, total) = load_snapshot(args.snapshot, args.tip_height)
    t_snapshot = time.time() - t0

    scanner = Scanner(p2pkh, p2sh)
    blocks_dir = os.path.join(args.datadir, "blocks")
    key = open(os.path.join(blocks_dir, "xor.dat"), "rb").read() if os.path.exists(os.path.join(blocks_dir, "xor.dat")) else b""
    files = sorted(f for f in os.listdir(blocks_dir) if f.startswith("blk") and f.endswith(".dat"))
    for name in files:
        scanner.scan_file(os.path.join(blocks_dir, name), key)
    t_scan = time.time() - t0 - t_snapshot

    rpc = RPC(args.datadir, args.rpcport)
    all_blocks = set().union(*scanner.reveals.values()) if scanner.reveals else set()
    heights = block_heights(rpc, all_blocks)
    exposed = {k for k, blocks in scanner.reveals.items()
               if any(b in heights and heights[b] <= args.tip_height for b in blocks)}

    by_age = defaultdict(lambda: [0, 0])
    sums = {"pkh": [0, 0, 0], "sh": [0, 0, 0]}   # addresses, coins, value
    for kind, h in exposed:
        outputs = (p2pkh if kind == "pkh" else p2sh)[h]
        sums[kind][0] += 1
        for height, value in outputs:
            sums[kind][1] += 1
            sums[kind][2] += value
            bucket = by_age[census.age_bucket(height, args.tip_height)]
            bucket[0] += 1
            bucket[1] += value
    p2pkh_value = sum(v for outs in p2pkh.values() for _, v in outs)
    p2pk_value = sum(v for (kind, _), (_, v) in other.items() if kind == "p2pk")
    checks = spot_check(rpc, scanner, exposed, args.spot_check) if args.spot_check else None

    result = {
        "base_block_hash": base_hash, "coins": coins, "total_value": total,
        "p2pkh_addresses": len(p2pkh), "p2pkh_value": p2pkh_value, "p2sh_addresses": len(p2sh),
        "exposed_reuse": {"p2pkh_addresses": sums["pkh"][0], "p2pkh_coins": sums["pkh"][1], "p2pkh_value": sums["pkh"][2],
                          "p2sh_addresses": sums["sh"][0], "p2sh_coins": sums["sh"][1], "p2sh_value": sums["sh"][2]},
        "exposed_reuse_by_age": {a: {"count": c, "value": v} for a, (c, v) in by_age.items()},
        "exposed_p2pk_value": p2pk_value,
        "scan": {"block_files": len(files), "blocks": scanner.blocks, "auxpow_blocks": scanner.auxpow_blocks,
                 "transactions": scanner.txs, "unparsed_blocks": scanner.bad_blocks,
                 "pubkeys_seen": scanner.pubkeys_seen, "reveal_blocks": len(all_blocks),
                 "reveal_blocks_in_main_chain": len(heights)},
        "spot_check": {"confirmed": checks[0], "sampled": checks[1]} if checks else None,
        "seconds": {"snapshot": round(t_snapshot), "scan": round(t_scan), "total": round(time.time() - t0)},
    }
    if args.json:
        json.dump(result, sys.stdout, indent=2)
        print()
        return

    def mars(v):
        return f"{v / census.COIN:,.2f} MARS"

    def pct(v):
        return f"{100 * v / total:.2f}%"

    e = result["exposed_reuse"]
    s = result["scan"]
    print(f"Snapshot {base_hash} at height {args.tip_height}: {coins:,} coins, {mars(total)}")
    print(f"P2PKH: {len(p2pkh):,} addresses, {mars(p2pkh_value)} ({pct(p2pkh_value)})")
    print(f"Scanned {s['block_files']} block files: {s['blocks']:,} blocks ({s['auxpow_blocks']:,} merged-mined), "
          f"{s['transactions']:,} transactions, {s['unparsed_blocks']} unparsed, {s['pubkeys_seen']:,} public keys seen")
    print(f"\nExposed by an earlier reveal (at or below height {args.tip_height}):")
    print(f"  P2PKH  {e['p2pkh_addresses']:>9,} addresses {e['p2pkh_coins']:>10,} coins  {mars(e['p2pkh_value']):>22}  {pct(e['p2pkh_value'])}")
    print(f"  P2SH   {e['p2sh_addresses']:>9,} addresses {e['p2sh_coins']:>10,} coins  {mars(e['p2sh_value']):>22}  {pct(e['p2sh_value'])}")
    print("\nBy age:")
    for _, label in census.AGE_BUCKETS:
        c, v = by_age.get(label, (0, 0))
        if c:
            print(f"  {label:11s} {c:>10,} coins  {mars(v):>22}  {pct(v)}")
    exposed_total = p2pk_value + e["p2pkh_value"] + e["p2sh_value"]
    print(f"\nExposed in total (P2PK at rest + revealed hashed): {mars(exposed_total)} ({pct(exposed_total)})")
    if checks:
        print(f"Spot checks against the node's decoded blocks: {checks[0]}/{checks[1]} confirmed")
    print(f"Time: snapshot {result['seconds']['snapshot']} s, scan {result['seconds']['scan']} s, total {result['seconds']['total']} s")


if __name__ == "__main__":
    main()
