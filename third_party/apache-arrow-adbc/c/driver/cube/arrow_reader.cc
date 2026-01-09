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

// Set to 1 to enable debug logging
#ifndef CUBE_DEBUG_LOGGING
#define CUBE_DEBUG_LOGGING 0
#endif

#if CUBE_DEBUG_LOGGING
#define DEBUG_LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_LOG(...) ((void)0)
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

#include "driver/cube/arrow_reader.h"
#include "format/generated/Message_generated.h"
#include "format/generated/Schema_generated.h"
#include <flatbuffers/flatbuffers.h>

namespace adbc::cube {

namespace {

// Arrow IPC format constants
const uint32_t ARROW_IPC_MAGIC = 0xFFFFFFFF;
const int ARROW_IPC_SCHEMA_MESSAGE_TYPE = 1;
const int ARROW_IPC_RECORD_BATCH_MESSAGE_TYPE = 3; // Fixed: was 0, should be 3

// Helper to read little-endian integers (Arrow IPC format uses little-endian)
inline uint32_t ReadLE32(const uint8_t *data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

inline int32_t ReadLE32Signed(const uint8_t *data) {
  return static_cast<int32_t>(ReadLE32(data));
}

// Helper to get bit from bitmap (Arrow validity bitmaps)
inline bool GetBit(const uint8_t *bitmap, int64_t index) {
  return (bitmap[index / 8] & (1 << (index % 8))) != 0;
}

} // namespace

CubeArrowReader::CubeArrowReader(std::vector<uint8_t> arrow_ipc_data)
    : buffer_(std::move(arrow_ipc_data)) {
  ArrowSchemaInit(&schema_);
}

CubeArrowReader::~CubeArrowReader() {
  if (schema_initialized_) {
    ArrowSchemaRelease(&schema_);
  }
}

ArrowErrorCode CubeArrowReader::Init(ArrowError *error) {
  DEBUG_LOG("[CubeArrowReader::Init] Starting with buffer size: %zu\n",
            buffer_.size());

  if (buffer_.empty()) {
    ArrowErrorSet(error, "Empty Arrow IPC buffer");
    return EINVAL;
  }

  // Debug: Save raw Arrow IPC data to file
  /* TODO enable based CUBE_DEBUG_LOGGING
  FILE* debug_file = fopen("/tmp/cube_arrow_ipc_data.bin", "wb");
  if (debug_file) {
    fwrite(buffer_.data(), 1, buffer_.size(), debug_file);
    fclose(debug_file);
    DEBUG_LOG( "[CubeArrowReader::Init] Saved %zu bytes to
  /tmp/cube_arrow_ipc_data.bin\n", buffer_.size());
  }
  */
  // Debug: Print first 128 bytes as hex
  DEBUG_LOG("[CubeArrowReader::Init] First 128 bytes (hex):\n");
  for (size_t i = 0; i < std::min(buffer_.size(), size_t(128)); i++) {
    if (i % 16 == 0)
      DEBUG_LOG("  %04zx: ", i);
    DEBUG_LOG("%02x ", buffer_[i]);
    if ((i + 1) % 16 == 0)
      DEBUG_LOG("\n");
  }
  if (buffer_.size() % 16 != 0)
    DEBUG_LOG("\n");

  // Parse Arrow IPC stream format
  // Format: [Continuation=0xFFFFFFFF][Size][Message][Padding]
  DEBUG_LOG("[CubeArrowReader::Init] Parsing Arrow IPC stream format\n");

  // Message 0: Schema message
  if (offset_ + 8 > static_cast<int64_t>(buffer_.size())) {
    ArrowErrorSet(error, "Buffer too small for schema message header");
    return EINVAL;
  }

  uint32_t continuation = ReadLE32(buffer_.data() + offset_);
  uint32_t msg_size = ReadLE32(buffer_.data() + offset_ + 4);
  DEBUG_LOG(
      "[CubeArrowReader::Init] Schema message: continuation=0x%x, size=%u\n",
      continuation, msg_size);

  if (continuation != ARROW_IPC_MAGIC) {
    ArrowErrorSet(error, "Invalid continuation marker for schema");
    return EINVAL;
  }

  // Parse schema message using FlatBuffers
  DEBUG_LOG("[CubeArrowReader::Init] Parsing FlatBuffer schema\n");
  auto status =
      ParseSchemaFlatBuffer(buffer_.data() + offset_ + 8, msg_size, error);
  if (status != NANOARROW_OK) {
    DEBUG_LOG("[CubeArrowReader::Init] FlatBuffer schema parsing failed\n");
    return status;
  }

  // Advance past schema message (align to 8 bytes)
  offset_ = 8 + msg_size;
  if (offset_ % 8 != 0) {
    offset_ += 8 - (offset_ % 8);
  }

  finished_ = false;
  DEBUG_LOG("[CubeArrowReader::Init] Schema initialized, offset now at %lld\n",
            (long long)offset_);
  return NANOARROW_OK;
}

ArrowErrorCode CubeArrowReader::GetSchema(ArrowSchema *out) {
  DEBUG_LOG("[CubeArrowReader::GetSchema] schema_initialized_=%d\n",
            schema_initialized_);
  if (!schema_initialized_) {
    DEBUG_LOG("[CubeArrowReader::GetSchema] Schema not initialized!\n");
    return EINVAL; // Schema not yet initialized
  }
  auto result = ArrowSchemaDeepCopy(&schema_, out);
  DEBUG_LOG("[CubeArrowReader::GetSchema] DeepCopy returned: %d\n", result);
  return result;
}

ArrowErrorCode CubeArrowReader::GetNext(ArrowArray *out) {
  DEBUG_LOG("[CubeArrowReader::GetNext] schema_initialized_=%d, finished_=%d, "
            "offset_=%lld\n",
            schema_initialized_, finished_, (long long)offset_);

  if (!schema_initialized_) {
    DEBUG_LOG("[CubeArrowReader::GetNext] Schema not initialized!\n");
    return EINVAL;
  }

  if (finished_) {
    DEBUG_LOG("[CubeArrowReader::GetNext] Already finished\n");
    return ENOMSG; // No more messages
  }

  // Parse RecordBatch message
  if (offset_ + 8 > static_cast<int64_t>(buffer_.size())) {
    DEBUG_LOG("[CubeArrowReader::GetNext] End of buffer\n");
    finished_ = true;
    return ENOMSG;
  }

  uint32_t continuation = ReadLE32(buffer_.data() + offset_);
  uint32_t msg_size = ReadLE32(buffer_.data() + offset_ + 4);
  DEBUG_LOG("[CubeArrowReader::GetNext] RecordBatch message: "
            "continuation=0x%x, size=%u\n",
            continuation, msg_size);

  if (continuation != ARROW_IPC_MAGIC) {
    // Might be EOS marker (0xFFFFFFFF 0x00000000)
    if (continuation == ARROW_IPC_MAGIC && msg_size == 0) {
      DEBUG_LOG("[CubeArrowReader::GetNext] Found EOS marker\n");
      finished_ = true;
      return ENOMSG;
    }
    DEBUG_LOG("[CubeArrowReader::GetNext] Invalid continuation marker: 0x%x\n",
              continuation);
    finished_ = true;
    return ENOMSG;
  }

  // Parse RecordBatch message using FlatBuffers
  DEBUG_LOG("[CubeArrowReader::GetNext] Parsing RecordBatch FlatBuffer\n");

  int64_t metadata_size = 8 + msg_size;
  int64_t body_offset = offset_ + metadata_size;
  if (body_offset % 8 != 0) {
    body_offset += 8 - (body_offset % 8);
  }

  const uint8_t *body_data = buffer_.data() + body_offset;
  int64_t body_size = buffer_.size() - body_offset;

  auto status =
      ParseRecordBatchFlatBuffer(buffer_.data() + offset_ + 8, msg_size,
                                 body_data, body_size, out, nullptr);

  if (status != NANOARROW_OK) {
    DEBUG_LOG("[CubeArrowReader::GetNext] Batch parsing failed\n");
    return status;
  }

  finished_ = true;
  DEBUG_LOG("[CubeArrowReader::GetNext] Successfully parsed RecordBatch\n");
  return NANOARROW_OK;
}

ArrowErrorCode CubeArrowReader::ParseMessage(ArrowError *error) {
  DEBUG_LOG(
      "[CubeArrowReader::ParseMessage] offset_=%lld, buffer_.size()=%zu\n",
      (long long)offset_, buffer_.size());

  if (offset_ >= static_cast<int64_t>(buffer_.size())) {
    DEBUG_LOG(
        "[CubeArrowReader::ParseMessage] Offset past end, setting finished\n");
    finished_ = true;
    return ENOMSG;
  }

  // Read message header
  if (offset_ + 8 > static_cast<int64_t>(buffer_.size())) {
    if (error) {
      ArrowErrorSet(error, "Incomplete message header");
    }
    finished_ = true;
    return ENOMSG;
  }

  const uint8_t *header = buffer_.data() + offset_;
  int32_t message_length = ReadLE32Signed(header);

  // Message length should be positive
  if (message_length <= 0) {
    if (error) {
      ArrowErrorSet(error, "Invalid message length: %d", message_length);
    }
    finished_ = true;
    return ENOMSG;
  }

  int32_t message_type = ReadLE32Signed(header + 4);
  const uint8_t *message_data = header + 8;

  if (offset_ + 8 + message_length > static_cast<int64_t>(buffer_.size())) {
    if (error) {
      ArrowErrorSet(error, "Message extends past buffer end");
    }
    finished_ = true;
    return ENOMSG;
  }

  offset_ += 8 + message_length;

  // Route based on message type
  if (message_type == ARROW_IPC_SCHEMA_MESSAGE_TYPE) {
    return ParseSchemaMessage(message_data, message_length, error);
  } else if (message_type == ARROW_IPC_RECORD_BATCH_MESSAGE_TYPE) {
    // For now, return empty array - would need full FlatBuffer parsing
    // This is a simplified implementation
    finished_ = true;
    return ENOMSG;
  } else {
    if (error) {
      ArrowErrorSet(error, "Unknown message type: %d", message_type);
    }
    return EINVAL;
  }
}

ArrowErrorCode CubeArrowReader::ParseSchemaMessage(const uint8_t *message_data,
                                                   int64_t message_length,
                                                   ArrowError *error) {
  // Simplified: just mark schema as initialized
  // In a full implementation, would parse FlatBuffer to get real schema
  schema_initialized_ = true;

  // For now, return a minimal schema
  // This allows the driver to compile and function at basic level
  // Full FlatBuffer parsing would go here
  return NANOARROW_OK;
}

ArrowErrorCode
CubeArrowReader::ParseRecordBatchMessage(const uint8_t *message_data,
                                         int64_t message_length,
                                         ArrowArray *out, ArrowError *error) {
  // Simplified: return empty array
  // In a full implementation, would parse FlatBuffer to get batch data
  return NANOARROW_OK;
}

// Static helper for bit access
bool CubeArrowReader::GetBit(const uint8_t *bitmap, int64_t index) {
  return ::adbc::cube::GetBit(bitmap, index);
}

// Map FlatBuffer Type enum to nanoarrow type
int CubeArrowReader::MapFlatBufferTypeToArrow(int fb_type) {
  switch (fb_type) {
  case org::apache::arrow::flatbuf::Type_Int:
    return NANOARROW_TYPE_INT64; // Assume INT64 for now
  case org::apache::arrow::flatbuf::Type_FloatingPoint:
    return NANOARROW_TYPE_DOUBLE;
  case org::apache::arrow::flatbuf::Type_Bool:
    return NANOARROW_TYPE_BOOL;
  case org::apache::arrow::flatbuf::Type_Utf8:
    return NANOARROW_TYPE_STRING;
  case org::apache::arrow::flatbuf::Type_Binary:
    return NANOARROW_TYPE_BINARY;
  case org::apache::arrow::flatbuf::Type_Date:
    return NANOARROW_TYPE_DATE32; // Default to DATE32
  case org::apache::arrow::flatbuf::Type_Time:
    return NANOARROW_TYPE_TIME64; // Default to TIME64
  case org::apache::arrow::flatbuf::Type_Timestamp:
    return NANOARROW_TYPE_TIMESTAMP; // Default to TIMESTAMP
  default:
    DEBUG_LOG("[MapFlatBufferTypeToArrow] Unsupported type: %d\n", fb_type);
    return NANOARROW_TYPE_UNINITIALIZED;
  }
}

// Get number of buffers needed for a type
int CubeArrowReader::GetBufferCountForType(int arrow_type) {
  switch (arrow_type) {
  case NANOARROW_TYPE_BOOL:
  case NANOARROW_TYPE_INT64:
  case NANOARROW_TYPE_DOUBLE:
  case NANOARROW_TYPE_DATE32:
  case NANOARROW_TYPE_DATE64:
  case NANOARROW_TYPE_TIME64:
  case NANOARROW_TYPE_TIMESTAMP:
    return 2; // validity + data
  case NANOARROW_TYPE_STRING:
  case NANOARROW_TYPE_BINARY:
    return 3; // validity + offsets + data
  default:
    return 2;
  }
}

// Extract buffer from RecordBatch FlatBuffer
void CubeArrowReader::ExtractBuffer(
    const org::apache::arrow::flatbuf::RecordBatch *batch, int buffer_index,
    const uint8_t *body_data, const uint8_t **out_ptr, int64_t *out_size) {

  if (!batch || !batch->buffers() ||
      buffer_index >= static_cast<int>(batch->buffers()->size())) {
    *out_ptr = nullptr;
    *out_size = 0;
    return;
  }

  auto buffer_meta = batch->buffers()->Get(buffer_index);
  if (!buffer_meta) {
    *out_ptr = nullptr;
    *out_size = 0;
    return;
  }

  int64_t offset = buffer_meta->offset();
  int64_t length = buffer_meta->length();

  *out_ptr = body_data + offset;
  *out_size = length;
}

// Parse Schema FlatBuffer message
ArrowErrorCode CubeArrowReader::ParseSchemaFlatBuffer(const uint8_t *fb_data,
                                                      int64_t fb_size,
                                                      ArrowError *error) {

  // Verify FlatBuffer
  flatbuffers::Verifier verifier(fb_data, fb_size);
  if (!::org::apache::arrow::flatbuf::VerifyMessageBuffer(verifier)) {
    ArrowErrorSet(error, "Invalid Schema FlatBuffer");
    return EINVAL;
  }

  auto message = ::org::apache::arrow::flatbuf::GetMessage(fb_data);
  if (!message || message->header_type() !=
                      ::org::apache::arrow::flatbuf::MessageHeader_Schema) {
    ArrowErrorSet(error, "Not a Schema message");
    return EINVAL;
  }

  auto schema = message->header_as_Schema();
  if (!schema || !schema->fields()) {
    ArrowErrorSet(error, "Invalid schema structure");
    return EINVAL;
  }

  // Clear previous metadata
  field_names_.clear();
  field_types_.clear();
  field_nullable_.clear();

  // Extract field metadata
  for (unsigned int i = 0; i < schema->fields()->size(); i++) {
    auto field = schema->fields()->Get(i);
    if (!field)
      continue;

    std::string name = field->name() ? field->name()->str() : "";
    field_names_.push_back(name);
    field_nullable_.push_back(field->nullable());

    int arrow_type = MapFlatBufferTypeToArrow(field->type_type());
    field_types_.push_back(arrow_type);

    DEBUG_LOG(
        "[ParseSchemaFlatBuffer] Field %u: name='%s', type=%d, nullable=%d\n",
        i, name.c_str(), arrow_type, field->nullable());
  }

  // Build nanoarrow schema
  ArrowSchemaInit(&schema_);
  auto status = ArrowSchemaSetTypeStruct(&schema_, field_names_.size());
  if (status != NANOARROW_OK) {
    ArrowErrorSet(error, "Failed to create struct schema");
    return status;
  }

  for (size_t i = 0; i < field_names_.size(); i++) {
    struct ArrowSchema *child = schema_.children[i];
    ArrowType arrow_type = static_cast<ArrowType>(field_types_[i]);

    // Use ArrowSchemaSetTypeDateTime for temporal types that require time units
    if (arrow_type == NANOARROW_TYPE_TIMESTAMP) {
      // Default to microsecond precision with no timezone
      status = ArrowSchemaSetTypeDateTime(child, NANOARROW_TYPE_TIMESTAMP,
                                          NANOARROW_TIME_UNIT_MICRO, NULL);
    } else if (arrow_type == NANOARROW_TYPE_TIME64) {
      // TIME64 uses microsecond or nanosecond
      status = ArrowSchemaSetTypeDateTime(child, NANOARROW_TYPE_TIME64,
                                          NANOARROW_TIME_UNIT_MICRO, NULL);
    } else {
      // Regular types including DATE32, DATE64
      status = ArrowSchemaSetType(child, arrow_type);
    }

    if (status != NANOARROW_OK) {
      ArrowErrorSet(error, "Failed to set child type");
      ArrowSchemaRelease(&schema_);
      return status;
    }

    status = ArrowSchemaSetName(child, field_names_[i].c_str());
    if (status != NANOARROW_OK) {
      ArrowErrorSet(error, "Failed to set child name");
      ArrowSchemaRelease(&schema_);
      return status;
    }

    if (!field_nullable_[i]) {
      child->flags &= ~ARROW_FLAG_NULLABLE;
    }
  }

  schema_initialized_ = true;
  DEBUG_LOG("[ParseSchemaFlatBuffer] Schema parsed: %zu fields\n",
            field_names_.size());
  return NANOARROW_OK;
}

// Parse RecordBatch FlatBuffer message
ArrowErrorCode CubeArrowReader::ParseRecordBatchFlatBuffer(
    const uint8_t *fb_data, int64_t fb_size, const uint8_t *body_data,
    int64_t body_size, ArrowArray *out, ArrowError *error) {

  // Verify FlatBuffer
  flatbuffers::Verifier verifier(fb_data, fb_size);
  if (!::org::apache::arrow::flatbuf::VerifyMessageBuffer(verifier)) {
    ArrowErrorSet(error, "Invalid RecordBatch FlatBuffer");
    return EINVAL;
  }

  auto message = ::org::apache::arrow::flatbuf::GetMessage(fb_data);
  if (!message ||
      message->header_type() !=
          ::org::apache::arrow::flatbuf::MessageHeader_RecordBatch) {
    ArrowErrorSet(error, "Not a RecordBatch message");
    return EINVAL;
  }

  auto batch = message->header_as_RecordBatch();
  if (!batch) {
    ArrowErrorSet(error, "Invalid batch structure");
    return EINVAL;
  }

  int64_t row_count = batch->length();
  DEBUG_LOG("[ParseRecordBatchFlatBuffer] Batch has %lld rows, %zu columns\n",
            (long long)row_count, field_names_.size());

  // Create struct array
  auto status = ArrowArrayInitFromType(out, NANOARROW_TYPE_STRUCT);
  if (status != NANOARROW_OK) {
    ArrowErrorSet(error, "Failed to init struct array");
    return status;
  }

  status = ArrowArrayAllocateChildren(out, field_names_.size());
  if (status != NANOARROW_OK) {
    ArrowErrorSet(error, "Failed to allocate children");
    ArrowArrayRelease(out);
    return status;
  }

  // Build array for each field
  int buffer_index = 0;
  for (size_t i = 0; i < field_names_.size(); i++) {
    struct ArrowArray *child = out->children[i];
    status = BuildArrayForField(i, row_count, batch, body_data, &buffer_index,
                                child, error);
    if (status != NANOARROW_OK) {
      DEBUG_LOG("[ParseRecordBatchFlatBuffer] Failed to build field %zu\n", i);
      ArrowArrayRelease(out);
      return status;
    }
  }

  // Set struct array length
  out->length = row_count;
  out->null_count = 0;

  DEBUG_LOG("[ParseRecordBatchFlatBuffer] Successfully parsed batch\n");
  return NANOARROW_OK;
}

// Build array for a specific field (type-specific handling)
ArrowErrorCode CubeArrowReader::BuildArrayForField(
    int field_index, int64_t row_count,
    const org::apache::arrow::flatbuf::RecordBatch *batch,
    const uint8_t *body_data, int *buffer_index_inout, ArrowArray *out,
    ArrowError *error) {

  if (field_index < 0 || field_index >= static_cast<int>(field_types_.size())) {
    ArrowErrorSet(error, "Invalid field index: %d", field_index);
    return EINVAL;
  }

  int arrow_type = field_types_[field_index];
  // int buffer_count = GetBufferCountForType(arrow_type);  // Unused for now

  // Extract validity buffer
  const uint8_t *validity_buffer = nullptr;
  int64_t validity_size = 0;
  ExtractBuffer(batch, *buffer_index_inout, body_data, &validity_buffer,
                &validity_size);
  (*buffer_index_inout)++;

  // Initialize array for this type
  auto status = ArrowArrayInitFromType(out, static_cast<ArrowType>(arrow_type));
  if (status != NANOARROW_OK) {
    ArrowErrorSet(error, "Failed to init array for type %d", arrow_type);
    return status;
  }

  status = ArrowArrayStartAppending(out);
  if (status != NANOARROW_OK) {
    ArrowErrorSet(error, "Failed to start appending");
    ArrowArrayRelease(out);
    return status;
  }

  // Type-specific data extraction
  switch (arrow_type) {
  case NANOARROW_TYPE_INT8: {
    const uint8_t *data_buffer = nullptr;
    int64_t data_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &data_buffer,
                  &data_size);
    (*buffer_index_inout)++;

    const int8_t *values = reinterpret_cast<const int8_t *>(data_buffer);
    for (int64_t i = 0; i < row_count; i++) {
      bool is_valid = !validity_buffer || GetBit(validity_buffer, i);
      if (is_valid) {
        status = ArrowArrayAppendInt(out, values[i]);
      } else {
        status = ArrowArrayAppendNull(out, 1);
      }
      if (status != NANOARROW_OK) {
        ArrowArrayRelease(out);
        return status;
      }
    }
    break;
  }

  case NANOARROW_TYPE_INT16: {
    const uint8_t *data_buffer = nullptr;
    int64_t data_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &data_buffer,
                  &data_size);
    (*buffer_index_inout)++;

    const int16_t *values = reinterpret_cast<const int16_t *>(data_buffer);
    for (int64_t i = 0; i < row_count; i++) {
      bool is_valid = !validity_buffer || GetBit(validity_buffer, i);
      if (is_valid) {
        status = ArrowArrayAppendInt(out, values[i]);
      } else {
        status = ArrowArrayAppendNull(out, 1);
      }
      if (status != NANOARROW_OK) {
        ArrowArrayRelease(out);
        return status;
      }
    }
    break;
  }

