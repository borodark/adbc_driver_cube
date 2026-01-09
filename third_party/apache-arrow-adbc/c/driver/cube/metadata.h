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

#include <memory>
#include <string>
#include <vector>

#include <nanoarrow/nanoarrow.h>

namespace adbc::cube {

// Helper for building Arrow schemas from Cube SQL metadata
class MetadataBuilder {
public:
  MetadataBuilder();
  ~MetadataBuilder();

  // Add a column to the schema
  void AddColumn(const std::string &column_name,
                 const std::string &cube_sql_type);

  // Build the final Arrow schema
  struct ArrowSchema Build();

private:
  std::vector<std::string> column_names_;
  std::vector<std::string> column_types_;
};

} // namespace adbc::cube
