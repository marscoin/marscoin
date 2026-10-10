// Copyright (c) 2026 The Marscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/pq_hd.h>

#include <bech32.h>
#include <crypto/common.h>
#include <crypto/hmac_sha512.h>
#include <hash.h>
#include <support/cleanse.h>
#include <util/strencodings.h>

#include <algorithm>

namespace pq::hd {

namespace {

constexpr size_t NODE_PAYLOAD_SIZE{64};

Node NodeFromHmacOutput(const unsigned char (&out)[CHMAC_SHA512::OUTPUT_SIZE])
{
    Node node;
    std::copy(out, out + 32, node.key.begin());
    std::copy(out + 32, out + 64, node.chaincode.begin());
    return node;
}

void Cleanse(std::vector<unsigned char>& data)
{
    memory_cleanse(data.data(), data.size());
}

} // namespace

Node::~Node()
{
    memory_cleanse(key.data(), key.size());
    memory_cleanse(chaincode.data(), chaincode.size());
}

KeygenSeeds::~KeygenSeeds()
{
    Cleanse(sk_seed);
    Cleanse(sk_prf);
    Cleanse(pk_seed);
}

std::optional<Node> RootFromSeed(const Span<const unsigned char> seed)
{
    if (seed.size() < MIN_SEED_SIZE || seed.size() > MAX_SEED_SIZE) return std::nullopt;
    unsigned char out[CHMAC_SHA512::OUTPUT_SIZE];
    CHMAC_SHA512{UCharCast(ROOT_HMAC_KEY.data()), ROOT_HMAC_KEY.size()}.Write(seed.data(), seed.size()).Finalize(out);
    Node root{NodeFromHmacOutput(out)};
    memory_cleanse(out, sizeof(out));
    return root;
}

std::optional<Node> DeriveChild(const Node& parent, const uint32_t index)
{
    if (!(index & HARDENED)) return std::nullopt;
    const unsigned char zero{0};
    unsigned char ser_index[4];
    WriteBE32(ser_index, index);
    unsigned char out[CHMAC_SHA512::OUTPUT_SIZE];
    CHMAC_SHA512{parent.chaincode.data(), parent.chaincode.size()}
        .Write(&zero, 1)
        .Write(parent.key.data(), parent.key.size())
        .Write(ser_index, sizeof(ser_index))
        .Finalize(out);
    Node child{NodeFromHmacOutput(out)};
    memory_cleanse(out, sizeof(out));
    return child;
}

std::optional<Node> DerivePath(const Node& node, const Span<const uint32_t> path)
{
    std::optional<Node> current{node};
    for (const uint32_t index : path) {
        current = DeriveChild(*current, index);
        if (!current) return std::nullopt;
    }
    return current;
}

std::vector<uint32_t> KeyPath(const uint32_t coin_type, const uint32_t account, const bool change, const uint32_t index)
{
    return {coin_type | HARDENED, account | HARDENED, (change ? 1U : 0U) | HARDENED, index | HARDENED};
}

std::optional<KeygenSeeds> DeriveKeygenSeeds(const Node& leaf, const sphincs::ParameterSet parameter_set)
{
    const size_t n{sphincs::SeedSize(parameter_set)};
    if (n == 0) return std::nullopt;
    const size_t length{3 * n};

    // RFC 5869 HKDF-Expand: T(i) = HMAC-SHA512(PRK, T(i-1) || info || i).
    // The PRK is the whole node, which has the 64 bytes RFC 5869 asks for.
    unsigned char prk[NODE_PAYLOAD_SIZE];
    std::copy(leaf.key.begin(), leaf.key.end(), prk);
    std::copy(leaf.chaincode.begin(), leaf.chaincode.end(), prk + 32);
    const unsigned char id{static_cast<unsigned char>(parameter_set)};
    std::vector<unsigned char> okm;
    okm.reserve(length + CHMAC_SHA512::OUTPUT_SIZE);
    unsigned char block[CHMAC_SHA512::OUTPUT_SIZE];
    size_t block_size{0};
    for (unsigned char counter{1}; okm.size() < length; ++counter) {
        CHMAC_SHA512{prk, sizeof(prk)}
            .Write(block, block_size)
            .Write(UCharCast(KEYGEN_INFO.data()), KEYGEN_INFO.size())
            .Write(&id, 1)
            .Write(&counter, 1)
            .Finalize(block);
        block_size = sizeof(block);
        okm.insert(okm.end(), block, block + block_size);
    }

    KeygenSeeds seeds;
    seeds.sk_seed.assign(okm.begin(), okm.begin() + n);
    seeds.sk_prf.assign(okm.begin() + n, okm.begin() + 2 * n);
    seeds.pk_seed.assign(okm.begin() + 2 * n, okm.begin() + 3 * n);
    memory_cleanse(prk, sizeof(prk));
    memory_cleanse(block, sizeof(block));
    Cleanse(okm);
    return seeds;
}

bool DeriveKeypair(const Node& leaf, const sphincs::ParameterSet parameter_set, std::vector<unsigned char>& public_key,
                   std::vector<unsigned char>& private_key, std::string& error)
{
    const std::optional<KeygenSeeds> seeds{DeriveKeygenSeeds(leaf, parameter_set)};
    if (!seeds) {
        public_key.clear();
        private_key.clear();
        error = "Unsupported SPHINCS+ parameter set";
        return false;
    }
    return sphincs::GenerateKeypairFromSeeds(parameter_set, seeds->sk_seed, seeds->sk_prf, seeds->pk_seed, public_key, private_key, error);
}

std::string EncodeNode(const Node& node)
{
    std::vector<unsigned char> payload(node.key.begin(), node.key.end());
    payload.insert(payload.end(), node.chaincode.begin(), node.chaincode.end());
    std::vector<uint8_t> data;
    data.reserve((payload.size() * 8 + 4) / 5);
    ConvertBits<8, 5, true>([&](uint8_t c) { data.push_back(c); }, payload.begin(), payload.end());
    std::string encoded{bech32::Encode(bech32::Encoding::BECH32M, std::string{NODE_HRP}, data)};
    Cleanse(payload);
    Cleanse(data);
    return encoded;
}

std::optional<Node> DecodeNode(const std::string_view str)
{
    bech32::DecodeResult decoded{bech32::Decode(std::string{str}, bech32::CharLimit::PQ_HD_NODE)};
    if (decoded.encoding != bech32::Encoding::BECH32M || decoded.hrp != NODE_HRP) {
        Cleanse(decoded.data);
        return std::nullopt;
    }
    std::vector<unsigned char> payload;
    payload.reserve(NODE_PAYLOAD_SIZE);
    const bool ok{ConvertBits<5, 8, false>([&](unsigned char c) { payload.push_back(c); }, decoded.data.begin(), decoded.data.end())};
    Cleanse(decoded.data);
    if (!ok || payload.size() != NODE_PAYLOAD_SIZE) {
        Cleanse(payload);
        return std::nullopt;
    }
    Node node;
    std::copy(payload.begin(), payload.begin() + 32, node.key.begin());
    std::copy(payload.begin() + 32, payload.end(), node.chaincode.begin());
    Cleanse(payload);
    return node;
}

uint256 NodeId(const Node& node)
{
    return (TaggedHash(std::string{NODE_ID_TAG}) << node.key << node.chaincode).GetSHA256();
}

std::string EncodeNodeId(const uint256& id)
{
    std::vector<uint8_t> data;
    data.reserve((uint256::size() * 8 + 4) / 5);
    ConvertBits<8, 5, true>([&](uint8_t c) { data.push_back(c); }, id.begin(), id.end());
    return bech32::Encode(bech32::Encoding::BECH32M, std::string{NODE_ID_HRP}, data);
}

std::optional<uint256> DecodeNodeId(const std::string_view str)
{
    const bech32::DecodeResult decoded{bech32::Decode(std::string{str})};
    if (decoded.encoding != bech32::Encoding::BECH32M || decoded.hrp != NODE_ID_HRP) return std::nullopt;
    std::vector<unsigned char> payload;
    payload.reserve(uint256::size());
    if (!ConvertBits<5, 8, false>([&](unsigned char c) { payload.push_back(c); }, decoded.data.begin(), decoded.data.end()) ||
        payload.size() != uint256::size()) {
        return std::nullopt;
    }
    return uint256{payload};
}

} // namespace pq::hd
