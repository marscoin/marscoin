# Mainnet public-key exposure census, October 2026

Work order MQ-31. This measures how much of the Marscoin supply sits in
outputs whose public key is already visible on chain, the outputs most at risk
if elliptic-curve cryptography fails.

An output's key is visible in two ways:

1. **At rest:** the output itself contains the public key (P2PK, bare multisig,
   P2TR).
2. **By an earlier reveal:** the output pays a hash (P2PKH, P2SH), but the key
   behind that hash has already appeared on chain. That happens through a
   spend from the same address, a P2PK or multisig output paying the same key,
   or a published redeem script. From then on every output at that address is
   as exposed as a P2PK output.

## Method

- A v28.1.2 node synced mainnet to height **3,576,274** (block
  `56948678e200b1be8f8344c5d2926a0e457159c9a7185caa9c71fff80c8c4a6e`).
- `dumptxoutset` wrote the UTXO set (`txoutset_hash`
  `aa40005db24ad2250b5076065ccf411e2a98282285252234a9e263100c37a748`).
- `contrib/devtools/utxo-exposure-census.py` classified every output.
- `contrib/devtools/utxo-reuse-exposure.py` read the node's raw block files:
  - It removed the block-file obfuscation and skipped merged-mining (auxpow)
    headers.
  - It parsed every transaction and collected each public key revealed by
    input scripts, P2SH redeem scripts, witnesses, and P2PK or bare multisig
    outputs.
  - It then matched those keys against the hashed outputs in the snapshot.
  - A reveal counts only if it is in a main-chain block at or below the
    snapshot height, checked through the node's RPC interface.

Checks:

- **Totals:** the snapshot totals (467,750 coins, 39,494,028.14 MARS) match the
  node's `gettxoutsetinfo` exactly.
- **Blocks:** the block scan parsed 3,576,283 blocks, which is the node's height
  at scan time plus genesis, with 0 unparseable blocks. 392,295 blocks were
  merged-mined.
- **Transactions:** it parsed 3,924,405 transactions, equal to the node's
  `getchaintxstats` count at that height.
- **Stale blocks:** none. The node had a single chain tip, and blocks after the
  snapshot height are ignored for reveals.
- **Spot checks:** 25 randomly sampled reveals were re-derived from the node's
  own decoded blocks (`getblock <hash> 2`). All 25 confirmed.

Reproduce with:

```
marscoin-cli dumptxoutset /tmp/utxo.dat
contrib/devtools/utxo-exposure-census.py /tmp/utxo.dat --tip-height <height>
contrib/devtools/utxo-reuse-exposure.py /tmp/utxo.dat --datadir <datadir> --tip-height <height> --spot-check 25
```

The block scan takes about half a minute on a laptop.

## Results

### Output types

| Class | Coins | MARS | Share of supply |
|---|---:|---:|---:|
| P2PK (key at rest) | 22,221 | 887,459.50 | 2.25% |
| P2PKH | 445,514 | 38,575,085.54 | 97.67% |
| P2SH | 15 | 31,483.10 | 0.08% |
| P2WPKH, P2WSH, P2TR, P2WPQH | 0 | 0 | 0% |

The P2PKH supply sits at 25,410 distinct addresses.

### Exposure

| Class | Coins | MARS | Share of supply |
|---|---:|---:|---:|
| Exposed at rest (P2PK) | 22,221 | 887,459.50 | 2.25% |
| Exposed by an earlier reveal (P2PKH, 4,176 addresses) | 248,962 | 17,820,215.19 | 45.12% |
| Exposed by an earlier reveal (P2SH) | 0 | 0 | 0% |
| **Exposed in total** | **271,183** | **18,707,674.69** | **47.37%** |
| Hashed, key not revealed | 196,567 | 20,786,353.45 | 52.63% |

### By age

Ages are approximate, computed from block height at 123-second blocks.

| Class | Age | Coins | MARS | Share |
|---|---|---:|---:|---:|
| Exposed at rest | 5–10 years | 1,360 | 8,500.00 | 0.02% |
| Exposed at rest | 10+ years | 20,861 | 878,959.50 | 2.23% |
| Exposed by reveal | < 1 year | 7,528 | 187,308.15 | 0.47% |
| Exposed by reveal | 1–3 years | 44,526 | 594,834.22 | 1.51% |
| Exposed by reveal | 3–5 years | 14,802 | 238,172.37 | 0.60% |
| Exposed by reveal | 5–10 years | 82,128 | 4,352,850.29 | 11.02% |
| Exposed by reveal | 10+ years | 99,978 | 12,447,050.17 | 31.52% |

For the hashed supply as a whole (revealed or not), 85% hasn't moved in five
years or more, and 53% in over ten.

## What it means

- **Nearly half the supply is exposed today: 47.37%.** Only 2.25% is exposed at
  rest. The rest is address reuse: 4,176 P2PKH addresses whose key earlier
  transactions already revealed hold 45.12% of all MARS.
- **Commit–delay–reveal can't protect these outputs.** Their keys are already
  public, so after an elliptic-curve break an attacker can sign for them
  directly. The only protections are:
  - moving the coins to P2WPQH before a break;
  - after a break, a derivation-secret rescue, which works only for keys from
    HD wallets (see `doc/quantum-emergency-rescue-v1.md`).
- **Much of the exposed value is old.** 31.52% of the supply is exposed and
  untouched for more than ten years. Some of it is probably lost. Whether such
  coins are frozen, recycled or left alone is the policy question of MQ-39, and
  the size makes it a first-order decision.
- **Migration should target the reused addresses first.** Address reuse
  concentrates the exposure in a few thousand addresses, so outreach to their
  owners (exchanges, pools, long-time holders) covers most of the exposed
  value. Wallets should stop reusing addresses now, independent of any PQ
  activation.
- **Mainnet has no SegWit or Taproot outputs,** so there is no Taproot exposure.

## Limits

- **The exposed share is a lower bound.** Keys revealed off chain are not
  counted, nor keys used on other chains: a key reused on a Bitcoin-derived
  chain shows its public key there. Scripts that reveal keys in non-standard
  encodings would also be missed.
- **Ages are approximate.** Marscoin's block spacing changed over its history.

Data: the full JSON reports and the snapshot are kept off repo. Both tools
regenerate them from any synced node. The reports contain aggregates only.
