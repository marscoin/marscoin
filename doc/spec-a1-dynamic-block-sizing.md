# Spec A1: Adaptive Block Weight Limit (ABWL)

**Status**: Draft v2
**Depends on**: Nothing (independent, can ship first)
**Required by**: A2 (PQ witness type -- 7,856-byte signatures need room)
**Based on**: BCH CHIP-2023-04 ABLA (activated May 2024, production-proven)

---

## 1. Problem

SPHINCS+ signatures are ~7,856 bytes vs ~72 bytes for ECDSA (~109x larger).
With fixed `MAX_BLOCK_WEIGHT = 4,000,000`, a block can hold roughly:

- Current ECDSA: ~2,500-3,000 typical transactions
- SPHINCS+ (single-input): ~400-500 transactions

This isn't catastrophic -- Marscoin doesn't saturate blocks today -- but it
eliminates headroom and makes the chain brittle under any future load. Dynamic
sizing solves this proactively and aligns with the big-block philosophy.

A naive "2x median" approach (draft v1) is insufficient because:
- It responds too quickly to spam (attacker fills blocks → ceiling jumps)
- It has no burst/surge capacity for legitimate demand spikes
- It shrinks as fast as it grows (no asymmetry to deter manipulation)

## 2. Reference: BCH ABLA (CHIP-2023-04)

BCH activated their Adaptive Blocksize Limit Algorithm in May 2024. It has
~2 years of production data and is considered settled by the BCH community.
We adapt it for Marscoin's weight-based system.

Source: https://gitlab.com/0353F40E/ebaa/-/blob/main/README.md

### How ABLA Works

Two functions combine to produce the block size limit:

```
y_n = ε_n + β_n
```

Where:
- `y_n` = block weight limit for block n
- `ε_n` = **control function** (EWMA of recent block weights, slow-moving)
- `β_n` = **elastic buffer** (surge capacity, decays over ~183 days)

**Control function** (ε): Tracks actual usage via an exponentially weighted
moving average. Grows slowly when blocks are full, shrinks when they're not.
Maximum sustained growth: 2x/year. Maximum sustained shrink: -75%/year.

**Elastic buffer** (β): Provides temporary extra capacity above the control
function. Accumulates during quiet periods, consumed during bursts. Allows
up to 4x/year growth for short bursts following extended quiet periods.
Half-life: ~183 days (~26,296 blocks at 10-min target).

**Asymmetry factor** (ζ = 1.5): The "neutral" block weight is ε/ζ (67% of
the control value). Below this, the limit shrinks. Above this, it grows --
but growing is harder than shrinking. This makes spam attacks expensive:
an attacker must consistently fill blocks above 67% to push the ceiling up,
and the ceiling drops faster when they stop.

## 3. Marscoin ABWL Parameters

Adapted from BCH ABLA for Marscoin's weight-based system.

### Constants

```
ABWL_FLOOR           = 4,000,000    // weight units (current MAX_BLOCK_WEIGHT)
ABWL_ZETA            = 1.5          // asymmetry factor (same as BCH)
ABWL_GAMMA           = 1/37,938     // per-block forget factor (same as BCH)
ABWL_DELTA           = 10           // elastic buffer gearing ratio (same as BCH)
ABWL_THETA           = 1/37,938     // buffer decay rate (same as BCH)
ABWL_TEMPORARY_MAX   = 128,000,000  // 32 MB equivalent, protocol safety cap
```

### Why These Values

**Floor = 4M**: Marscoin's current limit. No block can ever be smaller than
today's maximum. This is conservative -- even under zero usage, capacity
never regresses.

**Zeta, Gamma, Theta = same as BCH**: These values are the result of
extensive simulation and 2 years of production validation. No reason to
diverge. The growth rate (2x/year max sustained) is appropriate for
Marscoin -- it allows the chain to absorb SPHINCS+ adoption gradually.

**Delta = 10**: Same gearing ratio. The elastic buffer grows 10x faster
than the control function when demand spikes.

