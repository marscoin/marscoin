# Marsqnet Progress Report (April 2026)

## Where we started

Marscoin had a strong research direction for a quantum-era roadmap, but no
live network path to validate RandomX integration in production-like
conditions. Earlier historical code had traces of RandomX work, but those paths
were not preserved through the Bitcoin Core 28 rebase and were not active in
consensus behavior.

This created a gap between roadmap ambition and operational proof.

## Goal of this phase

Establish a practical, reproducible development network where Marscoin nodes can
run and validate a RandomX-enabled proof-of-work path without touching mainnet
consensus.

Phase objective:

- make RandomX executable and testable in node code,
- preserve strict non-activation guarantees for mainnet/testnet,
- prove multi-node sync and mining in real infrastructure.

## Architecture decisions and why they mattered

### 1. Pin upstream RandomX source provenance

We pinned upstream RandomX to a known release baseline (`tevador/RandomX`
`v2.0`) and vendored a snapshot in-tree. This avoids floating dependency drift
and gives deterministic provenance for audits.

### 2. Build-gate the integration

RandomX code paths were placed behind `--enable-randomx-vendor` so we can test
aggressively in dev environments while preserving default production behavior.

### 3. Introduce a wrapper before consensus wiring

A thin internal wrapper (`InitCache`, `HashOnce`, lifecycle handling) was added
first, then wired to actual RandomX VM execution. This reduced integration risk
and gave a clean seam for tests.

### 4. Add PoW dispatch scaffold

Header-aware PoW dispatch was introduced so RandomX can be selected by
consensus params while existing networks keep current behavior.

### 5. Create a named dev network identity

We renamed the dev path to `marsqnet` (with `qdevnet` compatibility alias) and
assigned distinct network identity values to avoid accidental overlap:

- message start bytes: `0x4d 0x71 0xa7 0xfa`
- default p2p port: `29338`
- bech32 hrp: `mqt`
- auxpow chain id (dev mode): `0x4D51`

## What we achieved technically

- RandomX v2 source vendored and pinned with metadata.
- VM-backed RandomX hashing wrapper integrated under build gate.
- PoW dispatch scaffold added with RandomX toggle support.
- `marsqnet` chain mode available for dev/test operations.
- Two-node smoke harness added (`contrib/devtools/marsqnet-randomx-smoke.sh`).
- CI job added to run Linux marsqnet RandomX smoke checks.

## Real-world deployment result

Marsqnet was brought up on real DigitalOcean infrastructure (three active
nodes) with service-managed daemons, peer connectivity, and successful block
propagation.

Observed state during validation:

- nodes active and connected (2-3 peers each),
- explorer4 mined blocks on marsqnet,
- other nodes synced to same chain height (`5`).

This is the first meaningful external-infrastructure proof that the current
RandomX dev scaffold can run as an actual network and not just local unit test
logic.

## Challenges we encountered and solved

1. **Autotools parallel build race on some hosts**
   - Symptom: transient `.Tpo -> .Po` rename failures.
   - Resolution: build critical targets sequentially per target where needed.

2. **Regtest-scoped config behavior for addnode settings**
   - Symptom: addnode options rejected unless scoped correctly.
   - Resolution: moved seed peer settings into service startup args.

3. **Host package/repo variance across droplets**
   - Symptom: apt metadata/repo differences and dependency friction.
   - Resolution: hardened deployment flow and retried with explicit update
     compatibility flags.

4. **Mixed host readiness**
   - Some nodes came up quickly, one candidate host still needs additional
     dependency tuning for compilation.

## What this means for Marscoin

Marscoin now has a credible bridge between research and execution:

- a live, named RandomX dev network,
- deterministic source pinning,
- repeatable smoke validation,
- and CI hooks that keep regressions visible.

This is exactly the base needed before opening a broader public quantum testnet
campaign.

## What comes next

1. Expand marsqnet fleet from 3 nodes to 5+ geographically diverse nodes.
2. Add deterministic cross-platform RandomX hash vector checks in CI.
3. Publish operator bootstrap docs and node install scripts.
4. Open semi-public onboarding for external testers/miners.
5. Continue parallel PQ workstreams (address format, signatures, wallet
   migration tooling) against this live network baseline.

## Invitation to developers

If you want to work at the edge of cryptocurrency protocol engineering,
Marscoin needs contributors now.

High-impact areas:

- RandomX validation hardening and vector testing,
- mining/pool integration tooling,
- post-quantum signature pipeline,
- migration safety UX and wallet tooling,
- testnet operations and observability.

Join the effort, review open quantum-track PRs, run marsqnet nodes, and help
shape a production-ready path from quantum roadmap to shipped protocol.
