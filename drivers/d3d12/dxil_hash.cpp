/**************************************************************************/
/*  dxil_hash.cpp                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

// Based on the patched public domain implementation released by Microsoft here:
// https://github.com/microsoft/hlsl-specs/blob/main/proposals/infra/INF-0004-validator-hashing.md

#include "dxil_hash.h"

#include <memory.h>

#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21

static const BYTE padding[64] = {
	0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static void FF(UINT &a, UINT b, UINT c, UINT d, UINT x, UINT8 s, UINT ac) {
	a += ((b & c) | (~b & d)) + x + ac;
	a = ((a << s) | (a >> (32 - s))) + b;
}

static void GG(UINT &a, UINT b, UINT c, UINT d, UINT x, UINT8 s, UINT ac) {
	a += ((b & d) | (c & ~d)) + x + ac;
	a = ((a << s) | (a >> (32 - s))) + b;
}

static void HH(UINT &a, UINT b, UINT c, UINT d, UINT x, UINT8 s, UINT ac) {
	a += (b ^ c ^ d) + x + ac;
	a = ((a << s) | (a >> (32 - s))) + b;
}

static void II(UINT &a, UINT b, UINT c, UINT d, UINT x, UINT8 s, UINT ac) {
	a += (c ^ (b | ~d)) + x + ac;
	a = ((a << s) | (a >> (32 - s))) + b;
}

void compute_dxil_hash(const BYTE *pData, UINT byteCount, BYTE *pOutHash) {
	UINT leftOver = byteCount & 0x3f;
	UINT padAmount;
	bool bTwoRowsPadding = false;
	if (leftOver < 56) {
		padAmount = 56 - leftOver;
	} else {
		padAmount = 120 - leftOver;
		bTwoRowsPadding = true;
	}
	UINT padAmountPlusSize = padAmount + 8;
	UINT state[4] = { 0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476 };
	UINT N = (byteCount + padAmountPlusSize) >> 6;
	UINT offset = 0;
	UINT NextEndState = bTwoRowsPadding ? N - 2 : N - 1;
	const BYTE *pCurrData = pData;
	for (UINT i = 0; i < N; i++, offset += 64, pCurrData += 64) {
		UINT x[16] = {};
		const UINT *pX;
		if (i == NextEndState) {
			if (!bTwoRowsPadding && i == N - 1) {
				UINT remainder = byteCount - offset;
				x[0] = byteCount << 3;
				memcpy((BYTE *)x + 4, pCurrData, remainder);
				memcpy((BYTE *)x + 4 + remainder, padding, padAmount);
				x[15] = 1 | (byteCount << 1);
			} else if (bTwoRowsPadding) {
				if (i == N - 2) {
					UINT remainder = byteCount - offset;
					memcpy(x, pCurrData, remainder);
					memcpy((BYTE *)x + remainder, padding, padAmount - 56);
					NextEndState = N - 1;
				} else if (i == N - 1) {
					x[0] = byteCount << 3;
					memcpy((BYTE *)x + 4, padding + padAmount - 56, 56);
					x[15] = 1 | (byteCount << 1);
				}
			}
			pX = x;
		} else {
			pX = (const UINT *)pCurrData;
		}

		UINT a = state[0];
		UINT b = state[1];
		UINT c = state[2];
		UINT d = state[3];

		/* Round 1 */
		FF(a, b, c, d, pX[0], S11, 0xd76aa478); /* 1 */
		FF(d, a, b, c, pX[1], S12, 0xe8c7b756); /* 2 */
		FF(c, d, a, b, pX[2], S13, 0x242070db); /* 3 */
		FF(b, c, d, a, pX[3], S14, 0xc1bdceee); /* 4 */
		FF(a, b, c, d, pX[4], S11, 0xf57c0faf); /* 5 */
		FF(d, a, b, c, pX[5], S12, 0x4787c62a); /* 6 */
		FF(c, d, a, b, pX[6], S13, 0xa8304613); /* 7 */
		FF(b, c, d, a, pX[7], S14, 0xfd469501); /* 8 */
		FF(a, b, c, d, pX[8], S11, 0x698098d8); /* 9 */
		FF(d, a, b, c, pX[9], S12, 0x8b44f7af); /* 10 */
		FF(c, d, a, b, pX[10], S13, 0xffff5bb1); /* 11 */
		FF(b, c, d, a, pX[11], S14, 0x895cd7be); /* 12 */
		FF(a, b, c, d, pX[12], S11, 0x6b901122); /* 13 */
		FF(d, a, b, c, pX[13], S12, 0xfd987193); /* 14 */
		FF(c, d, a, b, pX[14], S13, 0xa679438e); /* 15 */
		FF(b, c, d, a, pX[15], S14, 0x49b40821); /* 16 */

		/* Round 2 */
		GG(a, b, c, d, pX[1], S21, 0xf61e2562); /* 17 */
		GG(d, a, b, c, pX[6], S22, 0xc040b340); /* 18 */
		GG(c, d, a, b, pX[11], S23, 0x265e5a51); /* 19 */
		GG(b, c, d, a, pX[0], S24, 0xe9b6c7aa); /* 20 */
		GG(a, b, c, d, pX[5], S21, 0xd62f105d); /* 21 */
		GG(d, a, b, c, pX[10], S22, 0x2441453); /* 22 */
		GG(c, d, a, b, pX[15], S23, 0xd8a1e681); /* 23 */
		GG(b, c, d, a, pX[4], S24, 0xe7d3fbc8); /* 24 */
		GG(a, b, c, d, pX[9], S21, 0x21e1cde6); /* 25 */
		GG(d, a, b, c, pX[14], S22, 0xc33707d6); /* 26 */
		GG(c, d, a, b, pX[3], S23, 0xf4d50d87); /* 27 */
		GG(b, c, d, a, pX[8], S24, 0x455a14ed); /* 28 */
		GG(a, b, c, d, pX[13], S21, 0xa9e3e905); /* 29 */
		GG(d, a, b, c, pX[2], S22, 0xfcefa3f8); /* 30 */
		GG(c, d, a, b, pX[7], S23, 0x676f02d9); /* 31 */
		GG(b, c, d, a, pX[12], S24, 0x8d2a4c8a); /* 32 */

		/* Round 3 */
		HH(a, b, c, d, pX[5], S31, 0xfffa3942); /* 33 */
		HH(d, a, b, c, pX[8], S32, 0x8771f681); /* 34 */
		HH(c, d, a, b, pX[11], S33, 0x6d9d6122); /* 35 */
		HH(b, c, d, a, pX[14], S34, 0xfde5380c); /* 36 */
		HH(a, b, c, d, pX[1], S31, 0xa4beea44); /* 37 */
		HH(d, a, b, c, pX[4], S32, 0x4bdecfa9); /* 38 */
		HH(c, d, a, b, pX[7], S33, 0xf6bb4b60); /* 39 */
		HH(b, c, d, a, pX[10], S34, 0xbebfbc70); /* 40 */
		HH(a, b, c, d, pX[13], S31, 0x289b7ec6); /* 41 */
		HH(d, a, b, c, pX[0], S32, 0xeaa127fa); /* 42 */
		HH(c, d, a, b, pX[3], S33, 0xd4ef3085); /* 43 */
		HH(b, c, d, a, pX[6], S34, 0x4881d05); /* 44 */
		HH(a, b, c, d, pX[9], S31, 0xd9d4d039); /* 45 */
		HH(d, a, b, c, pX[12], S32, 0xe6db99e5); /* 46 */
		HH(c, d, a, b, pX[15], S33, 0x1fa27cf8); /* 47 */
		HH(b, c, d, a, pX[2], S34, 0xc4ac5665); /* 48 */

		/* Round 4 */
		II(a, b, c, d, pX[0], S41, 0xf4292244); /* 49 */
		II(d, a, b, c, pX[7], S42, 0x432aff97); /* 50 */
		II(c, d, a, b, pX[14], S43, 0xab9423a7); /* 51 */
		II(b, c, d, a, pX[5], S44, 0xfc93a039); /* 52 */
		II(a, b, c, d, pX[12], S41, 0x655b59c3); /* 53 */
		II(d, a, b, c, pX[3], S42, 0x8f0ccc92); /* 54 */
		II(c, d, a, b, pX[10], S43, 0xffeff47d); /* 55 */
		II(b, c, d, a, pX[1], S44, 0x85845dd1); /* 56 */
		II(a, b, c, d, pX[8], S41, 0x6fa87e4f); /* 57 */
		II(d, a, b, c, pX[15], S42, 0xfe2ce6e0); /* 58 */
		II(c, d, a, b, pX[6], S43, 0xa3014314); /* 59 */
		II(b, c, d, a, pX[13], S44, 0x4e0811a1); /* 60 */
		II(a, b, c, d, pX[4], S41, 0xf7537e82); /* 61 */
		II(d, a, b, c, pX[11], S42, 0xbd3af235); /* 62 */
		II(c, d, a, b, pX[2], S43, 0x2ad7d2bb); /* 63 */
		II(b, c, d, a, pX[9], S44, 0xeb86d391); /* 64 */

		state[0] += a;
		state[1] += b;
		state[2] += c;
		state[3] += d;
	}

	memcpy(pOutHash, state, 16);
}
