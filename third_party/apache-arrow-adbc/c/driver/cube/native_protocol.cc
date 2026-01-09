#include "native_protocol.h"

#include <arpa/inet.h> // for htonl, ntohl
#include <cstring>
#include <stdexcept>

namespace adbc::cube {

// Helper functions implementation
void MessageCodec::PutU32(std::vector<uint8_t> &buf, uint32_t value) {
  uint32_t net_value = htonl(value);
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&net_value);
  buf.insert(buf.end(), bytes, bytes + 4);
}

void MessageCodec::PutI64(std::vector<uint8_t> &buf, int64_t value) {
  // Convert to network byte order (big-endian)
  uint64_t net_value = static_cast<uint64_t>(value);
  for (int i = 7; i >= 0; --i) {
    buf.push_back(static_cast<uint8_t>((net_value >> (i * 8)) & 0xFF));
  }
}

void MessageCodec::PutU8(std::vector<uint8_t> &buf, uint8_t value) {
  buf.push_back(value);
}

void MessageCodec::PutString(std::vector<uint8_t> &buf,
                             const std::string &str) {
  PutU32(buf, static_cast<uint32_t>(str.length()));
  buf.insert(buf.end(), str.begin(), str.end());
}

void MessageCodec::PutOptionalString(std::vector<uint8_t> &buf,
                                     const std::string &str) {
  if (!str.empty()) {
    PutU8(buf, 1);
    PutString(buf, str);
  } else {
    PutU8(buf, 0);
  }
}

void MessageCodec::PutBytes(std::vector<uint8_t> &buf,
                            const std::vector<uint8_t> &bytes) {
  PutU32(buf, static_cast<uint32_t>(bytes.size()));
  buf.insert(buf.end(), bytes.begin(), bytes.end());
}

uint32_t MessageCodec::GetU32(const uint8_t *&ptr, const uint8_t *end) {
  if (ptr + 4 > end)
    throw std::runtime_error("Insufficient data for U32");
  uint32_t net_value;
  std::memcpy(&net_value, ptr, 4);
  ptr += 4;
  return ntohl(net_value);
}

int64_t MessageCodec::GetI64(const uint8_t *&ptr, const uint8_t *end) {
  if (ptr + 8 > end)
    throw std::runtime_error("Insufficient data for I64");
  uint64_t value = 0;
  for (int i = 0; i < 8; ++i) {
    value = (value << 8) | ptr[i];
  }
  ptr += 8;
  return static_cast<int64_t>(value);
}

uint8_t MessageCodec::GetU8(const uint8_t *&ptr, const uint8_t *end) {
  if (ptr >= end)
    throw std::runtime_error("Insufficient data for U8");
  return *ptr++;
}

std::string MessageCodec::GetString(const uint8_t *&ptr, const uint8_t *end) {
  uint32_t length = GetU32(ptr, end);
  if (ptr + length > end)
    throw std::runtime_error("Insufficient data for string");
  std::string result(reinterpret_cast<const char *>(ptr), length);
  ptr += length;
  return result;
}

std::string MessageCodec::GetOptionalString(const uint8_t *&ptr,
                                            const uint8_t *end) {
  uint8_t has_value = GetU8(ptr, end);
  if (has_value) {
    return GetString(ptr, end);
  }
  return "";
}

std::vector<uint8_t> MessageCodec::GetBytes(const uint8_t *&ptr,
                                            const uint8_t *end) {
  uint32_t length = GetU32(ptr, end);
  if (ptr + length > end)
    throw std::runtime_error("Insufficient data for bytes");
  std::vector<uint8_t> result(ptr, ptr + length);
  ptr += length;
  return result;
}

// Message implementations

std::vector<uint8_t> HandshakeRequest::Encode() const {
  std::vector<uint8_t> payload;
  MessageCodec::PutU8(payload, static_cast<uint8_t>(GetType()));
  MessageCodec::PutU32(payload, version);

  std::vector<uint8_t> result;
  MessageCodec::PutU32(result, static_cast<uint32_t>(payload.size()));
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}

std::vector<uint8_t> HandshakeResponse::Encode() const {
  std::vector<uint8_t> payload;
  MessageCodec::PutU8(payload, static_cast<uint8_t>(GetType()));
  MessageCodec::PutU32(payload, version);
  MessageCodec::PutString(payload, server_version);

  std::vector<uint8_t> result;
  MessageCodec::PutU32(result, static_cast<uint32_t>(payload.size()));
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}

std::unique_ptr<HandshakeResponse>
HandshakeResponse::Decode(const uint8_t *data, size_t length) {
  auto response = std::make_unique<HandshakeResponse>();
  const uint8_t *ptr = data;
  const uint8_t *end = data + length;

  uint8_t msg_type = MessageCodec::GetU8(ptr, end);
  if (msg_type != static_cast<uint8_t>(MessageType::HandshakeResponse)) {
    throw std::runtime_error("Invalid message type for HandshakeResponse");
  }

  response->version = MessageCodec::GetU32(ptr, end);
  response->server_version = MessageCodec::GetString(ptr, end);

  return response;
}

std::vector<uint8_t> AuthRequest::Encode() const {
  std::vector<uint8_t> payload;
  MessageCodec::PutU8(payload, static_cast<uint8_t>(GetType()));
  MessageCodec::PutString(payload, token);
  MessageCodec::PutOptionalString(payload, database);

  std::vector<uint8_t> result;
  MessageCodec::PutU32(result, static_cast<uint32_t>(payload.size()));
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}

