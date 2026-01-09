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

#include <cmath>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <sstream>

#include "driver/cube/parameter_converter.h"

namespace adbc::cube {

namespace {

// Helper to format dates as YYYY-MM-DD
std::string FormatDate(int32_t days_since_epoch) {
  // Unix epoch is 1970-01-01
  // But Arrow uses 1970-01-01 as day 0
  int64_t total_days = days_since_epoch;

  // Simplified: convert to year/month/day
  // Real implementation would use proper calendar math
  int year = 1970;
  int month = 1;
  int day = 1;

  // Add days to epoch date
  int remaining_days = total_days;

  // Count years
  while (remaining_days >= 365) {
    int days_in_year =
        (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 366 : 365;
    if (remaining_days >= days_in_year) {
      remaining_days -= days_in_year;
      year++;
    } else {
      break;
    }
  }

  // Count months
  const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool is_leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));

  for (int m = 0; m < 12; m++) {
    int days = days_in_month[m];
    if (m == 1 && is_leap)
      days = 29;

    if (remaining_days >= days) {
      remaining_days -= days;
      month++;
    } else {
      day = remaining_days + 1;
      break;
    }
  }

  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(4) << year << "-" << std::setw(2)
      << month << "-" << std::setw(2) << day;
  return oss.str();
}

// Helper to format timestamps as ISO 8601
std::string FormatTimestamp(int64_t micros_since_epoch) {
  // Convert microseconds to seconds and fractional part
  int64_t seconds = micros_since_epoch / 1000000;
  int32_t micros = micros_since_epoch % 1000000;

  // Get date from seconds (similar to FormatDate but with time)
  // Simplified version
  time_t t = seconds;
  struct tm tm_result = {};

#ifdef _WIN32
  gmtime_s(&tm_result, &t);
#else
  gmtime_r(&t, &tm_result);
#endif

  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(4) << (tm_result.tm_year + 1900) << "-"
      << std::setw(2) << (tm_result.tm_mon + 1) << "-" << std::setw(2)
      << tm_result.tm_mday << "T" << std::setw(2) << tm_result.tm_hour << ":"
      << std::setw(2) << tm_result.tm_min << ":" << std::setw(2)
      << tm_result.tm_sec << "." << std::setw(6) << micros;
  return oss.str();
}

} // namespace

ParameterConverter::ParameterConverter() {}

ParameterConverter::~ParameterConverter() {}

bool ParameterConverter::IsValueNull(const ArrowArray *array, int64_t row) {
  if (!array->buffers[0]) {
    // No validity buffer = no nulls
    return false;
  }

  const uint8_t *validity_bitmap =
      static_cast<const uint8_t *>(array->buffers[0]);
  int64_t byte_index = row / 8;
  int64_t bit_index = row % 8;

  return !(validity_bitmap[byte_index] & (1 << bit_index));
}

std::string ParameterConverter::ConvertInt8(const ArrowArray *array,
                                            int64_t row) {
  const int8_t *data = static_cast<const int8_t *>(array->buffers[1]);
  return std::to_string(data[row]);
}

std::string ParameterConverter::ConvertInt16(const ArrowArray *array,
                                             int64_t row) {
  const int16_t *data = static_cast<const int16_t *>(array->buffers[1]);
  return std::to_string(data[row]);
}

std::string ParameterConverter::ConvertInt32(const ArrowArray *array,
                                             int64_t row) {
  const int32_t *data = static_cast<const int32_t *>(array->buffers[1]);
  return std::to_string(data[row]);
}

std::string ParameterConverter::ConvertInt64(const ArrowArray *array,
                                             int64_t row) {
  const int64_t *data = static_cast<const int64_t *>(array->buffers[1]);
  return std::to_string(data[row]);
}

std::string ParameterConverter::ConvertUInt8(const ArrowArray *array,
                                             int64_t row) {
  const uint8_t *data = static_cast<const uint8_t *>(array->buffers[1]);
  return std::to_string(data[row]);
}

std::string ParameterConverter::ConvertUInt16(const ArrowArray *array,
                                              int64_t row) {
  const uint16_t *data = static_cast<const uint16_t *>(array->buffers[1]);
  return std::to_string(data[row]);
}

std::string ParameterConverter::ConvertUInt32(const ArrowArray *array,
                                              int64_t row) {
  const uint32_t *data = static_cast<const uint32_t *>(array->buffers[1]);
  return std::to_string(data[row]);
}

std::string ParameterConverter::ConvertUInt64(const ArrowArray *array,
                                              int64_t row) {
  const uint64_t *data = static_cast<const uint64_t *>(array->buffers[1]);
  return std::to_string(data[row]);
}

std::string ParameterConverter::ConvertFloat(const ArrowArray *array,
                                             int64_t row) {
  const float *data = static_cast<const float *>(array->buffers[1]);
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(6) << data[row];
  return oss.str();
}

std::string ParameterConverter::ConvertDouble(const ArrowArray *array,
                                              int64_t row) {
  const double *data = static_cast<const double *>(array->buffers[1]);
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(15) << data[row];
  return oss.str();
}

std::string ParameterConverter::ConvertString(const ArrowArray *array,
                                              int64_t row) {
  // For large strings, buffer[1] contains offsets, buffer[2] contains data
  const int32_t *offsets = static_cast<const int32_t *>(array->buffers[1]);
  const char *data = static_cast<const char *>(array->buffers[2]);

  int32_t start_offset = offsets[row];
  int32_t end_offset = offsets[row + 1];
  int32_t length = end_offset - start_offset;

  return std::string(data + start_offset, length);
}

