// Copyright (c) 2026 The Marscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/pq_hd.h>
#include <outputtype.h>
#include <script/descriptor.h>
#include <script/script.h>
#include <script/signingprovider.h>
#include <test/data/pq_hd_vectors.json.h>
#include <test/util/setup_common.h>
#include <univalue.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <map>
#include <string>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(pq_descriptor_tests, BasicTestingSetup)

namespace {

//! The root of the first vector and its keys, by path.
struct Vector {
    std::string root;
    std::string root_id;
    std::map<std::string, UniValue> keys;
};

Vector LoadVector()
{
    UniValue data;
    BOOST_REQUIRE(data.read(json_tests::pq_hd_vectors));
    const UniValue& first{data["vectors"][0]};
    Vector vector{first["root"]["encoded"].get_str(), first["root"]["id_encoded"].get_str(), {}};
    for (const UniValue& key : first["keys"].getValues()) vector.keys.emplace(key["path"].get_str(), key);
    return vector;
}

//! The pq_keys key of a program given in hex: its raw bytes, not the reversed display order.
uint256 ProgramKey(const std::string& hex)
{
    return uint256{ParseHex(hex)};
}

std::string StripChecksum(const std::string& desc)
{
    return desc.substr(0, desc.find('#'));
}

std::string ProgramOf(const CScript& script)
{
    int version;
    std::vector<unsigned char> program;
    BOOST_REQUIRE(script.IsWitnessProgram(version, program));
    BOOST_CHECK_EQUAL(version, 2);
    return HexStr(program);
}

} // namespace

BOOST_AUTO_TEST_CASE(wpq_round_trip)
{
    const Vector vector{LoadVector()};
    FlatSigningProvider keys;
    std::string error;
    const auto desc{Parse("wpq(" + vector.root + "/107h/0h/0h/*h)", keys, error)};
    BOOST_REQUIRE_MESSAGE(desc, error);
    BOOST_CHECK(desc->IsRange());
    BOOST_CHECK(desc->IsSolvable());
    BOOST_CHECK(desc->IsSingleType());
    BOOST_CHECK(desc->GetOutputType() == OutputType::BECH32_PQ);
    BOOST_REQUIRE_EQUAL(keys.pq_nodes.size(), 1U);

    // The public form names the node by its identifier only.
    const std::string pub{desc->ToString()};
    BOOST_CHECK_EQUAL(StripChecksum(pub), "wpq(" + vector.root_id + "/107h/0h/0h/*h)");
    BOOST_CHECK(pub.find("mpqprv") == std::string::npos);
    std::string normalized;
    BOOST_CHECK(desc->ToNormalizedString(keys, normalized));
    BOOST_CHECK_EQUAL(normalized, pub);

    std::string priv;
    BOOST_REQUIRE(desc->ToPrivateString(keys, priv));
    BOOST_CHECK_EQUAL(StripChecksum(priv), "wpq(" + vector.root + "/107h/0h/0h/*h)");
    BOOST_CHECK(!desc->ToPrivateString(FlatSigningProvider{}, priv));

    // Both forms parse back, with a checksum, to the same descriptor.
    FlatSigningProvider pub_keys, priv_keys;
    const auto from_pub{Parse(pub, pub_keys, error, /*require_checksum=*/true)};
    BOOST_REQUIRE_MESSAGE(from_pub, error);
    BOOST_CHECK(pub_keys.pq_nodes.empty());
    BOOST_CHECK(DescriptorID(*from_pub) == DescriptorID(*desc));
    const auto from_priv{Parse(priv, priv_keys, error, /*require_checksum=*/true)};
    BOOST_REQUIRE_MESSAGE(from_priv, error);
    BOOST_CHECK(DescriptorID(*from_priv) == DescriptorID(*desc));

    // The apostrophe notation is kept.
    FlatSigningProvider apostrophe_keys;
    const auto apostrophe{Parse("wpq(" + vector.root + "/107'/0'/0'/*')", apostrophe_keys, error)};
    BOOST_REQUIRE_MESSAGE(apostrophe, error);
    BOOST_CHECK_EQUAL(StripChecksum(apostrophe->ToString()), "wpq(" + vector.root_id + "/107'/0'/0'/*')");
    BOOST_CHECK(DescriptorID(*apostrophe) == DescriptorID(*desc));
}