std::vector<uint8_t> AuthResponse::Encode() const {
  std::vector<uint8_t> payload;
  MessageCodec::PutU8(payload, static_cast<uint8_t>(GetType()));
  MessageCodec::PutU8(payload, success ? 1 : 0);
  MessageCodec::PutString(payload, session_id);

  std::vector<uint8_t> result;
  MessageCodec::PutU32(result, static_cast<uint32_t>(payload.size()));
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}

std::unique_ptr<AuthResponse> AuthResponse::Decode(const uint8_t *data,
                                                   size_t length) {
  auto response = std::make_unique<AuthResponse>();
  const uint8_t *ptr = data;
  const uint8_t *end = data + length;

  uint8_t msg_type = MessageCodec::GetU8(ptr, end);
  if (msg_type != static_cast<uint8_t>(MessageType::AuthResponse)) {
    throw std::runtime_error("Invalid message type for AuthResponse");
  }

  response->success = MessageCodec::GetU8(ptr, end) != 0;
  response->session_id = MessageCodec::GetString(ptr, end);

  return response;
}

std::vector<uint8_t> QueryRequest::Encode() const {
  std::vector<uint8_t> payload;
  MessageCodec::PutU8(payload, static_cast<uint8_t>(GetType()));
  MessageCodec::PutString(payload, sql);

  std::vector<uint8_t> result;
  MessageCodec::PutU32(result, static_cast<uint32_t>(payload.size()));
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}

std::vector<uint8_t> QueryResponseSchema::Encode() const {
  std::vector<uint8_t> payload;
  MessageCodec::PutU8(payload, static_cast<uint8_t>(GetType()));
  MessageCodec::PutBytes(payload, arrow_ipc_schema);

  std::vector<uint8_t> result;
  MessageCodec::PutU32(result, static_cast<uint32_t>(payload.size()));
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}

std::unique_ptr<QueryResponseSchema>
QueryResponseSchema::Decode(const uint8_t *data, size_t length) {
  auto response = std::make_unique<QueryResponseSchema>();
  const uint8_t *ptr = data;
  const uint8_t *end = data + length;

  uint8_t msg_type = MessageCodec::GetU8(ptr, end);
  if (msg_type != static_cast<uint8_t>(MessageType::QueryResponseSchema)) {
    throw std::runtime_error("Invalid message type for QueryResponseSchema");
  }

  response->arrow_ipc_schema = MessageCodec::GetBytes(ptr, end);

  return response;
}

std::vector<uint8_t> QueryResponseBatch::Encode() const {
  std::vector<uint8_t> payload;
  MessageCodec::PutU8(payload, static_cast<uint8_t>(GetType()));
  MessageCodec::PutBytes(payload, arrow_ipc_batch);

  std::vector<uint8_t> result;
  MessageCodec::PutU32(result, static_cast<uint32_t>(payload.size()));
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}

std::unique_ptr<QueryResponseBatch>
QueryResponseBatch::Decode(const uint8_t *data, size_t length) {
  auto response = std::make_unique<QueryResponseBatch>();
  const uint8_t *ptr = data;
  const uint8_t *end = data + length;

  uint8_t msg_type = MessageCodec::GetU8(ptr, end);
  if (msg_type != static_cast<uint8_t>(MessageType::QueryResponseBatch)) {
    throw std::runtime_error("Invalid message type for QueryResponseBatch");
  }

  response->arrow_ipc_batch = MessageCodec::GetBytes(ptr, end);

  return response;
}

std::vector<uint8_t> QueryComplete::Encode() const {
  std::vector<uint8_t> payload;
  MessageCodec::PutU8(payload, static_cast<uint8_t>(GetType()));
  MessageCodec::PutI64(payload, rows_affected);

  std::vector<uint8_t> result;
  MessageCodec::PutU32(result, static_cast<uint32_t>(payload.size()));
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}

std::unique_ptr<QueryComplete> QueryComplete::Decode(const uint8_t *data,
                                                     size_t length) {
  auto response = std::make_unique<QueryComplete>();
  const uint8_t *ptr = data;
  const uint8_t *end = data + length;

  uint8_t msg_type = MessageCodec::GetU8(ptr, end);
  if (msg_type != static_cast<uint8_t>(MessageType::QueryComplete)) {
    throw std::runtime_error("Invalid message type for QueryComplete");
  }

  response->rows_affected = MessageCodec::GetI64(ptr, end);

  return response;
}

std::vector<uint8_t> ErrorMessage::Encode() const {
  std::vector<uint8_t> payload;
  MessageCodec::PutU8(payload, static_cast<uint8_t>(GetType()));
  MessageCodec::PutString(payload, code);
  MessageCodec::PutString(payload, message);

  std::vector<uint8_t> result;
  MessageCodec::PutU32(result, static_cast<uint32_t>(payload.size()));
  result.insert(result.end(), payload.begin(), payload.end());
  return result;
}

std::unique_ptr<ErrorMessage> ErrorMessage::Decode(const uint8_t *data,
                                                   size_t length) {
  auto response = std::make_unique<ErrorMessage>();
  const uint8_t *ptr = data;
  const uint8_t *end = data + length;

  uint8_t msg_type = MessageCodec::GetU8(ptr, end);
  if (msg_type != static_cast<uint8_t>(MessageType::Error)) {
    throw std::runtime_error("Invalid message type for ErrorMessage");
  }

  response->code = MessageCodec::GetString(ptr, end);
  response->message = MessageCodec::GetString(ptr, end);

  return response;
}

} // namespace adbc::cube
