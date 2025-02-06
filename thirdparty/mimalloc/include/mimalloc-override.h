/* ----------------------------------------------------------------------------
Copyright (c) 2018-2020 Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#pragma once
#ifndef MIMALLOC_OVERRIDE_H
#define MIMALLOC_OVERRIDE_H

/* ----------------------------------------------------------------------------
This header can be used to statically redirect malloc/free and new/delete
to the mimalloc variants. This can be useful if one can include this file on
each source file in a project (but be careful when using external code to
not accidentally mix pointers from different allocators).
-----------------------------------------------------------------------------*/

#include <mimalloc.h>

// Standard C allocation
#define malloc(n)               mi_malloc(n)
#define calloc(n,c)             mi_calloc(n,c)
#define realloc(p,n)            mi_realloc(p,n)
#define free(p)                 mi_free(p)

#define strdup(s)               mi_strdup(s)
#define strndup(s,n)            mi_strndup(s,n)
#define realpath(f,n)           mi_realpath(f,n)

// Microsoft extensions
#define _expand(p,n)            mi_expand(p,n)
#define _msize(p)               mi_usable_size(p)
#define _recalloc(p,n,c)        mi_recalloc(p,n,c)

#define _strdup(s)              mi_strdup(s)
#define _strndup(s,n)           mi_strndup(s,n)
#define _wcsdup(s)              (wchar_t*)mi_wcsdup((const unsigned short*)(s))
#define _mbsdup(s)              mi_mbsdup(s)
#define _dupenv_s(b,n,v)        mi_dupenv_s(b,n,v)
#define _wdupenv_s(b,n,v)       mi_wdupenv_s((unsigned short*)(b),n,(const unsigned short*)(v))

// Various Posix and Unix variants
#define reallocf(p,n)           mi_reallocf(p,n)
#define malloc_size(p)          mi_usable_size(p)
#define malloc_usable_size(p)   mi_usable_size(p)
#define malloc_good_size(sz)    mi_malloc_good_size(sz)
#define cfree(p)                mi_free(p)

#define valloc(n)               mi_valloc(n)
#define pvalloc(n)              mi_pvalloc(n)
#define reallocarray(p,s,n)     mi_reallocarray(p,s,n)
#define reallocarr(p,s,n)       mi_reallocarr(p,s,n)
#define memalign(a,n)           mi_memalign(a,n)
#define aligned_alloc(a,n)      mi_aligned_alloc(a,n)
#define posix_memalign(p,a,n)   mi_posix_memalign(p,a,n)
#define _posix_memalign(p,a,n)  mi_posix_memalign(p,a,n)

// Microsoft aligned variants
#define _aligned_malloc(n,a)                  mi_malloc_aligned(n,a)
#define _aligned_realloc(p,n,a)               mi_realloc_aligned(p,n,a)
#define _aligned_recalloc(p,s,n,a)            mi_aligned_recalloc(p,s,n,a)
#define _aligned_msize(p,a,o)                 mi_usable_size(p)
#define _aligned_free(p)                      mi_free(p)
#define _aligned_offset_malloc(n,a,o)         mi_malloc_aligned_at(n,a,o)
#define _aligned_offset_realloc(p,n,a,o)      mi_realloc_aligned_at(p,n,a,o)
#define _aligned_offset_recalloc(p,s,n,a,o)   mi_recalloc_aligned_at(p,s,n,a,o)

#endif // MIMALLOC_OVERRIDE_H
