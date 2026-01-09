#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace adbc::cube {

// Protocol version
constexpr uint32_t PROTOCOL_VERSION = 1;

// Message types
enum class MessageType : uint8_t {
  HandshakeRequest = 0x01,
  HandshakeResponse = 0x02,
  AuthRequest = 0x03,
  AuthResponse = 0x04,
  QueryRequest = 0x10,
  QueryResponseSchema = 0x11,
  QueryResponseBatch = 0x12,
  QueryComplete = 0x13,
  Error = 0xFF,
};

// Base message structure
struct Message {
  virtual ~Message() = default;
  virtual MessageType GetType() const = 0;
  virtual std::vector<uint8_t> Encode() const = 0;
};

// Handshake messages
struct HandshakeRequest : public Message {
  uint32_t version = PROTOCOL_VERSION;

  MessageType GetType() const override { return MessageType::HandshakeRequest; }
  std::vector<uint8_t> Encode() const override;
};

struct HandshakeResponse : public Message {
  uint32_t version;
  std::string server_version;

  MessageType GetType() const override {
    return MessageType::HandshakeResponse;
  }
  std::vector<uint8_t> Encode() const override;

  static std::unique_ptr<HandshakeResponse> Decode(const uint8_t *data,
                                                   size_t length);
};

// Authentication messages
struct AuthRequest : public Message {
  std::string token;
  std::string database; // optional

  MessageType GetType() const override { return MessageType::AuthRequest; }
  std::vector<uint8_t> Encode() const override;
};

struct AuthResponse : public Message {
  bool success;
  std::string session_id;

  MessageType GetType() const override { return MessageType::AuthResponse; }
  std::vector<uint8_t> Encode() const override;

  static std::unique_ptr<AuthResponse> Decode(const uint8_t *data,
                                              size_t length);
};

// Query messages
struct QueryRequest : public Message {
  std::string sql;

  MessageType GetType() const override { return MessageType::QueryRequest; }
  std::vector<uint8_t> Encode() const override;
};

struct QueryResponseSchema : public Message {
  std::vector<uint8_t> arrow_ipc_schema;

  MessageType GetType() const override {
    return MessageType::QueryResponseSchema;
  }
  std::vector<uint8_t> Encode() const override;

  static std::unique_ptr<QueryResponseSchema> Decode(const uint8_t *data,
                                                     size_t length);
};

struct QueryResponseBatch : public Message {
  std::vector<uint8_t> arrow_ipc_batch;

  MessageType GetType() const override {
    return MessageType::QueryResponseBatch;
  }
  std::vector<uint8_t> Encode() const override;

  static std::unique_ptr<QueryResponseBatch> Decode(const uint8_t *data,
                                                    size_t length);
};

struct QueryComplete : public Message {
  int64_t rows_affected;

  MessageType GetType() const override { return MessageType::QueryComplete; }
  std::vector<uint8_t> Encode() const override;

  static std::unique_ptr<QueryComplete> Decode(const uint8_t *data,
                                               size_t length);
};

struct ErrorMessage : public Message {
  std::string code;
  std::string message;

  MessageType GetType() const override { return MessageType::Error; }
  std::vector<uint8_t> Encode() const override;

  static std::unique_ptr<ErrorMessage> Decode(const uint8_t *data,
                                              size_t length);
};

// Helper functions for encoding/decoding
class MessageCodec {
public:
  // Encode helpers
  static void PutU32(std::vector<uint8_t> &buf, uint32_t value);
  static void PutI64(std::vector<uint8_t> &buf, int64_t value);
  static void PutU8(std::vector<uint8_t> &buf, uint8_t value);
  static void PutString(std::vector<uint8_t> &buf, const std::string &str);
  static void PutOptionalString(std::vector<uint8_t> &buf,
                                const std::string &str);
  static void PutBytes(std::vector<uint8_t> &buf,
                       const std::vector<uint8_t> &bytes);

  // Decode helpers
  static uint32_t GetU32(const uint8_t *&ptr, const uint8_t *end);
  static int64_t GetI64(const uint8_t *&ptr, const uint8_t *end);
  static uint8_t GetU8(const uint8_t *&ptr, const uint8_t *end);
  static std::string GetString(const uint8_t *&ptr, const uint8_t *end);
  static std::string GetOptionalString(const uint8_t *&ptr, const uint8_t *end);
  static std::vector<uint8_t> GetBytes(const uint8_t *&ptr, const uint8_t *end);
};

} // namespace adbc::cube
