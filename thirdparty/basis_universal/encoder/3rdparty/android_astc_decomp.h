// File: android_astc_decomp.h
#ifndef _TCUASTCUTIL_HPP
#define _TCUASTCUTIL_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief ASTC Utilities.
 *//*--------------------------------------------------------------------*/

#include <vector>
#include <stdint.h>

namespace basisu_astc
{
namespace astc
{

// Unpacks a single ASTC block to pDst
// If isSRGB is true, the spec requires the decoder to scale the LDR 8-bit endpoints to 16-bit before interpolation slightly differently, 
// which will lead to different outputs. So be sure to set it correctly (ideally it should match whatever the encoder did).
bool decompress_ldr(uint8_t* pDst, const uint8_t* data, bool isSRGB, int blockWidth, int blockHeight);
bool decompress_hdr(float* pDstRGBA, const uint8_t* data, int blockWidth, int blockHeight);
bool is_hdr(const uint8_t* data, int blockWidth, int blockHeight, bool& is_hdr);

} // astc
} // basisu

#endif
