# Spec A2: Post-Quantum Witness Type (Witness v2 / P2WPQH)

**Status**: Draft
**Depends on**: A1 (dynamic block sizing -- recommended but not strictly required)
**Required by**: A3 (PQ address encoding), A4 (wallet PQ key management)

---

## 1. Purpose

Define a new witness version (v2) that enables spending outputs locked to a
SPHINCS+ public key. This is the consensus-critical piece that turns the
existing `pq::sphincs` crypto scaffold into a spendable transaction type.

## 2. Design Overview

### New Witness Version: v2

Following the Bitcoin witness version pattern:
- v0 = SegWit (P2WPKH, P2WSH)
- v1 = Taproot (P2TR)
- **v2 = Post-Quantum (P2WPQH -- Pay-to-Witness-PQ-Hash)**

### Output Script (scriptPubKey)

```
OP_2 <32-byte-program>
```

Where the 32-byte program is `SHA256(parameter_set_id || sphincs_public_key)`:

```
program = SHA256(0x00 || pubkey[0..31])
```

This commits to both the parameter set and the public key without revealing
either on-chain until spend time. 32-byte program matches v0 (P2WSH) and
v1 (P2TR) conventions.

### Why hash the public key?

- SPHINCS+ public keys are 32 bytes (same as the program), so we *could*
  put the raw pubkey in the scriptPubKey. But:
  - Hashing commits to the parameter set ID (future-proofs for additional
    parameter sets).
  - Follows the established pattern (v0 hashes keys, v1 uses x-only directly).
  - Pre-image resistance: the public key isn't revealed until spend time,
    providing quantum protection for outputs at rest.

### Witness Stack (spending input)

```
witness: <signature_payload> <parameter_set_id> <sphincs_public_key>
```

Where:
- `sphincs_public_key`: 32 bytes (SLH-DSA-SHA2-128s)
- `parameter_set_id`: 1 byte (0x00 for SLH-DSA-SHA2-128s)
- `signature_payload`: 7,857 bytes (`[param_set_id][raw_signature]` -- the
  existing scaffold format from `pq_sphincs.h`)

Total witness: ~7,890 bytes per input.

### Verification Steps (in VerifyWitnessProgram)

```
1. Check witness stack has exactly 3 elements
2. Extract sphincs_public_key (stack[2]), parameter_set_id (stack[1]),
   signature_payload (stack[0])
3. Verify parameter_set_id is supported (IsSupportedParameterSet)
4. Verify program == SHA256(parameter_set_id || sphincs_public_key)
5. Validate signature format (ValidateSignatureEncoding)
6. Compute sighash over the transaction (see Sighash section)
7. Verify signature via pq::sphincs::VerifyMessage(parameter_set, pubkey,
   sighash, signature_payload)
```

## 3. Sighash

### New Sighash Computation

SPHINCS+ verification needs a message to verify against. This message is a
transaction sighash, similar to BIP143 (SegWit v0) and BIP341 (Taproot).

Define a new sighash tagged hash:

```
sighash = SHA256(SHA256("PQSighash") || SHA256("PQSighash") ||
                 epoch ||
                 sighash_type ||
                 nVersion ||
                 nLockTime ||
                 sha_prevouts ||
                 sha_amounts ||
                 sha_scriptpubkeys ||
                 sha_sequences ||
                 sha_outputs ||
                 spend_type ||
                 input_index)
```

This follows BIP341's structure closely. Key differences:
- Tagged with "PQSighash" instead of "TapSighash"
- No annex support initially (can be added later)
- No leaf version / script path (single spending path only in v1)

### Sighash Types

Support the standard set:
- `SIGHASH_ALL` (0x01): default, signs all inputs and outputs
- `SIGHASH_NONE` (0x02): signs inputs only
- `SIGHASH_SINGLE` (0x03): signs corresponding output only
- `SIGHASH_ANYONECANPAY` (0x80): combinable flag, signs only this input

The sighash type byte is NOT appended to the signature (unlike ECDSA).
SPHINCS+ signatures are fixed-size. The sighash type is encoded in
the sighash preimage itself.

Default: `SIGHASH_ALL` when no explicit type is provided.

## 4. Files to Modify