**Temporary max = 128M (vs BCH's 2B)**: Marscoin is a smaller chain.
128M weight (~32 MB equivalent) is generous headroom. BCH needed 2GB
for their scaling ambitions. We can raise this later if needed -- it's
a protocol parameter, not a fundamental design choice.

### Initial State at Activation

```
ε_activation = ABWL_FLOOR / 2 = 2,000,000
β_activation = ABWL_FLOOR / 2 = 2,000,000
y_activation = ε + β = 4,000,000  (matches current fixed limit exactly)
```

This ensures zero discontinuity at activation. The block weight limit
is exactly 4M immediately before and after the fork.

## 4. Algorithm (Pseudocode)

```
function GetAdaptiveBlockWeightLimit(pindexPrev, params):
    if pindexPrev.height < params.nABWLActivationHeight:
        return 4,000,000  // legacy fixed limit

    // Get previous block's state
    ε_prev = pindexPrev.nABWL_epsilon
    β_prev = pindexPrev.nABWL_beta
    y_prev = ε_prev + β_prev
    x_prev = min(pindexPrev.nBlockWeight, y_prev)  // clamp input

    ζ = ABWL_ZETA
    γ = ABWL_GAMMA
    δ = ABWL_DELTA
    θ = ABWL_THETA

    // -- Control function update --
    if ζ * x_prev > ε_prev:
        // Block was above neutral → grow
        // Discount growth by elastic buffer contribution
        discount = ζ * β_prev * (ζ * x_prev - ε_prev) / (ζ * y_prev - ε_prev)
        ε = ε_prev + γ * (ζ * x_prev - ε_prev - discount)
    else:
        // Block was at or below neutral → shrink
        ε = max(ε_prev + γ * (ζ * x_prev - ε_prev), ABWL_FLOOR / 2)

    // -- Elastic buffer update --
    if ζ * x_prev > ε_prev:
        // Consume buffer during growth, replenish proportional to ε growth
        β = max(β_prev - θ * β_prev + δ * (ε - ε_prev), ABWL_FLOOR / 2)
    else:
        // Decay buffer during quiet periods
        β = max(β_prev - θ * β_prev, ABWL_FLOOR / 2)

    // -- Output --
    y = min(ε + β, ABWL_TEMPORARY_MAX)
    return max(y, ABWL_FLOOR)
```

### Integer Arithmetic

All calculations must use integer arithmetic for consensus determinism.
BCH uses fixed-point scaling (multiply by large constants, divide at the end)
to avoid floating-point. We follow the same approach.

Implementation detail: γ = 1/37,938 is implemented as integer division:
```cpp
int64_t gamma_term = (zeta_scaled * x_prev - epsilon_prev) / 37938;
```

The exact integer arithmetic encoding should follow BCH's BCHN implementation
for proven correctness.

## 5. State Storage

### CBlockIndex Changes

Add two fields to `CBlockIndex` for the ABWL state:

```cpp
int64_t nABWL_epsilon{0};  // control function value at this block
int64_t nABWL_beta{0};     // elastic buffer value at this block
int64_t nBlockWeight{0};   // actual block weight (needed as algorithm input)
```

These are computed during `AcceptBlock()` and stored in the block index
database. On reindex, they are recomputed from block data.

At activation height, initialize:
```cpp
nABWL_epsilon = ABWL_FLOOR / 2;
nABWL_beta = ABWL_FLOOR / 2;
```

### Backward Compatibility

For blocks before activation, `nBlockWeight` can be computed from serialized
size (`nSize * WITNESS_SCALE_FACTOR` as approximation for pre-SegWit blocks,
or exact weight for SegWit blocks). The ABWL state fields are unused and
default to 0.

## 6. Files to Modify

### consensus/consensus.h

Keep existing constants. Add ABWL constants:

```cpp
// Adaptive Block Weight Limit (ABWL) parameters
static constexpr int64_t ABWL_WEIGHT_FLOOR = 4000000;
static constexpr int64_t ABWL_TEMPORARY_MAX = 128000000;
static constexpr int64_t ABWL_GAMMA_DIVISOR = 37938;
static constexpr int64_t ABWL_THETA_DIVISOR = 37938;
static constexpr int64_t ABWL_DELTA = 10;
// Zeta = 3/2 (use integer ratio to avoid float)
static constexpr int64_t ABWL_ZETA_NUM = 3;
static constexpr int64_t ABWL_ZETA_DEN = 2;
```

### consensus/params.h

```cpp
int nABWLActivationHeight{0};  // 0 = not activated
```

### validation.cpp

**CheckBlock()**: Use `ABWL_TEMPORARY_MAX` for fast-reject (context-free).

**ContextualCheckBlock()**: Call `GetAdaptiveBlockWeightLimit()` with
`pindexPrev` for the real dynamic check.

**New function**: `GetAdaptiveBlockWeightLimit()` implementing the algorithm
above. Should live in a new file `src/consensus/blockweight.cpp` for clean
separation.

### node/miner.cpp

`BlockAssembler` queries `GetAdaptiveBlockWeightLimit()` at template
creation time. `-blockmaxweight` CLI flag caps at this value (miner
can choose to build smaller, never larger).

### chain.h (CBlockIndex)

Add `nABWL_epsilon`, `nABWL_beta`, `nBlockWeight` fields.
Serialize/deserialize in block index DB.

### policy/policy.h

`DEFAULT_BLOCK_MAX_WEIGHT` becomes a fallback for pre-activation.
Post-activation, the default is `GetAdaptiveBlockWeightLimit() - 4000`.

## 7. Behavioral Properties

### Growth Dynamics

| Scenario | Per-block change | Annual result |
|----------|-----------------|---------------|
| All blocks 100% full | +0.0013%/block | ~2x/year |
| All blocks 67% full (neutral) | 0 | No change |
| All blocks empty | -0.0026%/block | ~-75%/year |
| Burst after long quiet | Up to 4x/year (buffer) | Buffer consumed |

### Spam Resistance

An attacker filling blocks to 100% consistently:
- Ceiling grows at most 2x/year
- Attack cost: must pay fees for every byte in every block, sustained
- When attacker stops: ceiling shrinks at 2x the per-block rate it grew
- Asymmetry factor means they lose ground faster than they gain it

### Neutral Point

The "neutral" block weight is `ε / ζ = ε / 1.5 = 67%` of the control value.
Below this, the ceiling shrinks. Above this, it grows. This means normal usage
at ~50-60% capacity keeps the ceiling stable -- only sustained heavy usage
pushes it up.

## 8. Test Plan

### Unit tests
- `GetAdaptiveBlockWeightLimit()` with:
  - Pre-activation height → returns fixed 4M
  - At activation → returns exactly 4M (ε/2 + β/2)
  - Sequence of full blocks → ceiling grows at expected rate
  - Sequence of empty blocks → ceiling shrinks to floor
  - Burst after quiet → elastic buffer provides surge capacity
  - Input clamping: block weight > limit → clamped to limit
  - Integer arithmetic: no overflow at extreme values
  - Floor enforcement: ceiling never drops below 4M

### Functional tests
- Mine blocks on regtest with ABWL active
- Block at exactly the limit → accepted
- Block 1 weight unit over → rejected
- Simulate 1000 blocks of various sizes → verify ceiling trajectory
  matches expected EWMA behavior

### Marsqnet validation
- Deploy before SPHINCS+ activation
- Monitor ceiling evolution under real traffic
- Verify ceiling adapts when PQ transactions are introduced

## 9. Activation

- **Marsqnet**: activate at current height + 100 block buffer
- **Mainnet**: activate at hard fork height (same fork as SPHINCS+)
- **Regtest**: height 0 (always active)
- Hard fork: blocks exceeding old 4M limit are rejected by non-upgraded nodes

## 10. Risks and Mitigations

**Consensus divergence from BCH implementation**: We adapt ABLA but change
the floor, temporary max, and initial state. Any arithmetic differences
could cause consensus bugs.
- Mitigation: Port BCH's integer arithmetic verbatim (proven correct),
  only change parameterization. Extensive test vectors.

**Disk format change**: Adding fields to CBlockIndex requires reindex
for existing nodes upgrading.
- Mitigation: This is a hard fork anyway. Reindex is expected.
  Document in upgrade instructions.

**Memory/CPU overhead**: EWMA computation per block is O(1) (constant
time, no window traversal). Much cheaper than the naive median approach
which required O(144) lookups.

## 11. References

- CHIP-2023-04: https://gitlab.com/0353F40E/ebaa/-/blob/main/README.md
- BCH ABLA Discussion: https://bitcoincashresearch.org/t/chip-2023-04-adaptive-blocksize-limit-algorithm-for-bitcoin-cash/1037
- BCH May 2024 Upgrade: https://upgradespecs.bitcoincashnode.org/2024-05-15-upgrade/
- BCHN v27.0.0 Implementation: https://docs.bitcoincashnode.org/doc/release-notes/release-notes-27.0.0/
