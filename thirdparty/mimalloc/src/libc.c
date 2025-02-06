/* ----------------------------------------------------------------------------
Copyright (c) 2018-2023, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

// --------------------------------------------------------
// This module defines various std libc functions to reduce
// the dependency on libc, and also prevent errors caused 
// by some libc implementations when called before `main`
// executes (due to malloc redirection)
// --------------------------------------------------------

#include "mimalloc.h"
#include "mimalloc/internal.h"
#include "mimalloc/prim.h"      // mi_prim_getenv

char _mi_toupper(char c) {
  if (c >= 'a' && c <= 'z') return (c - 'a' + 'A');
                       else return c;
}

int _mi_strnicmp(const char* s, const char* t, size_t n) {
  if (n == 0) return 0;
  for (; *s != 0 && *t != 0 && n > 0; s++, t++, n--) {
    if (_mi_toupper(*s) != _mi_toupper(*t)) break;
  }
  return (n == 0 ? 0 : *s - *t);
}

void _mi_strlcpy(char* dest, const char* src, size_t dest_size) {
  if (dest==NULL || src==NULL || dest_size == 0) return;
  // copy until end of src, or when dest is (almost) full
  while (*src != 0 && dest_size > 1) {
    *dest++ = *src++;
    dest_size--;
  }
  // always zero terminate
  *dest = 0;
}

void _mi_strlcat(char* dest, const char* src, size_t dest_size) {
  if (dest==NULL || src==NULL || dest_size == 0) return;
  // find end of string in the dest buffer
  while (*dest != 0 && dest_size > 1) {
    dest++;
    dest_size--;
  }
  // and catenate
  _mi_strlcpy(dest, src, dest_size);
}

size_t _mi_strlen(const char* s) {
  if (s==NULL) return 0;
  size_t len = 0;
  while(s[len] != 0) { len++; }
  return len;
}

size_t _mi_strnlen(const char* s, size_t max_len) {
  if (s==NULL) return 0;
  size_t len = 0;
  while(s[len] != 0 && len < max_len) { len++; }
  return len;
}

#ifdef MI_NO_GETENV
bool _mi_getenv(const char* name, char* result, size_t result_size) {
  MI_UNUSED(name);
  MI_UNUSED(result);
  MI_UNUSED(result_size);
  return false;
}
#else
bool _mi_getenv(const char* name, char* result, size_t result_size) {
  if (name==NULL || result == NULL || result_size < 64) return false;
  return _mi_prim_getenv(name,result,result_size);
}
#endif

// --------------------------------------------------------
// Define our own limited `_mi_vsnprintf` and `_mi_snprintf`
// This is mostly to avoid calling these when libc is not yet
// initialized (and to reduce dependencies)
// 
// format:      d i, p x u, s
// prec:        z l ll L
// width:       10
// align-left:  -
// fill:        0
// plus:        +
// --------------------------------------------------------

static void mi_outc(char c, char** out, char* end) {
  char* p = *out;
  if (p >= end) return;
  *p = c;
  *out = p + 1;
}

static void mi_outs(const char* s, char** out, char* end) {
  if (s == NULL) return;
  char* p = *out;
  while (*s != 0 && p < end) {
    *p++ = *s++;
  }
  *out = p;
}

static void mi_out_fill(char fill, size_t len, char** out, char* end) {
  char* p = *out;
  for (size_t i = 0; i < len && p < end; i++) {
    *p++ = fill;
  }
  *out = p;
}

static void mi_out_alignright(char fill, char* start, size_t len, size_t extra, char* end) {
  if (len == 0 || extra == 0) return;
  if (start + len + extra >= end) return;
  // move `len` characters to the right (in reverse since it can overlap)
  for (size_t i = 1; i <= len; i++) {
    start[len + extra - i] = start[len - i];
  }
  // and fill the start
  for (size_t i = 0; i < extra; i++) {
    start[i] = fill;
  }
}


static void mi_out_num(uintptr_t x, size_t base, char prefix, char** out, char* end) 
{
  if (x == 0 || base == 0 || base > 16) {
    if (prefix != 0) { mi_outc(prefix, out, end); }
    mi_outc('0',out,end);
  }
  else {
    // output digits in reverse
    char* start = *out;
    while (x > 0) {
      char digit = (char)(x % base);
      mi_outc((digit <= 9 ? '0' + digit : 'A' + digit - 10),out,end);
      x = x / base;
    }
    if (prefix != 0) { 
      mi_outc(prefix, out, end); 
    }
    size_t len = *out - start;
    // and reverse in-place
    for (size_t i = 0; i < (len / 2); i++) {
      char c = start[len - i - 1];
      start[len - i - 1] = start[i];
      start[i] = c;
    }
  }
}


#define MI_NEXTC()  c = *in; if (c==0) break; in++;

void _mi_vsnprintf(char* buf, size_t bufsize, const char* fmt, va_list args) {
  if (buf == NULL || bufsize == 0 || fmt == NULL) return;
  buf[bufsize - 1] = 0;
  char* const end = buf + (bufsize - 1);
  const char* in = fmt;
  char* out = buf;
  while (true) {
    if (out >= end) break;
    char c;
    MI_NEXTC();
    if (c != '%') {
      if ((c >= ' ' && c <= '~') || c=='\n' || c=='\r' || c=='\t') { // output visible ascii or standard control only
        mi_outc(c, &out, end);
      }
    }
    else {
      MI_NEXTC();
      char   fill = ' ';
      size_t width = 0;
      char   numtype = 'd';
      char   numplus = 0;
      bool   alignright = true; 
      if (c == '+' || c == ' ') { numplus = c; MI_NEXTC(); }
      if (c == '-') { alignright = false; MI_NEXTC(); }
      if (c == '0') { fill = '0'; MI_NEXTC(); }
      if (c >= '1' && c <= '9') {
        width = (c - '0'); MI_NEXTC();
        while (c >= '0' && c <= '9') {
          width = (10 * width) + (c - '0'); MI_NEXTC();
        }
        if (c == 0) break;  // extra check due to while
      }      
      if (c == 'z' || c == 't' || c == 'L') { numtype = c; MI_NEXTC(); }
      else if (c == 'l') {
        numtype = c; MI_NEXTC();
        if (c == 'l') { numtype = 'L'; MI_NEXTC(); }
      }

      char* start = out;
      if (c == 's') {
        // string
        const char* s = va_arg(args, const char*);
        mi_outs(s, &out, end);
      }
      else if (c == 'p' || c == 'x' || c == 'u') {
        // unsigned
        uintptr_t x = 0;
        if (c == 'x' || c == 'u') {
          if (numtype == 'z')       x = va_arg(args, size_t);
          else if (numtype == 't')  x = va_arg(args, uintptr_t); // unsigned ptrdiff_t
          else if (numtype == 'L')  x = (uintptr_t)va_arg(args, unsigned long long);
                               else x = va_arg(args, unsigned long);
        }
        else if (c == 'p') {
          x = va_arg(args, uintptr_t);
          mi_outs("0x", &out, end);
          start = out;
          width = (width >= 2 ? width - 2 : 0);
        }
        if (width == 0 && (c == 'x' || c == 'p')) {
          if (c == 'p')   { width = 2 * (x <= UINT32_MAX ? 4 : ((x >> 16) <= UINT32_MAX ? 6 : sizeof(void*))); }
          if (width == 0) { width = 2; }
          fill = '0';
        }
        mi_out_num(x, (c == 'x' || c == 'p' ? 16 : 10), numplus, &out, end);
      }
      else if (c == 'i' || c == 'd') {
        // signed
        intptr_t x = 0;
        if (numtype == 'z')       x = va_arg(args, intptr_t );
        else if (numtype == 't')  x = va_arg(args, ptrdiff_t);
        else if (numtype == 'L')  x = (intptr_t)va_arg(args, long long);
                             else x = va_arg(args, long);
        char pre = 0;
        if (x < 0) {
          pre = '-';
          if (x > INTPTR_MIN) { x = -x; }
        }
        else if (numplus != 0) {
          pre = numplus;
        }
        mi_out_num((uintptr_t)x, 10, pre, &out, end);
      }
      else if (c >= ' ' && c <= '~') {
        // unknown format
        mi_outc('%', &out, end);
        mi_outc(c, &out, end);
      }

      // fill & align
      mi_assert_internal(out <= end);
      mi_assert_internal(out >= start);
      const size_t len = out - start;
      if (len < width) {
        mi_out_fill(fill, width - len, &out, end);
        if (alignright && out <= end) {
          mi_out_alignright(fill, start, len, width - len, end);
        }
      }
    }
  }
  mi_assert_internal(out <= end);
  *out = 0;
}

void _mi_snprintf(char* buf, size_t buflen, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  _mi_vsnprintf(buf, buflen, fmt, args);
  va_end(args);
}
