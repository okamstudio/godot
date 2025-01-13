// Copyright 2022 The Manifold Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#ifdef MANIFOLD_DEBUG
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

/** @addtogroup Debug
 * @{
 */
struct userErr : public virtual std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct topologyErr : public virtual std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct geometryErr : public virtual std::runtime_error {
  using std::runtime_error::runtime_error;
};
using logicErr = std::logic_error;

template <typename Ex>
void AssertFail(const char* file, int line, const char* cond, const char* msg) {
  std::ostringstream output;
  output << "Error in file: " << file << " (" << line << "): \'" << cond
         << "\' is false: " << msg;
  throw Ex(output.str());
}

template <typename Ex>
void AssertFail(const char* file, int line, const std::string& cond,
                const std::string& msg) {
  std::ostringstream output;
  output << "Error in file: " << file << " (" << line << "): \'" << cond
         << "\' is false: " << msg;
  throw Ex(output.str());
}

// DEBUG_ASSERT is slightly slower due to the function call, but gives more
// detailed info.
#define DEBUG_ASSERT(condition, EX, msg) \
  if (!(condition)) AssertFail<EX>(__FILE__, __LINE__, #condition, msg);
// ASSERT has almost no overhead, so better to use for frequent calls like
// vector bounds checking.
#define ASSERT(condition, EX) \
  if (!(condition)) throw(EX);
#else
#define DEBUG_ASSERT(condition, EX, msg)
#define ASSERT(condition, EX)
#endif
/** @} */
