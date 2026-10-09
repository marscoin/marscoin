# Quantum emergency rescue: specification v1 (draft)

Status: draft for review. Work orders MQ-36 (commit–delay–reveal rescue),
MQ-37 (seed-knowledge rescue) and MQ-38 (break-glass soft fork) in
`doc/quantum-work-orders.md`. Nothing here is implemented yet.

This document specifies what Marscoin does if elliptic-curve signatures stop
being safe before every coin has moved to P2WPQH (witness v2, SLH-DSA). The
break could come from a quantum computer or from a classical algorithmic
result, and either could arrive without warning. The design assumes no warning.

## At a glance

- **Break-glass soft fork (MQ-38).** One buried activation height, `0` in
  normal releases. An emergency release sets it a short time ahead. From that
  height on:
  - outputs locked by ECDSA or Schnorr keys can be spent only through a rescue
    transaction
  - new outputs must be P2WPQH (or `OP_RETURN`)
  - P2WPQH is unaffected
- **Rescue = commit, wait, reveal (MQ-36, MQ-37).**
  1. The owner first publishes a hiding commitment that binds a secret to the
     exact rescue transactions.
  2. After a delay, the owner reveals the secret and spends to P2WPQH.
  3. The earliest commitment for a secret wins, and a secret that has been used
     once is closed for good.
- **Two kinds of secret.**
  - *Key secret*: the public key or script behind a hashed output that has
    never been spent from.
  - *Derivation secret*: a BIP32 seed, or an extended private key above at least
    one hardened derivation step. This also rescues outputs whose public key is
    already visible.
- **No STARK in consensus for v1.** A STARK proof of seed knowledge (MQ-37 in
  its original form) would let owners rescue without revealing the derivation
  secret. It is assessed in section 5 and recommended as later research, not
  for v1.
- **Proposed mainnet parameters** (section 6), at roughly 123 s per block:

  | Parameter | Blocks | About |
  |---|---|---|
  | Reveal delay | 720 | 24.6 h |
  | Commitment lifetime | 20,160 | 28.7 days |
  | Key-secret grace period | 10,080 | 14.3 days |
  | Break-glass lead time | 1,440 | 2 days |

## 1. Threat model and exposure classes

Assume an adversary who can compute the private key for any secp256k1 public
key it has seen, quickly enough to act within a block interval or less. The
adversary cannot invert SHA-256, HMAC-SHA512 or RIPEMD-160, and cannot forge
SLH-DSA signatures. The adversary may control some mining hashpower and can
see the mempool.

Every unspent output falls into one of these classes:

| Class | Output types | What the adversary can do after a break |
|---|---|---|
| **Exposed at rest** | P2PK; bare multisig; P2TR (key path; whether Taproot is active on mainnet is to be confirmed by MQ-31); any P2PKH/P2WPKH/P2SH/P2WSH whose key or script was already revealed by an earlier spend from the same address; any output whose public key can be computed from a leaked extended public key (xpub) | Compute the private key now and spend at will |
| **Hashed** | P2PKH, P2WPKH (key never revealed); P2SH, P2WSH (script never revealed) | Nothing until the owner spends. The spend reveals the key or script in the mempool, and the adversary can then sign a conflicting spend and race it |
| **Post-quantum** | P2WPQH | Nothing |
| **Other** | `OP_RETURN`, non-standard | Not relevant |

The race in the hashed class is the reason hashed outputs are not safe after a
break, even though nothing is visible at rest. The first transaction that
spends them hands the adversary the key. A plain soft fork that only disables
ECDSA would freeze these coins. A plain spend would lose them. A rescue path
has to order the honest owner first without revealing anything early.

The exposure census (MQ-31, `contrib/devtools/utxo-exposure-census.py`) reports
how much of the supply sits in each class by age. Its numbers drive the
decisions in section 7.

## 2. Break-glass soft fork (MQ-38)

### 2.1 Rules

Let `H_bg` be `Consensus::Params::nBreakGlassHeight`. `0` means not active, and
every normal release ships `0`. For a block at height `h ≥ H_bg`:

