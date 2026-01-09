// Set to 1 to enable debug logging
#ifndef CUBE_DEBUG_LOGGING
#define CUBE_DEBUG_LOGGING 0
#endif

#if CUBE_DEBUG_LOGGING
#define DEBUG_LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define DEBUG_LOG(...) ((void)0)
#endif

#include "native_client.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <stdexcept>

namespace adbc::cube {

// Helper to set error messages
void SetNativeClientError(AdbcError *error, const std::string &message) {
  if (!error) {
    return;
  }

  // If error already has a message, clean it up first
  if (error->message && error->release) {
    error->release(error);
  }

  // Allocate and set new message
  error->message = new char[message.length() + 1];
  std::strcpy(error->message, message.c_str());

  // Set release callback if not already set
  if (!error->release) {
    error->release = [](struct AdbcError* err) {
      if (err->message) {
        delete[] err->message;
        err->message = nullptr;
      }
      err->release = nullptr;
    };
  }
}

NativeClient::NativeClient() : socket_fd_(-1), authenticated_(false) {}

NativeClient::~NativeClient() { Close(); }

AdbcStatusCode NativeClient::Connect(const std::string &host, int port,
                                     AdbcError *error) {
  if (IsConnected()) {
    SetNativeClientError(error, "Already connected");
    return ADBC_STATUS_INVALID_STATE;
  }

  // Create socket
  socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_fd_ < 0) {
    SetNativeClientError(error, "Failed to create socket: " +
                                    std::string(strerror(errno)));
    return ADBC_STATUS_IO;
  }

  // Resolve hostname
  struct hostent *server = gethostbyname(host.c_str());
  if (server == nullptr) {
    close(socket_fd_);
    socket_fd_ = -1;
    SetNativeClientError(error, "Failed to resolve hostname: " + host);
    return ADBC_STATUS_IO;
  }

  // Setup server address
  struct sockaddr_in server_addr;
  std::memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  std::memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  server_addr.sin_port = htons(port);

  // Connect to server
  if (connect(socket_fd_, reinterpret_cast<struct sockaddr *>(&server_addr),
              sizeof(server_addr)) < 0) {
    close(socket_fd_);
    socket_fd_ = -1;
    SetNativeClientError(error, "Failed to connect to " + host + ":" +
                                    std::to_string(port) + ": " +
                                    std::string(strerror(errno)));
    return ADBC_STATUS_IO;
  }

  // Perform handshake
  auto status = PerformHandshake(error);
  if (status != ADBC_STATUS_OK) {
    Close();
    return status;
  }

  return ADBC_STATUS_OK;
}

AdbcStatusCode NativeClient::PerformHandshake(AdbcError *error) {
  // Send handshake request
  HandshakeRequest request;
  request.version = PROTOCOL_VERSION;

  auto data = request.Encode();
  auto status = WriteMessage(data, error);
  if (status != ADBC_STATUS_OK) {
    return status;
  }

  // Receive handshake response
  auto response_data = ReadMessage(error);
  if (response_data.empty()) {
    SetNativeClientError(error, "Empty handshake response");
    return ADBC_STATUS_IO;
  }

  // Skip length prefix (first 4 bytes) and decode
  try {
    auto response = HandshakeResponse::Decode(response_data.data() + 4,
                                              response_data.size() - 4);

    if (response->version != PROTOCOL_VERSION) {
      SetNativeClientError(
          error, "Protocol version mismatch. Client: " +
                     std::to_string(PROTOCOL_VERSION) +
                     ", Server: " + std::to_string(response->version));
      return ADBC_STATUS_INVALID_DATA;
    }

    server_version_ = response->server_version;
  } catch (const std::exception &e) {
    SetNativeClientError(error, "Failed to decode handshake response: " +
                                    std::string(e.what()));
    return ADBC_STATUS_INVALID_DATA;
  }

  return ADBC_STATUS_OK;
}

AdbcStatusCode NativeClient::Authenticate(const std::string &token,
                                          const std::string &database,
                                          AdbcError *error) {
  if (!IsConnected()) {
    SetNativeClientError(error, "Not connected");
    return ADBC_STATUS_INVALID_STATE;
  }

  if (authenticated_) {
    SetNativeClientError(error, "Already authenticated");
    return ADBC_STATUS_INVALID_STATE;
  }

  // Send authentication request
  AuthRequest request;
  request.token = token;
  request.database = database;

  auto data = request.Encode();
  auto status = WriteMessage(data, error);
  if (status != ADBC_STATUS_OK) {
    return status;
  }

  // Receive authentication response
  auto response_data = ReadMessage(error);
  if (response_data.empty()) {
    SetNativeClientError(error, "Empty authentication response");
    return ADBC_STATUS_IO;
  }

  // Skip length prefix and decode
  try {
    auto response = AuthResponse::Decode(response_data.data() + 4,
                                         response_data.size() - 4);

    if (!response->success) {
      SetNativeClientError(error, "Authentication failed");
      return ADBC_STATUS_UNAUTHENTICATED;
    }

    session_id_ = response->session_id;
    authenticated_ = true;
  } catch (const std::exception &e) {
    SetNativeClientError(error, "Failed to decode authentication response: " +
                                    std::string(e.what()));
    return ADBC_STATUS_INVALID_DATA;
  }

  return ADBC_STATUS_OK;
}

