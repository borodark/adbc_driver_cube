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

#include <algorithm>
#include <cctype>

#include "driver/cube/cube_types.h"

namespace adbc::cube {

// Helper to normalize SQL type names (lowercase, trim whitespace)
static std::string NormalizeTypeName(const std::string &type_name) {
  std::string normalized = type_name;
  // Trim leading/trailing whitespace
  size_t start = 0;
  size_t end = normalized.length();
  while (start < end && std::isspace(normalized[start]))
    start++;
  while (end > start && std::isspace(normalized[end - 1]))
    end--;
  normalized = normalized.substr(start, end - start);
  // Convert to lowercase
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return normalized;
}

ArrowType CubeTypeMapper::MapCubeTypeToArrowType(const std::string &cube_type) {
  std::string normalized = NormalizeTypeName(cube_type);

  // Integer types
  if (normalized == "bigint" || normalized == "int8") {
    return NANOARROW_TYPE_INT64;
  }
  if (normalized == "integer" || normalized == "int" || normalized == "int4") {
    return NANOARROW_TYPE_INT32;
  }
  if (normalized == "smallint" || normalized == "int2") {
    return NANOARROW_TYPE_INT16;
  }
  if (normalized == "tinyint" || normalized == "int1") {
    return NANOARROW_TYPE_INT8;
  }

  // Unsigned integer types
  if (normalized == "ubigint" || normalized == "uint8") {
    return NANOARROW_TYPE_UINT64;
  }
  if (normalized == "uinteger" || normalized == "uint" ||
      normalized == "uint4") {
    return NANOARROW_TYPE_UINT32;
  }
  if (normalized == "usmallint" || normalized == "uint2") {
    return NANOARROW_TYPE_UINT16;
  }
  if (normalized == "utinyint" || normalized == "uint1") {
    return NANOARROW_TYPE_UINT8;
  }

  // Floating point types
  if (normalized == "double" || normalized == "double precision" ||
      normalized == "float8") {
    return NANOARROW_TYPE_DOUBLE;
  }
  if (normalized == "real" || normalized == "float" || normalized == "float4") {
    return NANOARROW_TYPE_FLOAT;
  }

  // Boolean type
  if (normalized == "boolean" || normalized == "bool") {
    return NANOARROW_TYPE_BOOL;
  }

  // String types
  if (normalized == "varchar" || normalized == "character varying" ||
      normalized == "text" || normalized == "char" || normalized == "string") {
    return NANOARROW_TYPE_STRING;
  }

  // Binary types
  if (normalized == "bytea" || normalized == "binary" ||
      normalized == "varbinary") {
    return NANOARROW_TYPE_BINARY;
  }

  // Date type
  if (normalized == "date") {
    return NANOARROW_TYPE_DATE32;
  }

  // Time types
  if (normalized == "time" || normalized == "time without time zone" ||
      normalized == "time with time zone") {
    return NANOARROW_TYPE_TIME64;
  }

  // Timestamp types
  if (normalized == "timestamp" ||
      normalized == "timestamp without time zone" ||
      normalized == "timestamp with time zone" || normalized == "timestamptz") {
    return NANOARROW_TYPE_TIMESTAMP;
  }

  // Decimal/numeric types - map to string for safety
  // (would need decimal128 support for proper handling)
  if (normalized == "numeric" || normalized == "decimal" ||
      normalized == "number") {
    return NANOARROW_TYPE_STRING;
  }

  // JSON types - map to string
  if (normalized == "json" || normalized == "jsonb") {
    return NANOARROW_TYPE_STRING;
  }

  // UUID type - map to string
  if (normalized == "uuid") {
    return NANOARROW_TYPE_STRING;
  }

  // Unknown types - permissive fallback to BINARY
  // This allows queries to continue even with unknown Cube SQL types
  return NANOARROW_TYPE_BINARY;
}

std::string CubeTypeMapper::GetArrowTypeDescription(ArrowType type) {
  switch (type) {
  case NANOARROW_TYPE_NA:
    return "null";
  case NANOARROW_TYPE_BOOL:
    return "bool";
  case NANOARROW_TYPE_INT8:
    return "int8";
  case NANOARROW_TYPE_INT16:
    return "int16";
  case NANOARROW_TYPE_INT32:
    return "int32";
  case NANOARROW_TYPE_INT64:
    return "int64";
  case NANOARROW_TYPE_UINT8:
    return "uint8";
  case NANOARROW_TYPE_UINT16:
    return "uint16";
  case NANOARROW_TYPE_UINT32:
    return "uint32";
  case NANOARROW_TYPE_UINT64:
    return "uint64";
  case NANOARROW_TYPE_FLOAT:
    return "float";
  case NANOARROW_TYPE_DOUBLE:
    return "double";
  case NANOARROW_TYPE_STRING:
    return "string";
  case NANOARROW_TYPE_BINARY:
    return "binary";
  case NANOARROW_TYPE_DATE32:
    return "date32";
  case NANOARROW_TYPE_TIME64:
    return "time64";
  case NANOARROW_TYPE_TIMESTAMP:
    return "timestamp";
  default:
    return "unknown";
  }
}

} // namespace adbc::cube
