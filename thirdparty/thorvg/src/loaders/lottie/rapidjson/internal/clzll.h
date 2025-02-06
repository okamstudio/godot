// Tencent is pleased to support the open source community by making RapidJSON available.
//
// Copyright (C) 2015 THL A29 Limited, a Tencent company, and Milo Yip.
//
// Licensed under the MIT License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef RAPIDJSON_CLZLL_H_
#define RAPIDJSON_CLZLL_H_

#include "../rapidjson.h"

#if defined(_MSC_VER) && !defined(UNDER_CE)
#include <intrin.h>
#if defined(_WIN64)
#pragma intrinsic(_BitScanReverse64)
#else
#pragma intrinsic(_BitScanReverse)
#endif
#endif

RAPIDJSON_NAMESPACE_BEGIN
namespace internal {

inline uint32_t clzll(uint64_t x) {
    // Passing 0 to __builtin_clzll is UB in GCC and results in an
    // infinite loop in the software implementation.
    RAPIDJSON_ASSERT(x != 0);

#if defined(_MSC_VER) && !defined(UNDER_CE)
    unsigned long r = 0;
#if defined(_WIN64)
    _BitScanReverse64(&r, x);
#else
    // Scan the high 32 bits.
    if (_BitScanReverse(&r, static_cast<uint32_t>(x >> 32)))
        return 63 - (r + 32);

    // Scan the low 32 bits.
    _BitScanReverse(&r, static_cast<uint32_t>(x & 0xFFFFFFFF));
#endif // _WIN64

    return 63 - r;
#elif (defined(__GNUC__) && __GNUC__ >= 4) || RAPIDJSON_HAS_BUILTIN(__builtin_clzll)
    // __builtin_clzll wrapper
    return static_cast<uint32_t>(__builtin_clzll(x));
#else
    // naive version
    uint32_t r = 0;
    while (!(x & (static_cast<uint64_t>(1) << 63))) {
        x <<= 1;
        ++r;
    }

    return r;
#endif // _MSC_VER
}

#define RAPIDJSON_CLZLL RAPIDJSON_NAMESPACE::internal::clzll

} // namespace internal
RAPIDJSON_NAMESPACE_END

#endif // RAPIDJSON_CLZLL_H_
