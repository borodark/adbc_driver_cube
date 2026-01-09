#pragma once

#include <memory>
#include <string>
#include <vector>

#include "arrow_reader.h"
#include "native_protocol.h"
#include <arrow-adbc/adbc.h>

namespace adbc::cube {

/// Native client for connecting to Cube via custom Arrow IPC protocol
class NativeClient {
public:
  NativeClient();
  ~NativeClient();

  // Prevent copying
  NativeClient(const NativeClient &) = delete;
  NativeClient &operator=(const NativeClient &) = delete;

  /// Connect to the Cube ADBC Server
  /// @param host Server hostname or IP address
  /// @param port Server port (default: 8120)
  /// @param error Optional error output
  /// @return Status code
  AdbcStatusCode Connect(const std::string &host, int port,
                         AdbcError *error = nullptr);

  /// Authenticate with the server
  /// @param token Authentication token
  /// @param database Optional database name
  /// @param error Optional error output
  /// @return Status code
  AdbcStatusCode Authenticate(const std::string &token,
                              const std::string &database = "",
                              AdbcError *error = nullptr);

  /// Execute a query and return results as ArrowArrayStream
  /// @param sql SQL query string
  /// @param out Output ArrowArrayStream
  /// @param error Optional error output
  /// @return Status code
  AdbcStatusCode ExecuteQuery(const std::string &sql,
                              struct ArrowArrayStream *out,
                              AdbcError *error = nullptr);

  /// Close the connection
  void Close();

  /// Check if connected
  bool IsConnected() const { return socket_fd_ >= 0; }

  /// Get session ID (available after authentication)
  const std::string &GetSessionId() const { return session_id_; }

  /// Get server version (available after handshake)
  const std::string &GetServerVersion() const { return server_version_; }

private:
  /// Socket file descriptor
  int socket_fd_;

  /// Session ID received from server
  std::string session_id_;

  /// Server version string
  std::string server_version_;

  /// Connection state
  bool authenticated_;

  /// Read a complete message from the socket
  /// @param error Optional error output
  /// @return Message data (length + type + payload)
  std::vector<uint8_t> ReadMessage(AdbcError *error = nullptr);

  /// Write a message to the socket
  /// @param data Message data (should already include length prefix)
  /// @param error Optional error output
  /// @return Status code
  AdbcStatusCode WriteMessage(const std::vector<uint8_t> &data,
                              AdbcError *error = nullptr);

  /// Read exact number of bytes from socket
  /// @param buffer Output buffer
  /// @param length Number of bytes to read
  /// @param error Optional error output
  /// @return Status code
  AdbcStatusCode ReadExact(uint8_t *buffer, size_t length,
                           AdbcError *error = nullptr);

  /// Write exact number of bytes to socket
  /// @param buffer Input buffer
  /// @param length Number of bytes to write
  /// @param error Optional error output
  /// @return Status code
  AdbcStatusCode WriteExact(const uint8_t *buffer, size_t length,
                            AdbcError *error = nullptr);

  /// Perform handshake with server
  /// @param error Optional error output
  /// @return Status code
  AdbcStatusCode PerformHandshake(AdbcError *error = nullptr);

  /// Set error message
  void SetError(AdbcError *error, const std::string &message);
};

/// Helper function to create error message in AdbcError struct
void SetNativeClientError(AdbcError *error, const std::string &message);

} // namespace adbc::cube