- **B1. Legacy spends only through rescue.** Every input that spends an output
  of type P2PK, P2PKH, bare multisig, P2SH, P2WPKH, P2WSH or P2TR is valid only
  if the transaction is a valid rescue reveal (section 3.5). This applies
  whatever the script contains, because simpler rules are safer under pressure.
  Whether to exempt hash-lock-only scripts is open question Q6.
- **B2. No new legacy outputs.** Every output created at `h ≥ H_bg` must be
  P2WPQH, `OP_RETURN`, or a rescue commitment (an `OP_RETURN`). This includes
  coinbase outputs, so pool payout addresses must already be P2WPQH. Outputs to
  unknown witness versions are also refused.
- **B3. P2WPQH is unaffected.** P2WPQH spends follow the normal witness v2
  rules.

Both rules only tighten what is valid, so this is a soft fork. Nodes that
don't upgrade keep following the chain while upgraded miners hold most of the
hashpower.

### 2.2 Activation and the risk of a split

Activation is a buried height, not BIP9: the auxpow chain ID occupies the
upper version bits, so merged-mined blocks cannot signal versionbits (see
MQ-12).

- In an emergency, maintainers tag a release with `H_bg = tip + L` and publish
  reproducible binaries. `L` is the lead time, 1,440 blocks (about 2 days) on
  mainnet.
- Upgraded nodes start refusing non-rescue legacy spends into their mempools
  `L/10` blocks before `H_bg`, so miners don't carry them over the boundary.
- **Split risk.** If miners holding most of the merged-mining hashpower don't
  upgrade, an adversary can mine theft transactions into a chain that
  non-upgraded nodes accept. Upgraded nodes reject it, and the network splits.
  Marscoin's hashpower arrives through a few merged-mining pools, which makes
  coordination fast but makes each pool critical. The runbook (2.4) puts the
  pools first.
- **No rollback by default.** Buterin's 2024 Ethereum proposal includes
  reverting blocks after large-scale theft is detected. This spec does not:
  rolling back confirmed history is a separate, far more contentious decision.
  If it is ever considered, it is a governance decision made case by case,
  outside this mechanism (open question Q7).

### 2.3 Mempool and fees

- After `H_bg`, relay policy accepts legacy spends only as rescue reveals whose
  commitment is mature.
- Marscoin disables replace-by-fee, so a rescue owner sets a generous fee when
  building the reveal transactions. Each committed set of reveal transactions
  may include fee variants of the same spend (section 3.2); only one can
  confirm.
- Commitments pay normal fees plus a burn (section 3.3).

### 2.4 Decision process and runbook

**Triggers** (any one, confirmed independently by at least two maintainers):

1. A published classical or quantum attack that recovers secp256k1 private keys
   at practical cost, with a public demonstration.
2. On-chain evidence of key compromise. For example, dormant exposed outputs
   (P2PK, reused addresses) move in a pattern their owners deny, or a known test
   key is swept.
3. A credible standards or vendor disclosure (for example NIST, or a major
   cryptography group) that ECDSA over secp256k1 is broken.

**Who decides:** the Marscoin Foundation core maintainers, after a call with the
merged-mining pool operators and the main exchanges. The decision and its
evidence are published at the time it is made.

**Timeline** (targets):

| When | Action |
|---|---|
| T+0 | Trigger confirmed; decision call convened |
| T+12 h | Decision published (website, repository, social channels, exchange contacts) |
| T+24 h | Emergency release tagged with `H_bg = tip + L`; reproducible binaries and checksums published; pools and exchanges notified directly |
| T+24 h to `H_bg` | Pools switch payouts to P2WPQH and upgrade; exchanges pause legacy deposits; wallets ship rescue support |
| `H_bg` | Break-glass active; the derivation-secret window opens |
| `H_bg + W` | The key-secret window opens (section 3.6) |

The runbook, release scripts and announcement templates must exist and be
rehearsed on marsqnet before mainnet PQ activation (section 8).

## 3. Commit–delay–reveal rescue (MQ-36, and MQ-37 without a STARK)

### 3.1 Secrets

