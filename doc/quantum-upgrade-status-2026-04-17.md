# Quantum Upgrade Status & Roadmap

**Date**: 2026-04-17
**Branch**: `feature/quantum-upgrade` (bc99bbea51)
**Internal working document -- not for check-in**

---

## 1. Proposal Recap (Three Pillars)

1. **SPHINCS+ (SLH-DSA-SHA2-128s)** replacing ECDSA -- post-quantum signatures
2. **RandomX v2** replacing Scrypt -- CPU-egalitarian mining (mining fairness upgrade bundled with quantum work)
3. **UTXO Recycling** -- 24-month migration window, unmigrated coins redistributed as mining rewards + governance treasury, no supply inflation

Reference: https://www.marscoin.org/academy/quantum-upgrade/

---

## 2. What's Done

| Feature | Status | Evidence |
|---------|--------|---------|
| SPHINCS+ crypto backend | **Done** | liboqs 0.15.0 vendored, keygen/sign/verify working, deterministic KAT passing in CI |
| SPHINCS+ scaffold types | **Done** | `pq_sphincs.h/cpp` -- ParameterSet enum, payload format `[param_set_id][sig_bytes]`, format validation |
| RandomX vendoring | **Done** | tevador/RandomX v2.0 pinned, wrapper with `InitCache/HashOnce/ReleaseCache` |
| RandomX consensus profile | **Done** | `randomx_profile.h` -- FLAG_V2 validation, profile integrity check |
| MarsQNet devnet | **Done** | Live 3-node network, distinct ports/message bytes/address prefixes, blocks producing and propagating |
| PQ address prefix detection | **Scaffolded** | `IsPostQuantumAddress()` in key_io -- recognizes `mars1pq...` but returns "not enabled yet" |
| Wallet migration status RPC | **Scaffolded** | `getquantummigrationstatus` -- read-only, reports balances, hardcoded `phase: scaffold` |
| CI infrastructure | **Done** | Linux CLI build, macOS GUI build, PQ OQS KAT, PQ Boost unit tests |
| Build gating | **Done** | `--enable-pq-oqs-vendor` and `--enable-randomx-vendor`, non-activating by default |
| Test vectors | **Done** | JSON vectors for SPHINCS+, RandomX, PQ addresses; deterministic KAT with MARSQNET/SIGNATUR seeds |

### PRs Merged

| PR | Title | Target |
|----|-------|--------|
| #38 | PQ address prefix hooks | feature/quantum-upgrade |
| #40 | SPHINCS+ parsing scaffold | feature/quantum-upgrade |
| #41 | RandomX v2 vendoring + profile | feature/quantum-upgrade |
| #42 | Migration wallet status RPC | feature/quantum-upgrade |
| #44 | liboqs vendoring + KAT | 28.x (absorbed into feature/quantum-upgrade via merge) |

---

## 3. What's Not Started

| Feature | Status | Notes |
|---------|--------|-------|
| Dynamic block sizing | **Absent** | Fixed `MAX_BLOCK_WEIGHT = 4000000`, no adaptive logic |
| SPHINCS+ script/consensus | **Absent** | No witness version, no OP codes, no script interpreter changes |
| PQ address full encoding | **Absent** | No keygen, no scriptPubKey mapping, no round-trip encode/decode |
| Wallet PQ key management | **Absent** | No SPHINCS+ key type in ScriptPubKeyMan |
| RandomX PoW dispatch | **Absent** | No Scrypt-to-RandomX switching by block height |
| Difficulty recalibration | **Absent** | Only DGW3/ASERT, no RandomX-specific adjustment |
| UTXO recycling rules | **Absent** | Branch exists (`feat/utxo-recycling-rules`), no commits |
| Governance treasury | **Absent** | No treasury allocation code |
| Migration tx creation | **Absent** | RPC is read-only, no fund movement |
| Fee escalation schedule | **Absent** | No legacy tx surcharge logic |
| BADS attestation | **Absent** | No code |

---

## 4. Roadmap (Dependency Order)

### Phase A -- PQ Transactions on Testnet

The critical path. Everything else depends on having a working PQ transaction type.

**A1. Dynamic Block Sizing**
- BCH-style adaptive sizing based on 144-block median
- 1 MB floor, soft ceiling grows with demand
- Independent of PQ crypto -- can test on marsqnet with regular transactions first
- Prerequisite for SPHINCS+ activation (7,856-byte signatures need room)
- Consensus change to `MAX_BLOCK_WEIGHT` calculation in validation

**A2. PQ Witness Version + Script Type**
- Define new witness version (v2) for SPHINCS+ outputs
- Wire `pq::sphincs::VerifyMessage` into script interpreter (`EvalScript` / `VerifyWitnessProgram`)
- Define scriptPubKey format committing to a SPHINCS+ public key hash
- This is the single hardest piece -- where the crypto scaffold meets consensus

**A3. PQ Address Full Encoding**
- Extend `key_io.cpp` beyond prefix detection
- `mars1pq...` encodes/decodes to a scriptPubKey with the PQ witness version
- Bech32m encoding with the PQ witness version byte

