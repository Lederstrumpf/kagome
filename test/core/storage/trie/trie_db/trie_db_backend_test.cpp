/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "storage/trie/impl/polkadot_trie_db.hpp"

#include <memory>

#include <gtest/gtest.h>
#include "mock/core/storage/persistent_map_mock.hpp"
#include "mock/core/storage/write_batch_mock.hpp"
#include "testutil/literals.hpp"
#include "testutil/outcome.hpp"

using kagome::common::Buffer;
using kagome::storage::face::PersistentMapMock;
using kagome::storage::face::WriteBatchMock;
using kagome::storage::trie::PolkadotTrieDb;
using kagome::storage::trie::PolkadotTrieDbBackend;
using testing::Invoke;
using testing::Return;

static const Buffer kNodePrefix{1};
static const Buffer kRootHashKey{0};

class TrieDbBackendTest : public testing::Test {
 public:
  void SetUp() {}

  std::shared_ptr<PersistentMapMock<Buffer, Buffer>> storage =
      std::make_shared<PersistentMapMock<Buffer, Buffer>>();
  PolkadotTrieDbBackend backend{storage, kNodePrefix, kRootHashKey};
};

/**
 * @given trie backend
 * @when put a value to it
 * @then it puts a prefixed value to the storage
 */
TEST_F(TrieDbBackendTest, Put) {
  Buffer prefixed{kNodePrefix};
  prefixed.put("abc"_buf);
  EXPECT_CALL(*storage, put_rvalueHack(prefixed, "123"_buf))
      .WillOnce(Return(outcome::success()));
  EXPECT_OUTCOME_TRUE_1(backend.put("abc"_buf, "123"_buf));
}

/**
 * @given trie backend
 * @when get a value from it
 * @then it takes a prefixed value from the storage
 */
TEST_F(TrieDbBackendTest, Get) {
  Buffer prefixed{kNodePrefix};
  prefixed.put("abc"_buf);
  EXPECT_CALL(*storage, get(prefixed)).WillOnce(Return("123"_buf));
  EXPECT_OUTCOME_TRUE_1(backend.get("abc"_buf));
}

/**
 * @given trie backend
 * @when use a batch to put values to storage
 * @then batch processes prefixes as the backend itself would
 */
TEST_F(TrieDbBackendTest, Batch) {
  EXPECT_CALL(*storage, batch()).WillOnce(Invoke([]() {
    auto batch = std::make_unique<WriteBatchMock<Buffer, Buffer>>();
    Buffer prefixed{kNodePrefix};
    prefixed.put("abc"_buf);
    EXPECT_CALL(*batch, put_rvalue(prefixed, "123"_buf))
        .WillOnce(Return(outcome::success()));
    return batch;
  }));
  auto batch = backend.batch();
  EXPECT_OUTCOME_TRUE_1(batch->put("abc"_buf, "123"_buf));
}

/**
 * @given trie backend
 * @when saving and fetching the root hash
 * @then it is saved by the key specified during construction of backend and is
 * correctly fetched
 */
TEST_F(TrieDbBackendTest, Root) {
  Buffer root_hash{"12345"_buf};
  EXPECT_CALL(*storage, put(kRootHashKey, root_hash))
      .WillOnce(Return(outcome::success()));
  EXPECT_OUTCOME_TRUE_1(backend.saveRootHash(root_hash));
  EXPECT_CALL(*storage, get(kRootHashKey)).WillOnce(Return(root_hash));
  EXPECT_OUTCOME_TRUE(hash, backend.getRootHash());
  ASSERT_EQ(hash, root_hash);
}