A rescue proves knowledge of a secret the adversary cannot have:

| Type | Secret `X` | Protects | Why the adversary can't have it |
|---|---|---|---|
| `0x01` key secret | The serialized public key (33 or 65 bytes) behind a P2PKH/P2WPKH output, or the redeem or witness script behind a P2SH/P2WSH output | Hashed outputs | Only a hash is on chain. Breaking EC needs the key first |
| `0x02` derivation secret | A BIP32 extended private key (32-byte key ‖ 32-byte chain code). The derivation path from it to every spent key must contain at least one hardened step | Hashed **and** exposed outputs of HD wallets | Getting a node above a hardened step from anything below it needs an HMAC-SHA512 preimage |
| `0x03` seed secret | A BIP32 seed (16 to 64 bytes); the master node is `HMAC-SHA512("Bitcoin seed", seed)` | Same as `0x02` | The master node is a hash of the seed |

**Why one hardened step is required.** With an EC break and a leaked xpub
(public key plus chain code), non-hardened derivation runs backwards. The
adversary computes the child's private key, subtracts the HMAC tweak, and gets
the parent private key. Across a hardened step that needs an HMAC-SHA512
preimage, which the adversary doesn't have.

**Which node to reveal.** Marscoin descriptor wallets derive at
`m/84h/0h/0h/{0,1}/*` on mainnet (and `44h`, `49h`, `86h` for the other
types).
- Revealing the coin-level node `m/84h/0h` is sound, because the account step
  `0h` below it is hardened.
- It exposes every account under that node. It also exposes Bitcoin keys if the
  same master key was ever used in a Bitcoin wallet: Marscoin descriptor wallets
  use coin type `0h`, the same as Bitcoin.
- The account node `m/84h/0h/0h` is **not** sound, because only non-hardened
  steps lie below it.
- Older Marscoin Core HD wallets derive at `m/0'/0'/i'`, all hardened, so any
  ancestor works.
- Wallets from the Marscoin 1.x era used random, non-HD keys. Those outputs can
  only use a key secret, and only while the key is unrevealed.

### 3.2 Commitment contents

Tagged hashes are BIP340-style: `TaggedHash(tag, m) = SHA256(SHA256(tag) ‖ SHA256(tag) ‖ m)`.

For each rescue transaction `T`, the **reveal digest** is
`TaggedHash("Marscoin/Rescue/leaf", d)`, where `d` serializes:
- `nVersion`, `nLockTime`
- every input's outpoint and `nSequence`, in order
- every output except the rescue-data output (3.5), value and scriptPubKey, in
  order

It excludes `scriptSig`, witnesses and the rescue-data output itself. Because
the digest excludes signatures and its own proof data, the owner can commit
before signing and can commit to several transactions without a circular
dependency.

The **reveal set root** `R` is the root of a Merkle tree over the reveal
digests of every transaction the owner wants to be able to broadcast for this
secret. Leaves are sorted and each node is
`TaggedHash("Marscoin/Rescue/node", min(a,b) ‖ max(a,b))`. A single transaction
gives `R = digest`. There are at most 4,096 leaves (depth 12).

The owner computes:

```
AID = TaggedHash("Marscoin/Rescue/aid", type ‖ X)
SDP = TaggedHash("Marscoin/Rescue/sdp", type ‖ X ‖ R)
```

`AID` indexes the commitment, and only someone who knows `X` can compute it.
`SDP` binds the secret to the exact rescue transactions. This follows the
structure of Dryja's 2025 proposal (an address ID and a commitment that binds
the key to the spending transaction; see section 9). Here the commitment binds
to a set of transactions, and derivation secrets are added.

### 3.3 Commitment transaction

- A commitment is an output with scriptPubKey
  `OP_RETURN <"MRC" ‖ 0x01 ‖ AID ‖ SDP>`: a 68-byte push.
- Its value must be at least `COMMIT_BURN`, which is burned. This prices the
  index entry; the proposed value is 0.01 MARS (open question Q2).
- One transaction may carry many commitments. Commitments are hiding, so an
  **aggregator** can batch other people's commitments without learning anything.
  It can delay them but cannot steal.
