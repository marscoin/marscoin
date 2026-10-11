# RandomX key binding, version 1 (MQ-16)

Status: draft, for marsqnet v2. Mainnet adoption of RandomX is a separate
decision (MQ-43).

## Problem

RandomX hashes with a key that selects its 256 MB cache (and the 2 GB dataset
in full mode). Marsqnet takes the key from the header being hashed. For a
merged-mined (auxpow) block that is the parent header, which the miner builds.
Every block mined on marsqnet v2 has used the parent's null `hashPrevBlock` as
its key, so the key never changes. This has two consequences:

- A fixed key gives hardware a fixed target. RandomX rotates its key with the
  chain so that nobody can optimize memory or circuits for one dataset.
- A miner who picks a new key for each block makes every validating node
  rebuild its cache, about a second of CPU, for each header or block it checks.

## Rule

From activation height `A` on a chain with `fPowUseRandomX`:

1. **Seed height.** For a block at height `h`:

   ```
   S(h) = 0                                      if h <= E + L
   S(h) = (h - L - 1) rounded down to a multiple of E   otherwise
   ```

   with epoch `E = 2048` and lag `L = 64`, the values Monero uses. The key
   changes every 2048 blocks, and each key is fixed 64 blocks before it is
   first used.

2. **Key.** The key for height `h` is the hash of the block at height `S(h)` on
   the chain the block extends: the ancestor of its parent, or the genesis
   block for `S(h) = 0`. The key depends only on the block's own history, so
   every node computes the same key for the same block.

3. **Proof of work.** The RandomX hash of the block's own 80-byte header, with
   that key, must not exceed the target set by `nBits`.

4. **No merged mining.** A block with the auxpow version flag is invalid
   (`auxpow-randomx`). RandomX blocks are mined directly. No parent chain could
   hash with Marscoin's key anyway.

   Merged mining reuses one parent-chain hash as Marscoin's proof of work. That
   works for scrypt, which has no key. A RandomX hash depends on the key, so one
   hash can count on two chains only if both use the same key. With a key from
   Marscoin's chain, no parent chain (Monero, for example, which keys from its
   own chain) produces a usable hash. If Marscoin took the parent's key instead,
   the key would come from outside Marscoin, which is the problem this spec
   fixes.

   Marsqnet loses nothing real: its auxpow blocks have only ever had parents
   that the node builds for itself. Mainnet's scrypt merged mining is
   unaffected; this rule applies only to RandomX chains.

   For a future mainnet RandomX decision (MQ-43): Tari merge-mines with Monero
   by adopting Monero's key schedule. Marscoin's key would then rotate, but
   bound to Monero's chain, which Marscoin nodes can't fully verify themselves.

5. **Exact difficulty.** `nBits` must equal the required value. Below height
   126000, mainnet's history accepts a difficulty within 50% of it, and marsqnet
   inherited that tolerance; a RandomX chain drops it at `A`.

Blocks below `A` keep the old rules, so the existing chain stays valid.

This is a consensus change: nodes on the old and new rules compute different
hashes for the same block. Every node must run the new rule before height `A`.

## Where it is checked

The key needs the block's height and history, so the RandomX proof of work
moves from the context-free header check to the contextual one, where the
parent is known:

- `ContextualCheckBlockHeader` checks proof of work for RandomX chains, under
  either rule depending on height. `CheckBlockHeader` keeps the context-free
  check only for chains without RandomX.
- The early anti-DoS check on a `headers` message (`HasValidProofOfWork`) can't
  know heights, so it passes RandomX headers through. Each header is still
  checked by `ContextualCheckBlockHeader` before it is stored, and a peer that
  sends an invalid one is punished there, at the cost of one RandomX hash per
  invalid header.
- The low-work headers sync (presync) doesn't store headers, so it checks their
  proof of work itself. It records the hash at each seed height it passes, one
  per epoch, and those supply the keys for later headers. Seeds at or below
  the sync's starting block come from the block index.
- Reading a block from disk skips the RandomX proof of work. The block was
  validated before it was stored, and reads by block index entry still check
  the hash.

## Mining

- `generatetoaddress` and `generateblock` build plain blocks (no auxpow) from
  `A` on, and hash them with the seed key from the chain, using the miner's own
  cache (#97).
- `getblocktemplate` reports `randomxseedhash` and `randomxseedheight` for
  RandomX chains, so external miners know the key.
- `createauxblock` and `submitauxblock` refuse to work on RandomX chains from
  `A` on.

## Caches

Only two keys are ever needed: the current epoch's, and during syncs or reorgs
near a boundary, the previous one. Validation keeps a cache per key for those
two. Light mode (256 MB) remains the verification mode. Full mode (2 GB
dataset) is a mining choice and doesn't affect validity.

## Activation on marsqnet v2

`A = 1600`. When this was chosen on 2026-10-11, v2 was at height 1230 with
blocks about 123 seconds apart, which left about 12 hours to deploy the
release to every v2 node. The first key change after activation comes at
height 2113 (`S` becomes 2048), about 17 hours later. Before `A`, nothing
changes. From `A` on, old nodes reject the new blocks, so the switch also tests
the upgrade procedure.

For testing, marsqnet accepts debug-only options that override `A`, `E` and
`L`: `-randomxkeybindingheight`, `-randomxseedepoch` (a power of two) and
`-randomxseedlag`.

## Tests

- `S(h)` around both boundaries: `h = E + L`, `E + L + 1`, and each epoch
  change.
- Contextual validation accepts a block hashed with the correct seed key, and
  rejects one hashed with the null key, with the previous epoch's key, or with
  the auxpow flag.
- The rule switches exactly at `A`; blocks below `A` keep validating under the
  old rule.
- Header sync across an epoch boundary, including presync, with a small epoch
  and lag configured for tests.
- `generatetoaddress` produces valid blocks before and after `A`.
