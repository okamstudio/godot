/* ----------------------------------------------------------------------------
Copyright (c) 2018-2023, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

// Select the implementation of the primitives
// depending on the OS.

#if defined(_WIN32)
#include "windows/prim.c"  // VirtualAlloc (Windows)

#elif defined(__APPLE__)
#include "osx/prim.c"      // macOSX (actually defers to mmap in unix/prim.c)

#elif defined(__wasi__)
#define MI_USE_SBRK
#include "wasi/prim.c"     // memory-grow or sbrk (Wasm)

#elif defined(__EMSCRIPTEN__)
#include "emscripten/prim.c" // emmalloc_*, + pthread support

#else
#include "unix/prim.c"     // mmap() (Linux, macOSX, BSD, Illumnos, Haiku, DragonFly, etc.)

#endif
