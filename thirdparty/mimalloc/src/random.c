/* ----------------------------------------------------------------------------
Copyright (c) 2019-2021, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#include "mimalloc.h"
#include "mimalloc/internal.h"
#include "mimalloc/prim.h"    // _mi_prim_random_buf
#include <string.h>       // memset

/* ----------------------------------------------------------------------------
We use our own PRNG to keep predictable performance of random number generation
and to avoid implementations that use a lock. We only use the OS provided
random source to initialize the initial seeds. Since we do not need ultimate
performance but we do rely on the security (for secret cookies in secure mode)
we use a cryptographically secure generator (chacha20).
-----------------------------------------------------------------------------*/

#define MI_CHACHA_ROUNDS (20)   // perhaps use 12 for better performance?


/* ----------------------------------------------------------------------------
Chacha20 implementation as the original algorithm with a 64-bit nonce
and counter: https://en.wikipedia.org/wiki/Salsa20
The input matrix has sixteen 32-bit values:
Position  0 to  3: constant key
Position  4 to 11: the key
Position 12 to 13: the counter.
Position 14 to 15: the nonce.

The implementation uses regular C code which compiles very well on modern compilers.
(gcc x64 has no register spills, and clang 6+ uses SSE instructions)
-----------------------------------------------------------------------------*/

static inline uint32_t rotl(uint32_t x, uint32_t shift) {
  return (x << shift) | (x >> (32 - shift));
}

static inline void qround(uint32_t x[16], size_t a, size_t b, size_t c, size_t d) {
  x[a] += x[b]; x[d] = rotl(x[d] ^ x[a], 16);
  x[c] += x[d]; x[b] = rotl(x[b] ^ x[c], 12);
  x[a] += x[b]; x[d] = rotl(x[d] ^ x[a], 8);
  x[c] += x[d]; x[b] = rotl(x[b] ^ x[c], 7);
}

static void chacha_block(mi_random_ctx_t* ctx)
{
  // scramble into `x`
  uint32_t x[16];
  for (size_t i = 0; i < 16; i++) {
    x[i] = ctx->input[i];
  }
  for (size_t i = 0; i < MI_CHACHA_ROUNDS; i += 2) {
    qround(x, 0, 4,  8, 12);
    qround(x, 1, 5,  9, 13);
    qround(x, 2, 6, 10, 14);
    qround(x, 3, 7, 11, 15);
    qround(x, 0, 5, 10, 15);
    qround(x, 1, 6, 11, 12);
    qround(x, 2, 7,  8, 13);
    qround(x, 3, 4,  9, 14);
  }

  // add scrambled data to the initial state
  for (size_t i = 0; i < 16; i++) {
    ctx->output[i] = x[i] + ctx->input[i];
  }
  ctx->output_available = 16;

  // increment the counter for the next round
  ctx->input[12] += 1;
  if (ctx->input[12] == 0) {
    ctx->input[13] += 1;
    if (ctx->input[13] == 0) {  // and keep increasing into the nonce
      ctx->input[14] += 1;
    }
  }
}

static uint32_t chacha_next32(mi_random_ctx_t* ctx) {
  if (ctx->output_available <= 0) {
    chacha_block(ctx);
    ctx->output_available = 16; // (assign again to suppress static analysis warning)
  }
  const uint32_t x = ctx->output[16 - ctx->output_available];
  ctx->output[16 - ctx->output_available] = 0; // reset once the data is handed out
  ctx->output_available--;
  return x;
}

static inline uint32_t read32(const uint8_t* p, size_t idx32) {
  const size_t i = 4*idx32;
  return ((uint32_t)p[i+0] | (uint32_t)p[i+1] << 8 | (uint32_t)p[i+2] << 16 | (uint32_t)p[i+3] << 24);
}

static void chacha_init(mi_random_ctx_t* ctx, const uint8_t key[32], uint64_t nonce)
{
  // since we only use chacha for randomness (and not encryption) we
  // do not _need_ to read 32-bit values as little endian but we do anyways
  // just for being compatible :-)
  memset(ctx, 0, sizeof(*ctx));
  for (size_t i = 0; i < 4; i++) {
    const uint8_t* sigma = (uint8_t*)"expand 32-byte k";
    ctx->input[i] = read32(sigma,i);
  }
  for (size_t i = 0; i < 8; i++) {
    ctx->input[i + 4] = read32(key,i);
  }
  ctx->input[12] = 0;
  ctx->input[13] = 0;
  ctx->input[14] = (uint32_t)nonce;
  ctx->input[15] = (uint32_t)(nonce >> 32);
}

static void chacha_split(mi_random_ctx_t* ctx, uint64_t nonce, mi_random_ctx_t* ctx_new) {
  memset(ctx_new, 0, sizeof(*ctx_new));
  _mi_memcpy(ctx_new->input, ctx->input, sizeof(ctx_new->input));
  ctx_new->input[12] = 0;
  ctx_new->input[13] = 0;
  ctx_new->input[14] = (uint32_t)nonce;
  ctx_new->input[15] = (uint32_t)(nonce >> 32);
  mi_assert_internal(ctx->input[14] != ctx_new->input[14] || ctx->input[15] != ctx_new->input[15]); // do not reuse nonces!
  chacha_block(ctx_new);
}


/* ----------------------------------------------------------------------------
Random interface
-----------------------------------------------------------------------------*/