### script/interpreter.h

Add constants and enum value:

```cpp
static constexpr size_t WITNESS_V2_PQ_PROGRAM_SIZE = 32;

enum class SigVersion {
    BASE = 0,
    WITNESS_V0 = 1,
    TAPROOT = 2,
    TAPSCRIPT = 3,
    WITNESS_V2_PQ = 4,  // new
};

// New verification flag
SCRIPT_VERIFY_WITNESS_V2 = (1U << 21),
```

### script/interpreter.cpp

Add v2 dispatch in `VerifyWitnessProgram()` (after the v1/Taproot block):

```cpp
else if (witversion == 2 && program.size() == WITNESS_V2_PQ_PROGRAM_SIZE && !is_p2sh) {
    if (!(flags & SCRIPT_VERIFY_WITNESS_V2)) {
        return set_success(serror);  // soft-fork safe: unknown version succeeds
    }

    // Witness stack: <sig_payload> <param_set_id> <pubkey>
    if (witness.stack.size() != 3) {
        return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH);
    }

    const auto& sig_payload = witness.stack[0];
    const auto& param_set_raw = witness.stack[1];
    const auto& pubkey = witness.stack[2];

    // Validate parameter set
    if (param_set_raw.size() != 1 || !pq::sphincs::IsSupportedParameterSet(param_set_raw[0])) {
        return set_error(serror, SCRIPT_ERR_PQ_UNSUPPORTED_PARAM_SET);
    }
    const auto param_set = static_cast<pq::sphincs::ParameterSet>(param_set_raw[0]);

    // Validate public key length
    if (pubkey.size() != pq::sphincs::SPHINCS_PUBLIC_KEY_SIZE_SHA2_128S) {
        return set_error(serror, SCRIPT_ERR_PQ_PUBKEY_SIZE);
    }

    // Verify program commitment: SHA256(param_set_id || pubkey)
    uint256 expected_program;
    CSHA256().Write(param_set_raw.data(), 1)
             .Write(pubkey.data(), pubkey.size())
             .Finalize(expected_program.begin());
    if (memcmp(expected_program.begin(), program.data(), 32) != 0) {
        return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH);
    }

    // Validate signature format
    const std::string format_err = pq::sphincs::ValidateSignatureEncoding(sig_payload);
    if (!format_err.empty()) {
        return set_error(serror, SCRIPT_ERR_PQ_SIG_FORMAT);
    }

    // Compute sighash
    uint256 sighash;
    // ... compute PQ sighash from transaction data ...

    // Verify SPHINCS+ signature
    std::string verify_err;
    if (!pq::sphincs::VerifyMessage(param_set,
            Span<const unsigned char>(pubkey.data(), pubkey.size()),
            Span<const unsigned char>(sighash.begin(), 32),
            Span<const unsigned char>(sig_payload.data(), sig_payload.size()),
            verify_err)) {
        return set_error(serror, SCRIPT_ERR_PQ_SIG_VERIFY);
    }

    return set_success(serror);
}
```

### script/script_error.h

Add new error codes:

```cpp
SCRIPT_ERR_PQ_UNSUPPORTED_PARAM_SET,
SCRIPT_ERR_PQ_PUBKEY_SIZE,
SCRIPT_ERR_PQ_SIG_FORMAT,
SCRIPT_ERR_PQ_SIG_VERIFY,
```

### script/solver.h

Add output type:

```cpp
enum class TxoutType {
    // ... existing ...
    WITNESS_V2_PQ,  // P2WPQH (Pay-to-Witness-PQ-Hash)
};
```

### script/solver.cpp

Add to Solver():

```cpp
if (witnessversion == 2 && witnessprogram.size() == WITNESS_V2_PQ_PROGRAM_SIZE) {
    vSolutionsRet.push_back(std::move(witnessprogram));
    return TxoutType::WITNESS_V2_PQ;
}
```

### addresstype.h

Add destination type:

```cpp
struct WitnessV2PQ : public BaseHash<uint256> {
    WitnessV2PQ() : BaseHash() {}
    explicit WitnessV2PQ(const uint256& hash) : BaseHash(hash) {}
};

using CTxDestination = std::variant<
    CNoDestination, PubKeyDestination, PKHash, ScriptHash,
    WitnessV0ScriptHash, WitnessV0KeyHash, WitnessV1Taproot,
    PayToAnchor, WitnessV2PQ, WitnessUnknown>;
```

