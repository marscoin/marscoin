# Post-quantum key derivation: specification v1 (PQ HD v1)

Status: proposed, work order MQ-22. Implemented in Marscoin Core (sections 4
and 5.1). The constants below become final when the first mainnet wallet uses
them. Until then they may change, and test-network coins held under them would
have to be swept.

This document defines how a wallet derives its P2WPQH (witness v2, SLH-DSA)
keys from a seed, so that a seed or descriptor backup restores them. Marscoin
Core, Electrum-Mars and MarsWallet implement the same derivation, so one seed
gives the same P2WPQH addresses in each of them.

- Reference code: `src/crypto/pq_hd.{h,cpp}`.
- Independent reference implementation and vector generator:
  `contrib/devtools/pq-hd-vectors.py` (pure Python, including SLH-DSA key
  generation checked against the NIST ACVP vectors).
- Test vectors: `src/test/data/pq_hd_vectors.json`.

## 1. Requirements

1. **Recoverable.** Every key follows from the seed, so a backup made once
   restores all future keys.
2. **Nothing for an EC break to attack.** No step computes an elliptic-curve
   key. The security of the keys rests on HMAC-SHA512, SHA-256 and SLH-DSA
   only.
3. **Independent of the BIP32 tree.** Wallets also keep legacy keys derived
   with BIP32 from the same seed. Those keys, their public keys, and any BIP32
   node revealed in an emergency rescue (`doc/quantum-emergency-rescue-v1.md`,
   type `0x02`) must reveal nothing about the post-quantum keys.
4. **Parameter-set agility.** A future parameter set (MQ-14) gets unrelated
   keys from the same seed and path.

### Why not BIP32 below a new purpose

BIP32 nodes are elliptic-curve key pairs. With an EC break, anyone who knows a
node's extended public key computes its private key, and from there every node
below it, hardened or not. Extended public keys are not well protected:

- Marscoin Core descriptor wallets store the master extended public key in the
  wallet file without encryption, so a stolen encrypted wallet file would give
  away every key.
- Users hand account-level xpubs to watch-only services.

A post-quantum key below any such node is only as safe as that node's EC key.
SLH-DSA public keys also can't be derived from a public parent, so BIP32's
public derivation has nothing to offer here.

## 2. Definitions

`HMAC-SHA512(K, M)` is RFC 2104 HMAC with SHA-512. `ser32(i)` is the 4-byte
big-endian encoding of `i`. `||` is concatenation.

### 2.1 Seed

`S` is the BIP32 seed, 16 to 64 bytes: the bytes a BIP32 wallet feeds into
`HMAC-SHA512("Bitcoin seed", S)`.

- BIP39 wallets (MarsWallet, Electrum-Mars with a BIP39 seed): `S` is the
  64-byte seed computed from the mnemonic and passphrase.
- Electrum-Mars native seeds: `S` is the 64-byte seed Electrum computes from
  the mnemonic (PBKDF2 with the salt `"electrum" || passphrase`).
- Marscoin Core: see section 5.1.

### 2.2 Nodes

A node is a pair `(k, c)` of a 32-byte key and a 32-byte chain code. Both are
secret. A node has no public form.

**Root.**

```
I = HMAC-SHA512(K = "Marscoin PQ seed", M = S)
root = (I[0:32], I[32:64])
```

The key is the ASCII string without a terminator, 16 bytes. Because BIP32 uses
`"Bitcoin seed"`, the BIP32 master and the root are unrelated outputs of the
same pseudorandom function.

**Child.** Only hardened indices exist (`i >= 2^31`). For a parent `(k, c)`:

```
I = HMAC-SHA512(K = c, M = 0x00 || k || ser32(i))
child = (I[0:32], I[32:64])
```

A non-hardened index is invalid. Unlike BIP32, `I[0:32]` is used as it is: it
is not an EC scalar, so there is no range check and no failure case. This is
the hardened rule of SLIP-0010 for ed25519, with a different root.

### 2.3 Paths

Keys live at

```
m / coin_type' / account' / change' / index'
```