- Anyone may post a commitment, paid from any spendable input:
  - before `H_bg`, a legacy coin
  - after `H_bg`, a P2WPQH coin, or through an aggregator

  An owner whose coins are all legacy needs an aggregator, or someone else's
  P2WPQH coin, after `H_bg`. A free public aggregator run by the Foundation is
  open question Q3.
- Commitments are accepted from `H_r = nRescueHeight` onward. This activation is
  separate from break-glass and is meant to ship with mainnet PQ activation
  (MQ-34), so owners can commit **pre-emptively** long before any emergency.

### 3.4 Commitment index (consensus state)

Nodes keep an index from `AID` to the list of commitments carrying it, ordered
by `(height, position in block)`. It is updated in block connect and disconnect
with undo data, like the UTXO set.

- **Expiry.** A commitment at height `c` expires at height `max(c, H_bg) + E`,
  unless it has been consumed. Pre-emptive commitments therefore stay alive until
  `E` blocks after break-glass.
- **Active commitment.** For an `AID` at height `h`, it is the earliest
  non-expired commitment. Once an `AID` has been consumed (3.5), its consuming
  commitment stays active until that commitment expires, and later commitments
  for the same `AID` are ignored forever.
- **Size.** Each entry is about 140 bytes. One million commitments take about
  140 MB of index and burn 10,000 MARS at the proposed `COMMIT_BURN`.

### 3.5 Reveal validity

A **rescue reveal** is a transaction with exactly one rescue-data output:
`OP_RETURN <"MRR" ‖ 0x01 ‖ claims>`, value 0, split into pushes of at most 520
bytes. Each claim holds:
- `type`
- `X`
- a Merkle proof, as a list of sibling hashes
- the indices of the inputs it covers
- for types `0x02` and `0x03`, one BIP32 path per covered input

At height `h`, a rescue reveal is valid if, besides all normal rules:

1. Every input that B1 restricts is covered by exactly one claim.
2. For each claim:
   1. Compute `AID` from `type ‖ X`, and find its active commitment `C` at
      height `h`. `C` must exist and be at least `D` blocks old
      (`height(C) ≤ h − D`).
   2. Compute `R` from this transaction's reveal digest and the claim's Merkle
      proof. Require `TaggedHash("Marscoin/Rescue/sdp", type ‖ X ‖ R) = SDP(C)`.
   3. **Type `0x01`:** `X` must be the key or script the input actually uses (the
      key that hashes to the P2PKH/P2WPKH program, or the script that hashes to
      the P2SH/P2WSH program). The reveal must also satisfy `h ≥ H_bg + W`
      (section 3.6).
   4. **Types `0x02` and `0x03`:** for each covered input, derive along its path
      from the revealed node, or from the master node of the revealed seed.
      - Type `0x02` paths must contain a hardened step.
      - The derived key must match the spent output:
        - P2PK: the public key is equal
        - P2PKH and P2WPKH: HASH160 matches
        - P2TR: the BIP86 key-path output key matches (no script tree)
      - Multisig and other scripts are not supported by derivation secrets in v1
        (open question Q5).
3. The input's normal script still validates. The owner holds the keys, so the
   signature check costs nothing extra, and keeping it means old validation code
   paths don't change.
4. Every output other than the rescue-data output is P2WPQH. Rescued coins land
   in post-quantum outputs.

When a rescue reveal confirms, each claimed `AID` is marked **consumed** with
its commitment and `R`. Other transactions in the same `R` stay valid until
that commitment expires. Nothing else can ever use that `AID` again.

### 3.6 Ordering, and why the grace period exists

- **Earliest commitment wins.** For a hashed output, nobody but the owner can
  compute `AID` before the owner reveals. An adversary who learns the key from
  the owner's reveal can only commit later, so its commitment is never active.
  It cannot redirect the spend even if miners delay the owner's reveal,
  because validity depends on chain data, not on who reaches the mempool first.
- **Why there is a delay `D` at all.** It protects against reorgs. To win, an
  adversary who saw a reveal would have to reorg the chain to before the
  owner's commitment and insert its own earlier one. `D` is set well beyond any
  plausible reorg depth on a merged-mined chain.