#if MI_DEBUG>1
static bool mi_random_is_initialized(mi_random_ctx_t* ctx) {
  return (ctx != NULL && ctx->input[0] != 0);
}
#endif

void _mi_random_split(mi_random_ctx_t* ctx, mi_random_ctx_t* ctx_new) {
  mi_assert_internal(mi_random_is_initialized(ctx));
  mi_assert_internal(ctx != ctx_new);
  chacha_split(ctx, (uintptr_t)ctx_new /*nonce*/, ctx_new);
}

uintptr_t _mi_random_next(mi_random_ctx_t* ctx) {
  mi_assert_internal(mi_random_is_initialized(ctx));
  #if MI_INTPTR_SIZE <= 4
    return chacha_next32(ctx);
  #elif MI_INTPTR_SIZE == 8
    return (((uintptr_t)chacha_next32(ctx) << 32) | chacha_next32(ctx));
  #else
  # error "define mi_random_next for this platform"
  #endif
}


/* ----------------------------------------------------------------------------
To initialize a fresh random context.
If we cannot get good randomness, we fall back to weak randomness based on a timer and ASLR.
-----------------------------------------------------------------------------*/

uintptr_t _mi_os_random_weak(uintptr_t extra_seed) {
  uintptr_t x = (uintptr_t)&_mi_os_random_weak ^ extra_seed; // ASLR makes the address random
  x ^= _mi_prim_clock_now();  
  // and do a few randomization steps
  uintptr_t max = ((x ^ (x >> 17)) & 0x0F) + 1;
  for (uintptr_t i = 0; i < max; i++) {
    x = _mi_random_shuffle(x);
  }
  mi_assert_internal(x != 0);
  return x;
}

static void mi_random_init_ex(mi_random_ctx_t* ctx, bool use_weak) {
  uint8_t key[32];
  if (use_weak || !_mi_prim_random_buf(key, sizeof(key))) {
    // if we fail to get random data from the OS, we fall back to a
    // weak random source based on the current time
    #if !defined(__wasi__)
    if (!use_weak) { _mi_warning_message("unable to use secure randomness\n"); }
    #endif
    uintptr_t x = _mi_os_random_weak(0);
    for (size_t i = 0; i < 8; i++) {  // key is eight 32-bit words.
      x = _mi_random_shuffle(x);
      ((uint32_t*)key)[i] = (uint32_t)x;
    }
    ctx->weak = true;
  }
  else {
    ctx->weak = false;
  }
  chacha_init(ctx, key, (uintptr_t)ctx /*nonce*/ );
}

void _mi_random_init(mi_random_ctx_t* ctx) {
  mi_random_init_ex(ctx, false);
}

void _mi_random_init_weak(mi_random_ctx_t * ctx) {
  mi_random_init_ex(ctx, true);
}

void _mi_random_reinit_if_weak(mi_random_ctx_t * ctx) {
  if (ctx->weak) {
    _mi_random_init(ctx);
  }
}

/* --------------------------------------------------------
test vectors from <https://tools.ietf.org/html/rfc8439>
----------------------------------------------------------- */
/*
static bool array_equals(uint32_t* x, uint32_t* y, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (x[i] != y[i]) return false;
  }
  return true;
}
static void chacha_test(void)
{
  uint32_t x[4] = { 0x11111111, 0x01020304, 0x9b8d6f43, 0x01234567 };
  uint32_t x_out[4] = { 0xea2a92f4, 0xcb1cf8ce, 0x4581472e, 0x5881c4bb };
  qround(x, 0, 1, 2, 3);
  mi_assert_internal(array_equals(x, x_out, 4));

  uint32_t y[16] = {
       0x879531e0,  0xc5ecf37d,  0x516461b1,  0xc9a62f8a,
       0x44c20ef3,  0x3390af7f,  0xd9fc690b,  0x2a5f714c,
       0x53372767,  0xb00a5631,  0x974c541a,  0x359e9963,
       0x5c971061,  0x3d631689,  0x2098d9d6,  0x91dbd320 };
  uint32_t y_out[16] = {
       0x879531e0,  0xc5ecf37d,  0xbdb886dc,  0xc9a62f8a,
       0x44c20ef3,  0x3390af7f,  0xd9fc690b,  0xcfacafd2,
       0xe46bea80,  0xb00a5631,  0x974c541a,  0x359e9963,
       0x5c971061,  0xccc07c79,  0x2098d9d6,  0x91dbd320 };
  qround(y, 2, 7, 8, 13);
  mi_assert_internal(array_equals(y, y_out, 16));

  mi_random_ctx_t r = {
    { 0x61707865, 0x3320646e, 0x79622d32, 0x6b206574,
      0x03020100, 0x07060504, 0x0b0a0908, 0x0f0e0d0c,
      0x13121110, 0x17161514, 0x1b1a1918, 0x1f1e1d1c,
      0x00000001, 0x09000000, 0x4a000000, 0x00000000 },
    {0},
    0
  };
  uint32_t r_out[16] = {
       0xe4e7f110, 0x15593bd1, 0x1fdd0f50, 0xc47120a3,
       0xc7f4d1c7, 0x0368c033, 0x9aaa2204, 0x4e6cd4c3,
       0x466482d2, 0x09aa9f07, 0x05d7c214, 0xa2028bd9,
       0xd19c12b5, 0xb94e16de, 0xe883d0cb, 0x4e3c50a2 };
  chacha_block(&r);
  mi_assert_internal(array_equals(r.output, r_out, 16));
}
*/