  case NANOARROW_TYPE_INT32: {
    const uint8_t *data_buffer = nullptr;
    int64_t data_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &data_buffer,
                  &data_size);
    (*buffer_index_inout)++;

    const int32_t *values = reinterpret_cast<const int32_t *>(data_buffer);
    for (int64_t i = 0; i < row_count; i++) {
      bool is_valid = !validity_buffer || GetBit(validity_buffer, i);
      if (is_valid) {
        status = ArrowArrayAppendInt(out, values[i]);
      } else {
        status = ArrowArrayAppendNull(out, 1);
      }
      if (status != NANOARROW_OK) {
        ArrowArrayRelease(out);
        return status;
      }
    }
    break;
  }

  case NANOARROW_TYPE_INT64: {
    const uint8_t *data_buffer = nullptr;
    int64_t data_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &data_buffer,
                  &data_size);
    (*buffer_index_inout)++;

    const int64_t *values = reinterpret_cast<const int64_t *>(data_buffer);
    for (int64_t i = 0; i < row_count; i++) {
      bool is_valid = !validity_buffer || GetBit(validity_buffer, i);
      if (is_valid) {
        status = ArrowArrayAppendInt(out, values[i]);
      } else {
        status = ArrowArrayAppendNull(out, 1);
      }
      if (status != NANOARROW_OK) {
        ArrowArrayRelease(out);
        return status;
      }
    }
    break;
  }

  case NANOARROW_TYPE_UINT8: {
    const uint8_t *data_buffer = nullptr;
    int64_t data_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &data_buffer,
                  &data_size);
    (*buffer_index_inout)++;

    const uint8_t *values = data_buffer;
    for (int64_t i = 0; i < row_count; i++) {
      bool is_valid = !validity_buffer || GetBit(validity_buffer, i);
      if (is_valid) {
        status = ArrowArrayAppendUInt(out, values[i]);
      } else {
        status = ArrowArrayAppendNull(out, 1);
      }
      if (status != NANOARROW_OK) {
        ArrowArrayRelease(out);
        return status;
      }
    }
    break;
  }

  case NANOARROW_TYPE_UINT16: {
    const uint8_t *data_buffer = nullptr;
    int64_t data_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &data_buffer,
                  &data_size);
    (*buffer_index_inout)++;

    const uint16_t *values = reinterpret_cast<const uint16_t *>(data_buffer);
    for (int64_t i = 0; i < row_count; i++) {
      bool is_valid = !validity_buffer || GetBit(validity_buffer, i);
      if (is_valid) {
        status = ArrowArrayAppendUInt(out, values[i]);
      } else {
        status = ArrowArrayAppendNull(out, 1);
      }
      if (status != NANOARROW_OK) {
        ArrowArrayRelease(out);
        return status;
      }
    }
    break;
  }

  case NANOARROW_TYPE_UINT32: {
    const uint8_t *data_buffer = nullptr;
    int64_t data_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &data_buffer,
                  &data_size);
    (*buffer_index_inout)++;

    const uint32_t *values = reinterpret_cast<const uint32_t *>(data_buffer);
    for (int64_t i = 0; i < row_count; i++) {
      bool is_valid = !validity_buffer || GetBit(validity_buffer, i);
      if (is_valid) {
        status = ArrowArrayAppendUInt(out, values[i]);
      } else {
        status = ArrowArrayAppendNull(out, 1);
      }
      if (status != NANOARROW_OK) {
        ArrowArrayRelease(out);
        return status;
      }
    }
    break;
  }

  case NANOARROW_TYPE_UINT64: {
    const uint8_t *data_buffer = nullptr;
    int64_t data_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &data_buffer,
                  &data_size);
    (*buffer_index_inout)++;

    const uint64_t *values = reinterpret_cast<const uint64_t *>(data_buffer);
    for (int64_t i = 0; i < row_count; i++) {
      bool is_valid = !validity_buffer || GetBit(validity_buffer, i);
      if (is_valid) {
        status = ArrowArrayAppendUInt(out, values[i]);
      } else {
        status = ArrowArrayAppendNull(out, 1);
      }
      if (status != NANOARROW_OK) {
        ArrowArrayRelease(out);
        return status;
      }
    }
    break;
  }

  case NANOARROW_TYPE_FLOAT: {
    const uint8_t *data_buffer = nullptr;
    int64_t data_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &data_buffer,
                  &data_size);
    (*buffer_index_inout)++;

    const float *values = reinterpret_cast<const float *>(data_buffer);
    for (int64_t i = 0; i < row_count; i++) {
      bool is_valid = !validity_buffer || GetBit(validity_buffer, i);
      if (is_valid) {
        status = ArrowArrayAppendDouble(out, static_cast<double>(values[i]));
      } else {
        status = ArrowArrayAppendNull(out, 1);
      }
      if (status != NANOARROW_OK) {
        ArrowArrayRelease(out);
        return status;
      }
    }
    break;
  }

  case NANOARROW_TYPE_DOUBLE: {
    const uint8_t *data_buffer = nullptr;
    int64_t data_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &data_buffer,
                  &data_size);
    (*buffer_index_inout)++;

    const double *values = reinterpret_cast<const double *>(data_buffer);
    for (int64_t i = 0; i < row_count; i++) {
      bool is_valid = !validity_buffer || GetBit(validity_buffer, i);
      if (is_valid) {
        status = ArrowArrayAppendDouble(out, values[i]);
      } else {
        status = ArrowArrayAppendNull(out, 1);
      }
      if (status != NANOARROW_OK) {
        ArrowArrayRelease(out);
        return status;
      }
    }
    break;
  }

  case NANOARROW_TYPE_BOOL: {
    const uint8_t *data_buffer = nullptr;
    int64_t data_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &data_buffer,
                  &data_size);
    (*buffer_index_inout)++;

    for (int64_t i = 0; i < row_count; i++) {
      bool is_valid = !validity_buffer || GetBit(validity_buffer, i);
      if (is_valid) {
        bool value = GetBit(data_buffer, i);
        status = ArrowArrayAppendInt(out, value ? 1 : 0);
      } else {
        status = ArrowArrayAppendNull(out, 1);
      }
      if (status != NANOARROW_OK) {
        ArrowArrayRelease(out);
        return status;
      }
    }
    break;
  }

  case NANOARROW_TYPE_STRING: {
    const uint8_t *offsets_buffer = nullptr;
    int64_t offsets_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &offsets_buffer,
                  &offsets_size);
    (*buffer_index_inout)++;

    const uint8_t *data_buffer = nullptr;
    int64_t data_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &data_buffer,
                  &data_size);
    (*buffer_index_inout)++;

    const int32_t *offsets = reinterpret_cast<const int32_t *>(offsets_buffer);
    for (int64_t i = 0; i < row_count; i++) {
      bool is_valid = !validity_buffer || GetBit(validity_buffer, i);
      if (is_valid) {
        int32_t start = offsets[i];
        int32_t end = offsets[i + 1];
        int32_t length = end - start;
        struct ArrowStringView view;
        view.data = reinterpret_cast<const char *>(data_buffer + start);
        view.size_bytes = length;
        status = ArrowArrayAppendString(out, view);
      } else {
        status = ArrowArrayAppendNull(out, 1);
      }
      if (status != NANOARROW_OK) {
        ArrowArrayRelease(out);
        return status;
      }
    }
    break;
  }

  case NANOARROW_TYPE_DATE32: {
    const uint8_t *data_buffer = nullptr;
    int64_t data_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &data_buffer,
                  &data_size);
    (*buffer_index_inout)++;

    const int32_t *values = reinterpret_cast<const int32_t *>(data_buffer);
    for (int64_t i = 0; i < row_count; i++) {
      bool is_valid = !validity_buffer || GetBit(validity_buffer, i);
      if (is_valid) {
        status = ArrowArrayAppendInt(out, values[i]);
      } else {
        status = ArrowArrayAppendNull(out, 1);
      }
      if (status != NANOARROW_OK) {
        ArrowArrayRelease(out);
        return status;
      }
    }
    break;
  }

  case NANOARROW_TYPE_DATE64: {
    const uint8_t *data_buffer = nullptr;
    int64_t data_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &data_buffer,
                  &data_size);
    (*buffer_index_inout)++;

    const int64_t *values = reinterpret_cast<const int64_t *>(data_buffer);
    for (int64_t i = 0; i < row_count; i++) {
      bool is_valid = !validity_buffer || GetBit(validity_buffer, i);
      if (is_valid) {
        status = ArrowArrayAppendInt(out, values[i]);
      } else {
        status = ArrowArrayAppendNull(out, 1);
      }
      if (status != NANOARROW_OK) {
        ArrowArrayRelease(out);
        return status;
      }
    }
    break;
  }

  case NANOARROW_TYPE_TIME64: {
    const uint8_t *data_buffer = nullptr;
    int64_t data_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &data_buffer,
                  &data_size);
    (*buffer_index_inout)++;

    const int64_t *values = reinterpret_cast<const int64_t *>(data_buffer);
    for (int64_t i = 0; i < row_count; i++) {
      bool is_valid = !validity_buffer || GetBit(validity_buffer, i);
      if (is_valid) {
        status = ArrowArrayAppendInt(out, values[i]);
      } else {
        status = ArrowArrayAppendNull(out, 1);
      }
      if (status != NANOARROW_OK) {
        ArrowArrayRelease(out);
        return status;
      }
    }
    break;
  }

  case NANOARROW_TYPE_TIMESTAMP: {
    const uint8_t *data_buffer = nullptr;
    int64_t data_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &data_buffer,
                  &data_size);
    (*buffer_index_inout)++;

    const int64_t *values = reinterpret_cast<const int64_t *>(data_buffer);
    for (int64_t i = 0; i < row_count; i++) {
      bool is_valid = !validity_buffer || GetBit(validity_buffer, i);
      if (is_valid) {
        status = ArrowArrayAppendInt(out, values[i]);
      } else {
        status = ArrowArrayAppendNull(out, 1);
      }
      if (status != NANOARROW_OK) {
        ArrowArrayRelease(out);
        return status;
      }
    }
    break;
  }

  case NANOARROW_TYPE_BINARY: {
    const uint8_t *offsets_buffer = nullptr;
    int64_t offsets_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &offsets_buffer,
                  &offsets_size);
    (*buffer_index_inout)++;

    const uint8_t *data_buffer = nullptr;
    int64_t data_size = 0;
    ExtractBuffer(batch, *buffer_index_inout, body_data, &data_buffer,
                  &data_size);
    (*buffer_index_inout)++;

    const int32_t *offsets = reinterpret_cast<const int32_t *>(offsets_buffer);
    for (int64_t i = 0; i < row_count; i++) {
      bool is_valid = !validity_buffer || GetBit(validity_buffer, i);
      if (is_valid) {
        int32_t start = offsets[i];
        int32_t end = offsets[i + 1];
        int32_t length = end - start;
        struct ArrowBufferView view;
        view.data.as_uint8 = data_buffer + start;
        view.size_bytes = length;
        status = ArrowArrayAppendBytes(out, view);
      } else {
        status = ArrowArrayAppendNull(out, 1);
      }
      if (status != NANOARROW_OK) {
        ArrowArrayRelease(out);
        return status;
      }
    }
    break;
  }

  default:
    ArrowErrorSet(error, "Unsupported Arrow type: %d", arrow_type);
    ArrowArrayRelease(out);
    return EINVAL;
  }

  status = ArrowArrayFinishBuildingDefault(out, error);
  if (status != NANOARROW_OK) {
    ArrowArrayRelease(out);
    return status;
  }

  return NANOARROW_OK;
}

