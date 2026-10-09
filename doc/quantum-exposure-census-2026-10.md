# Mainnet public-key exposure census, October 2026

Work order MQ-31. This measures how much of the Marscoin supply sits in
outputs whose public key is already visible on chain, the outputs most at risk
if elliptic-curve cryptography fails.

## Method

- A v28.1.2 node synced mainnet to height **3,576,274** (block
  `56948678e200b1be8f8344c5d2926a0e457159c9a7185caa9c71fff80c8c4a6e`).
- `dumptxoutset` wrote the UTXO set (`txoutset_hash`
  `aa40005db24ad2250b5076065ccf411e2a98282285252234a9e263100c37a748`).
- `contrib/devtools/utxo-exposure-census.py` classified every output.
- Check: the tool's totals (467,750 coins, 39,494,028.14 MARS) match the
  node's `gettxoutsetinfo` exactly.

Reproduce with:

```
marscoin-cli dumptxoutset /tmp/utxo.dat
contrib/devtools/utxo-exposure-census.py /tmp/utxo.dat --tip-height <height>
```

## Results

| Class | Coins | MARS | Share of supply |
|---|---:|---:|---:|
| Exposed at rest (P2PK) | 22,221 | 887,459.50 | 2.25% |
| Hashed: P2PKH | 445,514 | 38,575,085.54 | 97.67% |
| Hashed: P2SH | 15 | 31,483.10 | 0.08% |
| P2WPKH, P2WSH, P2TR, P2WPQH | 0 | 0 | 0% |

By age (approximate, from block height at 123-second blocks):

| Class | Age | Coins | MARS | Share |
|---|---|---:|---:|---:|
| Exposed | 5–10 years | 1,360 | 8,500.00 | 0.02% |
| Exposed | 10+ years | 20,861 | 878,959.50 | 2.23% |
| Hashed | < 1 year | 11,548 | 1,317,631.16 | 3.34% |
| Hashed | 1–3 years | 65,626 | 2,804,296.50 | 7.10% |
| Hashed | 3–5 years | 45,973 | 866,207.13 | 2.19% |
| Hashed | 5–10 years | 112,521 | 12,731,425.32 | 32.24% |
| Hashed | 10+ years | 209,861 | 20,887,008.51 | 52.89% |

## What it means

- **Exposure at rest is small: 2.25% of the supply.** It's almost entirely
  P2PK outputs more than ten years old, consistent with early mining rewards.
  These cannot be protected by commit–delay–reveal. Only a derivation-secret
  rescue could save them, and only if their keys came from HD wallets, which
  is unlikely at that age.
- **Mainnet has no SegWit or Taproot outputs at all,** so there is no Taproot
  exposure.
- **The supply is highly dormant.** About 85% sits in hashed outputs that
  haven't moved in five years or more, and 53% in outputs untouched for over
  ten. That matters for the migration window and for UTXO recycling (MQ-39).

## Limits

- **Address reuse isn't counted yet.** A hashed output becomes exposed once
  its public key has been revealed by an earlier spend from the same address,
  and that needs a scan of all spent inputs. The share of hashed value already
  exposed this way is therefore unknown, and the exposed figure above is a
  lower bound.
- **Ages are approximate.** Marscoin's block spacing changed over its history.

Data: the full JSON report and the snapshot are kept off-repo. The tool
regenerates both from any synced node.
