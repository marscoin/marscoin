# Quantum upgrade work orders

Prepared 8 October 2026 from a review of the marsqnet testnet and a code audit
of `feature/quantum-upgrade` at `68ce6319b6` (PR #53). File and line references
point at that revision. Owner: Marscoin core maintainer.

This turns every observation from that review into a work order, ordered by
priority from top to bottom. It is the execution index for the quantum program
tracked in #32, #34, #36, #37 and #43.

## Strategic frame

- Signatures stay **hash-based only**. SLH-DSA's security rests on SHA-256,
  which the chain already depends on for txids and Merkle trees. Do not add
  lattice-, code-, isogeny- or elliptic-curve-based assumptions to consensus,
  including for signature-size savings.
- Plan for elliptic curves failing **without warning**, through a classical
  algorithmic break as well as quantum hardware. Migrating coins before that
  happens is the priority, and the rescue paths have to work after it.
- Unbundle the three pillars. The PQ soft fork ships first and on its own.
  RandomX is a separate hard-fork track. UTXO recycling is the final step of
  migration rather than a standalone feature.

## Status rules

- **Queued:** specified, not started.
- **In progress:** active work, with a dated evidence note and next action.
- **Waiting:** names the exact missing input or decision.
- **Complete:** the acceptance checks have evidence (tests, logs, revision).
- **Research:** the deliverable is a written recommendation, not code.
- Record dates, revisions and test evidence. A historical observation below is
  not a claim about current state once work has started.
- Host names, addresses and credentials stay in operator notes, out of this repo.

## Ordered index

| ID | Work order | Track | Status | Depends on |
| --- | --- | --- | --- | --- |
| [MQ-01](#mq-01--guard-pq-wallet-functions-to-chains-that-enforce-pq) | Guard PQ wallet functions to chains that enforce PQ | A · Safety | PR #55, build-machine tests pending | — |
| [MQ-02](#mq-02--encrypt-pq-private-keys-and-enforce-wallet-lock) | Encrypt PQ private keys and enforce wallet lock | A · Safety | PR #55, build-machine tests pending | — |
| [MQ-03](#mq-03--fix-mars1pq-taproot-decode-regression) | Fix `mars1pq` Taproot decode regression | A · Safety | PR #55, build-machine tests pending | — |
| [MQ-04](#mq-04--restore-marsqnet-block-production) | Restore marsqnet block production | B · Testnet ops | In progress: 24 h check | — |
| [MQ-05](#mq-05--redundant-block-producers) | Redundant block producers | B · Testnet ops | Queued | MQ-04 |
| [MQ-06](#mq-06--uniform-identifiable-testnet-builds) | Uniform, identifiable testnet builds | B · Testnet ops | Queued | MQ-04 |
| [MQ-07](#mq-07--monitoring-that-leads-to-action) | Monitoring that leads to action | B · Testnet ops | Waiting: operator applies fix | — |
| [MQ-08](#mq-08--faucet-and-dashboard-reflect-chain-health) | Faucet and dashboard reflect chain health | B · Testnet ops | Queued | — |
| [MQ-09](#mq-09--testnet-host-cleanup) | Testnet host cleanup | B · Testnet ops | Queued | — |
| [MQ-10](#mq-10--switch-to-fips-205-slh-dsa) | Switch to FIPS 205 SLH-DSA | C · Consensus | PR #61 | — |
| [MQ-11](#mq-11--complete-pq-signature-hash) | Complete PQ signature hash | C · Consensus | PR #64 (stacked on #61) | MQ-10 |
| [MQ-12](#mq-12--separate-pq-activation-from-abwl) | Separate PQ activation from ABWL | C · Consensus | PR #69 (stacked on #55) | MQ-01 |
| [MQ-13](#mq-13--always-compiled-minimal-slh-dsa-verifier) | Always-compiled, minimal SLH-DSA verifier | C · Consensus | PR #71 (stacked on #64) | MQ-10 |
| [MQ-14](#mq-14--parameter-set-agility-and-p2sh-wrapped-v2) | Parameter-set agility and P2SH-wrapped v2 | C · Consensus | Queued | MQ-10 |
| [MQ-15](#mq-15--randomx-must-fail-closed) | RandomX must fail closed | C · Consensus | PR #56 (merge with MQ-18) | — |
| [MQ-16](#mq-16--randomx-key-binding-and-auxpow-rules) | RandomX key binding and auxpow rules | C · Consensus | Queued | MQ-15 |
| [MQ-17](#mq-17--marsqnet-difficulty-retargeting) | Marsqnet difficulty retargeting | C · Consensus | Draft PR #63 (ASERT) | MQ-18 |
| [MQ-18](#mq-18--marsqnet-as-its-own-chain-type) | Marsqnet as its own chain type | C · Consensus | Draft PR #63 (after #56 and #62) | MQ-15, MQ-46 |
| [MQ-19](#mq-19--abwl-persistence-and-block-index-versioning) | ABWL persistence and block-index versioning | C · Consensus | PR #58, build-machine tests pending | — |
| [MQ-20](#mq-20--abwl-end-to-end-capacity) | ABWL end-to-end capacity | C · Consensus | Queued | MQ-19, MQ-28 |
| [MQ-21](#mq-21--restore-the-mainnet-context-free-block-bound) | Restore the mainnet context-free block bound | C · Consensus | PR #57 | — |
| [MQ-22](#mq-22--seed-derived-pq-keys-and-backup) | Seed-derived PQ keys and backup | D · Wallet | Queued | MQ-10 |
| [MQ-23](#mq-23--pq-keys-in-the-key-manager) | PQ keys in the key manager | D · Wallet | Queued | MQ-22 |
| [MQ-24](#mq-24--migration-tooling) | Migration tooling | D · Wallet | Queued | MQ-23 |
| [MQ-25](#mq-25--script-level-and-unit-tests) | Script-level and unit tests | E · Verification | Queued | — |
| [MQ-26](#mq-26--functional-tests) | Functional tests | E · Verification | In progress: PR #66 (framework), #65 | — |
| [MQ-27](#mq-27--ci-and-release-discipline) | CI and release discipline | E · Verification | In progress: secret scanning (MQ-45) | — |
| [MQ-28](#mq-28--performance-and-stress-testing) | Performance and stress testing | E · Verification | In progress: benchmarks PR #74 | MQ-25 |
| [MQ-29](#mq-29--documentation-refresh) | Documentation refresh | E · Verification | In progress: sighash spec in #64 | — |
| [MQ-30](#mq-30--marsqnet-v2-fresh-genesis) | Marsqnet v2 (fresh genesis) | F · Network | Queued | Track C, MQ-22 |
| [MQ-31](#mq-31--mainnet-exposure-census) | Mainnet exposure census | G · Mainnet | PR #73 | — |
| [MQ-32](#mq-32--unbundled-roadmap-and-crypto-policy) | Unbundled roadmap and crypto policy | G · Mainnet | Waiting: owner decision | MQ-31 helps |
| [MQ-33](#mq-33--external-security-review) | External security review | G · Mainnet | Queued | Tracks C, D |
| [MQ-34](#mq-34--mainnet-pq-soft-fork-activation) | Mainnet PQ soft-fork activation | G · Mainnet | Queued | MQ-30, MQ-33 |
| [MQ-35](#mq-35--ecosystem-migration) | Ecosystem migration | G · Mainnet | Queued | MQ-34 |
| [MQ-36](#mq-36--commitdelayreveal-rescue-path) | Commit–delay–reveal rescue path | H · Emergency | Draft spec PR #70 | MQ-12 |
| [MQ-37](#mq-37--seed-knowledge-proof-rescue) | Seed-knowledge proof rescue | H · Emergency | Draft spec PR #70 | — |
| [MQ-38](#mq-38--break-glass-emergency-soft-fork) | Break-glass emergency soft fork | H · Emergency | Draft spec PR #70 | MQ-36 |
| [MQ-39](#mq-39--sunset-and-utxo-recycling-spec) | Sunset and UTXO recycling spec | H · Emergency | Queued | MQ-31, MQ-36 |
| [MQ-40](#mq-40--smaller-hash-based-parameter-set) | Smaller hash-based parameter set | I · Research | Research | MQ-14 |
| [MQ-41](#mq-41--non-signature-public-key-crypto-threat-model) | Non-signature public-key crypto threat model | I · Research | Research | — |
| [MQ-42](#mq-42--ecosystem-public-key-and-identity-review) | Ecosystem public-key and identity review | I · Research | Research | — |
| [MQ-43](#mq-43--randomx-mainnet-track) | RandomX mainnet track | I · Research | Waiting: owner decision | MQ-15–17 |
| [MQ-44](#mq-44--correct-public-claims-about-marsqnet) | Correct public claims about marsqnet | G · Mainnet | Queued | — |
| [MQ-45](#mq-45--secret-scanning-in-ci) | Secret scanning in CI | E · Verification | PR #54 | — |
| [MQ-46](#mq-46--regtest-that-can-mine) | Regtest that can mine (unblocks functional tests) | E · Verification | PR #62 | — |
| [MQ-47](#mq-47--fresh-mainnet-nodes-dont-bootstrap) | Fresh mainnet nodes don't bootstrap | B · Network ops | PR #67, backport #68 | — |
| [MQ-48](#mq-48--backport-the-generatetoaddress-fix-to-28x) | Backport the generatetoaddress fix to 28.x | B · Network ops | PR #68 (with #67) | #65 |
| [MQ-49](#mq-49--signet-cant-start) | Signet can't start | E · Verification | Queued | — |
| [MQ-50](#mq-50--end-address-reuse-in-marscoin-wallets-and-services) | End address reuse in Marscoin wallets and services | G · Mainnet | Queued | MQ-31 |

---

## Track A · Safety

### MQ-01 · Guard PQ wallet functions to chains that enforce PQ

**Status:** Queued

Finding: `getnewpqaddress` (`src/wallet/rpc/wallet.cpp:214`) has no chain
check. Mainnet sets `nABWLActivationHeight = 0` (`src/kernel/chainparams.cpp:125`),
so block validation never sets `SCRIPT_VERIFY_WITNESS_V2`
(`src/validation.cpp:2402-2407`). Without that flag, v2 programs pass
unchecked (`src/script/interpreter.cpp:1981`). Sending to witness v2 is
standard. A mainnet user on a build of this branch can therefore receive real
coins to an address any miner can spend. The released v28.1.2 does not contain
this code.

Scope: refuse `getnewpqaddress`, PQ signing, and wallet sends to v2
destinations unless PQ is active at the current tip. Use the deployment from
MQ-12 once it exists. Return a clear error naming the reason.

Acceptance:

- On mainnet parameters, `getnewpqaddress` and a wallet send to a `mars1z`
  address both fail with an explanatory error.
- On marsqnet both work.
- A functional test covers both cases.

Evidence, 2026-10-08: implemented on `fix/pq-safety-guards` (local, five
commits covering MQ-01–03 plus a test-framework fix).
- `Consensus::Params::IsPQWitnessActive` is the single activation check, used
  by block validation and the wallet; MQ-12 changes only that helper.
- While PQ is inactive, `getnewpqaddress`, sends to `mars1z` and PQ signing
  each fail with an explanation.
- Test-only option `-testactivationheight=abwl@<h>` (regtest).
- New unit tests pass, and `wallet_pq_safety.py` passes for descriptor and
  legacy wallets.
- Full-suite comparison against a baseline is running on the build machine.

Decisions:
- Owner accepted (2026-10-08) that a mainnet user on a branch build who already
  received coins to `mars1z` cannot move them, because signing is refused while
  PQ is inactive. No override flag.
- `-testactivationheight=abwl@…` also controls PQ until MQ-12 adds a separate
  name.

### MQ-02 · Encrypt PQ private keys and enforce wallet lock

**Status:** Queued

Finding: `WritePQKey` stores `param_set_id || pubkey || privkey` in plaintext
(`src/wallet/walletdb.cpp:306-314`). `EncryptWallet` and the crypter never touch
`pqkey` records. `getnewpqaddress` has no `EnsureWalletIsUnlocked`, and PQ
signing in `CWallet::SignTransaction` (`src/wallet/wallet.cpp:2200-2226`) has no
`IsLocked` check.

Scope: encrypt PQ private keys with the wallet master key, as for ECDSA keys.
Encrypt existing plaintext records during `encryptwallet`. Require an unlocked
wallet for PQ key generation and signing. Define the upgrade path for wallets
that already hold plaintext PQ keys.

Acceptance:

- After `encryptwallet`, the wallet file contains no plaintext PQ private-key
  bytes (test scans the file).
- PQ signing fails while locked and succeeds after `walletpassphrase`.
- A wallet created before this change is encrypted correctly.

Evidence, 2026-10-08 (`fix/pq-safety-guards`):
- Encrypted wallets store PQ keys as `cpqkey` records: parameter-set byte and
  public key in the clear, secret key encrypted with the master key.
- `encryptwallet` converts existing plaintext keys in the same transaction.
  Unlocking converts any left by earlier builds.
- Signing and key generation require an unlocked wallet.
- `IsMine` checks that a key record exists without reading the secret.

Open points:
- Keys converted on unlock can leave plaintext in free database pages until
  the next file rewrite (`encryptwallet` itself rewrites the file).
- Key storage assumes the secret key ends with the public key. That holds for
  SPHINCS+ and FIPS 205; re-check it in MQ-10.

### MQ-03 · Fix `mars1pq` Taproot decode regression

**Status:** Queued

Finding: `IsPostQuantumAddress` rejects any string starting `<hrp>1pq`
(`src/key_io.cpp:98-101`, `340-344`). Valid Taproot addresses whose first data
character is `q` (about 1 in 32) match and become undecodable on every chain.
This exists only on the quantum branch. The `1pq` format was superseded by
P2WPQH `mars1z` addresses.

Scope: remove the `1pq` scaffold or make detection exact, and update
`src/test/key_io_tests.cpp:149`.

Acceptance: test vectors with `mars1pq…` Taproot addresses decode and
round-trip, and the old scaffold test is removed or rewritten.

Evidence, 2026-10-08 (`fix/pq-safety-guards`): `IsPostQuantumAddress` and the
`1pq` rejection removed. `key_io_taproot_hrp1pq_roundtrip` round-trips a valid
`mars1pq…` Taproot address. The unused prefix-vector file was deleted.

## Track B · Testnet operations

### MQ-04 · Restore marsqnet block production

**Status:** In progress. Block production restored 2026-10-08; 24-hour check
pending.

Finding: no marsqnet block since 2026-06-09 (height 3436). The only block
producer's node crash-loops with
`txindex: best block of the index not found. Please rebuild the index.` Its
systemd unit reported 2,590,327 restarts on 2026-10-08, and its local chain
stops at height 2711. The miner script keeps calling a dead RPC every 600 s.
One transaction has been waiting in the mempool of the other nodes.

Scope: stop the unit, restart once with `-reindex`, let it sync to the network
tip, confirm the miner resumes, and confirm what happens to the waiting
transaction.

Acceptance: all reachable nodes advance about one block per 10 minutes for 24
hours, and the dashboard shows the current tip.

Evidence, 2026-10-08:

- The txindex pointed past the producer's local chain (2711), so only the index
  was moved aside (kept as `txindex.bak-20261008`); no reindex was needed.
- After restarting, the node synced to 3436 and mined 3437 within a minute, and
  all three reachable nodes agreed at 3437.
- The waiting transaction was a faucet payout of 1 PQ coin from about
  2026-09-19. It never reached the producer, so it was relayed there by hand
  (`bf883931…3a45`).
- Next: the 24-hour check on block production.

### MQ-05 · Redundant block producers

**Status:** Queued

Finding: one host produced every block. It runs two producers, a
`generateblock` loop every 600 s and a heartbeat timer, but both depend on the
same local node. When that node failed, the chain stopped.

Scope: run at least two independent producers on separate hosts, staggered,
with producer health in monitoring. After MQ-30, move to real RandomX mining by
outside participants and the pool.

Acceptance: stopping either producer does not stop the chain, verified by
actually stopping one.

Evidence, 2026-10-09: after the restart, only the `generateblock` loop
produces blocks (about one per 10 minutes). The heartbeat timer's
`generatetoaddress` call fails on every run with "Could not connect to the
server 127.0.0.1:18443": its CLI doesn't use the node's RPC settings, so it
falls back to the regtest default port. It would mine empty blocks anyway
until PR #65 lands. In effect there is one producer, on one host.

### MQ-06 · Uniform, identifiable testnet builds

**Status:** Queued

Finding: no node runs PR #53. Nodes run different revisions: one binary was
built 2026-04-20, another is at PR #52. All advertise `/Marscoin:28.1.0/`, the
same as mainnet. The onboarding docs configure only `--enable-randomx-vendor`
(`doc/marsqnet-onboarding.md:22`, `doc/wiki-marsqnet-quickstart.md:22`). Nodes
built that way cannot verify PQ spends and reject blocks that contain them.

Scope: build from a tagged revision with both vendor flags, deploy to every
node, add a distinct user-agent comment for marsqnet builds, publish the build
revision, and fix the onboarding docs.

Acceptance: `getpeerinfo` on every node shows the same tagged revision, and
following the onboarding docs produces a node that verifies PQ spends.

### MQ-07 · Monitoring that leads to action

**Status:** Queued

Finding: the watchdog detected the stall and was configured to email every 6
hours for four months; the chain stayed down. Its mainnet check reports "RPC not
responding" while the mainnet node is healthy and at the current tip. That node
was started by hand outside its systemd unit, and the watchdog's CLI call fails
in its environment (cause not yet confirmed).

Scope: fix the mainnet false positive, run the mainnet node under its unit,
route alerts to a channel someone watches and test delivery, and escalate if an
alert stays open more than 24 hours.

Acceptance: an induced marsqnet stall reaches the operator within 30 minutes, a
healthy mainnet node reports green, and recovery notices arrive.

Evidence, 2026-10-08: cause of the false positive confirmed. The watchdog unit
runs without `HOME`, so `marscoin-cli` can't find the operator's config and
falls back to the default RPC port; with `HOME` set it returns the correct
height. Fix: add `Environment=HOME=<operator home>` to the watchdog unit in a
drop-in, then `systemctl daemon-reload`. That needs an operator, because this
session's permissions don't allow writes on servers. The watchdog reported
marsqnet as recovered at 2026-10-08 22:15 UTC.

### MQ-08 · Faucet and dashboard reflect chain health

**Status:** Queued

Finding: the public faucet stayed funded (about 148 MARS) and accepted requests
during the four-month stall, so payouts could never confirm. At least one person
used it: a 1-coin PQ payout from about 2026-09-19 sat unconfirmed until
2026-10-08. Transactions also did not reach the block producer on their own. The dashboard's
`data.json` exposes height and best hash but no tip age or stall indicator.

Scope: pause the faucet with a clear message when tip age exceeds a threshold,
and show tip age and a stall banner on the dashboard.

Acceptance: a simulated stall pauses the faucet and shows the banner, and both
recover automatically.

### MQ-09 · Testnet host cleanup

**Status:** Queued

Finding: a shell loop waiting for a build has been running on one host since
2026-04-19. One node was launched by hand with `-reindex` in its command line,
outside systemd, so relaunching it the same way reindexes again. One peer host
is not reachable with current operator SSH keys.

Scope: kill the leftover loop, put every marsqnet daemon under a systemd unit
without `-reindex`, record the host inventory in operator notes, and restore
access to the unreachable peer or decommission it.

Acceptance: every node is managed by systemd, restarts cleanly, and appears in a
current inventory.

## Track C · Consensus correctness

Everything in this track changes consensus. Finish it before marsqnet v2
(MQ-30) and before any mainnet activation, because these rules cannot change
after launch.

### MQ-10 · Switch to FIPS 205 SLH-DSA

**Status:** Queued

Finding: `ParameterSet::SLH_DSA_SHA2_128S` maps to
`OQS_SIG_alg_sphincs_sha2_128s_simple` (`src/crypto/pq_sphincs.cpp:22`), the
round-3 SPHINCS+ submission. The FIPS 205 algorithm
`OQS_SIG_alg_slh_dsa_pure_sha2_128s` is vendored but unused
(`src/crypto/oqs_vendor/liboqs/src/sig/sig.h:168`). Signatures from the two are not
interchangeable. Docs and enum names claim SLH-DSA.

Scope: switch to FIPS 205 pure SLH-DSA-SHA2-128s. Decide the context string;
a fixed Marscoin domain-separation context is recommended. Regenerate known-answer
tests against NIST ACVP vectors. Retire the round-3 identifier.

Acceptance: KATs match the NIST ACVP SLH-DSA-SHA2-128s vectors, and marsqnet v2
launches with FIPS 205 signatures only.

Evidence, 2026-10-08 (PR #61):
- Parameter-set ID `0x01` = FIPS 205 SLH-DSA-SHA2-128s, pure, signed with the
  context `marscoin-p2wpqh-v1`. The round-3 ID `0x00` is retired.
- liboqs is built with only this algorithm.
- 30 NIST ACVP vectors (keyGen 10, sigVer 14, sigGen 6) pass in unit tests and
  in the CI KAT script.
- The full per-case unit sweep shows no result changes against the baseline.

### MQ-11 · Complete PQ signature hash

**Status:** Queued

Finding: `GetSigHashPQ` (`src/script/interpreter.cpp:1786-1815`) commits to
version, nLockTime, sha_prevouts, sha_sequences, sha_outputs and the input
index only. It does not commit to spent amounts, spent scriptPubKeys, a sighash
type, spend type/annex, or the key or program. The wallet ignores the requested
sighash type (`src/script/sign.cpp:94-122`). The comment at line 1795 says
"double-SHA256" while the code is a BIP340-style tag. Line 2019 still says
"scaffold".

Scope: specify a BIP341-style message covering epoch, hash type, version,
locktime, prevouts, amounts, scriptPubKeys, sequences, outputs, spend type,
input index, annex, parameter-set ID and program. Either support
ALL/NONE/SINGLE/ANYONECANPAY or restrict the types deliberately. Publish test
vectors.

Acceptance: a spec with test vectors exists, unit tests pass, and an offline
signer can compute the fee from the data it signs.

Evidence, 2026-10-08 (PR #64):
- BIP341-style message under tag `Marscoin/P2WPQH/sighash`, committing to
  spent amounts and scriptPubKeys, with BIP341's hash types and a final
  binding to the parameter-set ID and public key.
- Payload is `[id][sig]` (DEFAULT) or `[id][sig][type]`.
- Spec: `doc/quantum-p2wpqh-sighash-v1.md`.
- 13 vectors, from an independent Python implementation whose shared logic
  reproduces Bitcoin's BIP341 vectors.
- 5 test cases (201 assertions) pass, including real FIPS 205 spends for every
  hash type under mutation.
- Also fixed: the deferring checker didn't forward the PQ sighash, and the
  P2WPQH size estimate was one weight unit short.

Follow-up: PSBT signing of P2WPQH inputs is not supported yet; needed for
hardware and offline signers before mainnet.

### MQ-12 · Separate PQ activation from ABWL

**Status:** Queued

Finding: `SCRIPT_VERIFY_WITNESS_V2` is turned on by `nABWLActivationHeight`
(`src/validation.cpp:2402-2407`). There is no PQ deployment of its own.

Scope: give PQ its own deployment (versionbits or a per-chain buried height),
independent of ABWL and RandomX, and report it in `getdeploymentinfo`.

Acceptance: tests show PQ active with ABWL inactive and the reverse, and
mainnet stays inactive until parameters are chosen in MQ-34.

Design constraint found 2026-10-08: BIP9 versionbits can't be used for
activation on Marscoin. The auxpow chain ID occupies the upper version bits
and the auxpow flag is bit 8, so merged-mined blocks can't signal, and nodes
misread the auxpow flag as an unknown versionbit ("Unknown new rules activated
(versionbit 8)"). Use a per-chain buried activation height.

Evidence, 2026-10-09 (PR #69):
- `nPQWitnessActivationHeight` (mainnet 0, testnet 1, regtest 1), read by
  `IsPQWitnessActive`.
- New regtest option `-testactivationheight=pqwitness@<h>`.
- Unit and functional tests pass, including independence from ABWL in both
  directions.
- Integration with #63: marsqnet must also set this height.

### MQ-13 · Always-compiled, minimal SLH-DSA verifier

**Status:** Queued

Finding: verification depends on `--enable-pq-oqs-vendor` (`configure.ac:619`,
default off). Without it, `VerifyMessage` returns false
(`src/crypto/pq_sphincs.cpp:171-174`). Once PQ is active, such nodes reject every
block that spends a v2 output and split from OQS-enabled nodes. PR #53's commit
message documents this failure.

Scope: compile SLH-DSA verification into consensus unconditionally. Prefer a
small, auditable verify-only implementation over the full liboqs for consensus;
wallet signing may keep a separate implementation. Include it in depends and
reproducible builds.

Acceptance: a default `./configure` build verifies PQ spends, and no consensus
path depends on an optional build flag.

Evidence, 2026-10-09 (PR #71):
- slhdsa-c (FIPS 205, Apache-2.0 OR ISC OR MIT) is vendored byte-identical to
  upstream `a0fc1ff2` in `src/crypto/slhdsa/` and built into every node.
- liboqs is removed (7,042 files, 153 MB). `--enable-pq-oqs-vendor` remains as
  a no-op.
- All 30 NIST vectors pass. Interoperability with the old liboqs backend is
  byte-exact for keygen and deterministic signing, and verification works both
  ways.
- A plain `./configure` build signs and verifies P2WPQH spends.
- Finding: release builds never passed the old flag, so released binaries had
  no P2WPQH verification. That would have split the chain at activation.
- The CI check is renamed "Linux PQ SLH-DSA KAT".

### MQ-14 · Parameter-set agility and P2SH-wrapped v2

**Status:** Queued

Finding: the parameter-set byte is committed into the program
(`program = SHA256(param_set_id || pubkey)`, `src/script/interpreter.cpp:2003-2009`),
which allows new parameter sets without a new witness version. But the pubkey
length check is hardcoded to the 128s size (line 1999). The program hash is
untagged. P2SH-wrapped v2 falls through to the unknown-witness rule (line
1979): unencumbered, discouraged only by policy.

Scope: per-parameter-set size table; decide on a tagged program hash, together
with MQ-10; the wallet never creates wrapped v2; wrapped-v2 spends are
non-standard and documented. Reserve parameter-set IDs for hash-based schemes
only.

Acceptance: adding a parameter set needs only a table entry and KATs, and tests
cover wrapped-v2 handling.

### MQ-15 · RandomX must fail closed

**Status:** Queued

Finding: `GetProofOfWorkHash` returns `uint256{}` when RandomX is not compiled
in or when cache init or hashing fails (`src/pow.cpp:416`, `429`, `438`). A zero
hash passes `CheckProofOfWorkImpl` (`src/pow.cpp:450-466`), so proof of work is
effectively unchecked in those cases. The RandomX vendor flag defaults to off
(`configure.ac:613`).

Scope: a RandomX failure makes the block invalid and never yields a passing
hash. A node refuses to start on a RandomX chain without RandomX support.

Acceptance: tests show a build without RandomX refusing to start on marsqnet,
and an injected hashing failure rejecting the block.

Evidence, 2026-10-08 (branch `fix/randomx-fail-closed`, local, two commits):

- `GetProofOfWorkHash` returns `std::optional<uint256>` and `CheckProofOfWork`
  rejects a missing hash. Init refuses a RandomX chain on a build without
  RandomX.
- **New finding:** `src/pow.cpp`, `src/test/pow_tests.cpp` and
  `src/test/crypto_tests.cpp` never included `config/bitcoin-config.h`, so
  their `ENABLE_RANDOMX_VENDOR` / `ENABLE_PQ_OQS_VENDOR` blocks were always
  compiled out:
  - Every marsqnet build has returned a zero proof-of-work hash. The live chain
    has never checked RandomX, and any header with valid `nBits` passes.
  - The RandomX branch of `pow.cpp` had a latent compile error, now fixed.
  - The RandomX vector test and the SPHINCS+ known-answer test had never run.
    Both pass now.
- With real RandomX checking, the current marsqnet genesis fails proof of work
  (`non-AUX proof of work failed` while reading block 0), so this build cannot
  join the existing marsqnet. It depends on a RandomX genesis (MQ-18, MQ-30).
  Do not deploy it to current marsqnet nodes.
- Tests: the three new tests plus the two previously dead ones pass. The full
  `pow_tests`/`crypto_tests` show only the 8 pre-existing `pow_tests` failures
  (MQ-27).
- The injected-failure test is not done. The optional return type prevents a
  failed hash from passing, but nothing exercises a forced InitCache/HashOnce
  failure yet.

### MQ-16 · RandomX key binding and auxpow rules

**Status:** Queued

Finding: the RandomX key is the header's `hashPrevBlock`. For auxpow blocks it
comes from the parent block, which is null for locally mined parents
(`src/auxpow.cpp:179-183`), so the key is not bound to the Marscoin chain.
Marsqnet allows auxpow from height 0 with `fStrictChainId = false`.
Verification runs in light mode only (`src/randomx_wrapper.cpp:17`).

Scope: specify the key schedule (for example a lagged key block taken from the
Marscoin chain), decide whether merged mining is allowed under RandomX at all,
enforce strict chain IDs, and document full-dataset mining versus light-mode
verification. See #34.

Acceptance: a spec plus tests, including rejection of an auxpow block whose key
is not derived from the Marscoin chain.

### MQ-17 · Marsqnet difficulty retargeting

**Status:** Queued

Finding: `GetNextWorkRequired_V1` ignores `fPowNoRetargeting`, min-difficulty
blocks are allowed, and powLimit is `00000fff…` (`src/pow.cpp:248-330`).

Scope: marsqnet uses the difficulty rules intended for a RandomX mainnet, so the
testnet exercises them. Decide on any min-difficulty exception.

Acceptance: a functional test and marsqnet v2 both show difficulty following
hashrate changes.

### MQ-18 · Marsqnet as its own chain type

**Status:** Queued

Finding: `-chain=marsqnet` maps to `ChainType::REGTEST`
(`src/common/args.cpp:788`) plus a `randomx_devnet` flag
(`src/chainparams.cpp:47-49`). It inherits the regtest genesis, which is the
mainnet genesis (`src/kernel/chainparams.cpp:647`), halving every 150 blocks,
and the regtest checkpoint. `getblockchaininfo` reports `"regtest"` and the data
directory uses a `regtest/` subfolder. Nodes log
`Unknown new rules activated (versionbit 8)`, probably the AuxPoW version flag
being read as a versionbits signal (unconfirmed).

Scope: add `ChainType::MARSQNET` with its own genesis, parameters, data
directory and chain name, and a mainnet-like subsidy schedule. Confirm the
versionbit-8 cause, check whether mainnet logs the same warning, and fix it.

Acceptance: `getblockchaininfo` reports `"marsqnet"`, the genesis is distinct,
and there is no spurious warning.

Decision (2026-10-08, owner): use an easier marsqnet powLimit, with retargeting
(MQ-17) raising difficulty as miners join. Background: with RandomX actually checked (MQ-15), marsqnet
needs a genesis mined under RandomX. Its minimum difficulty also matters.
At the current powLimit (`00000fff…`), each block needs on the order of a
million RandomX hashes. The node's built-in `generateblock` hashes in light
mode, so one heartbeat miner would be far slower than the 10-minute target.
Options:
- an easier marsqnet powLimit, so a single CPU can keep the chain moving with
  retargeting (MQ-17) raising difficulty as miners join; or
- keep a mainnet-like powLimit and rely on external fast-mode miners and the
  pool.

Recommendation: the easier powLimit for marsqnet, with the mainnet value
decided separately in MQ-43.

Evidence, 2026-10-08 (branch `fix/marsqnet-chain-type`, local, built on #56 and
MQ-46):
- `ChainType::MARSQNET` with its own data directory and RPC port 29337.
  `getblockchaininfo` reports `"marsqnet"`. Regtest is plain regtest again.
- powLimit `00ffff…`, about 256 RandomX light-mode hashes per block (one CPU
  measured at 36–37 H/s, so about 7 s per block at the limit).
- ASERT at every height (`fPowAlwaysAsert`): 2-hour half-life, 123 s spacing.
- Strict chain ID; mainnet-like subsidy schedule.
- New genesis mined with the node's own `GetProofOfWorkHash`: nonce 185, nBits
  `0x2000ffff`, hash `61174bcc…9a08`.
- Blocks carry chain ID `0x4D51` in the top version bits, so the
  "unknown versionbit 8" warning no longer appears.
- The two-node smoke test passes with real RandomX. The full unit sweep matches
  the baseline except for six previously hanging tests, which now finish.

Open decisions:
- v2 keeps v1's message start and P2P port, so old v1 nodes would connect but
  fail to sync. Recommendation: give v2 new message-start bytes.
- Fresh nodes stay in initial block download until a recent block exists, and
  `createauxblock` refuses during IBD, so the first v2 blocks must come from
  the built-in miner.

### MQ-19 · ABWL persistence and block-index versioning

**Status:** Queued

Finding: new `CBlockIndex` fields are serialized (`src/chain.h:425-428`) but not
copied in `LoadBlockIndexGuts` (`src/node/blockstorage.cpp:118-131`), so the
limit drops back to the 4M floor after a restart. The fields are appended to
every block-index record without a version check, so existing data directories
probably fail to read (`src/node/blockstorage.cpp:136`). That last point is
inferred from code and has not been run.

Severity update, 2026-10-08:
- The appended fields are written and expected on **every chain, mainnet
  included**.
- Records written by released builds end before those fields, so reading them
  fails and `LoadBlockIndexGuts` reports "failed to read value". Anyone running
  this branch on an existing mainnet data directory would need a full
  `-reindex`. That is probably why one marsqnet node was launched with
  `-reindex`.
- This is inferred from code; a regression test should confirm it.

Scope: load the fields and keep released data directories readable.
Recommended approach:
- Leave `CDiskBlockIndex` in the upstream format.
- Store ABWL state under its own database key, written only for blocks where
  ABWL is active, and load it in a separate pass.
- Mainnet then writes no ABWL records at all.
- Test upgrading an old data directory.

Evidence, 2026-10-08 (branch `fix/abwl-index-compat`, local, one commit):
`CDiskBlockIndex` is back to the upstream format, and BlockTreeDB stores ABWL
state under key `W` only for blocks that carry it, loading it in a separate
pass. The new test `abwl_state_persists_outside_block_index_record` shows ABWL
fields don't change the record bytes and that state survives a write and load.
It passes, as do all ABWL and `block_malleation` tests. Records written by
earlier builds of this branch still load, but their ABWL state isn't
recovered. Marsqnet restarts from a new genesis anyway (MQ-30).

Acceptance: a restart preserves the limit, and an old data directory loads or
upgrades by a documented path.

### MQ-20 · ABWL end-to-end capacity

**Status:** Queued

Finding: the ABWL floor equals `MAX_BLOCK_WEIGHT`, and every other limit still
caps blocks at 4M weight or 4 MB: the miner clamp to `DEFAULT_BLOCK_MAX_WEIGHT`
(`src/node/miner.cpp:67`), `MAX_BLOCK_SERIALIZED_SIZE`
(`src/consensus/consensus.h:13`), the 4 MB P2P message limit (`src/net.h:63`),
and the `getblocktemplate` weight limit (`src/rpc/mining.cpp:965`). ABWL cannot
make room for 7.8 KB signatures yet.

Scope: carry the dynamic limit through the miner, `getblocktemplate`, P2P
message size, serialized-size checks and compact blocks. Set a hard ceiling
from MQ-28 data on verification time.

Acceptance: a functional test grows blocks past 4M weight under sustained PQ
load and relays them between nodes, and the ceiling is enforced.

Finding, 2026-10-08 (from code, not yet reproduced):
- ABWL state is computed in `ReceivedBlockTransactions`, when a block's data
  arrives, from its parent's state. During initial sync, blocks routinely
  arrive before their parents' data, so a parent's state may still be unset.
- `ContextualCheckBlock` reads the parent's state in `AcceptBlock`, before
  ancestors are connected.
- Today the limit never drops below 4M and blocks can't exceed 4M, so the check
  never binds. Before ABWL can raise the limit, compute state when blocks are
  connected (in order) and enforce the dynamic limit there.

### MQ-21 · Restore the mainnet context-free block bound

**Status:** Queued

Finding: the context-free `CheckBlock` weight limit was raised to 128M on all
chains, mainnet included (`src/validation.cpp:4000`). The contextual check still
enforces 4M on mainnet, but the early bound protects against oversized blocks
before context is available.

Scope: tie the context-free bound to each chain's ABWL ceiling, and keep
mainnet at the upstream value until ABWL activates there.

Acceptance: a test shows mainnet `CheckBlock` rejecting blocks over 4M weight.

Evidence, 2026-10-08 (branch `fix/abwl-mainnet-bound`, local, one commit):
`GetContextFreeMaxBlockWeight` returns `MAX_BLOCK_WEIGHT` on chains without
ABWL and the ABWL ceiling otherwise. The new test
`abwl_context_free_bound_per_chain` builds a block just over 4M weight and
shows `bad-blk-length` on mainnet but not on regtest. It passes, and the full
`validation_tests` show only the pre-existing `subsidy_limit_test` failure
(MQ-27).

## Track D · Wallet

### MQ-22 · Seed-derived PQ keys and backup

**Status:** Queued. Required before mainnet.

Finding: PQ keys come from liboqs's system RNG through `OQS_SIG_keypair`
(`src/crypto/pq_sphincs.cpp:75`), not from the HD seed. They are not in
descriptors, so `listdescriptors` does not export them, and a seed backup cannot
recover them.

Scope: derive PQ keys deterministically from the wallet seed using only hash
functions (for example HMAC-SHA512 over seed and path, expanded into the
SLH-DSA key seeds). Add a descriptor type, import and export, and recovery by
rescan.

Acceptance: a wallet restored from its seed or descriptors alone finds and
spends its PQ coins.

### MQ-23 · PQ keys in the key manager

**Status:** Queued

Finding: PQ keys are not loaded at startup. `IsMine` and signing open a new
`WalletBatch` and read the database on each call
(`src/wallet/wallet.cpp:1633-1646`, `2211-2216`). The record parser hardcodes a
1+32+64-byte layout (`src/wallet/walletdb.cpp:317-335`).

Scope: integrate PQ keys with the descriptor ScriptPubKeyMan and an in-memory
index, with a per-parameter-set layout.

Acceptance: `IsMine` performs no database reads, and a wallet with 10,000 PQ
keys loads and rescans in a time comparable to a legacy wallet of that size.

### MQ-24 · Migration tooling

**Status:** Queued

Finding: `getquantummigrationstatus` (`src/wallet/rpc/wallet.cpp:150-210`) is
read-only and hardcodes `migration_enabled=false`, `phase="scaffold"` and
`migrated_balance=0`. It counts P2WPQH coins as legacy balance. See #37.

Scope: correct accounting split into PQ, hashed-key legacy and exposed-key
legacy; a sweep command from legacy to PQ; Qt support.

Acceptance: status is correct for a mixed wallet, a one-command sweep works on
marsqnet, and a functional test covers both.

## Track E · Verification and documentation

### MQ-25 · Script-level and unit tests

**Status:** Queued

Finding: there is no test of `VerifyWitnessProgram` for v2, the PQ sighash,
wallet PQ signing or `getnewpqaddress`. RandomX tests
(`src/test/pow_tests.cpp:216-240`) never exercise `CheckProofOfWork`. ABWL tests
(`src/test/validation_tests.cpp:366-477`) cover only pure functions.

Scope: script vectors for v2 (valid, wrong pubkey, wrong parameter set, bad
signature, wrong sizes, wrapped), sighash vectors, PoW tests through
`CheckProofOfWork`, ABWL inside block validation, and fuzz targets for PQ
witness parsing.

Acceptance: all run in CI.

### MQ-26 · Functional tests

**Status:** Queued

Finding: nothing in `test/functional` mentions pq, sphincs, mars1z or quantum.
No test runs a P2WPQH output through create, fund and spend.

Scope: tests for the PQ lifecycle (PQ to PQ, mixed inputs, invalid spends
rejected in mempool and blocks, reorgs), PQ wallet encryption, PQ backup and
restore, ABWL, RandomX proof of work, and large-block relay.

Findings, 2026-10-08:
- The test framework wrote `bitcoin.conf`, but marscoind reads
  `marscoin.conf`. Functional-test nodes ignored their config, started on
  mainnet and connected to mainnet peers. Fixed on `fix/pq-safety-guards`
  (`CONF_FILENAME`).
- `test_runner.py` can't build its block cache, so no functional test can run
  through it (MQ-46).
- `rpc_validateaddress` uses Bitcoin `bc1` vectors, and
  `rpc_invalid_address_message` expects "bitcoin address" in help text. Both
  fail for reasons unrelated to quantum work.
- Now that regtest can mine (#62), more inherited failures are visible:
  - `rpc_blockchain` and `mining_basic`: `CreateNewBlock: TestBlockValidity
    failed: time-too-new` under mocktime, even though `MAX_FUTURE_BLOCK_TIME`
    is the upstream 2 hours. Needs investigation.
  - `feature_block`: the test framework's P2P interface never completes its
    handshake with the node. Every test that uses the P2P interface will hit
    this, so it is the most important to fix next.
  - `wallet_basic`: Bitcoin subsidy and maturity assumptions.

Progress, 2026-10-08:
- PR #66 adapts the framework:
  - config file and binary names
  - P2P version 70060
  - scrypt proof of work and the chain ID for Python-built blocks
  - the Marscoin genesis time, which was the cause of `time-too-new`
  - AuxPoW header parsing and a pure-Python scrypt fallback
  - config paths in five tests, and bech32 test nodes

  The suite went from 71 to about 118 passing out of 312 runs; 68 skip
  without BDB.
- Node bug found: `generatetoaddress` mined empty blocks (PR #65).
- Remaining failures by cause:
  - RBF is disabled by design ("core: never allow rbf to occur").
  - Bitcoin constants and vectors: addresses, message magic, legacy
    activations, versionbits, assumeutxo hashes.
  - Signet can't start (MQ-49).
  - Several P2P tests, `feature_block` and `feature_maxuploadtarget` are not
    yet diagnosed.
- Minor: the `-mempoolfullrbf` help text still advertises replace-by-fee.

Acceptance: all are in `test_runner.py` and pass in CI.

### MQ-27 · CI and release discipline

**Status:** Queued

Finding: whether the CI jobs pass is unknown, and recent local builds used
`--disable-tests`. 48 commits sit on `feature/quantum-upgrade`, none in `28.x`.
The only testnet tag is `marsqnet-baseline-2026-04-13`. Merged local branches
remain.

Pre-existing unit test failures, confirmed against the unmodified base
2026-10-08: eight `pow_tests` cases (`ChainParams_*_sanity` for MAIN, TESTNET,
TESTNET4 and SIGNET, and the four `get_next_work*` cases), and
`validation_tests/subsidy_limit_test` with 13,557 failed assertions. These are
upstream Bitcoin tests that assume Bitcoin's parameters. Until they are adapted
to Marscoin's parameters, the full suite cannot be a required CI check.

Also failing before any of today's changes, judged from the errors:
- 12 wallet tests and two `blockmanager_tests` use `TestChain100Setup`, which
  asserts Bitcoin's regtest tip hash.
- `blockmanager_flush_block_file` expects Bitcoin's `ReadBlockFromDisk` log
  text where Marscoin logs `ReadBlockOrHeader`.
- `key_io_valid_gen`/`key_io_valid_parse` use Bitcoin test data.

CI, 2026-10-08: the macOS ARM64 GUI job fails on every PR because Homebrew now
ships Boost 1.92. Its `is_index_list` rejects index lists declared as structs
derived from `indexed_by<...>`. Fixed in PR #60, which uses type aliases.

Running the whole suite in one process makes it worse: the first abort leaves
the process broken and many later tests fail on a duplicate-argument
assertion. Run each case in its own process until the inherited tests are
fixed. A side-by-side baseline comparison is running on the build machine.

Scope: adapt or replace those inherited tests, then make CI build with both
vendor flags and run all tests as required checks. Add a lint check that any
file testing a `config/bitcoin-config.h` macro includes that header (see
MQ-15). Also a tag for every testnet deployment, a branch policy, and removal
of merged branches.

Acceptance: CI is green on the feature branch, and every deployed testnet binary
maps to a tag.

Progress, 2026-10-09:
- PR #72 adds `test/lint/lint-config-macros.py` as a CI job (stacked on #56).
  It flags files that test configure-defined macros without including
  `config/bitcoin-config.h`, and tests of `ENABLE_*` macros that nothing
  defines.
- It catches both silent failures from this review: the RandomX include in
  `pow.cpp` and the stale `ENABLE_PQ_OQS_VENDOR` guard found while verifying
  #71.
- Per-case unit sweeps, compared with a baseline, have now caught three
  silently missing or hanging test groups. Make such sweeps part of release
  checks.

### MQ-28 · Performance and stress testing

**Status:** Queued

Finding: SLH-DSA signing and verification cost has never been measured. The
live testnet saw one PQ spend (block 1383, 2026-04-20) and five PQ outputs that
were never spent. No full-block PQ load test exists.

Scope: benchmarks for signing, verification and sighash. A marsqnet stress
campaign with full blocks of PQ spends, initial sync time, mempool behavior,
signing time for wallets with many inputs, and fee estimation.

Acceptance: published numbers, used to set the MQ-20 ceiling and DoS limits.

Measurements, 2026-10-09 (PR #74, Apple Silicon, one thread):
- SLH-DSA-SHA2-128s keygen 28.3 ms, signing 216.8 ms, verification 224.6 µs.
  The existing ECDSA P2WPKH script verification takes 12.4 µs.
- Per byte, P2WPQH verification (about 28 µs/KB) is no more expensive than
  ECDSA (about 115 µs/KB), so validation cost tracks block size.
- A 4M-weight block holds about 500 P2WPQH inputs (about 0.11 s on one
  thread). The 128M ABWL ceiling allows about 16,000 inputs (about 3.6 s on
  one thread, less in parallel).
- Signing about 0.2 s per input makes large consolidations slow (100 inputs
  takes about 22 s); wallets need progress feedback.
- Still to do: a full-block PQ stress run and initial-sync timing on marsqnet
  v2.

### MQ-29 · Documentation refresh

**Status:** Queued

Finding: `doc/quantum-signatures-sphincs-v1.md:97-102` says there are no
witness, wallet or consensus changes. `doc/randomx-consensus-profile-v1.md:27`
says there are no block validation changes. Nothing documents P2WPQH, `mars1z`,
`getnewpqaddress`, the sighash or ABWL. `doc/quantum-address-format-v1.md` still
describes the `1pq` format.

Scope: BIP-style specifications for P2WPQH, the sighash, addresses, ABWL, the
RandomX profile and marsqnet, plus the onboarding fix from MQ-06.

Acceptance: docs match the code at a tag and have been reviewed.

## Track F · Network

### MQ-30 · Marsqnet v2 (fresh genesis)

**Status:** Queued. Depends on Track C and MQ-22.

Scope: launch a new marsqnet on the final rules, with external miners and the
pool, a faucet, seed nodes and an announcement. Retire the current marsqnet.

Acceptance: at least three independent node operators and two independent
miners, a 30-day soak following `doc/marsqnet-soak-checklist.md`, and an agreed
PQ transaction volume.

## Track G · Mainnet path

### MQ-31 · Mainnet exposure census

**Status:** Queued

Scope: scan the mainnet UTXO set and report the share of supply in each class:
P2PK, address types whose public keys have been revealed by earlier spends,
Taproot outputs (confirm whether Taproot is active on mainnet), multisig with
revealed keys, and unexposed hashed outputs. Break down by dormancy.

Acceptance: a repeatable script and a published aggregate report.

Result, 2026-10-09 (PR #73), mainnet at height 3,576,274, totals matching
`gettxoutsetinfo`:
- 2.25% of the supply (887,459.50 MARS) is exposed at rest, almost all in P2PK
  outputs over ten years old.
- 97.75% is hashed: P2PKH 97.67%, P2SH 0.08%.
- There are no SegWit, Taproot or P2WPQH outputs.
- About 85% of the supply hasn't moved in five years or more.
- Part 2 (address reuse): **47.37% of the supply is exposed in total.**
  - 45.12% is P2PKH at 4,176 addresses whose key an earlier spend revealed.
  - Only 52.63% is behind an unrevealed hash.
  - 31.52% of the supply is exposed and untouched for 10+ years.
  - Commit–delay–reveal can't protect the reused 45%; only pre-emptive
    migration, or an HD-only derivation-secret rescue, can.
  - Checks: all 3,576,283 blocks and 3,924,405 transactions parsed, and 25/25
    spot checks matched.

### MQ-32 · Unbundled roadmap and crypto policy

**Status:** Waiting: owner decision

Scope: decide and publish the order of work (PQ soft fork first, RandomX as a
separate hard fork, recycling as the last step of migration) and the hash-only
consensus crypto policy. Update #32 and #43.

Acceptance: the owner approves the roadmap and the issues reflect it.

### MQ-33 · External security review

**Status:** Queued

Scope: an independent review of the SLH-DSA integration, sighash, activation,
wallet key handling and ABWL DoS limits before mainnet parameters are set.

Acceptance: a written report, with every finding resolved or explicitly
accepted.

### MQ-34 · Mainnet PQ soft-fork activation

**Status:** Queued

Scope: activation parameters, miner coordination, a release, and communication
with node operators, exchanges and the pool.

Acceptance: PQ is active on mainnet and PQ spends confirm.

### MQ-35 · Ecosystem migration

**Status:** Queued

Scope: wallets create PQ addresses by default after activation (Qt, web wallet,
mobile), explorers display `mars1z`, the pool pays out to PQ, exchanges accept PQ
deposits, and the Martian Republic adopts PQ. Run a migration campaign with
public progress based on MQ-31.

Acceptance: the share of supply migrated is tracked publicly.

## Track H · Emergency and rescue

### MQ-36 · Commit–delay–reveal rescue path

**Status:** Queued

Scope: a protocol for spending hashed-key legacy outputs safely after an
elliptic-curve break. The owner commits to a hash of the outpoint, public key
and destination, waits a fixed number of blocks, then reveals. Write the spec
and implement it behind its own deployment.

Acceptance: a spec, a marsqnet implementation, and a test showing that a
front-runner who learns the public key at reveal time cannot redirect the spend.

### MQ-37 · Seed-knowledge proof rescue

**Status:** Research

Scope: assess a hash-based STARK proving knowledge of the BIP32 seed behind an
exposed key through hardened derivation. Hardened derivation is HMAC-SHA512, so
the proof stays sound after elliptic curves fall. Measure proof size and
verification cost. Prior art: Vitalik Buterin, "How to hard-fork to save most
users' funds in a quantum emergency" (ethresear.ch, 2024).

Acceptance: a feasibility study with prototype numbers.

Progress, 2026-10-09 (draft spec, PR #70, covering MQ-36 to MQ-38):
- Main finding: revealing a BIP32 extended private key with at least one
  hardened step stays sound after a curve break, because forging the parent is
  an HMAC-SHA512 preimage problem. HD-wallet owners can therefore rescue even
  exposed-key outputs without a STARK in consensus.
- The STARK route is deferred: rough estimates are 100 KB–1 MB proofs,
  10–100 ms verification, and a large frozen verifier.

### MQ-38 · Break-glass emergency soft fork

**Status:** Queued

Scope: a pre-written, reviewed and tested soft fork that disables plain
ECDSA/Schnorr spends except through the MQ-36 and MQ-37 paths. It must be
deployable within days, with a runbook stating who decides and how.

Acceptance: code on a branch with tests, a runbook, and a marsqnet dry run.

### MQ-39 · Sunset and UTXO recycling spec

**Status:** Queued

Finding: UTXO recycling has no code or spec in this repo (tracked in #36).
Nothing restricts or sunsets legacy ECDSA spends.

Scope: a governance framework running from migration window to rescue window to
freeze or recycle, covering legal and community process, and a written spec.

Acceptance: a published spec and a defined community process.

## Track I · Research

### MQ-40 · Smaller hash-based parameter set

**Status:** Research

Scope: evaluate SLH-DSA variants that cap signatures per key for smaller
signatures (the "SPHINCS-" direction), the signing limits a UTXO key needs,
wallet enforcement of those limits, and standardization status. Consider
hash-based options only.

Acceptance: a recommendation with sizes, security levels and limits.

### MQ-41 · Non-signature public-key crypto threat model

**Status:** Research

Finding: BIP324 v2 transport is on by default (`src/net.h:95`) and uses
ElligatorSwift ECDH on secp256k1 (`src/bip324.cpp:40-41`). Tor onion service
creation is on by default (`src/torcontrol.h:24`) with ED25519-V3 keys
(`src/torcontrol.cpp:468`). I2P uses Ed25519 (`src/i2p.cpp:358`, `426`). None
has a post-quantum alternative.

Scope: document what each one protects (privacy, not consensus or funds), track
upstream hybrid PQ key-exchange work for BIP324, and keep consensus free of
these assumptions.

Acceptance: a threat-model document.

### MQ-42 · Ecosystem public-key and identity review

**Status:** Research

Scope: inventory where the ecosystem depends on elliptic-curve or other
public-key cryptography: Martian Republic citizenship and voting signatures,
the web wallet, APIs and TLS. Plan a PQ path for each (hash-based identity
signatures, hybrid TLS).

Acceptance: an inventory and a plan.

### MQ-43 · RandomX mainnet track

**Status:** Waiting: owner decision

Scope: a separate hard-fork plan for RandomX on mainnet, covering the pool spec,
miner software, the merged-mining decision from MQ-16 and activation,
independent of the PQ soft fork. See #34.

Acceptance: an owner decision and a written plan.

### MQ-44 · Correct public claims about marsqnet

**Status:** Queued

Finding: the April 2026 marsqnet announcement says blocks were "mined on the
RandomX algorithm" and that CI runs RandomX checks. No marsqnet build has
checked RandomX proof of work (MQ-15), and the CI smoke test passes because
proof of work is unchecked.

Scope: publish a short, factual correction with the MQ-15 fix and the marsqnet
v2 plan. Check future announcements against tagged code before publishing.

Acceptance: the announcement is corrected or annotated, and the correction
links to the fix.

### MQ-45 · Secret scanning in CI

**Status:** In progress: PR #54.

Marscoin is built in public, so nothing that looks like a credential may land
in the repository.

Scope:
- `.github/workflows/secret-scan.yml` runs gitleaks 8.30.1 (a pinned release
  verified by SHA-256) on every push and pull request, scanning only new
  commits. A weekly run and manual runs scan the full history.
- `.gitleaks.toml` allowlists vendored crypto and test-vector data.
- `.gitleaksignore` lists reviewed historical findings by fingerprint.
- Contributors scan new commits locally before pushing.

Evidence, 2026-10-08:
- The first full-history scan found 137 matches in 28,107 commits. All were
  reviewed: public test vectors, example values in upstream docs, alert-system
  public keys, a PGP fingerprint, package checksums and identifiers. None is a
  credential.
- With the new configuration, the full history scans clean. The new commits on
  every fix branch scan clean, and a planted test credential is detected.

Acceptance: the workflow runs on GitHub and passes, it fails on a planted
credential in a test PR, and branch protection requires it.

### MQ-46 · Regtest that can mine

**Status:** Queued

Finding, 2026-10-08:
- Regtest uses powLimit `00000fff…`, about 2^20 Scrypt hashes per block
  (roughly 190 s on the development laptop), so `test_runner.py` can't build
  its block cache and no functional test can mine.
- `generatetoaddress` on regtest also fails with "block does not have our
  chain ID (got 8192, expected 1)". The version-bits field collides with the
  strict chain-ID check; `-blockversion=65540` works around it.

Scope: give regtest a trivial powLimit (as in upstream Bitcoin Core) and fix
the chain-ID check for regtest blocks, so the functional test suite can run in
CI. Coordinate with MQ-17/18 for marsqnet.

Acceptance: `test_runner.py` builds its cache and runs the PQ functional tests
end to end, including a confirmed P2WPQH spend.

Progress, 2026-10-08 (branch `fix/regtest-mining`, local, one commit):
- Plain regtest uses powLimit `7fff…` and a new genesis (nonce 2, nBits
  `0x207fffff`, hash `6b876256…ab76`, mined and checked in Python against the
  existing genesis). Its genesis checkpoint was Bitcoin's regtest genesis hash
  and now uses the real one.
- `GetNextWorkRequired` honors `fPowNoRetargeting`, which only regtest sets.
- The block assembler writes the auxpow chain ID into `nVersion`, as
  `createauxblock` already did. Without it, blocks from `getblocktemplate`,
  `generateblock` and `generatetoaddress` fail the strict chain-ID check, which
  likely means non-merged mining via `getblocktemplate` is also broken on
  mainnet.
- Marsqnet keeps its current parameters until MQ-18.
- New unit tests: `pow_tests/regtest_keeps_difficulty` and
  `validation_tests/block_template_has_auxpow_chain_id`.
- Verified on the build machine:
  - The per-case unit sweep shows no regressions: the six formerly hanging
    tests now finish (2 pass, 4 fail inside an inherited test) and the two new
    tests pass.
  - With #55's framework fixes, `test_runner.py` builds its block cache.
    `rpc_generate`, `wallet_encryption --descriptors` and
    `wallet_pq_safety --descriptors` pass. Published as PR #62.

### MQ-47 · Fresh mainnet nodes don't bootstrap

**Status:** Queued

Finding, 2026-10-08:
- A fresh v28.1.2 mainnet node (empty data directory, default seeding) made no
  outbound connection attempts at all over 28 minutes, and then again in a
  second run with `-debug=net`. The DNS seed returned 3 addresses, all of which
  accept connections on 8338, and 5 hard-coded seeds were loaded, but the log
  shows no "trying ... connection" lines.
- With one `-addnode` the node connected at once, synced 130,000 headers in
  under a minute, and then began automatic outbound connections normally.
- New users who install a release without an `-addnode` may never sync. This
  needs reproducing on a second machine and a root cause, starting with
  addrman selection of DNS and fixed-seed addresses (service flags,
  timestamps).

Acceptance: a fresh release node syncs with default settings, and a regression
test or documented procedure covers it.

Root cause and fix, 2026-10-08:
- `SeedsServiceFlags()` had `NODE_WITNESS` commented out (`7de89800ad`), so
  seeded addresses carried `NODE_NETWORK` only.
- Regular outbound connections require every desirable flag, which includes
  `NODE_WITNESS`. Feelers start only once outbound slots are full. So a node
  holding only seed addresses never connects.
- All 9 live peers checked advertise `NODE_WITNESS`.
- The fix restores the upstream value. Run side by side for 3 minutes from
  empty data directories: unpatched v28.1.2 got 0 peers and 0 attempts;
  patched got 7 peers, 354,000 headers and 49 attempts.
- PR #67 (feature branch) and backport PR #68 (`28.x`, together with #65).

### MQ-48 · Backport the generatetoaddress fix to 28.x

**Status:** Queued

Finding, 2026-10-08:
- `generatetoaddress` and `generatetodescriptor` mine empty blocks, because
  `generateBlocks` passes `.use_mempool = false`. That shipped in v28.1.0
  through v28.1.2 (commit `6d7e42f6a8`).
- Pool mining and `getblocktemplate` are unaffected, but anyone mining with
  these RPCs never confirms mempool transactions.
- Fixed in PR #65 and verified on regtest: `nTx` went from 1 to 2, and the
  test transaction went from 0 to 1 confirmation.

Scope: cherry-pick #65 to `28.x` for the next point release.

### MQ-49 · Signet can't start

**Status:** Queued

Finding, 2026-10-08: Marscoin's signet uses Bitcoin's signet genesis
(`1598918400, 52613770, 0x1e0377ae`), mined for SHA256d. It fails Marscoin's
scrypt proof-of-work check, so a signet node won't start, and `feature_signet`
and `tool_signet_miner` fail.

Scope: give signet a Marscoin genesis, or remove signet support if it isn't
wanted.

### MQ-50 · End address reuse in Marscoin wallets and services

**Status:** Queued

Finding, 2026-10-09 (MQ-31): 45.12% of the supply sits at 4,176 P2PKH
addresses whose public key an earlier spend already revealed. After an
elliptic-curve break, these coins could be taken directly, and
commit–delay–reveal can't protect them.

Scope:
- Audit the project's own wallets and services for address reuse: the web
  wallet, the Martian Republic stack, pool payouts, the faucet, and Electrum
  and mobile wallets. Fix default behavior that reuses addresses, such as
  fixed payout or change addresses.
- Publish guidance for holders and services: stop reusing addresses now, and
  prepare to move to P2WPQH once it activates.
- Target migration outreach at the largest reused addresses (exchanges,
  pools, long-time holders) using aggregate census data, never publishing
  individual addresses.

Acceptance: no project-run wallet or service reuses addresses by default, and
the next census shows the reused share falling.

Audit, 2026-10-09 (read-only, project wallet and service repositories):
1. **Martian Republic civic wallet: reuse by design.** The citizen identity is
   one P2PKH address (`m/44'/2'/0'/0/0`). It is the civic receive address and
   the change address for citizenship applications and endorsements, so its
   key is revealed at the first action and the balance stays there. API login
   signs with the same key.
   - Fix: send civic change to a fresh internal address, stop showing the
     identity address for receiving, and in the longer run prove identity by
     signature rather than by spending from the identity address.
2. **Fixed donation addresses:** on the website (also used as the docs'
   example address), the Electrum development fund, and the Mars Society
   donation address.
   - Fix: take donations at per-request HD addresses and sweep to fresh ones.
     Use an obviously fake example address in docs.
3. **Electrum with imported keys:** change goes back to the first input's
   address by default (`use_change=False` for imported wallets).
   `get_receiving_address` falls back to an already-used address.
   - Fix: sweep imported keys into an HD wallet, or default `use_change` on.
4. **MarsWallet and ByteWallet change handling:** `getChangeAddressAsync` is
   unimplemented, with fallbacks to a stale change index or change index 0.
   Swaps in ByteWallet use a cached address.
   - Fix: ship the existing change-address fix and port it to ByteWallet.
5. **Cross-chain key reuse:** the mobile wallets and Electrum's BIP39 option
   use coin type 2, which is Litecoin's SLIP-44 number. The same seed yields
   the same keys on Litecoin, so a key revealed there also exposes the
   matching Marscoin outputs. Relevant to MQ-36/37 and to future wallet
   defaults.
- Rescue note: derivation below the account level is not hardened (Electrum
  `m/0/i`, BIP44 `/0/i`). A rescue must rely on the seed or a hardened-path
  secret, as the draft spec (PR #70) does.
- Minor: marscoin-electrumx sets `P2PKH_VERBYTE = 0x30` while the clients use
  `0x32`. This affects only address-based RPCs.
