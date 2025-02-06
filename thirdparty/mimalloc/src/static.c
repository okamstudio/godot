/* ----------------------------------------------------------------------------
Copyright (c) 2018-2020, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#if defined(__sun)
// same remarks as os.c for the static's context.
#undef _XOPEN_SOURCE
#undef _POSIX_C_SOURCE
#endif

#include "mimalloc.h"
#include "mimalloc/internal.h"

// For a static override we create a single object file
// containing the whole library. If it is linked first
// it will override all the standard library allocation
// functions (on Unix's).
#include "alloc.c"          // includes alloc-override.c
#include "alloc-aligned.c"
#include "alloc-posix.c"
#include "arena.c"
#include "bitmap.c"
#include "heap.c"
#include "init.c"
#include "libc.c"
#include "options.c"
#include "os.c"
#include "page.c"           // includes page-queue.c
#include "random.c" 
#include "segment.c"
#include "segment-map.c"
#include "stats.c"
#include "prim/prim.c"
#if MI_OSX_ZONE
#include "prim/osx/alloc-override-zone.c"
#endif