BOOST_AUTO_TEST_CASE(wpq_expand)
{
    const Vector vector{LoadVector()};
    FlatSigningProvider keys;
    std::string error;
    const auto desc{Parse("wpq(" + vector.root + "/107h/0h/0h/*h)", keys, error)};
    BOOST_REQUIRE_MESSAGE(desc, error);

    DescriptorCache cache;
    for (int pos : {0, 1}) {
        const UniValue& expected{vector.keys.at(strprintf("m/107h/0h/0h/%dh", pos))};
        std::vector<CScript> scripts;
        FlatSigningProvider out;
        BOOST_REQUIRE(desc->Expand(pos, keys, scripts, out, &cache));
        BOOST_REQUIRE_EQUAL(scripts.size(), 1U);
        BOOST_CHECK_EQUAL(ProgramOf(scripts[0]), expected["program"].get_str());

        // The public key is known, the private key is not.
        uint8_t id;
        std::vector<unsigned char> pubkey, privkey;
        BOOST_REQUIRE(out.GetPQKey(ProgramKey(expected["program"].get_str()), id, pubkey, privkey));
        BOOST_CHECK_EQUAL(id, expected["parameter_set"].getInt<int>());
        BOOST_CHECK_EQUAL(HexStr(pubkey), expected["pubkey"].get_str());
        BOOST_CHECK(privkey.empty());
        BOOST_CHECK(out.pq_nodes.empty());

        // The cache alone expands the same position.
        std::vector<CScript> cached_scripts;
        FlatSigningProvider cached_out;
        BOOST_REQUIRE(desc->ExpandFromCache(pos, cache, cached_scripts, cached_out));
        BOOST_CHECK(cached_scripts == scripts);

        // The private expansion adds the full SLH-DSA key.
        FlatSigningProvider priv_out;
        desc->ExpandPrivate(pos, keys, priv_out);
        BOOST_REQUIRE(priv_out.GetPQKey(ProgramKey(expected["program"].get_str()), id, pubkey, privkey));
        BOOST_CHECK_EQUAL(HexStr(privkey), expected["sk_seed"].get_str() + expected["sk_prf"].get_str() + expected["pubkey"].get_str());

        // Merging keeps the private key whichever side has it.
        FlatSigningProvider merged{cached_out};
        merged.Merge(FlatSigningProvider{priv_out});
        BOOST_REQUIRE(merged.GetPQKey(ProgramKey(expected["program"].get_str()), id, pubkey, privkey));
        BOOST_CHECK(!privkey.empty());
        FlatSigningProvider merged_back{priv_out};
        merged_back.Merge(FlatSigningProvider{cached_out});
        BOOST_REQUIRE(merged_back.GetPQKey(ProgramKey(expected["program"].get_str()), id, pubkey, privkey));
        BOOST_CHECK(!privkey.empty());
    }
    std::vector<CScript> scripts;
    FlatSigningProvider out;
    BOOST_CHECK(!desc->ExpandFromCache(2, cache, scripts, out));
    BOOST_CHECK(!desc->Expand(-1, keys, scripts, out, nullptr));

    // Without the node, the public form expands only what is cached.
    FlatSigningProvider pub_keys;
    const auto pub{Parse(desc->ToString(), pub_keys, error)};
    BOOST_REQUIRE(pub);
    BOOST_CHECK(!pub->Expand(0, pub_keys, scripts, out, nullptr));
    BOOST_CHECK(pub->ExpandFromCache(1, cache, scripts, out));
    FlatSigningProvider no_private;
    pub->ExpandPrivate(1, pub_keys, no_private);
    BOOST_CHECK(no_private.pq_keys.empty());

    // A hiding provider passes neither the node nor private keys on.
    const HidingSigningProvider hiding{&keys, /*hide_secret=*/true, /*hide_origin=*/false};
    BOOST_CHECK(!desc->Expand(0, hiding, scripts, out, nullptr));
}

