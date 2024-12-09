/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "storage/face/write_batch.hpp"

#include <gmock/gmock.h>

namespace kagome::storage::face {

  template <typename K, typename V>
  class WriteBatchMock : public WriteBatch<K, V> {
   public:
    MOCK_METHOD(outcome::result<void>, commit, (), (override));

    MOCK_METHOD(void, clear, (), (override));

    MOCK_METHOD2_T(put,
                   outcome::result<void>(const View<K> &key, const V &value));
    outcome::result<void> put(const View<K> &key,
                              OwnedOrView<V> &&value) override {
      return put(key, value.mut());
    }

    MOCK_METHOD1_T(remove, outcome::result<void>(const View<K> &key));
  };

  template <typename Space, typename K, typename V>
  class SpacedBatchMock : public SpacedBatch<Space, K, V> {
   public:
    MOCK_METHOD(outcome::result<void>, commit, (), (override));

    MOCK_METHOD(void, clear, (), (override));

    MOCK_METHOD3_T(put,
                   outcome::result<void>(Space space,
                                         const View<K> &key,
                                         const V &value));
    outcome::result<void> put(Space space,
                              const View<K> &key,
                              OwnedOrView<V> &&value) override {
      return put(space, key, value.mut());
    }

    MOCK_METHOD2_T(remove,
                   outcome::result<void>(Space space, const View<K> &key));
  };

}  // namespace kagome::storage::face
