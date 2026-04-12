# RandomX Consensus Profile v1 (Scaffold)

This document defines the non-activating RandomX consensus profile scaffold for
the quantum-upgrade program.

## Purpose

- Pin an exact upstream RandomX source baseline for future consensus use.
- Prevent accidental floating updates across node implementations.
- Establish deterministic flag requirements before activation work starts.

## Pinned Upstream Baseline

- Repository: `tevador/RandomX`
- Tag: `v2.0`
- Commit: `e0db3c4a8de36d77f50c12f7099bc37401cab88c`

## Required Consensus Flag Policy (Scaffold)

- `RANDOMX_FLAG_V2` must be present in the consensus verification profile.
- Additional runtime optimization flags (JIT, large pages, full mem, hardware
  AES) are implementation details and must not change hash results.

## Current Scope

- This branch adds profile constants and tests only.
- No block validation, mining, retargeting, or chainparams activation changes.
- No RandomX source vendoring in this PR.

## Follow-up Work

1. Vendor the pinned RandomX source at the selected commit.
2. Add deterministic block-header-to-hash test vectors.
3. Add non-activating PoW verification path behind explicit feature gating.
4. Launch quantum-testnet rehearsal before any mainnet transition planning.