- **A revealed derivation secret exposes keys below it.** After a type `0x02` or
  `0x03` reveal, everyone can compute every key below that node. The
  derivation-secret `AID` is closed, but nothing stops key-secret claims for
  those individual keys.
  - So during the **grace period** `[H_bg, H_bg + W)`, type `0x01` reveals are
    invalid, and only derivation secrets work.
  - Owners of HD wallets must sweep everything below the revealed node, in one
    committed reveal set, during that window. Anything they leave out can later
    be claimed by anyone with a key secret.
  - Owners of non-HD hashed coins wait `W` blocks, losing nothing, since their
    key is still secret.
- **Leaked xpubs.** If an adversary knows a hashed output's key because an xpub
  leaked, it can pre-commit a key secret before the owner. The grace period lets
  the owner win with a derivation secret first. After `H_bg + W`, such outputs
  are exposed.

### 3.7 Griefing and abuse

| Attempt | Result |
|---|---|
| Junk commitments | Cost `COMMIT_BURN` plus fees each; bounded by block weight; expire after `E` |
| Front-running a hashed output's commitment | Impossible: `AID` needs `X` |
| Front-running an exposed output with a key secret | Possible, which is why exposed outputs need derivation secrets and why the grace period exists |
| Owner commits to the wrong `R` | The owner waits until `E` (at most about 4 weeks) for that commitment to expire, then commits again. `X` stays secret meanwhile |
| Censoring an owner's reveal | Delays the rescue; can't steal it (3.6). Censoring commitments is possible but untargeted, because commitments are hiding |
| Commitments that reference a P2WPQH output | Irrelevant: P2WPQH never needs rescue |

### 3.8 What the rescue path cannot do

- Outputs that are exposed at rest and **not** HD-derived: P2PK coinbase
  outputs from Marscoin's early years, imported keys, paper wallets. Nobody can
  prove ownership of these without the private key, which the adversary also
  has. They end up frozen by B1.
- Multisig and complex scripts, except through a key secret (the script) while
  it is unrevealed.
- Coins whose owner doesn't act within the windows.
- Brain wallets (`k = SHA256(passphrase)`) technically have a hash preimage. Most
  are weak and long swept, so supporting them is open question Q4.

## 4. Relationship to migration and recycling

The lifecycle, in order:

1. **Migration window** (MQ-34, MQ-35). P2WPQH is active, and wallets, the pool
   and exchanges default to it. Commitment indexing (`H_r`) activates at the
   same time, so careful holders can commit pre-emptively for cold storage they
   can't move yet. The census tracks migration publicly.
2. **Optional new-output freeze.** If the census shows migration has stalled,
   rule B2 alone (no new legacy outputs) can ship ahead of B1 as a normal soft
   fork with a long lead time. BIP-361's Phase A takes the same approach.
3. **Break-glass** (B1 and B2), triggered by the runbook. Alternatively, if
   nothing has happened by a published date, a scheduled sunset can use the same
   code with a long `L` (open question Q1).
4. **Rescue window.** First derivation secrets, then key secrets from
   `H_bg + W` onward. Commitments expire `E` blocks after `H_bg`, so any later
   rescue needs a fresh commitment and delay.
5. **Sunset and UTXO recycling** (MQ-39). Legacy outputs still unspent after the
   rescue window are frozen by B1. What happens to them is a governance decision
   that this document doesn't make.

The census feeds steps 1–3 directly:
- the exposed share by age sizes the risk of an emergency
- the dormant hashed share says how long the rescue window must be
- the post-quantum share measures how far migration has got

## 5. Seed-knowledge proofs with a STARK (MQ-37 research)

**Idea.** Buterin proposed this for Ethereum in 2024. Instead of revealing a
seed or derivation node, the owner proves in zero knowledge that they know a
preimage (seed) whose derivation yields the spent key. A STARK built only on
hash functions keeps that proof sound after elliptic curves fall. BIP-361 also
mentions ZK-STARK rescue protocols that use BIP32 hardened derivation.