// Arrow stream callbacks
static int CubeArrowStreamGetSchema(struct ArrowArrayStream *stream,
                                    struct ArrowSchema *out) {
  DEBUG_LOG("[CubeArrowStreamGetSchema] Called\n");
  auto *reader = static_cast<CubeArrowReader *>(stream->private_data);
  DEBUG_LOG("[CubeArrowStreamGetSchema] Reader pointer: %p\n",
            static_cast<void *>(reader));
  auto status = reader->GetSchema(out);
  DEBUG_LOG("[CubeArrowStreamGetSchema] Returning status: %d\n", status);
  return status;
}

static int CubeArrowStreamGetNext(struct ArrowArrayStream *stream,
                                  struct ArrowArray *out) {
  DEBUG_LOG("[CubeArrowStreamGetNext] Called\n");
  auto *reader = static_cast<CubeArrowReader *>(stream->private_data);
  DEBUG_LOG("[CubeArrowStreamGetNext] Reader pointer: %p\n",
            static_cast<void *>(reader));
  auto status = reader->GetNext(out);
  DEBUG_LOG("[CubeArrowStreamGetNext] Status: %d\n", status);
  if (status == ENOMSG) {
    // End of stream - return success with null array
    out->release = nullptr;
    DEBUG_LOG("[CubeArrowStreamGetNext] End of stream\n");
    return NANOARROW_OK;
  }
  DEBUG_LOG("[CubeArrowStreamGetNext] Returning status: %d\n", status);
  return status;
}

static const char *
CubeArrowStreamGetLastError(struct ArrowArrayStream *stream) {
  return "Error accessing Cube Arrow stream";
}

static void CubeArrowStreamRelease(struct ArrowArrayStream *stream) {
  if (stream->private_data != nullptr) {
    auto *reader = static_cast<CubeArrowReader *>(stream->private_data);
    delete reader;
    stream->private_data = nullptr;
  }
  stream->release = nullptr;
}

void CubeArrowReader::ExportTo(struct ArrowArrayStream *stream) {
  stream->get_schema = CubeArrowStreamGetSchema;
  stream->get_next = CubeArrowStreamGetNext;
  stream->get_last_error = CubeArrowStreamGetLastError;
  stream->release = CubeArrowStreamRelease;
  stream->private_data = this;
}

} // namespace adbc::cube
