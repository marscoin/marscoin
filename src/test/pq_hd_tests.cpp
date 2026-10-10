// Copyright (c) 2026 The Marscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <addresstype.h>
#include <bech32.h>
#include <crypto/pq_hd.h>
#include <crypto/pq_sphincs.h>
#include <crypto/sha256.h>
#include <key.h>
#include <key_io.h>
#include <test/data/pq_hd_vectors.json.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <univalue.h>
#include <util/bip32.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

using pq::sphincs::ParameterSet;

BOOST_FIXTURE_TEST_SUITE(pq_hd_tests, BasicTestingSetup)

namespace {

UniValue LoadVectors()
{
    UniValue data;
    BOOST_REQUIRE(data.read(json_tests::pq_hd_vectors));
    return data;
}

std::vector<uint32_t> ParsePath(std::string path)
{
    std::replace(path.begin(), path.end(), 'h', '\'');
    std::vector<uint32_t> indices;
    BOOST_REQUIRE(ParseHDKeypath(path, indices));
    return indices;
}

void CheckNode(const pq::hd::Node& node, const UniValue& expected)
{
    BOOST_CHECK_EQUAL(HexStr(node.key), expected["key"].get_str());
    BOOST_CHECK_EQUAL(HexStr(node.chaincode), expected["chaincode"].get_str());
}

//! The witness version and program of a Bech32m address, whatever its prefix.
std::optional<std::pair<uint8_t, std::vector<unsigned char>>> DecodeWitnessAddress(const std::string& address)
{
    const bech32::DecodeResult decoded{bech32::Decode(address)};
    if (decoded.encoding != bech32::Encoding::BECH32M || decoded.data.empty()) return std::nullopt;
    std::vector<unsigned char> program;
    if (!ConvertBits<5, 8, false>([&](unsigned char c) { program.push_back(c); }, decoded.data.begin() + 1, decoded.data.end())) {
        return std::nullopt;
    }
    return std::make_pair(decoded.data[0], program);
}

} // namespace

BOOST_AUTO_TEST_CASE(pq_hd_vectors)
{
    const UniValue data{LoadVectors()};
    size_t keys_checked{0};
    for (const UniValue& vector : data["vectors"].getValues()) {
        const std::vector<unsigned char> seed{ParseHex(vector["seed"].get_str())};
        const std::optional<pq::hd::Node> root{pq::hd::RootFromSeed(seed)};
        BOOST_REQUIRE(root);
        CheckNode(*root, vector["root"]);

        const std::string encoded{vector["root"]["encoded"].get_str()};
        BOOST_CHECK_EQUAL(pq::hd::EncodeNode(*root), encoded);
        const std::optional<pq::hd::Node> decoded{pq::hd::DecodeNode(encoded)};
        BOOST_REQUIRE(decoded);
        BOOST_CHECK(*decoded == *root);

        const uint256 id{pq::hd::NodeId(*root)};
        BOOST_CHECK_EQUAL(HexStr(id), vector["root"]["id"].get_str());
        BOOST_CHECK_EQUAL(pq::hd::EncodeNodeId(id), vector["root"]["id_encoded"].get_str());
        BOOST_CHECK(pq::hd::DecodeNodeId(vector["root"]["id_encoded"].get_str()) == id);

        for (const UniValue& key : vector["keys"].getValues()) {
            const std::string path{key["path"].get_str()};
            BOOST_TEST_MESSAGE("seed " << vector["seed"].get_str() << " path " << path);
            const std::optional<pq::hd::Node> leaf{pq::hd::DerivePath(*root, ParsePath(path))};
            BOOST_REQUIRE(leaf);
            CheckNode(*leaf, key);

            const uint8_t id{static_cast<uint8_t>(key["parameter_set"].getInt<int>())};
            BOOST_REQUIRE(pq::sphincs::IsSupportedParameterSet(id));
            const auto parameter_set{static_cast<ParameterSet>(id)};
            const std::optional<pq::hd::KeygenSeeds> seeds{pq::hd::DeriveKeygenSeeds(*leaf, parameter_set)};
            BOOST_REQUIRE(seeds);
            BOOST_CHECK_EQUAL(HexStr(seeds->sk_seed), key["sk_seed"].get_str());
            BOOST_CHECK_EQUAL(HexStr(seeds->sk_prf), key["sk_prf"].get_str());
            BOOST_CHECK_EQUAL(HexStr(seeds->pk_seed), key["pk_seed"].get_str());

            std::vector<unsigned char> pubkey, privkey;
            std::string error;
            BOOST_REQUIRE_MESSAGE(pq::hd::DeriveKeypair(*leaf, parameter_set, pubkey, privkey, error), error);
            BOOST_CHECK_EQUAL(HexStr(pubkey), key["pubkey"].get_str());
            // An SLH-DSA secret key is SK.seed || SK.prf || PK.seed || PK.root.
            BOOST_CHECK_EQUAL(HexStr(privkey), key["sk_seed"].get_str() + key["sk_prf"].get_str() + key["pubkey"].get_str());

            uint256 program;
            CSHA256().Write(&id, 1).Write(pubkey.data(), pubkey.size()).Finalize(program.begin());
            BOOST_CHECK_EQUAL(HexStr(program), key["program"].get_str());

            const std::string address{key["address"].get_str()};
            const auto witness{DecodeWitnessAddress(address)};
            BOOST_REQUIRE(witness);
            BOOST_CHECK_EQUAL(witness->first, 2);
            BOOST_CHECK_EQUAL(HexStr(witness->second), key["program"].get_str());
            if (address.rfind("mars1", 0) == 0) {
                // The test setup selects mainnet, whose P2WPQH addresses start with mars1z.
                BOOST_CHECK_EQUAL(EncodeDestination(WitnessV2PQ{program}), address);
            }
            ++keys_checked;
        }
    }
    BOOST_CHECK_GT(keys_checked, 0U);
}