std::string ParameterConverter::ConvertBinary(const ArrowArray *array,
                                              int64_t row) {
  // Similar to string but binary data
  const int32_t *offsets = static_cast<const int32_t *>(array->buffers[1]);
  const uint8_t *data = static_cast<const uint8_t *>(array->buffers[2]);

  int32_t start_offset = offsets[row];
  int32_t end_offset = offsets[row + 1];
  int32_t length = end_offset - start_offset;

  // Convert binary to hex string for PostgreSQL
  std::ostringstream oss;
  oss << "\\x";
  for (int32_t i = 0; i < length; i++) {
    oss << std::setfill('0') << std::setw(2) << std::hex
        << data[start_offset + i];
  }
  return oss.str();
}

std::string ParameterConverter::ConvertBool(const ArrowArray *array,
                                            int64_t row) {
  const uint8_t *data = static_cast<const uint8_t *>(array->buffers[1]);
  int64_t byte_index = row / 8;
  int64_t bit_index = row % 8;

  bool value = (data[byte_index] & (1 << bit_index)) != 0;
  return value ? "true" : "false";
}

std::string ParameterConverter::ConvertDate32(const ArrowArray *array,
                                              int64_t row) {
  const int32_t *data = static_cast<const int32_t *>(array->buffers[1]);
  return FormatDate(data[row]);
}

std::string ParameterConverter::ConvertDate64(const ArrowArray *array,
                                              int64_t row) {
  const int64_t *data = static_cast<const int64_t *>(array->buffers[1]);
  // Date64 is milliseconds since epoch
  return FormatDate(data[row] / 86400000); // ms to days
}

std::string ParameterConverter::ConvertTime64(const ArrowArray *array,
                                              int64_t row) {
  const int64_t *data = static_cast<const int64_t *>(array->buffers[1]);

  // Convert microseconds to HH:MM:SS.FFFFFF
  int64_t micros = data[row];
  int32_t hours = (micros / 3600000000LL) % 24;
  int32_t minutes = (micros / 60000000LL) % 60;
  int32_t seconds = (micros / 1000000LL) % 60;
  int32_t microseconds = micros % 1000000;

  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(2) << hours << ":" << std::setw(2)
      << minutes << ":" << std::setw(2) << seconds << "." << std::setw(6)
      << microseconds;
  return oss.str();
}

std::string ParameterConverter::ConvertTimestamp(const ArrowArray *array,
                                                 int64_t row,
                                                 const ArrowSchema *schema) {
  const int64_t *data = static_cast<const int64_t *>(array->buffers[1]);

  // Get the time unit from schema
  // For now, assume microseconds (most common)
  return FormatTimestamp(data[row]);
}

std::string ParameterConverter::ConvertArrowValue(const ArrowArray *array,
                                                  int64_t row,
                                                  const ArrowSchema *schema,
                                                  bool *is_null) {
  *is_null = IsValueNull(array, row);

  if (*is_null) {
    return "";
  }

  // Route based on Arrow type
  switch (schema->format[0]) {
  case 'c': // int8
    return ConvertInt8(array, row);
  case 's': // int16
    return ConvertInt16(array, row);
  case 'i': // int32
    return ConvertInt32(array, row);
  case 'l': // int64
    return ConvertInt64(array, row);
  case 'C': // uint8
    return ConvertUInt8(array, row);
  case 'S': // uint16
    return ConvertUInt16(array, row);
  case 'I': // uint32
    return ConvertUInt32(array, row);
  case 'L': // uint64
    return ConvertUInt64(array, row);
  case 'f': // float
    return ConvertFloat(array, row);
  case 'g': // double
    return ConvertDouble(array, row);
  case 'u': // utf8 string
  case 'U': // large utf8 string
    return ConvertString(array, row);
  case 'z': // binary
  case 'Z': // large binary
    return ConvertBinary(array, row);
  case 'b': // bool
    return ConvertBool(array, row);
  case 'd': // date (check precision)
    if (schema->format[1] == 'D') {
      return ConvertDate32(array, row); // Date32
    } else {
      return ConvertDate64(array, row); // Date64
    }
  case 't': // time (check precision)
    if (schema->format[1] == 't') {
      return ConvertTime64(array, row);
    }
    break;
  case 'T': // timestamp
    return ConvertTimestamp(array, row, schema);
  }

  // Fallback for unknown types
  return "";
}

std::vector<std::string>
ParameterConverter::ConvertArrowArrayToParams(const ArrowArray *values,
                                              const ArrowSchema *schema) {
  std::vector<std::string> result;

  if (!values || !schema) {
    return result;
  }

  for (int64_t row = 0; row < values->length; row++) {
    bool is_null = false;
    std::string param_value = ConvertArrowValue(values, row, schema, &is_null);
    result.push_back(param_value);
  }

  return result;
}

const char **ParameterConverter::GetParamValuesCArray(
    const std::vector<std::string> &param_values) {
  if (param_values.empty()) {
    return nullptr;
  }

  // Allocate C array of string pointers
  char **result =
      static_cast<char **>(malloc(param_values.size() * sizeof(char *)));

  for (size_t i = 0; i < param_values.size(); i++) {
    result[i] = const_cast<char *>(param_values[i].c_str());
  }

  return const_cast<const char **>(result);
}

} // namespace adbc::cube
