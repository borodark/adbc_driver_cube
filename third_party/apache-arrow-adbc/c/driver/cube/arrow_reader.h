// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <arrow-adbc/adbc.h>
#include <nanoarrow/nanoarrow.h>

// Forward declaration for FlatBuffer types (in global namespace)
namespace org {
namespace apache {
namespace arrow {
namespace flatbuf {
struct RecordBatch;
}
} // namespace arrow
} // namespace apache
} // namespace org

namespace adbc::cube {

// Helper class to deserialize Arrow IPC format results from Cube SQL
class CubeArrowReader {
public:
  // Create reader from raw Arrow IPC bytes
  // Takes ownership of the buffer
  explicit CubeArrowReader(std::vector<uint8_t> arrow_ipc_data);
  ~CubeArrowReader();

  // Initialize the reader and parse the schema
  // Must be called before GetSchema or GetNext
  ArrowErrorCode Init(ArrowError *error);

  // Get the Arrow schema
  ArrowErrorCode GetSchema(ArrowSchema *out);

  // Get the next RecordBatch
  // Returns ENOMSG (no message) when no more batches
  ArrowErrorCode GetNext(ArrowArray *out);

  // Create an ArrowArrayStream from this reader
  // The stream will manage the reader's lifetime
  void ExportTo(struct ArrowArrayStream *stream);

private:
  // Parse Arrow IPC message at current offset
  ArrowErrorCode ParseMessage(ArrowError *error);

  // Parse schema message (first message in stream)
  ArrowErrorCode ParseSchemaMessage(const uint8_t *message_data,
                                    int64_t message_length, ArrowError *error);

  // Parse RecordBatch message
  ArrowErrorCode ParseRecordBatchMessage(const uint8_t *message_data,
                                         int64_t message_length,
                                         ArrowArray *out, ArrowError *error);

  // FlatBuffer parsing methods
  ArrowErrorCode ParseSchemaFlatBuffer(const uint8_t *fb_data, int64_t fb_size,
                                       ArrowError *error);

  ArrowErrorCode ParseRecordBatchFlatBuffer(const uint8_t *fb_data,
                                            int64_t fb_size,
                                            const uint8_t *body_data,
                                            int64_t body_size, ArrowArray *out,
                                            ArrowError *error);

  ArrowErrorCode
  BuildArrayForField(int field_index, int64_t row_count,
                     const org::apache::arrow::flatbuf::RecordBatch *batch,
                     const uint8_t *body_data, int *buffer_index_inout,
                     ArrowArray *out, ArrowError *error);

  void ExtractBuffer(const org::apache::arrow::flatbuf::RecordBatch *batch,
                     int buffer_index, const uint8_t *body_data,
                     const uint8_t **out_ptr, int64_t *out_size);

  int MapFlatBufferTypeToArrow(int fb_type);
  int GetBufferCountForType(int arrow_type);
  static bool GetBit(const uint8_t *bitmap, int64_t index);

  std::vector<uint8_t> buffer_;     // Raw Arrow IPC bytes
  int64_t offset_ = 0;              // Current position in buffer
  struct ArrowSchema schema_;       // Parsed schema
  bool schema_initialized_ = false; // Whether schema has been parsed
  bool finished_ = false;           // Whether we've reached end of stream

  // Schema metadata (parsed from FlatBuffer)
  std::vector<std::string> field_names_;
  std::vector<int> field_types_;
  std::vector<bool> field_nullable_;
};

} // namespace adbc::cube