- `coin_type` is 107 (Marscoin's SLIP-44 number) on mainnet and 1 on every test
  network (testnet, marsqnet, signet, regtest).
- `change` is 0 for receiving and 1 for change addresses.
- `account` and `index` count up from 0.

There is no purpose level: the root already belongs to this scheme alone.

### 2.4 Key generation

For a leaf node `(k, c)` and a parameter set with id `p` and security
parameter `n` (`n = 16` for `0x01`, SLH-DSA-SHA2-128s):

```
OKM = HKDF-Expand-SHA512(PRK = k || c, info = "Marscoin P2WPQH keygen" || p, L = 3n)
SK.seed = OKM[0:n]
SK.prf  = OKM[n:2n]
PK.seed = OKM[2n:3n]
(SK, PK) = slh_keygen_internal(SK.seed, SK.prf, PK.seed)    (FIPS 205, Algorithm 18)
```

HKDF-Expand is RFC 5869: `T(1) = HMAC-SHA512(PRK, info || 0x01)`,
`T(i) = HMAC-SHA512(PRK, T(i-1) || info || i)`, and `OKM` is the first `L`
bytes of `T(1) || T(2) || ...`. `p` is one byte. For `0x01`, `L = 48` and one
block suffices.

### 2.5 Address

As for every P2WPQH output: the witness program is `SHA256(p || PK)`, witness
version 2, encoded with Bech32m (`mars1z...` on mainnet, `mqt1z...` on
marsqnet).

## 3. Node encoding

A node is exported (for example in a descriptor) as Bech32m with the prefix
`mpqprv` and the 64-byte payload `k || c`. The result is always 116 characters,
longer than the 90 that BIP173 guarantees error detection for. Errors are still
detected with high probability, and a descriptor adds its own checksum.

The encoding is a private key: anyone holding it can spend every coin below
that node.

### 3.1 Node identifier

A node is named in public (for example in a descriptor without private keys)
by its identifier, a tagged hash as in BIP340:

```
id = SHA256(SHA256("Marscoin/PQHD/node-id") || SHA256("Marscoin/PQHD/node-id") || k || c)
```

It is encoded as Bech32m with the prefix `mpqid` and the 32-byte payload `id`,
64 characters in all. An identifier reveals nothing about the node.

## 4. Descriptors

Marscoin Core describes the keys with

```
wpq(NODE/PATH/*h)
```

where `NODE` is an `mpqprv` node, normally the root, and every path step,
including the wildcard, is hardened. Example on mainnet:
`wpq(mpqprv1.../107h/0h/0h/*h)` for receiving and `.../107h/0h/1h/*h` for
change.

- The public form names the node by its identifier,
  `wpq(mpqid1.../107h/0h/0h/*h)`. That is what the wallet file and
  `listdescriptors` without private keys show. Nothing in it leads to the
  node, and nobody can derive keys from it.
- `wpq()` is only valid at the top level, not inside `sh()`, `wsh()` or `tr()`.
- `importdescriptors` accepts only the private form. Watch-only P2WPQH
  tracking uses `addr()` descriptors.
- Generating a key takes about 28 ms (SLH-DSA-SHA2-128s, MQ-28 benchmarks).
  The wallet looks ahead `min(-keypool, 100)` keys per chain and caches their
  public keys, so a locked wallet still recognizes them and hands them out.
  Deriving more needs the node, so the wallet has to be unlocked.

## 5. Wallets

### 5.1 Marscoin Core

Core keeps only the BIP32 master key of a descriptor wallet, not its seed, so
it draws a fresh 32-byte random `S` for the post-quantum keys. It stores the
root, encrypted like any other private key, and discards `S`. The `wpq()`
descriptors are the backup (`listdescriptors true`).

- New descriptor wallets get active receiving and change `wpq()` descriptors on
  chains where P2WPQH is scheduled (`nPQWitnessActivationHeight > 0`: the test
  networks today, not mainnet).
- A wallet created earlier, or a blank one, gets them on its first
  `getnewpqaddress`, which needs the wallet unlocked. The call returns a
  warning, because backups taken before then don't contain the keys.
- Change goes to a P2WPQH address when the transaction pays a P2WPQH address
  or the wallet has no other change keys, and only where witness v2 is
  enforced.
- Wallets without private keys, external-signer wallets and legacy (BDB)
  wallets get no `wpq()` descriptors. Legacy wallets keep generating random
  post-quantum keys that only a wallet backup restores.

### 5.2 Seed-phrase wallets

Electrum-Mars and MarsWallet derive `S` from the user's seed phrase as in
section 2.1. Restoring the phrase restores the post-quantum keys. When
scanning, use a gap limit of 20 per `change` chain.

## 6. Interaction with the emergency rescue

The rescue specification (`doc/quantum-emergency-rescue-v1.md`, draft) lets an
owner prove ownership of legacy coins by revealing a secret after an EC break.
Rescued coins must go to P2WPQH outputs.

- Revealing a BIP32 node (type `0x02`, for example the coin-level node
  `m/84h/0h`) reveals nothing about the post-quantum tree.
- Revealing the seed `S` (type `0x03`) reveals the whole post-quantum tree. A
  rescue that reveals a seed must not send the coins to P2WPQH addresses from
  that seed. Wallets should prefer a type `0x02` reveal, or rescue into a
  wallet with a different seed.

## 7. Security notes

- **Independence.** The root and the BIP32 master are HMAC-SHA512 outputs of
  `S` under different keys. Knowing the BIP32 master, or breaking any EC key,
  gives no information about `S` or the root.
- **Siblings and parents.** A child node reveals nothing about its parent or
  its siblings: computing either needs the parent chain code, which only
  enters through HMAC.
- **Parameter sets.** The parameter-set id is part of the HKDF info, so the same
  leaf gives unrelated seeds for different parameter sets.
- **Key generation.** FIPS 205 key generation takes three `n`-byte seeds. Here
  they come from a key derivation over secret data instead of directly from a
  random bit generator, so the keys are only as strong as `S`: at least 128
  bits of entropy.
- **Quantum attacks.** The tree depends on HMAC-SHA512 with 256-bit secrets,
  well beyond the 128-bit post-quantum security of SLH-DSA-SHA2-128s.
- **Handling.** Nodes and `OKM` are secret and should be wiped after use. The
  reference code does this.
