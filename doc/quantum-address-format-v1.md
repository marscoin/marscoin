# Post-Quantum Address Format v1 (Scaffold)

This document defines the initial scaffold for Marscoin post-quantum addresses.
It is intentionally non-consensus and non-activating in this branch.

## Goals

- Reserve a recognizable user-facing prefix for post-quantum recipients.
- Add parser detection hooks so wallet/RPC can return actionable errors.
- Prepare for a follow-up consensus proposal without disrupting existing address types.

## Reserved Prefix

- Mainnet: `mars1pq...`
- Testnet: `tmars1pq...`
- Signet: `tb1pq...`
- Regtest: `bcrt1pq...`

The reserved prefix is defined as:

`<bech32_hrp> + "1pq"`

Detection in this scaffold is case-insensitive and prefix-only. Payload format,
checksum details, and script mapping are deferred to the full specification.

## Current Behavior (Scaffold Branch)

- `DecodeDestination()` recognizes the reserved prefix.
- Decoding does not produce a spendable destination yet.
- The API returns `CNoDestination` with a deterministic error string:

`Post-quantum address format is recognized but not enabled yet`

## Test Vectors (Prefix Detection Only)

### Expected `IsPostQuantumAddress(...) == true`

- `mars1pqexampleaddress0000000000000000000000`
- `tmars1pqexampleaddress000000000000000000000`
- `tb1pqexampleaddress000000000000000000000000`
- `bcrt1pqexampleaddress0000000000000000000000`

### Expected `IsPostQuantumAddress(...) == false`

- `mars1qxy2kgdygjrsqtzq2n0yrf2493p83kkfjhx0wlh`
- `M4f82c4d228f34c5bde0f6968dcfdbf67e`
- `bc1qw508d6qejxtdg4y5r3zarvary0c5xw7kg3g4ty`

## Next Steps

1. Specify payload encoding and checksum scheme.
2. Define scriptPubKey mapping and destination type.
3. Add full round-trip vectors (`encode -> decode -> script`).
4. Gate activation logic behind explicit consensus deployment parameters.