AdbcStatusCode NativeClient::ExecuteQuery(const std::string &sql,
                                          struct ArrowArrayStream *out,
                                          AdbcError *error) {
  if (!IsConnected()) {
    SetNativeClientError(error, "Not connected");
    return ADBC_STATUS_INVALID_STATE;
  }

  if (!authenticated_) {
    SetNativeClientError(error, "Not authenticated");
    return ADBC_STATUS_UNAUTHENTICATED;
  }

  // Send query request
  QueryRequest request;
  request.sql = sql;

  auto data = request.Encode();
  auto status = WriteMessage(data, error);
  if (status != ADBC_STATUS_OK) {
    return status;
  }

  // Initialize output stream to a safe empty state
  // This ensures the stream can be safely released even if we return early with an error
  memset(out, 0, sizeof(*out));

  // Collect Arrow IPC batch data (which includes schema)
  // NOTE: We only use the batch data, not the schema-only message,
  // because each is a complete Arrow IPC stream with EOS markers.
  // Using both would create: [Schema][EOS][Schema][Batch][EOS]
  // which PyArrow sees as two separate streams.
  std::vector<uint8_t> arrow_ipc_data;
  bool query_complete = false;

  while (!query_complete) {
    auto response_data = ReadMessage(error);
    if (response_data.empty()) {
      SetNativeClientError(error, "Empty query response");
      return ADBC_STATUS_IO;
    }

    // Check message type (byte at offset 4, after length prefix)
    MessageType msg_type = static_cast<MessageType>(response_data[4]);

    try {
      switch (msg_type) {
      case MessageType::QueryResponseSchema: {
        // Skip schema-only message - we'll get schema from batch
        DEBUG_LOG(
            "[NativeClient::ExecuteQuery] Skipping schema-only message\n");
        break;
      }

      case MessageType::QueryResponseBatch: {
        auto response = QueryResponseBatch::Decode(response_data.data() + 4,
                                                   response_data.size() - 4);
        // Use only batch data (contains both schema and data)
        arrow_ipc_data = std::move(response->arrow_ipc_batch);
        DEBUG_LOG("[NativeClient::ExecuteQuery] Got batch data: %zu bytes\n",
                  arrow_ipc_data.size());
        break;
      }

      case MessageType::QueryComplete: {
        auto response = QueryComplete::Decode(response_data.data() + 4,
                                              response_data.size() - 4);
        // rows_affected = response->rows_affected;  // Unused for now
        (void)response; // Suppress unused variable warning
        query_complete = true;
        break;
      }

      case MessageType::Error: {
        DEBUG_LOG("[NativeClient::ExecuteQuery] Received Error message, size=%zu\n",
                  response_data.size());

        if (response_data.size() < 5) {  // Need at least length(4) + msgtype(1)
          SetNativeClientError(error, "Error message too short");
          return ADBC_STATUS_INVALID_DATA;
        }

        try {
          auto response = ErrorMessage::Decode(response_data.data() + 4,
                                               response_data.size() - 4);
          DEBUG_LOG("[NativeClient::ExecuteQuery] Decoded error: code=%s, message=%s\n",
                    response->code.c_str(), response->message.c_str());
          SetNativeClientError(error, "Query error [" + response->code +
                                          "]: " + response->message);
        } catch (const std::exception &decode_error) {
          DEBUG_LOG("[NativeClient::ExecuteQuery] Failed to decode error message: %s\n",
                    decode_error.what());
          SetNativeClientError(error, "Query failed (error message decode failed): " +
                                          std::string(decode_error.what()));
        }
        return ADBC_STATUS_UNKNOWN;
      }

      default: {
        SetNativeClientError(
            error, "Unexpected message type: " +
                       std::to_string(static_cast<uint8_t>(msg_type)));
        return ADBC_STATUS_INVALID_DATA;
      }
      }
    } catch (const std::exception &e) {
      SetNativeClientError(error, "Failed to decode response: " +
                                      std::string(e.what()));
      return ADBC_STATUS_INVALID_DATA;
    }
  }

  // Parse Arrow IPC data using CubeArrowReader
  if (arrow_ipc_data.empty()) {
    SetNativeClientError(error, "No Arrow IPC data received");
    return ADBC_STATUS_INVALID_DATA;
  }

  try {
    auto reader = std::make_unique<CubeArrowReader>(std::move(arrow_ipc_data));
    ArrowError arrow_error;
    memset(&arrow_error, 0, sizeof(arrow_error)); // Initialize to zeros
    auto init_status = reader->Init(&arrow_error);
    if (init_status != NANOARROW_OK) {
      std::string error_msg = "Failed to initialize Arrow reader: ";
      error_msg += arrow_error.message;
      SetNativeClientError(error, error_msg);
      DEBUG_LOG("[NativeClient::ExecuteQuery] Init failed with status %d: %s\n",
                init_status, error_msg.c_str());
      return ADBC_STATUS_INTERNAL;
    }

    // Release the empty stream before replacing it with real data
    if (out->release != nullptr) {
      out->release(out);
    }

    // Export to ArrowArrayStream
    DEBUG_LOG(
        "[NativeClient::ExecuteQuery] Exporting to ArrowArrayStream...\n");
    reader->ExportTo(out);
    DEBUG_LOG("[NativeClient::ExecuteQuery] Export complete\n");

    // Reader ownership transferred to ArrowArrayStream
    reader.release();

  } catch (const std::exception &e) {
    SetNativeClientError(error, "Failed to parse Arrow IPC data: " +
                                    std::string(e.what()));
    DEBUG_LOG("[NativeClient::ExecuteQuery] Exception: %s\n", e.what());
    return ADBC_STATUS_INVALID_DATA;
  }

  return ADBC_STATUS_OK;
}