### key_io.cpp

Update `DecodeDestination()` for v2 addresses:

```cpp
if (version == 2 && data.size() == WITNESS_V2_PQ_PROGRAM_SIZE) {
    WitnessV2PQ pq;
    std::copy(data.begin(), data.end(), pq.begin());
    return pq;
}
```

Remove/update the existing `IsPostQuantumAddress()` scaffold -- it becomes
real address decoding instead of returning "not enabled yet".

Address format: `mars1z...` (bech32m, witness version 2).

Bech32 character set: `qpzry9x8gf2tvdw0s3jn54khce6mua7l`
- Version 0 → index 0 → `q` (hence `mars1q...` -- existing SegWit)
- Version 1 → index 1 → `p` (hence `mars1p...` -- Taproot)
- Version 2 → index 2 → `z` (hence `mars1z...` -- **Post-Quantum**)

PQ addresses: `mars1z...` on mainnet, `tmars1z...` on testnet.

The earlier `mars1pq...` scaffold prefix (PR #38) was a placeholder convention
within witness v1 space. Real bech32m encoding uses the version byte directly.
The scaffold `IsPostQuantumAddress()` function should be updated to detect
`mars1z...` format instead, or replaced entirely by standard v2 address
decoding.

### script/sigcache.h

Add SPHINCS+ signature cache entry computation:

```cpp
CSHA256 m_salted_hasher_sphincs;  // new hasher for PQ signatures

void ComputeEntrySPHINCS(uint256& entry, const uint256& hash,
                         Span<const unsigned char> sig,
                         Span<const unsigned char> pubkey) const;
```

Tag byte: `'P'` (vs `'E'` for ECDSA and `'S'` for Schnorr).

Cache entry: `SHA256(nonce || 'P' || 31_zeros || sighash || pubkey || SHA256(sig))`

Note: we hash the signature before caching because at 7,856 bytes, including
the raw signature in the cache key computation is expensive. The SHA256 of the
signature is sufficient for uniqueness.

### consensus/params.h

Add activation parameter:

```cpp
int nWitnessV2ActivationHeight{0};  // 0 = not activated
```

### validation.cpp

In `GetBlockScriptFlags()`, add `SCRIPT_VERIFY_WITNESS_V2` flag for blocks
at or above activation height.

## 5. Weight Accounting

SPHINCS+ signatures are in the witness, so they benefit from the SegWit
witness discount (1/4 weight vs non-witness data).

Per-input weight for a P2WPQH spend:
```
Non-witness: ~41 bytes (prevout 36 + sequence 4 + scriptSig length 1)
Witness:     ~7,894 bytes (sig 7857 + paramset 1 + pubkey 32 + lengths 4)

Weight = 41 * 4 + 7,894 * 1 = 164 + 7,894 = 8,058 weight units
```

Compare to P2WPKH:
```
Non-witness: ~41 bytes
Witness:     ~108 bytes (sig ~72 + pubkey 33 + lengths 3)

Weight = 41 * 4 + 108 = 272 weight units
```

P2WPQH inputs are ~30x heavier than P2WPKH. With dynamic block sizing (A1),
the ceiling adapts to accommodate this.

A typical single-input, single-output P2WPQH transaction:
```
Overhead:  ~10 bytes (version 4 + locktime 4 + marker/flag 2)
Input:     ~41 bytes non-witness + ~7,894 witness
Output:    ~43 bytes (value 8 + scriptPubKey 34 + length 1)

Total weight = (10 + 41 + 43) * 4 + 7,894 = 376 + 7,894 = 8,270
```

Under the 4M floor: ~483 PQ transactions per block.
Under 8M dynamic ceiling: ~967 PQ transactions per block.

## 6. Interaction with Existing Address Scaffold

The `IsPostQuantumAddress()` function in `key_io.cpp` currently detects
`mars1pq...` addresses. With real v2 witness encoding, PQ addresses become
`mars1z...` instead.

Migration path:
1. Remove `IsPostQuantumAddress()` scaffold entirely
2. Real PQ addresses use standard bech32m v2 decoding
3. The `mars1pq...` prefix was never spendable, so there's nothing to
   migrate -- it was purely a detection hook

Alternatively, keep `IsPostQuantumAddress()` as a convenience function
that checks for witness version 2, but have it detect `mars1z...` format.

## 7. Test Plan

### Unit tests (src/test/)

**script_tests.cpp**:
- Valid P2WPQH spend with correct signature → accepted
- Wrong public key → program mismatch error
- Wrong parameter set → unsupported error
- Truncated signature → format error
- Tampered signature → verify failure
- Wrong sighash type → verify failure
- Stack with != 3 elements → mismatch error
- P2SH-wrapped v2 → rejected (not allowed for v1+)
- Pre-activation height → silent success (soft-fork safe)

**key_io_tests.cpp**:
- Encode/decode PQ address round-trip
- `mars1z...` format validation
- Cross-network prefix validation

**crypto_tests.cpp**:
- Extend existing `pq_sphincs_signature_scaffold` test to include
  sighash computation and full verification flow

### Functional tests (test/functional/)

- `feature_pq_witness.py`: Create P2WPQH output, spend it, verify mempool
  acceptance and block inclusion
- Test with dynamic block sizing (A1) active
- Test migration: send from legacy address to PQ address to PQ address

### Marsqnet validation

- Deploy on marsqnet after A1 is stable
- Create real PQ transactions between nodes
- Verify block propagation with large witness data
- Stress test: fill blocks with PQ transactions, verify ceiling adaptation

## 8. Security Considerations

### Quantum safety of outputs at rest

Unspent P2WPQH outputs commit to `SHA256(param_set || pubkey)`. The public
key is not revealed until spend time. Even if a quantum computer can derive
a SPHINCS+ private key from a public key (which it can't -- SPHINCS+ is
hash-based), it would need to break SHA256 preimage resistance first.

### Relay and mempool policy

SPHINCS+ verification is slower than ECDSA (~1ms vs ~0.1ms per signature).
Consider:
- Rate limiting PQ transaction relay
- Higher minimum fee for PQ transactions (proportional to weight)
- Signature cache is critical for performance (avoid re-verifying)

### Sighash malleability

The sighash commits to all relevant transaction data. No known malleability
vectors beyond the standard ones (which are addressed by sighash types).

## 9. Open Questions

1. **Script path spending?** This spec defines only a "key path" (direct
   pubkey + signature). Should v2 support a script path similar to Taproot?
   Recommendation: not in v1 of v2. Add it later as a leaf version if needed.
   Keep the initial design simple.

2. **Hybrid signatures?** Some proposals combine ECDSA + PQ for belt-and-
   suspenders security. This adds complexity. Given Marscoin's clean break
   philosophy (eliminate all EC primitives), hybrid is probably not needed.

3. **Multi-input transactions?** Each input carries its own ~7.9KB witness.
   A 10-input PQ transaction is ~79KB. Under dynamic blocks this is fine,
   but wallet software should prefer UTXO consolidation during migration
   to reduce future input counts.

4. **Parameter set upgrades?** The design supports multiple parameter sets
   via the `parameter_set_id` byte. If a stronger SPHINCS+ variant is
   needed later (e.g., SHA2-256s for 256-bit security), it can be added
   as parameter set 0x01 without changing the witness structure.

## 10. Implementation Order

```
1. Add SigVersion::WITNESS_V2_PQ and script error codes
2. Add WitnessV2PQ destination type and CTxDestination variant
3. Add TxoutType::WITNESS_V2_PQ and Solver() detection
4. Implement PQ sighash computation (tagged hash)
5. Implement VerifyWitnessProgram v2 dispatch
6. Add SCRIPT_VERIFY_WITNESS_V2 flag and activation logic
7. Update key_io.cpp for mars1z... address encoding/decoding
8. Add signature cache support for SPHINCS+
9. Write unit tests
10. Write functional tests
11. Deploy on marsqnet
```

Steps 1-3 are type system plumbing (low risk).
Step 4-5 are the consensus-critical core.
Step 6 gates activation.
Steps 7-8 are integration.
Steps 9-11 are validation.
