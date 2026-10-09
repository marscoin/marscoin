# SPHINCS+ Signature Scaffold v1

This document defines the first non-activating scaffold for SPHINCS+
signature handling in Marscoin Core.

## Purpose

- Add explicit types and parser validation hooks for SPHINCS+ payloads.
- Keep consensus behavior unchanged while implementation work is staged.
- Provide stable test vectors and deterministic error outputs.

## Backend decision

SLH-DSA comes from a pinned, vendored copy of slhdsa-c, compiled into every
build. Consensus can't depend on an optional component: a node built without
signature verification would reject blocks that spend witness v2 outputs.

- Implementation: `pq-code-package/slhdsa-c` (the FIPS 205 code liboqs wraps)
- Upstream commit: `a0fc1ff253930060d0246aebca06c2538eb92b88`
- Vendored path: `src/crypto/slhdsa/` (byte-identical to upstream; provenance
  and file hashes in its README)
- Build: always, as `crypto/libmarscoin_crypto_slhdsa.la`; no configure flag.
  `--enable-pq-oqs-vendor` is obsolete and ignored.
- Parameter set used: SLH-DSA-SHA2-128s, pure interface

Rationale:

- avoid system-dependent crypto availability differences,
- ensure reproducible consensus behavior,
- keep provenance explicit through pinned source metadata.

## Scaffold Payload Format

For test-only parsing, the payload format is:

`[1-byte parameter set id][raw signature bytes]`

Currently recognized parameter set id:

- `0x01`: FIPS 205 `SLH-DSA-SHA2-128s`, pure interface, signed with the context
  string `marscoin-p2wpqh-v1`
- `0x00` (round-3 SPHINCS+-SHA2-128s-simple) is retired and rejected

Current size constants for this scaffold:

- Public key bytes: `32`
- Secret key bytes: `64`
- Signature bytes: `7856`

## Current Validation Semantics

Format validation (`ValidateSignatureEncoding`) checks:

1. payload is non-empty,
2. parameter set id is supported,
3. payload length matches expected size for that parameter set.

Deterministic error strings:

- `Empty SPHINCS+ payload`
- `Unsupported SPHINCS+ parameter set`
- `Invalid SPHINCS+ payload length`

Cryptographic operations (`src/crypto/pq_sphincs.h`), all with the context
string `marscoin-p2wpqh-v1`:

- **`VerifyMessage`**: validates format, checks the parameter set and key
  length, and verifies the signature (FIPS 205 `slh_verify`).
- **`GenerateKeypairFromSeeds`**: FIPS 205 `slh_keygen_internal` from
  SK.seed, SK.prf and PK.seed; deterministic.
- **`SignMessageWithRandomness`**: FIPS 205 `slh_sign` with caller-supplied
  `opt_rand` (or the deterministic variant), producing `[param_set_id][signature]`.
- **`GenerateKeypair`** and **`SignMessage`**: the same with fresh randomness
  from `GetStrongRandBytes` (hedged signing).

## Known-answer tests

NIST ACVP vectors for SLH-DSA-SHA2-128s (`src/test/data/slh_dsa_sha2_128s_acvp.json`:
10 keyGen, 14 sigVer, 6 sigGen) are checked by:

1. Boost unit test `crypto_tests/pq_slh_dsa_acvp_vectors`, with
   `crypto_tests/pq_slh_dsa_signing_context` covering the Marscoin context.
2. Standalone script `contrib/devtools/slh-dsa-kat.sh`, which compiles only the
   vendored sources (CI job `linux-pq-slh-dsa-kat`).
3. CI job `linux-pq-boost-tests`, which also runs
   `script_tests/p2wpqh_spend_verifies` in a default build.

## Explicit Non-Goals (v1 Scaffold)

- No transaction/script activation
- No OP code or witness program changes
- No wallet signing path
- No mempool/consensus acceptance changes

## Follow-up Work

1. Bind parsed payloads to new signature destination/script types.
2. Gate consensus activation behind deployment parameters.
3. Commission external review for backend/runtime assumptions.
