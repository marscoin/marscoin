# P2WPQH signature hash, version 1

This document specifies the message that a P2WPQH (witness version 2,
post-quantum) signature signs, the signature hash types, and how the hash type
is encoded in the witness. It follows BIP341's signature message closely and
differs only where noted.

Status: consensus rule for witness v2 on chains where P2WPQH is active
(currently marsqnet and regtest). Not active on mainnet. Implemented by
`SignatureHashPQ` and `GenericTransactionSignatureChecker::CheckPQSignature` in
`src/script/interpreter.cpp`.

## Witness

A P2WPQH input's witness has exactly three items:

```
<signature payload> <parameter set id> <public key>
```

The scriptPubKey is `OP_2 <32-byte program>`, where
`program = SHA256(parameter set id || public key)`.

There is no annex: a P2WPQH witness always has exactly three items.

## Signature payload and hash types

The payload is one of:

| Payload | Hash type |
| --- | --- |
| `[param id][signature]` | `SIGHASH_DEFAULT` (0x00) |
| `[param id][signature][hash type]` | the explicit hash type byte |

For parameter set 0x01 (FIPS 205 SLH-DSA-SHA2-128s), the signature is 7856
bytes, so the payload is 7857 or 7858 bytes. Any other length is invalid.

Valid hash types are BIP341's set:

| Value | Name | Inputs signed | Outputs signed |
| --- | --- | --- | --- |
| 0x00 | `SIGHASH_DEFAULT` | all | all |
| 0x01 | `SIGHASH_ALL` | all | all |
| 0x02 | `SIGHASH_NONE` | all | none |
| 0x03 | `SIGHASH_SINGLE` | all | the one with the same index |
| 0x81 | `SIGHASH_ALL\|ANYONECANPAY` | this one | all |
| 0x82 | `SIGHASH_NONE\|ANYONECANPAY` | this one | none |
| 0x83 | `SIGHASH_SINGLE\|ANYONECANPAY` | this one | the one with the same index |

`SIGHASH_DEFAULT` is expressed only by omitting the byte. An explicit 0x00 byte
is invalid. Any other value is invalid. `SIGHASH_SINGLE` and
`SIGHASH_SINGLE|ANYONECANPAY` are invalid when the input index has no output
with the same index.

`SIGHASH_DEFAULT` signs the same data as `SIGHASH_ALL`, but because the hash
type itself is committed, a `SIGHASH_DEFAULT` signature is not a valid
`SIGHASH_ALL` signature, or the reverse.

## Signature message

The signature hash is a BIP340-style tagged hash:

```
sighash = SHA256(SHA256(tag) || SHA256(tag) || message)
tag     = "Marscoin/P2WPQH/sighash"
```

The message is the concatenation of:

| Field | Size | Present |
| --- | --- | --- |
| epoch, `0x00` | 1 | always |
| hash type | 1 | always |
| `nVersion` | 4 | always |
| `nLockTime` | 4 | always |
| `sha_prevouts`: SHA256 of all serialized outpoints | 32 | unless ANYONECANPAY |
| `sha_amounts`: SHA256 of all spent amounts (8 bytes each) | 32 | unless ANYONECANPAY |
| `sha_scriptpubkeys`: SHA256 of all spent scriptPubKeys, each with its compact-size length | 32 | unless ANYONECANPAY |
| `sha_sequences`: SHA256 of all `nSequence` values | 32 | unless ANYONECANPAY |
| `sha_outputs`: SHA256 of all serialized outputs | 32 | only for `SIGHASH_DEFAULT` and `SIGHASH_ALL` (with or without ANYONECANPAY) |
| `spend_type`, `0x00` | 1 | always |
| this input's outpoint (36), spent output (amount 8, scriptPubKey with length), `nSequence` (4) | varies | with ANYONECANPAY |
| input index | 4 | without ANYONECANPAY |
| `sha_single_output`: SHA256 of the output with this input's index | 32 | with `SIGHASH_SINGLE` |
| parameter set id | 1 | always |
| public key | 32 (for 0x01) | always |

All integers are little-endian. The SHA256 values are single SHA256, computed
as in BIP341.

