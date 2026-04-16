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
- Build gate: `--enable-pq-oqs-vendor`
- Default state: disabled (non-activating)

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

Validation currently checks only:

1. payload is non-empty,
2. parameter set id is supported,
3. payload length matches expected size for that parameter set.

No cryptographic verification is performed in this scaffold.

Deterministic error strings:

- `Empty SPHINCS+ payload`
- `Unsupported SPHINCS+ parameter set`
- `Invalid SPHINCS+ payload length`

## Explicit Non-Goals (v1 Scaffold)

- No transaction/script activation
- No OP code or witness program changes
- No wallet signing path
- No mempool/consensus acceptance changes

## Follow-up Work

1. Bind parsed payloads to new signature destination/script types.
2. Vendor pinned `liboqs` snapshot and wire backend build/link path.
3. Add deterministic vector-based sign/verify tests.
4. Gate consensus activation behind deployment parameters.
