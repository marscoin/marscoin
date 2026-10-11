# slhdsa-c (vendored)

FIPS 205 SLH-DSA implementation used for P2WPQH (witness v2) signatures. It is
compiled into every Marscoin build; there is no configure flag.

- Upstream: https://github.com/pq-code-package/slhdsa-c
- Commit: `a0fc1ff253930060d0246aebca06c2538eb92b88`, the revision vendored by
  liboqs 0.15.0, which Marscoin used before. liboqs patched these files only to
  add its `OQS_API` export markers and `#include <oqs/oqs.h>`; those patches
  are not applied here.
- License: Apache-2.0 OR ISC OR MIT, see `LICENSE`. Marscoin uses it under MIT.

Only the files needed for the SHA-2 parameter sets are included, and they are
**byte-identical to upstream at that commit**:

| File | SHA-256 |
| --- | --- |
| `LICENSE` | `f5f353e7c2308465c5e82021a06acfba74d3d5e70d13e4af8226e9051a25ce2c` |
| `cbmc.h` | `91058b937e92736535be4c8372a6eae7cafd5ac00abc891476c7c23231c4cf80` |
| `plat_local.h` | `960145bea8fec1d21b00ffeaf5bb9de2aab8cb15b3ae22e8f847f5f44dfc594b` |
| `sha2_256.c` | `b1b74abe3d4b6b925ca4ce3ded43a5f2188f0f16a41f8ae91c93de95fc0fa07b` |
| `sha2_512.c` | `84b914b342a16d07716b1830668e3728892b422fc322f05fadec3993f60f2832` |
| `sha2_api.h` | `4315f4891acfdcadfdc220770fd3a8a080aec94f6346eec9d8badbea1bb3a6ba` |
| `slh_adrs.h` | `6a734cb673b12299165908f53fdade618d9005287b1a902de8ac90c211710d4c` |
| `slh_dsa.c` | `fbbf5c4ed4ea70bf9d90c1de4ddb168d5671e94621a708f110f57a81de0a5613` |
| `slh_dsa.h` | `aa5eb9234f9dc342d5a41af02a572fd68c4a832b05561cd4c95447a2be5cfe2e` |
| `slh_param.h` | `d848f41c5f5ac2b911d077209da0f9fb313ffcc8865c55b43dd09cdfce009ac7` |
| `slh_sha2.c` | `075066c5af445933868eeb42120451662aefd696cf5a4c71ce464fe758f07604` |
| `slh_sys.h` | `7280d918931f9302c7bd47043f477796102e777fa1d30ff667b479eb0fe9ae94` |
| `slh_var.h` | `0c87ae009a76c07f3c0c066b2d255e5f6cb18e6f38f7df3d805e395d37ef96b0` |

The SHAKE parameter sets (`slh_shake.c`, `sha3_*`) and HashSLH-DSA pre-hashing
(`slh_prehash.c`) are not included. Consensus uses only SLH-DSA-SHA2-128s with
the pure interface and the context string `marscoin-p2wpqh-v1`; see
`src/crypto/pq_sphincs.h`.

To update: replace the files with a newer upstream revision unchanged, update
the commit and hashes above, and run the NIST ACVP vectors in
`src/test/crypto_tests.cpp` (`pq_slh_dsa_acvp_vectors`) and
`contrib/devtools/slh-dsa-kat.sh`. Don't edit these files in place.