### Differences from BIP341

- The tag is `Marscoin/P2WPQH/sighash`, not `TapSighash`.
- The message ends with the parameter set id and the public key.
  - That binds the signature to one key and one signature scheme, even if the
    same key is ever used under another program or a future parameter set
    reuses a key format.
  - The parameter set determines the key's length, so no length prefix is
    needed.
- There is no annex, script path, key version or code separator data.
  `spend_type` is always 0, and its annex bit is reserved for a future version.

The FIPS 205 signing context `marscoin-p2wpqh-v1` (see
`doc/quantum-signatures-sphincs-v1.md`) is applied on top of this message by
SLH-DSA itself.

## Rationale

- **Amounts and scriptPubKeys of every spent output.** As in BIP341, an
  offline or hardware signer can then compute the fee and see which outputs it
  spends from the signed data alone. A signer can't be tricked into signing
  for a larger fee by misreporting an input it doesn't own.
- **Hash types.** ANYONECANPAY and SINGLE allow crowdfunding-style
  transactions, adding fee inputs later, and other constructions. The valid set
  and its meaning match BIP341, so existing tooling and intuition carry over.
- **`SIGHASH_DEFAULT`** saves a witness byte for the common case.
- **Explicit 0x00 is invalid** so each signature has one encoding, which avoids
  third-party malleation of the witness size.

## Requirements on implementations

- Verifiers need every spent output, the same as for Taproot.
  `PrecomputedTransactionData` computes the BIP341 hashes when a witness-bearing
  input spends a P2WPQH output. A checker without that data fails, or asserts
  in consensus code.
- Signers need every spent output as well. `MutableTransactionSignatureCreator`
  refuses to sign a P2WPQH input without them.
- Wallet fee estimation counts the optional hash type byte, so the input size
  estimate is a maximum. In weight units: 41 × 4 non-witness bytes, plus the
  witness: item count 1, payload 3 + 7858, param id 1 + 1, pubkey 1 + 32.

## Test vectors

`src/test/data/p2wpqh_sighash_vectors.json` contains one fixed transaction
spending three outputs (P2WPQH, P2WPKH, P2WPQH) with two outputs, and the
expected signature hash for several inputs and hash types. The vectors are
generated by an independent Python implementation,
`contrib/devtools/gen-p2wpqh-sighash-vectors.py`. That implementation's shared
logic was checked against the BIP341 key-path vectors (all seven reproduce
exactly when the tag and the final two fields are switched to BIP341's).
`src/test/pq_sighash_tests.cpp` checks `SignatureHashPQ` against them. The
sighash values below are the raw SHA256 output in byte order.

| Input | Hash type | Signature hash |
| --- | --- | --- |
| 0 | 0x00 | `7ca0eab308f5c645ed6a3817ec3d7f253b33f6e56292050d03789c8209bf4666` |
| 0 | 0x01 | `545f25422e7c019f973af4d2fced3e22fee0321442965caec874c05f1f66d368` |
| 0 | 0x02 | `83c5f5921d1eb877bd3fa3a3f94a524612844d716c80425f84238d450d3169eb` |
| 0 | 0x03 | `2acabce61fed7318ae4e15d01be433026e193e817012009550f281f4e7f4d9a2` |
| 0 | 0x81 | `ee3046586ab58988fc98521b18cfd48accca15f6e96149030d2ce767434445cf` |
| 0 | 0x82 | `5912df9ae187c3802b1136b422b3c58109f898890c1f5c90144a271d60888cfa` |
| 0 | 0x83 | `166110b261272426ad2088286fddbf33a991f0413cd629a1b8f2d409a70f94f6` |
| 2 | 0x00 | `75512c6c2c61f87e4a531f8f6da5ee7268b71677b85d88ed88347f0a568208b7` |
| 2 | 0x81 | `66907a885eeac31e04b10068267a73c1c9bd5baa4c61cb27b1d563754701176b` |
| 2 | 0x03 | undefined (no output 2) |
| 2 | 0x83 | undefined (no output 2) |
| 0 | 0x04 | undefined (invalid hash type) |
| 0 | 0x84 | undefined (invalid hash type) |
