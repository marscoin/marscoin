# SPHINCS+ Signature Scaffold v1

This document defines the first non-activating scaffold for SPHINCS+
signature handling in Marscoin Core.

## Purpose

- Add explicit types and parser validation hooks for SPHINCS+ payloads.
- Keep consensus behavior unchanged while implementation work is staged.
- Provide stable test vectors and deterministic error outputs.

## Backend decision

Marscoin will use a pinned, vendored backend strategy for SPHINCS/SLH-DSA,
following the same determinism model used for RandomX integration.

- Selected integration library: `open-quantum-safe/liboqs`
- Upstream tag: `0.15.0`
- Upstream commit: `97f6b86b1b6d109cfd43cf276ae39c2e776aed80`
- Build gate: `--enable-pq-oqs-vendor`
- Default state: disabled (non-activating)
- Vendored snapshot path: `src/crypto/oqs_vendor/liboqs`
- Vendor build helper: `src/crypto/oqs_vendor/build-liboqs-vendor.sh`
- Minimal build target: `SIG_sphincs_sha2_128s_simple`

Rationale:

- avoid system-dependent crypto availability differences,
- ensure reproducible consensus behavior,
- keep provenance explicit through pinned source metadata.

## Scaffold Payload Format

For test-only parsing, the payload format is:

`[1-byte parameter set id][raw signature bytes]`

Currently recognized parameter set id:

- `0x00`: `SLH-DSA-SHA2-128s`

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

When the OQS backend is enabled (`--enable-pq-oqs-vendor`), full
cryptographic operations are available:

- **`GenerateKeypair`**: produces a SPHINCS+ keypair via OQS.
- **`SignMessage`**: signs a message and produces a scaffold payload
  (`[param_set_id][raw_signature]`), then self-validates the output
  format before returning.
- **`VerifyMessage`**: validates format, checks parameter set match,
  validates key length, and performs cryptographic verification via OQS.

When the backend is not enabled, these functions return descriptive
errors and the scaffold operates in format-validation-only mode.

## Deterministic Known Answer Test (KAT)

A deterministic RNG (xorshift64, Marsaglia constants 13/7/17) is used
to produce reproducible test vectors for regression detection.

Seeds:

- Keygen: `0x4d415253514e4554` (ASCII: `MARSQNET`)
- Signing: `0x5349474e41545552` (ASCII: `SIGNATUR`)
- Message: `marsqnet-sign-test`

Expected values:

- Public key: `8a69a3c0db2ef0d7c439fff27bab906d2b8bcb8c7048556e30bc3bb8ffc8403b`
- Payload SHA-256: `6b2f4f0a998f29de8919c170a8dad69a44c735e37deb3168c2a0b95d87c2fd23`

These values are validated by:

1. Boost unit test: `crypto_tests/pq_sphincs_signature_scaffold` (tests the Marscoin wrapper layer)
2. Standalone KAT script: `contrib/devtools/oqs-sphincs-kat.sh` (tests liboqs directly)
3. CI job: `linux-pq-boost-tests` (runs Boost test with OQS backend enabled)

## Explicit Non-Goals (v1 Scaffold)

- No transaction/script activation
- No OP code or witness program changes
- No wallet signing path
- No mempool/consensus acceptance changes

## Follow-up Work

1. Bind parsed payloads to new signature destination/script types.
2. Gate consensus activation behind deployment parameters.
3. Commission external review for backend/runtime assumptions.