BOOST_AUTO_TEST_CASE(wpq_not_ranged)
{
    const Vector vector{LoadVector()};
    FlatSigningProvider keys;
    std::string error;
    const auto desc{Parse("wpq(" + vector.root + "/107h/0h/1h/0h)", keys, error)};
    BOOST_REQUIRE_MESSAGE(desc, error);
    BOOST_CHECK(!desc->IsRange());
    for (int pos : {0, 7}) {
        std::vector<CScript> scripts;
        FlatSigningProvider out;
        BOOST_REQUIRE(desc->Expand(pos, keys, scripts, out, nullptr));
        BOOST_CHECK_EQUAL(ProgramOf(scripts.at(0)), vector.keys.at("m/107h/0h/1h/0h")["program"].get_str());
    }
}

BOOST_AUTO_TEST_CASE(wpq_sizes)
{
    const Vector vector{LoadVector()};
    FlatSigningProvider keys;
    std::string error;
    const auto desc{Parse("wpq(" + vector.root + "/107h/0h/0h/*h)", keys, error)};
    BOOST_REQUIRE(desc);
    BOOST_CHECK_EQUAL(*desc->ScriptSize(), 34);
    // 3 + (1 + 7856 + 1) for the payload, 1 + 1 for the parameter set id, 1 + 32 for the key.
    BOOST_CHECK_EQUAL(*desc->MaxSatisfactionWeight(true), 7896);
    BOOST_CHECK_EQUAL(*desc->MaxSatisfactionElems(), 3);
    std::set<CPubKey> pubkeys;
    std::set<CExtPubKey> ext_pubs;
    desc->GetPubKeys(pubkeys, ext_pubs);
    BOOST_CHECK(pubkeys.empty() && ext_pubs.empty());
}

BOOST_AUTO_TEST_CASE(wpq_parse_errors)
{
    const Vector vector{LoadVector()};
    const auto check_error = [](const std::string& desc, const std::string& expected) {
        FlatSigningProvider keys;
        std::string error;
        BOOST_CHECK_MESSAGE(!Parse(desc, keys, error), desc);
        BOOST_CHECK_MESSAGE(error.find(expected) != std::string::npos, desc + ": " + error);
        return error;
    };
    check_error("wpq(" + vector.root + "/107h/0/0h/*h)", "every derivation step must be hardened");
    check_error("wpq(" + vector.root + "/107h/0h/0h/*)", "wildcard must be hardened");
    check_error("wpq(" + vector.root + "/107h/x/*h)", "is not a valid uint32");
    check_error("sh(wpq(" + vector.root + "/107h/0h/0h/*h))", "Can only have wpq() at top level");
    check_error("wsh(wpq(" + vector.root + "/107h/0h/0h/*h))", "Can only have wpq() at top level");

    // A mistyped private node is not echoed back.
    std::string typo{vector.root};
    typo.back() = typo.back() == 'q' ? 'p' : 'q';
    const std::string error{check_error("wpq(" + typo + "/107h/0h/0h/*h)", "neither an mpqprv node nor an mpqid identifier")};
    BOOST_CHECK(error.find(typo) == std::string::npos);

    FlatSigningProvider keys;
    std::string parse_error;
    BOOST_CHECK(!Parse("wpq(" + vector.root + "/107h/0h/0h/*h)extra", keys, parse_error));
}

BOOST_AUTO_TEST_SUITE_END()
