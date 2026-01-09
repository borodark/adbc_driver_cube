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

#include "driver/cube/metadata.h"

#include "driver/cube/cube_types.h"

namespace adbc::cube {

MetadataBuilder::MetadataBuilder() {}

MetadataBuilder::~MetadataBuilder() {}

void MetadataBuilder::AddColumn(const std::string &column_name,
                                const std::string &cube_sql_type) {
  column_names_.push_back(column_name);
  column_types_.push_back(cube_sql_type);
}

struct ArrowSchema MetadataBuilder::Build() {
  struct ArrowSchema schema = {};

  // Initialize schema structure
  schema.format = "+sn"; // Struct type with no nullability
  schema.n_children = static_cast<int64_t>(column_names_.size());

  if (schema.n_children == 0) {
    schema.release(&schema);
    return schema;
  }

  // Allocate children array
  schema.children = static_cast<struct ArrowSchema **>(
      malloc(schema.n_children * sizeof(struct ArrowSchema *)));

  // Build each child field
  for (int64_t i = 0; i < schema.n_children; i++) {
    schema.children[i] =
        static_cast<struct ArrowSchema *>(malloc(sizeof(struct ArrowSchema)));
    struct ArrowSchema *child = schema.children[i];

    ArrowSchemaInit(child);

    // Set field name
    ArrowSchemaSetName(child, column_names_[i].c_str());

    // Set Arrow type based on Cube SQL type
    ArrowType arrow_type =
        CubeTypeMapper::MapCubeTypeToArrowType(column_types_[i]);

    // Format string for type
    switch (arrow_type) {
    case NANOARROW_TYPE_BOOL:
      child->format = "c";
      break;
    case NANOARROW_TYPE_INT8:
      child->format = "c";
      break;
    case NANOARROW_TYPE_INT16:
      child->format = "s";
      break;
    case NANOARROW_TYPE_INT32:
      child->format = "i";
      break;
    case NANOARROW_TYPE_INT64:
      child->format = "l";
      break;
    case NANOARROW_TYPE_UINT8:
      child->format = "C";
      break;
    case NANOARROW_TYPE_UINT16:
      child->format = "S";
      break;
    case NANOARROW_TYPE_UINT32:
      child->format = "I";
      break;
    case NANOARROW_TYPE_UINT64:
      child->format = "L";
      break;
    case NANOARROW_TYPE_FLOAT:
      child->format = "f";
      break;
    case NANOARROW_TYPE_DOUBLE:
      child->format = "g";
      break;
    case NANOARROW_TYPE_STRING:
      child->format = "u";
      break;
    case NANOARROW_TYPE_BINARY:
      child->format = "z";
      break;
    case NANOARROW_TYPE_DATE32:
      child->format = "tdD";
      break;
    case NANOARROW_TYPE_TIME64:
      child->format = "ttu";
      break;
    case NANOARROW_TYPE_TIMESTAMP:
      child->format = "tsu:";
      break;
    default:
      child->format = "z"; // Binary as fallback
      break;
    }

    // Store metadata about original Cube SQL type
    child->metadata =
        nullptr; // Simplified: would store type mapping in metadata
  }

  // Set parent schema release function
  schema.release = [](struct ArrowSchema *s) {
    if (s->children != nullptr) {
      for (int64_t i = 0; i < s->n_children; i++) {
        if (s->children[i] != nullptr) {
          if (s->children[i]->release != nullptr) {
            s->children[i]->release(s->children[i]);
          }
          free(s->children[i]);
        }
      }
      free(s->children);
    }
    s->children = nullptr;
    s->n_children = 0;
  };

  return schema;
}

} // namespace adbc::cube
