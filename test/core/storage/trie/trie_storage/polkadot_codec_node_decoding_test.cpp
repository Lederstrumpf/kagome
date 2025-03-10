/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>

#include <gtest/gtest.h>
#include "storage/trie/polkadot_trie/trie_node.hpp"
#include "storage/trie/serialization/buffer_stream.hpp"
#include "storage/trie/serialization/polkadot_codec.hpp"
#include "testutil/literals.hpp"
#include "testutil/outcome.hpp"

using namespace kagome;
using namespace common;
using namespace storage;
using namespace trie;
using namespace testing;

struct NodeDecodingTest
    : public ::testing::TestWithParam<std::shared_ptr<TrieNode>> {
  std::unique_ptr<PolkadotCodec> codec = std::make_unique<PolkadotCodec>();
};

TEST_P(NodeDecodingTest, GetHeader) {
  auto node = GetParam();

  EXPECT_OUTCOME_TRUE(
      encoded, codec->encodeNode(*node, storage::trie::StateVersion::V0, {}));
  EXPECT_OUTCOME_TRUE(decoded, codec->decodeNode(encoded));
  auto decoded_node = std::dynamic_pointer_cast<TrieNode>(decoded);
  EXPECT_EQ(decoded_node->getKeyNibbles(), node->getKeyNibbles());
  EXPECT_EQ(decoded_node->getValue(), node->getValue());
}

template <typename T>
std::shared_ptr<TrieNode> make(const common::Buffer &key_nibbles,
                               const common::Buffer &value) {
  auto node = std::make_shared<T>();
  node->setKeyNibbles(key_nibbles);
  node->setValue(value);
  return node;
}

std::shared_ptr<TrieNode> branch_with_2_children = []() {
  auto node =
      std::make_shared<BranchNode>(KeyNibbles{"010203"_hex2buf}, "0a"_hex2buf);
  auto child1 =
      std::make_shared<LeafNode>(KeyNibbles{"01"_hex2buf}, "0b"_hex2buf);
  auto child2 =
      std::make_shared<LeafNode>(KeyNibbles{"02"_hex2buf}, "0c"_hex2buf);
  node->setChild(0, child1);
  node->setChild(1, child2);
  return node;
}();

using T = TrieNode::Type;

static const std::vector<std::shared_ptr<TrieNode>> DECODING_CASES = {
    make<LeafNode>("010203"_hex2buf, "abcdef"_hex2buf),
    make<LeafNode>("0a0b0c"_hex2buf, "abcdef"_hex2buf),
    make<BranchNode>("010203"_hex2buf, "abcdef"_hex2buf),
    branch_with_2_children};

INSTANTIATE_TEST_SUITE_P(PolkadotCodec,
                         NodeDecodingTest,
                         ValuesIn(DECODING_CASES));
