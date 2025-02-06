/* ----------------------------------------------------------------------------
Copyright (c) 2018-2023, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#pragma once
#ifndef MIMALLOC_TRACK_H
#define MIMALLOC_TRACK_H

/* ------------------------------------------------------------------------------------------------------
Track memory ranges with macros for tools like Valgrind address sanitizer, or other memory checkers.
These can be defined for tracking allocation:

  #define mi_track_malloc_size(p,reqsize,size,zero)
  #define mi_track_free_size(p,_size)

The macros are set up such that the size passed to `mi_track_free_size`
always matches the size of `mi_track_malloc_size`. (currently, `size == mi_usable_size(p)`).
The `reqsize` is what the user requested, and `size >= reqsize`.
The `size` is either byte precise (and `size==reqsize`) if `MI_PADDING` is enabled,
or otherwise it is the usable block size which may be larger than the original request.
Use `_mi_block_size_of(void* p)` to get the full block size that was allocated (including padding etc).
The `zero` parameter is `true` if the allocated block is zero initialized.

Optional:

  #define mi_track_align(p,alignedp,offset,size)
  #define mi_track_resize(p,oldsize,newsize)
  #define mi_track_init()

The `mi_track_align` is called right after a `mi_track_malloc` for aligned pointers in a block.
The corresponding `mi_track_free` still uses the block start pointer and original size (corresponding to the `mi_track_malloc`).
The `mi_track_resize` is currently unused but could be called on reallocations within a block.
`mi_track_init` is called at program start.

The following macros are for tools like asan and valgrind to track whether memory is 
defined, undefined, or not accessible at all:

  #define mi_track_mem_defined(p,size)
  #define mi_track_mem_undefined(p,size)
  #define mi_track_mem_noaccess(p,size)

-------------------------------------------------------------------------------------------------------*/

#if MI_TRACK_VALGRIND
// valgrind tool

#define MI_TRACK_ENABLED      1
#define MI_TRACK_HEAP_DESTROY 1           // track free of individual blocks on heap_destroy
#define MI_TRACK_TOOL         "valgrind"

#include <valgrind/valgrind.h>
#include <valgrind/memcheck.h>

#define mi_track_malloc_size(p,reqsize,size,zero) VALGRIND_MALLOCLIKE_BLOCK(p,size,MI_PADDING_SIZE /*red zone*/,zero)
#define mi_track_free_size(p,_size)               VALGRIND_FREELIKE_BLOCK(p,MI_PADDING_SIZE /*red zone*/)
#define mi_track_resize(p,oldsize,newsize)        VALGRIND_RESIZEINPLACE_BLOCK(p,oldsize,newsize,MI_PADDING_SIZE /*red zone*/)
#define mi_track_mem_defined(p,size)              VALGRIND_MAKE_MEM_DEFINED(p,size)
#define mi_track_mem_undefined(p,size)            VALGRIND_MAKE_MEM_UNDEFINED(p,size)
#define mi_track_mem_noaccess(p,size)             VALGRIND_MAKE_MEM_NOACCESS(p,size)

#elif MI_TRACK_ASAN
// address sanitizer

#define MI_TRACK_ENABLED      1
#define MI_TRACK_HEAP_DESTROY 0
#define MI_TRACK_TOOL         "asan"

#include <sanitizer/asan_interface.h>

#define mi_track_malloc_size(p,reqsize,size,zero) ASAN_UNPOISON_MEMORY_REGION(p,size)
#define mi_track_free_size(p,size)                ASAN_POISON_MEMORY_REGION(p,size)
#define mi_track_mem_defined(p,size)              ASAN_UNPOISON_MEMORY_REGION(p,size)
#define mi_track_mem_undefined(p,size)            ASAN_UNPOISON_MEMORY_REGION(p,size)
#define mi_track_mem_noaccess(p,size)             ASAN_POISON_MEMORY_REGION(p,size)

#elif MI_TRACK_ETW
// windows event tracing

#define MI_TRACK_ENABLED      1
#define MI_TRACK_HEAP_DESTROY 1
#define MI_TRACK_TOOL         "ETW"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "../src/prim/windows/etw.h"

#define mi_track_init()                           EventRegistermicrosoft_windows_mimalloc();
#define mi_track_malloc_size(p,reqsize,size,zero) EventWriteETW_MI_ALLOC((UINT64)(p), size)
#define mi_track_free_size(p,size)                EventWriteETW_MI_FREE((UINT64)(p), size)

#else
// no tracking

#define MI_TRACK_ENABLED      0
#define MI_TRACK_HEAP_DESTROY 0 
#define MI_TRACK_TOOL         "none"

#define mi_track_malloc_size(p,reqsize,size,zero)
#define mi_track_free_size(p,_size)

#endif

// -------------------
// Utility definitions

#ifndef mi_track_resize
#define mi_track_resize(p,oldsize,newsize)      mi_track_free_size(p,oldsize); mi_track_malloc(p,newsize,false)
#endif

#ifndef mi_track_align
#define mi_track_align(p,alignedp,offset,size)  mi_track_mem_noaccess(p,offset)
#endif

#ifndef mi_track_init
#define mi_track_init()
#endif

#ifndef mi_track_mem_defined
#define mi_track_mem_defined(p,size)
#endif

#ifndef mi_track_mem_undefined
#define mi_track_mem_undefined(p,size)
#endif

#ifndef mi_track_mem_noaccess
#define mi_track_mem_noaccess(p,size)
#endif


#if MI_PADDING
#define mi_track_malloc(p,reqsize,zero) \
  if ((p)!=NULL) { \
    mi_assert_internal(mi_usable_size(p)==(reqsize)); \
    mi_track_malloc_size(p,reqsize,reqsize,zero); \
  }
#else
#define mi_track_malloc(p,reqsize,zero) \
  if ((p)!=NULL) { \
    mi_assert_internal(mi_usable_size(p)>=(reqsize)); \
    mi_track_malloc_size(p,reqsize,mi_usable_size(p),zero); \
  }
#endif

#endif