**What it would add over section 3.**
- The secret is never revealed, so partial rescues are safe: the owner can
  rescue one output and leave others for later.
- It hides which outputs belong together, which helps privacy.
- It isn't needed for soundness. The derivation-secret reveal in section 3 is
  already sound.

**What the statement contains, for a typical Marscoin descriptor wallet path
`m/84h/0h/0h/0/i` proved from a seed:**
- one master HMAC-SHA512 and three hardened HMAC-SHA512 steps
- two non-hardened steps, each needing the parent public key: one secp256k1
  scalar multiplication plus one HMAC
- the final public key (one more scalar multiplication) and its HASH160
- in total, about 3 secp256k1 scalar multiplications, roughly 24 SHA-512
  compressions, one SHA-256 and one RIPEMD-160

Elliptic-curve arithmetic inside the proof is just computation. Soundness still
rests only on the hash functions.

**Estimates.** These are rough order-of-magnitude estimates, not measurements;
none were benchmarked for this document.

| Metric | Estimate |
|---|---|
| Prover work | 10⁶ to 10⁷ steps of a general STARK virtual machine with hashing and secp256k1 accelerators. Seconds to a few minutes on a laptop |
| Proof size | About 100 KB to 1 MB per proof as a plain STARK. Recursion can batch many claims into one proof. Wrapping in a pairing-based SNARK to shrink it is **not** acceptable, because pairings are elliptic-curve cryptography |
| Verification | About 10 to 100 ms of CPU per proof, dominated by Merkle-path hashing |
| Witness weight | Comparable to dozens of SLH-DSA signatures per unbatched proof |

**What consensus would need.** A complete STARK verifier frozen into consensus
forever:
- the field, hash function and FRI parameters
- the exact program or constraint system that encodes BIP32 derivation and
  address matching
- every edge case

That is likely tens of thousands of lines of new consensus-critical code. It
would need independent audits, and it could never change without another fork.
It is by far the largest consensus addition considered in the quantum program.

**Limits it shares with section 3.** It needs keys derived from a hash
preimage. Random-key wallets, imported keys and the early P2PK outputs gain
nothing.

**Recommendation.**
- Do **not** build this for v1. The derivation-secret reveal in section 3 gives
  most of the protection using primitives Marscoin already has.
- Revisit once a hash-based STARK verifier has been independently audited and
  used in production elsewhere.
- Prototype the derivation statement off-chain first, to replace the estimates
  above with measurements. The prototype is the deliverable that MQ-37 asks for.

## 6. Parameters

| Parameter | Mainnet | Marsqnet | Notes |
|---|---|---|---|
| `nRescueHeight` (`H_r`) | With PQ activation (MQ-34) | With PQ activation | Commitments indexed from here |
| `nBreakGlassHeight` (`H_bg`) | 0; set only by an emergency release | Set by the dry-run release | Buried height |
| `L` (lead time) | 1,440 blocks (about 2 days) | 100 | Trades speed against the risk of a split |
| `D` (reveal delay) | 720 blocks (about 24.6 h) | 20 | Exceeds any plausible reorg |
| `E` (commitment lifetime) | 20,160 blocks (about 28.7 days) | 500 | Counted from `max(c, H_bg)` |
| `W` (key-secret grace) | 10,080 blocks (about 14.3 days) | 100 | Derivation secrets only during this window |
| `COMMIT_BURN` | 0.01 MARS (placeholder) | 0.01 tMARS | Prices index entries |
| Reveal set size | ≤ 4,096 transactions (depth 12) | Same | Per commitment |

Block-time conversions assume Marscoin's 123-second spacing.

## 7. Using the census

The thresholds below are proposals. The owner sets the real ones.

- **Exposed supply above about 10%**, or any single exposed output large enough
  to fund an attack, means break-glass must be ready to ship within days.
  Rehearse it on marsqnet and keep the release branch current.
- **Dormant hashed coins** (no movement for years) whose owners may need time to
  learn about the rescue argue for a longer `W` and rescue window, and for an
  early `H_r` so pre-emptive commitments are possible.
