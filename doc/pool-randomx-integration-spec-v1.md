# Marscoin Pool AI Spec: RandomX Support (v1)

## Purpose

Define the integration contract for pool software and AI automation to support
Marscoin `marsqnet` RandomX mining workflow during the current development
phase, and to remain forward-compatible with public testnet/mainnet transitions.

This document focuses on pool-side responsibilities. Node-side consensus logic
is maintained in Marscoin Core.

## Network profile (current dev phase)

- Chain: `marsqnet`
- P2P port: `29338`
- RPC default (recommended local): `29332`
- Bech32 hrp: `mqt`
- Node software target: Marscoin Core quantum branch builds with
  `--enable-randomx-vendor`

## Pool integration goals

1. Request and distribute valid work templates to CPU miners.
2. Validate submitted shares using the same RandomX profile assumptions.
3. Submit valid blocks to `marscoind` and track acceptance/rejection reasons.
4. Expose clear telemetry for hashrate, stale shares, reject reasons, and block
   candidates.

## Required node RPC interactions

At minimum, pool stack should support:

- `getblocktemplate`
- `submitblock`
- `getmininginfo`
- `getblockchaininfo`
- `getnetworkinfo`

Recommended health checks:

- `getconnectioncount`
- `getpeerinfo`
- `getchaintips`

## Work-unit model (v1)

Pool should treat a job as:

- immutable block template fields from node,
- mutable nonce/time fields according to node policy,
- explicit job id + template hash for replay safety.

Each share submission should carry:

- worker id,
- job id,
- nonce/time mutation values,
- full candidate header context needed for reconstruction.

## RandomX policy assumptions (current)

Pool implementation must align with Marscoin node profile semantics:

- RandomX profile baseline pinned by node implementation,
- no pool-local override of consensus-critical algorithm variant,
- share validation and block candidate construction must be deterministic.

If pool and node disagree on algorithm/profile/flags, pool must fail fast and
emit explicit diagnostics.

## Share validation tiers

### Tier 1: cheap pre-check

- job id exists and active,
- timestamp/nonce within allowed ranges,
- no duplicate nonce replay for same worker/job.

### Tier 2: RandomX share hash check

- recompute hash from reconstructed candidate,
- compare against share target,
- if meets network target, promote to block candidate.

### Tier 3: node submission

- call `submitblock`,
- persist node response (`null`, `duplicate`, `high-hash`, etc.),
- map response to worker accounting and telemetry.

## Stratum compatibility guidance

If using Stratum-like transport:

- include chain id/label in subscribe response (`marsqnet`),
- include job versioning to invalidate stale templates quickly,
- include explicit endianness and field-encoding contract in worker docs.

## Security and abuse controls

- per-worker rate limits,
- duplicate-share suppression,
- malformed payload quarantine,
- nonce exhaustion detection,
- mandatory auth for worker sessions,
- reject reason telemetry visible to operators.

## Persistence requirements

Pool should persist at least:

- submitted shares (accepted/rejected + reason),
- candidate blocks and node responses,
- worker hashrate windows,
- payout-eligible accounting snapshots.

## Observability requirements

Expose metrics for:

- active workers,
- accepted/rejected/stale share rates,
- block candidate submissions and acceptance,
- median template age,
- node RPC latency/error rates,
- effective hashrate by worker and globally.

## AI automation requirements

Pool AI agent should be able to:

1. detect template staleness and trigger refresh,
2. classify reject spikes by likely cause (node drift, stale jobs, malformed
   payloads, peer instability),
3. suggest parameter tuning (job interval, stale timeout, worker difficulty),
4. produce incident summaries with actionable next steps.

AI must never silently change consensus-critical assumptions.

## Compatibility matrix (v1)

Pool rollout phases:

1. **Simulation mode**: ingest templates, emulate shares, no submission.
2. **Shadow mode**: validate real shares, no payout effect.
3. **Active mode**: full share acceptance + candidate submission.

Promotion between phases requires explicit operator approval.

## Failure handling policy

If node reports repeated reject reasons indicating incompatibility:

- halt candidate submission,
- keep collecting diagnostic shares,
- raise high-priority alert,
- require manual acknowledgement before resume.

## Definition of done for pool RandomX support

1. Pool can mine against marsqnet node templates continuously.
2. Valid shares accepted with stable low reject ratio.
3. At least one block candidate accepted by node from pool path.
4. Monitoring dashboards and incident alerts operational.
5. Runbook documented for restart, failover, and reject triage.

## Future extensions

- payout accounting for production economics,
- multi-node template quorum validation,
- external miner compatibility matrix,
- production hardening for public testnet and mainnet transitions.