void NativeClient::Close() {
  if (socket_fd_ >= 0) {
    close(socket_fd_);
    socket_fd_ = -1;
  }
  authenticated_ = false;
  session_id_.clear();
  server_version_.clear();
}

std::vector<uint8_t> NativeClient::ReadMessage(AdbcError *error) {
  // Read 4-byte length prefix
  uint8_t length_buf[4];
  auto status = ReadExact(length_buf, 4, error);
  if (status != ADBC_STATUS_OK) {
    return {};
  }

  // Decode length (big-endian)
  uint32_t length = (static_cast<uint32_t>(length_buf[0]) << 24) |
                    (static_cast<uint32_t>(length_buf[1]) << 16) |
                    (static_cast<uint32_t>(length_buf[2]) << 8) |
                    (static_cast<uint32_t>(length_buf[3]));

  if (length == 0 || length > 100 * 1024 * 1024) { // 100MB max
    SetNativeClientError(error,
                         "Invalid message length: " + std::to_string(length));
    return {};
  }

  // Read payload
  std::vector<uint8_t> payload(length);
  status = ReadExact(payload.data(), length, error);
  if (status != ADBC_STATUS_OK) {
    return {};
  }

  // Return length prefix + payload (for easier parsing)
  std::vector<uint8_t> result;
  result.insert(result.end(), length_buf, length_buf + 4);
  result.insert(result.end(), payload.begin(), payload.end());

  return result;
}

AdbcStatusCode NativeClient::WriteMessage(const std::vector<uint8_t> &data,
                                          AdbcError *error) {
  return WriteExact(data.data(), data.size(), error);
}

AdbcStatusCode NativeClient::ReadExact(uint8_t *buffer, size_t length,
                                       AdbcError *error) {
  size_t total_read = 0;
  while (total_read < length) {
    ssize_t n = read(socket_fd_, buffer + total_read, length - total_read);
    if (n < 0) {
      if (errno == EINTR)
        continue; // Interrupted, retry
      SetNativeClientError(error, "Socket read error: " +
                                      std::string(strerror(errno)));
      return ADBC_STATUS_IO;
    }
    if (n == 0) {
      SetNativeClientError(error, "Connection closed by server");
      return ADBC_STATUS_IO;
    }
    total_read += n;
  }
  return ADBC_STATUS_OK;
}

AdbcStatusCode NativeClient::WriteExact(const uint8_t *buffer, size_t length,
                                        AdbcError *error) {
  size_t total_written = 0;
  while (total_written < length) {
    ssize_t n =
        write(socket_fd_, buffer + total_written, length - total_written);
    if (n < 0) {
      if (errno == EINTR)
        continue; // Interrupted, retry
      SetNativeClientError(error, "Socket write error: " +
                                      std::string(strerror(errno)));
      return ADBC_STATUS_IO;
    }
    total_written += n;
  }
  return ADBC_STATUS_OK;
}

} // namespace adbc::cube