- **Post-quantum share** is the headline migration metric to publish.

## 8. Marsqnet dry run

Before mainnet PQ activation (MQ-34), on marsqnet v2:

1. Activate `H_r` early. Create P2PKH, P2WPKH, P2SH, P2PK and (if supported)
   P2TR outputs, both from HD and non-HD keys. Make some pre-emptive
   commitments.
2. Ship a test release with `H_bg = tip + 100`, and time how long the pool,
   explorer, faucet and nodes take to upgrade.
3. Check that after `H_bg`:
   - plain legacy spends and new legacy outputs are rejected
   - P2WPQH spends work
   - derivation-secret reveals work during the grace period
   - key-secret reveals are refused until `H_bg + W` and work after it
4. **Front-runner test.** Give a simulated adversary the private keys, standing
   in for an EC break. Confirm it cannot:
   - redirect a key-secret reveal by committing after seeing it, even when the
     honest reveal is withheld from blocks for longer than `D`
   - use a derivation secret it learned from an honest reveal
   - claim outputs inside a consumed reveal set

   Confirm that it **can** sweep outputs left outside the set after `H_bg + W`.
   That limitation is documented, so the test proves it rather than hides it.
5. **Reorg test.** Reorg a chain shallower than `D` across a commitment; the
   index and undo data must stay consistent.
6. **Pool payout switch.** Coinbase outputs after `H_bg` must be P2WPQH, and the
   built-in miner and `createauxblock` must honour that.

## 9. Prior art

Every citation below was checked against the source on 2026-10-08.

- **J. Bonneau and A. Miller, "Fawkescoin: A cryptocurrency without public-key
  cryptography."** Security Protocols Workshop 2014, LNCS 8809, Springer.
  <https://doi.org/10.1007/978-3-319-12400-1_35>; PDF:
  <https://jbonneau.com/doc/BM14-SPW-fawkescoin.pdf>. A cryptocurrency that
  replaces signatures with hash-based Guy Fawkes commit-then-reveal signatures.
  It is the origin of commit-and-reveal ownership.
- **I. Stewart, D. Ilie, A. Zamyatin, S. Werner, M. F. Torshizi, W. J.
  Knottenbelt, "Committing to quantum resistance: a slow defence for Bitcoin
  against a fast quantum computing attack."** Royal Society Open Science 5(6),
  180410, 2018. <https://doi.org/10.1098/rsos.180410>; preprint
  <https://eprint.iacr.org/2018/213>.
  - A "simple but slow commit–delay–reveal protocol" for moving funds to
    quantum-resistant outputs.
  - It works "even if ECDSA has already been compromised" and can be deployed as
    a soft fork.
- **V. Buterin, "How to hard-fork to save most users' funds in a quantum
  emergency."** ethresear.ch, 9 March 2024.
  <https://ethresear.ch/t/how-to-hard-fork-to-save-most-users-funds-in-a-quantum-emergency/18901>.
  - Rolls back blocks after theft is detected and disables plain externally
    owned accounts.
  - Recovers accounts with a STARK proving knowledge of a hash preimage, such as
    a BIP32 seed, from which the key was derived.
  - Keys not derived by hashing are not covered.
- **T. Dryja, "Post-Quantum commit / reveal Fawkescoin variant as a soft
  fork."** bitcoindev mailing list, 28 May 2025.
  <https://mailing-list.bitcoindevs.xyz/bitcoindev/cc2f8908-f6fa-45aa-93d7-6f926f9ba627n@googlegroups.com/>.
  - Commitments in `OP_RETURN` outputs combine an address ID (a different hash
    of the public key), a proof of knowledge that binds the key to the spending
    txid, and the txid.
  - The first valid commitment for an address ID wins, and nodes index
    commitments in a new key-value store. A minimum delay (100 blocks in the
    example) is discussed.
  - Anyone can aggregate commitments, and a non-zero output amount is suggested
    against spam.
  - BIP-360's text and Bitcoin Optech #421 refer to the developed scheme as
    "Lifeboat". A standalone primary write-up of Lifeboat was not found and is
    not cited here.
