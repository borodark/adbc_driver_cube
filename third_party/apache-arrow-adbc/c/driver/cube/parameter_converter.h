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
#include <string>
#include <vector>

#include <arrow-adbc/adbc.h>
#include <nanoarrow/nanoarrow.h>

namespace adbc::cube {

// Helper class to convert Arrow arrays to PostgreSQL parameter format
// Converts Arrow values to text strings for use with PQexecParams()
class ParameterConverter {
public:
  ParameterConverter();
  ~ParameterConverter();

  // Convert a single Arrow value to PostgreSQL text format
  // Returns the text representation, or empty string for NULL values
  // Sets is_null to true for NULL values
  static std::string ConvertArrowValue(const ArrowArray *array, int64_t row,
                                       const ArrowSchema *schema,
                                       bool *is_null);

  // Convert Arrow array to vector of PostgreSQL parameter strings
  // Returns parameter values and nullness flags for use with PQexecParams
  static std::vector<std::string>
  ConvertArrowArrayToParams(const ArrowArray *values,
                            const ArrowSchema *schema);

  // Get the C-style parameter values array for PQexecParams
  // Should be freed with free() after use
  static const char **
  GetParamValuesCArray(const std::vector<std::string> &param_values);

private:
  // Type-specific converters
  static std::string ConvertInt8(const ArrowArray *array, int64_t row);
  static std::string ConvertInt16(const ArrowArray *array, int64_t row);
  static std::string ConvertInt32(const ArrowArray *array, int64_t row);
  static std::string ConvertInt64(const ArrowArray *array, int64_t row);
  static std::string ConvertUInt8(const ArrowArray *array, int64_t row);
  static std::string ConvertUInt16(const ArrowArray *array, int64_t row);
  static std::string ConvertUInt32(const ArrowArray *array, int64_t row);
  static std::string ConvertUInt64(const ArrowArray *array, int64_t row);
  static std::string ConvertFloat(const ArrowArray *array, int64_t row);
  static std::string ConvertDouble(const ArrowArray *array, int64_t row);
  static std::string ConvertString(const ArrowArray *array, int64_t row);
  static std::string ConvertBinary(const ArrowArray *array, int64_t row);
  static std::string ConvertBool(const ArrowArray *array, int64_t row);
  static std::string ConvertDate32(const ArrowArray *array, int64_t row);
  static std::string ConvertDate64(const ArrowArray *array, int64_t row);
  static std::string ConvertTime64(const ArrowArray *array, int64_t row);
  static std::string ConvertTimestamp(const ArrowArray *array, int64_t row,
                                      const ArrowSchema *schema);

  // Helper to check if value is NULL
  static bool IsValueNull(const ArrowArray *array, int64_t row);
};

} // namespace adbc::cube
