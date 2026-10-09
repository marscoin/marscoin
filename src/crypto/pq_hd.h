// Copyright (c) 2026 The Marscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_PQ_HD_H
#define BITCOIN_CRYPTO_PQ_HD_H

#include <crypto/pq_sphincs.h>
#include <span.h>
#include <uint256.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/**
 * Hierarchical deterministic post-quantum keys, PQ HD v1
 * (doc/quantum-pq-key-derivation-v1.md).
 *
 * The tree uses only HMAC-SHA512. No node has an elliptic-curve public key, so
 * an attacker who can break EC finds nothing public to attack. Its root is
 * derived from the seed independently of the BIP32 master, so revealing a
 * BIP32 node (for example in an emergency rescue) reveals nothing about it.
 */
namespace pq::hd {

//! HMAC key that turns a seed into the root node (BIP32 uses "Bitcoin seed").
static constexpr std::string_view ROOT_HMAC_KEY{"Marscoin PQ seed"};
//! HKDF info prefix for the FIPS 205 key-generation seeds; the parameter set id follows it.
static constexpr std::string_view KEYGEN_INFO{"Marscoin P2WPQH keygen"};
//! Bech32m prefix of an encoded node. A node is always secret.
static constexpr std::string_view NODE_HRP{"mpqprv"};
//! Tag of the tagged hash that identifies a node.
static constexpr std::string_view NODE_ID_TAG{"Marscoin/PQHD/node-id"};
//! Bech32m prefix of an encoded node identifier. An identifier is public.
static constexpr std::string_view NODE_ID_HRP{"mpqid"};

static constexpr uint32_t HARDENED{0x80000000};
//! SLIP-44 coin type for mainnet paths (Marscoin is 107); every test network uses 1.
static constexpr uint32_t COIN_TYPE_MAINNET{107};
static constexpr uint32_t COIN_TYPE_TEST{1};

//! Seed size bounds, as in BIP32.
static constexpr size_t MIN_SEED_SIZE{16};
static constexpr size_t MAX_SEED_SIZE{64};

/** A node of the tree. Both halves are secret; there is no public form. */
struct Node {
    std::array<unsigned char, 32> key{};
    std::array<unsigned char, 32> chaincode{};

    Node() = default;
    Node(const Node&) = default;
    Node& operator=(const Node&) = default;
    ~Node();

    friend bool operator==(const Node& a, const Node& b) { return a.key == b.key && a.chaincode == b.chaincode; }
};

/** The three FIPS 205 key-generation seeds of a leaf. */
struct KeygenSeeds {
    std::vector<unsigned char> sk_seed;
    std::vector<unsigned char> sk_prf;
    std::vector<unsigned char> pk_seed;

    ~KeygenSeeds();
};

//! Root node: HMAC-SHA512(ROOT_HMAC_KEY, seed). Fails unless the seed has 16 to 64 bytes.
std::optional<Node> RootFromSeed(Span<const unsigned char> seed);

//! Hardened child: HMAC-SHA512(parent.chaincode, 0x00 || parent.key || ser32(index)).
//! Fails if index is not hardened; the tree has no non-hardened derivation.
std::optional<Node> DeriveChild(const Node& parent, uint32_t index);

//! Follow a path of hardened indices.
std::optional<Node> DerivePath(const Node& node, Span<const uint32_t> path);

//! The path m/coin_type'/account'/change'/index' of a key (every step hardened).
std::vector<uint32_t> KeyPath(uint32_t coin_type, uint32_t account, bool change, uint32_t index);

//! HKDF-Expand-SHA512 (RFC 5869) with PRK = key || chaincode and
//! info = KEYGEN_INFO || id, split into SK.seed, SK.prf and PK.seed of n bytes each.
std::optional<KeygenSeeds> DeriveKeygenSeeds(const Node& leaf, sphincs::ParameterSet parameter_set);

//! The SLH-DSA key pair of a leaf (FIPS 205 slh_keygen_internal of its seeds).
bool DeriveKeypair(const Node& leaf, sphincs::ParameterSet parameter_set, std::vector<unsigned char>& public_key,
                   std::vector<unsigned char>& private_key, std::string& error);

//! Bech32m encoding of a node: NODE_HRP with the 64-byte payload key || chaincode.
std::string EncodeNode(const Node& node);
std::optional<Node> DecodeNode(std::string_view str);

//! Public identifier of a node, TaggedHash(NODE_ID_TAG, key || chaincode). It
//! names a node (for example in a public descriptor) without revealing it.
uint256 NodeId(const Node& node);
//! Bech32m encoding of a node identifier: NODE_ID_HRP with the 32-byte identifier.
std::string EncodeNodeId(const uint256& id);
std::optional<uint256> DecodeNodeId(std::string_view str);

} // namespace pq::hd

#endif // BITCOIN_CRYPTO_PQ_HD_H
