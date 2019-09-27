/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef KAGOME_PROTOBUF_KEY_HPP
#define KAGOME_PROTOBUF_KEY_HPP

#include <vector>

namespace libp2p::crypto {
  /**
   * Strict type for key, which is encoded into Protobuf format
   */
  struct ProtobufKey {
    std::vector<uint8_t> key;
  };
}  // namespace libp2p::crypto

#endif  // KAGOME_PROTOBUF_KEY_HPP