BOOST_AUTO_TEST_CASE(pq_hd_invalid_inputs)
{
    const UniValue data{LoadVectors()};

    for (const UniValue& seed : data["invalid_seeds"].getValues()) {
        BOOST_CHECK(!pq::hd::RootFromSeed(ParseHex(seed.get_str())));
    }
    BOOST_CHECK(pq::hd::RootFromSeed(std::vector<unsigned char>(pq::hd::MIN_SEED_SIZE, 0x01)));
    BOOST_CHECK(pq::hd::RootFromSeed(std::vector<unsigned char>(pq::hd::MAX_SEED_SIZE, 0x01)));

    for (const UniValue& entry : data["invalid_paths"].getValues()) {
        const std::optional<pq::hd::Node> root{pq::hd::RootFromSeed(ParseHex(entry["seed"].get_str()))};
        BOOST_REQUIRE(root);
        BOOST_CHECK_MESSAGE(!pq::hd::DerivePath(*root, ParsePath(entry["path"].get_str())), entry["comment"].get_str());
    }

    for (const UniValue& entry : data["invalid_encodings"].getValues()) {
        BOOST_CHECK_MESSAGE(!pq::hd::DecodeNode(entry["encoded"].get_str()), entry["comment"].get_str());
    }
    for (const UniValue& entry : data["invalid_id_encodings"].getValues()) {
        BOOST_CHECK_MESSAGE(!pq::hd::DecodeNodeId(entry["encoded"].get_str()), entry["comment"].get_str());
    }
}

BOOST_AUTO_TEST_CASE(pq_hd_key_path)
{
    using pq::hd::HARDENED;
    BOOST_CHECK(pq::hd::KeyPath(pq::hd::COIN_TYPE_MAINNET, 0, false, 5) ==
                (std::vector<uint32_t>{107 | HARDENED, 0 | HARDENED, 0 | HARDENED, 5 | HARDENED}));
    BOOST_CHECK(pq::hd::KeyPath(pq::hd::COIN_TYPE_TEST, 2, true, 7) ==
                (std::vector<uint32_t>{1 | HARDENED, 2 | HARDENED, 1 | HARDENED, 7 | HARDENED}));
}

BOOST_AUTO_TEST_CASE(pq_hd_independent_of_bip32)
{
    // The same seed gives unrelated BIP32 and PQ HD roots, so no BIP32 secret
    // (and no EC public key) reveals anything about the PQ tree.
    const std::vector<unsigned char> seed{ParseHex("000102030405060708090a0b0c0d0e0f")};
    CExtKey bip32_master;
    bip32_master.SetSeed(MakeByteSpan(seed));
    const std::optional<pq::hd::Node> root{pq::hd::RootFromSeed(seed)};
    BOOST_REQUIRE(root);
    BOOST_CHECK(!std::equal(root->key.begin(), root->key.end(), UCharCast(bip32_master.key.begin())));
    BOOST_CHECK(!std::equal(root->chaincode.begin(), root->chaincode.end(), bip32_master.chaincode.begin()));
}

BOOST_AUTO_TEST_CASE(pq_hd_unsupported_parameter_set)
{
    const std::optional<pq::hd::Node> root{pq::hd::RootFromSeed(std::vector<unsigned char>(32, 0x07))};
    BOOST_REQUIRE(root);
    for (const uint8_t id : {0x00, 0x02, 0xff}) {
        BOOST_CHECK(!pq::hd::DeriveKeygenSeeds(*root, static_cast<ParameterSet>(id)));
        std::vector<unsigned char> pubkey, privkey;
        std::string error;
        BOOST_CHECK(!pq::hd::DeriveKeypair(*root, static_cast<ParameterSet>(id), pubkey, privkey, error));
        BOOST_CHECK(pubkey.empty() && privkey.empty());
    }
}

BOOST_AUTO_TEST_SUITE_END()
