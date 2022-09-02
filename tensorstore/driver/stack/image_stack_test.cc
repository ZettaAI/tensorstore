// Copyright 2022 The TensorStore Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <assert.h>

#include <string>
#include <type_traits>

#include <gtest/gtest.h>
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include <nlohmann/json.hpp>
#include "tensorstore/context.h"
#include "tensorstore/index.h"
#include "tensorstore/kvstore/kvstore.h"
#include "tensorstore/kvstore/operations.h"
#include "tensorstore/open.h"
#include "tensorstore/progress.h"
#include "tensorstore/tensorstore.h"
#include "tensorstore/util/future.h"
#include "tensorstore/util/result.h"
#include "tensorstore/util/status.h"
#include "tensorstore/util/status_testutil.h"

namespace {

using ::tensorstore::Context;
using ::tensorstore::CopyProgressFunction;
using ::tensorstore::DimensionIndex;
using ::tensorstore::Index;
using ::tensorstore::ReadProgressFunction;

// hexdump -e \"\"\ 16/1\ \"\ 0x%02x,\"\ \"\\n\" image.png
static constexpr unsigned char kPng[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x08, 0x02, 0x00, 0x00, 0x00, 0xd3, 0x10, 0x3f, 0x31, 0x00, 0x00, 0x00,
    0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00, 0x00, 0x48, 0x00, 0x00, 0x00,
    0x48, 0x00, 0x46, 0xc9, 0x6b, 0x3e, 0x00, 0x00, 0x02, 0xbb, 0x49, 0x44,
    0x41, 0x54, 0x78, 0xda, 0xed, 0xd3, 0x01, 0x09, 0x00, 0x30, 0x10, 0xc4,
    0xb0, 0x7b, 0x98, 0x7f, 0xcd, 0x13, 0xd2, 0x84, 0x5a, 0xe8, 0x6d, 0x3b,
    0xa9, 0xda, 0xdb, 0x0d, 0xb2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00,
    0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66,
    0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20,
    0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c,
    0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4,
    0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01,
    0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34,
    0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00,
    0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66,
    0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20,
    0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c,
    0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4,
    0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01,
    0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34,
    0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00,
    0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66,
    0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20,
    0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c,
    0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4,
    0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01,
    0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34,
    0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00,
    0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66,
    0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20,
    0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c,
    0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4,
    0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01,
    0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34,
    0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00,
    0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66,
    0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20,
    0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c,
    0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4,
    0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01,
    0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34,
    0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00,
    0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66,
    0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20,
    0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c,
    0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4,
    0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01,
    0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34,
    0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00,
    0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66,
    0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20,
    0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c,
    0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4,
    0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01,
    0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34,
    0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00,
    0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66,
    0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20,
    0xcd, 0x00, 0xa4, 0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c,
    0x40, 0x9a, 0x01, 0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4,
    0x19, 0x80, 0x34, 0x03, 0x90, 0x66, 0x00, 0xd2, 0x0c, 0x40, 0x9a, 0x01,
    0x48, 0x33, 0x00, 0x69, 0x06, 0x20, 0xcd, 0x00, 0xa4, 0x19, 0x80, 0xb4,
    0x0f, 0x1a, 0x65, 0x05, 0xfc, 0x4f, 0xed, 0x72, 0x2f, 0x00, 0x00, 0x00,
    0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};

class ImageDriverReadTest : public ::testing::Test {
 public:
  tensorstore::Result<tensorstore::Context> PrepareTest() {
    absl::Cord data = absl::MakeCordFromExternal(
        absl::string_view(reinterpret_cast<const char*>(kPng), sizeof(kPng)),
        [] {});

    ::nlohmann::json spec{
        {"driver", "memory"},
        {"path", "a.png"},
    };

    auto context = tensorstore::Context::Default();
    TENSORSTORE_ASSIGN_OR_RETURN(
        auto kvs, tensorstore::kvstore::Open(spec, context).result());
    TENSORSTORE_RETURN_IF_ERROR(tensorstore::kvstore::Write(kvs, {}, data));
    return context;
  }
};

TEST_F(ImageDriverReadTest, OpenImageStack) {
  TENSORSTORE_ASSERT_OK_AND_ASSIGN(auto context, PrepareTest());

  ::nlohmann::json spec{
      {"driver", "stack"},
      {
          "layers",
          {
              {
                  {"driver", "png"},
                  {"kvstore", {{"driver", "memory"}, {"path", "a.png"}}},
                  {"transform",
                   {
                       {"input_inclusive_min", {0, 0, 0}},
                       {"input_exclusive_max", {256, 256, 1}},
                       {"output",
                        {{{"input_dimension", 0}},
                         {{"input_dimension", 1}},
                         {{"input_dimension", 2}, {"offset", 0}}}},
                   }},
              },
              {
                  {"driver", "png"},
                  {"kvstore", {{"driver", "memory"}, {"path", "a.png"}}},
                  {"transform",
                   {
                       {"input_inclusive_min", {0, 0, 1}},
                       {"input_exclusive_max", {256, 256, 2}},
                       {"output",
                        {{{"input_dimension", 0}},
                         {{"input_dimension", 1}},
                         {{"input_dimension", 2}, {"offset", 1}}}},
                   }},
              },
          },
      },
  };

  // Path is embedded in kvstore, so we don't write it.
  TENSORSTORE_ASSERT_OK_AND_ASSIGN(
      auto store,
      tensorstore::Open(spec, context, tensorstore::ReadWriteMode::read)
          .result());

  TENSORSTORE_ASSERT_OK_AND_ASSIGN(auto array,
                                   tensorstore::Read<>(store).result());
}

}  // namespace