**A4. Wallet PQ Key Management**
- Add SPHINCS+ key type to wallet's `ScriptPubKeyMan`
- Generate and store PQ keypairs
- Sign transactions with PQ keys via `pq::sphincs::SignMessage`
- `sendtoaddress` works with PQ outputs

**Milestone**: Create, send, and spend PQ transactions on marsqnet.

### Phase B -- RandomX Consensus (Can Parallel with A2-A4)

**B1. PoW Dispatch**
- Switching logic: below activation height use Scrypt, at/above use RandomX
- Wire `randomx_wrapper::HashOnce` into `CheckProofOfWork`
- Header-aware algorithm selection based on consensus parameters

**B2. Difficulty Recalibration**
- Difficulty reset at switchover height (RandomX hashrates differ from Scrypt)
- Set initial RandomX difficulty from marsqnet observations
- Let DGW3/ASERT recalibrate from there

**Milestone**: marsqnet mines with RandomX under real consensus rules.

### Phase C -- Migration Tooling

**C1. Migration Transaction Creation**
- Upgrade `getquantummigrationstatus` from read-only to actionable
- Add `migrateutxos` RPC: select legacy UTXOs, create txs moving funds to PQ addresses
- Wallet-level only, no consensus changes

**C2. Fee Escalation Schedule**
- After month 18: progressive surcharges on legacy transactions
- Policy check in `AcceptToMemoryPool`
- Consensus-enforced surcharge multiplier based on block height

**Milestone**: Users can migrate their funds with a single RPC call.

### Phase D -- UTXO Recycling + Treasury

**D1. UTXO Expiry Consensus Rules**
- After 24-month cutoff: unmigrated UTXOs become unspendable
- New coinbase rule: miners claim fraction of recycled value per block
- Recycled value spread over 4-8 years to avoid supply shock

**D2. Governance Treasury Allocation**
- 10-20% of recycled coins route to treasury address
- Governance mechanism TBD (multisig? on-chain voting? Martian Republic structure?)

**Milestone**: Recycling economics are live and treasury accumulates.

### Phase E -- Hardening

**E1. BADS (Post-Quantum Attestation)**
- Post-quantum proof-of-identity attestation system
- Depends on PQ transactions working (Phase A)

**E2. External Audit + Bug Bounty**
- Commission review of SPHINCS+ integration, RandomX consensus, UTXO recycling
- Bug bounty program before mainnet activation

**Milestone**: Independent validation of all consensus-critical code.

### Phase F -- Mainnet Activation

- Hard fork with coordinated community upgrade
- RandomX deployment
- Migration window opens (24-month clock starts)

---

## 5. Dependency Graph

```
A1 Dynamic blocks ─────────────────────────────────────┐
                                                        │
A2 PQ witness/script type ──┬── A3 PQ address encoding  │
                            │                           │
                            └── A4 Wallet PQ keys ──────┤
                                                        │
B1 RandomX PoW dispatch ──── B2 Difficulty recalibration │  (parallel with A2-A4)
                                                        │
                            C1 Migration tx creation ───┤  (needs A4)
                            C2 Fee escalation ──────────┤
                                                        │
                            D1 UTXO expiry rules ───────┤  (needs C1)
                            D2 Governance treasury ─────┤  (needs D1)
                                                        │
                            E1 BADS ────────────────────┤  (needs A4)
                            E2 External audit ──────────┤
                                                        │
                            F  Mainnet hard fork ───────┘
```

---

## 6. Design Decisions (Recorded)

- **Dynamic blocks over fixed**: 7,856-byte SPHINCS+ signatures (~100x ECDSA) require adaptive block sizing. Big-block philosophy aligned with original Satoshi vision. Storage is cheap; latency is the real constraint (especially Mars).
- **SPHINCS+ over ML-DSA/FALCON**: Conservative security (SHA-256 only), stateless (no key reuse catastrophes), proven by QRL's 7+ year deployment.
- **RandomX framing**: Mining fairness upgrade, not purely quantum defense. Grover's algorithm gives only quadratic speedup on hash preimages. RandomX eliminates ASIC/FPGA dominance, aligns with colony general-purpose hardware.
- **UTXO recycling**: Unique to small community coins. Lost/hacked coins return to circulation via community governance. No supply inflation -- reveals true circulating supply.
- **Unified security model**: Post-upgrade, zero elliptic curve primitives remain. Mining (RandomX): AES + SHA-256. Signatures (SPHINCS+): SHA-256 only. Address derivation: hash-based. No Shor-vulnerable surface.

---

## 7. Branch Strategy

```
feature/quantum-upgrade  ←  all quantum work funnels here
    ↑
    periodic merges from 28.x to stay current
    ↑
28.x  ←  mainline development
    ↑
    merge feature/quantum-upgrade when full stack is ready
```

Reconciled 2026-04-17: merged 28.x into feature/quantum-upgrade to absorb PR #44.
Going forward: all new quantum PRs target feature/quantum-upgrade.