- **conduition, "DropKick: A Minimal Commit/Reveal Rescue Protocol."** 1 August
  2026. <https://conduition.io/bitcoin/dropkick/>.
  - Commitments are aggregated in Merkle trees and verified against block data
    without a node-side index.
  - The delay is rigid (examples range from 50 to 1,440 blocks), with a minimum
    fee ratio `1/D` against miner censorship.
  - It supports arbitrary "knowledge asymmetries".
  - Summarized and compared with Lifeboat in **Bitcoin Optech Newsletter #421**,
    4 September 2026, <https://bitcoinops.org/en/newsletters/2026/09/04/>.
- **Hunter Beast, Ethan Heilman, Isabel Foxen Duke, "BIP 360: Pay-to-Merkle-Root
  (P2MR)."** Draft, created 2024-12-18. <https://bip360.org/bip360.html>.
  - A witness v2 output (`OP_2 <32-byte Merkle root>`) without Taproot's key
    path, protecting against long-exposure attacks. It defines no post-quantum
    signatures itself.
  - Note: Bitcoin's P2MR also uses witness v2. Marscoin's witness v2 is P2WPQH,
    so the two are unrelated designs that share a version number.
- **J. Lopp, C. Papathanasiou, I. Smith, J. Ross, S. Vaile, P.-L.
  Dallaire-Demers, "BIP 361: Post Quantum Migration and Legacy Signature
  Sunset."** Draft, assigned 2026-02-11. <https://bips.dev/361/>.
  - Phase A stops sends to quantum-vulnerable addresses about 160,000 blocks
    (about 3 years) after activation.
  - Phase B, two years later, encumbers ECDSA/Schnorr spends with a
    quantum-safe rescue protocol, and mentions ZK-STARK rescue that uses BIP32
    hardened derivation.

**Not cited:** coverage reporting that Lifeboat would activate automatically on
an on-chain proof that a quantum computer exists. It comes from secondary
reports of a 2026 talk and couldn't be checked against a primary source.

## 10. Open questions for the owner

1. **Scheduled sunset as well as emergency?** Should break-glass also have a
   published date, as BIP-361 does, or stay emergency-only?
2. **`COMMIT_BURN` value.** 0.01 MARS is a placeholder. It should price spam
   without excluding small holders.
3. **Public aggregator.** Should the Foundation run a free, open-source commitment
   aggregator, so holders with only legacy coins can commit after `H_bg`?
4. **Brain wallets.** Support them as a secret type, or not?
5. **Multisig and complex scripts.** Is a multisig derivation-secret path worth
   v1's complexity, or is the key-secret path (the unrevealed script) enough?
6. **Hash-lock-only scripts.** Exempt them from B1, or keep B1 uniform?
7. **Rollback stance.** Confirm "no rollback by default", and who could ever
   decide otherwise.
8. **Window lengths.** Are `D`, `E` and `W` right, given how fast Marscoin
   holders can realistically be reached?
9. **Historic wallets.** Which wallets Marscoin users actually hold matters: 1.x
   random keys, Core legacy HD, descriptor wallets, third-party BIP39 wallets
   and their coin types. It determines how much supply derivation secrets can
   cover. The census can only partly answer this.
10. **Taproot status on mainnet.** It affects which outputs count as exposed
    (MQ-31).

## 11. Recommended order of work

1. Confirm the parameters and open questions above. Run the census (MQ-31) and
   publish the exposure numbers.
2. Implement the commitment index and commitment rules behind `H_r`, with unit
   and functional tests, and ship them with PQ activation, so pre-emptive
   commitments are possible from day one.
3. Implement rescue reveals for key secrets and derivation secrets, and
   break-glass rules B1–B3 behind `H_bg`. Add wallet support to build commitment
   and reveal sets and to export the right derivation node.
4. Write the runbook, release scripts and announcement templates. Run the
   marsqnet dry run (section 8).
5. Get an external review of the whole rescue design together with the P2WPQH
   review (MQ-33).
6. Prototype the STARK statement off-chain (section 5). Revisit consensus
   integration only after independent audits of a hash-based STARK verifier.
