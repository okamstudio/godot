/* ----------------------------------------------------------------------------
Copyright (c) 2018-2022, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

#include "mimalloc.h"
#include "mimalloc/internal.h"

#if defined(MI_MALLOC_OVERRIDE)

#if !defined(__APPLE__)
#error "this file should only be included on macOS"
#endif

/* ------------------------------------------------------
   Override system malloc on macOS
   This is done through the malloc zone interface.
   It seems to be most robust in combination with interposing
   though or otherwise we may get zone errors as there are could
   be allocations done by the time we take over the
   zone.
------------------------------------------------------ */

#include <AvailabilityMacros.h>
#include <malloc/malloc.h>
#include <string.h>  // memset
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
// only available from OSX 10.6
extern malloc_zone_t* malloc_default_purgeable_zone(void) __attribute__((weak_import));
#endif

/* ------------------------------------------------------
   malloc zone members
------------------------------------------------------ */

static size_t zone_size(malloc_zone_t* zone, const void* p) {
  MI_UNUSED(zone);
  if (!mi_is_in_heap_region(p)){ return 0; } // not our pointer, bail out
  return mi_usable_size(p);
}

static void* zone_malloc(malloc_zone_t* zone, size_t size) {
  MI_UNUSED(zone);
  return mi_malloc(size);
}

static void* zone_calloc(malloc_zone_t* zone, size_t count, size_t size) {
  MI_UNUSED(zone);
  return mi_calloc(count, size);
}

static void* zone_valloc(malloc_zone_t* zone, size_t size) {
  MI_UNUSED(zone);
  return mi_malloc_aligned(size, _mi_os_page_size());
}

static void zone_free(malloc_zone_t* zone, void* p) {
  MI_UNUSED(zone);
  mi_cfree(p);
}

static void* zone_realloc(malloc_zone_t* zone, void* p, size_t newsize) {
  MI_UNUSED(zone);
  return mi_realloc(p, newsize);
}

static void* zone_memalign(malloc_zone_t* zone, size_t alignment, size_t size) {
  MI_UNUSED(zone);
  return mi_malloc_aligned(size,alignment);
}

static void zone_destroy(malloc_zone_t* zone) {
  MI_UNUSED(zone);
  // todo: ignore for now?
}

static unsigned zone_batch_malloc(malloc_zone_t* zone, size_t size, void** ps, unsigned count) {
  size_t i;
  for (i = 0; i < count; i++) {
    ps[i] = zone_malloc(zone, size);
    if (ps[i] == NULL) break;
  }
  return i;
}

static void zone_batch_free(malloc_zone_t* zone, void** ps, unsigned count) {
  for(size_t i = 0; i < count; i++) {
    zone_free(zone, ps[i]);
    ps[i] = NULL;
  }
}

static size_t zone_pressure_relief(malloc_zone_t* zone, size_t size) {
  MI_UNUSED(zone); MI_UNUSED(size);
  mi_collect(false);
  return 0;
}

static void zone_free_definite_size(malloc_zone_t* zone, void* p, size_t size) {
  MI_UNUSED(size);
  zone_free(zone,p);
}

static boolean_t zone_claimed_address(malloc_zone_t* zone, void* p) {
  MI_UNUSED(zone);
  return mi_is_in_heap_region(p);
}


/* ------------------------------------------------------
   Introspection members
------------------------------------------------------ */

static kern_return_t intro_enumerator(task_t task, void* p,
                            unsigned type_mask, vm_address_t zone_address,
                            memory_reader_t reader,
                            vm_range_recorder_t recorder)
{
  // todo: enumerate all memory
  MI_UNUSED(task); MI_UNUSED(p); MI_UNUSED(type_mask); MI_UNUSED(zone_address);
  MI_UNUSED(reader); MI_UNUSED(recorder);
  return KERN_SUCCESS;
}

static size_t intro_good_size(malloc_zone_t* zone, size_t size) {
  MI_UNUSED(zone);
  return mi_good_size(size);
}

static boolean_t intro_check(malloc_zone_t* zone) {
  MI_UNUSED(zone);
  return true;
}

static void intro_print(malloc_zone_t* zone, boolean_t verbose) {
  MI_UNUSED(zone); MI_UNUSED(verbose);
  mi_stats_print(NULL);
}

static void intro_log(malloc_zone_t* zone, void* p) {
  MI_UNUSED(zone); MI_UNUSED(p);
  // todo?
}

static void intro_force_lock(malloc_zone_t* zone) {
  MI_UNUSED(zone);
  // todo?
}

static void intro_force_unlock(malloc_zone_t* zone) {
  MI_UNUSED(zone);
  // todo?
}

static void intro_statistics(malloc_zone_t* zone, malloc_statistics_t* stats) {
  MI_UNUSED(zone);
  // todo...
  stats->blocks_in_use = 0;
  stats->size_in_use = 0;
  stats->max_size_in_use = 0;
  stats->size_allocated = 0;
}

static boolean_t intro_zone_locked(malloc_zone_t* zone) {
  MI_UNUSED(zone);
  return false;
}


/* ------------------------------------------------------
  At process start, override the default allocator
------------------------------------------------------ */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#if defined(__clang__)
#pragma clang diagnostic ignored "-Wc99-extensions"
#endif

static malloc_introspection_t mi_introspect = {
  .enumerator = &intro_enumerator,
  .good_size = &intro_good_size,
  .check = &intro_check,
  .print = &intro_print,
  .log = &intro_log,
  .force_lock = &intro_force_lock,
  .force_unlock = &intro_force_unlock,
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6) && !defined(__ppc__)
  .statistics = &intro_statistics,
  .zone_locked = &intro_zone_locked,
#endif
};

static malloc_zone_t mi_malloc_zone = {
  // note: even with designators, the order is important for C++ compilation
  //.reserved1 = NULL,
  //.reserved2 = NULL,
  .size = &zone_size,
  .malloc = &zone_malloc,
  .calloc = &zone_calloc,
  .valloc = &zone_valloc,
  .free = &zone_free,
  .realloc = &zone_realloc,
  .destroy = &zone_destroy,
  .zone_name = "mimalloc",
  .batch_malloc = &zone_batch_malloc,
  .batch_free = &zone_batch_free,
  .introspect = &mi_introspect,
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6) && !defined(__ppc__)
  #if defined(MAC_OS_X_VERSION_10_14) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_14)
  .version = 10,
  #else
  .version = 9,
  #endif
  // switch to version 9+ on OSX 10.6 to support memalign.
  .memalign = &zone_memalign,
  .free_definite_size = &zone_free_definite_size,
  #if defined(MAC_OS_X_VERSION_10_7) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7)
  .pressure_relief = &zone_pressure_relief,
  #endif
  #if defined(MAC_OS_X_VERSION_10_14) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_14)
  .claimed_address = &zone_claimed_address,
  #endif
#else
  .version = 4,
#endif
};

#ifdef __cplusplus
}
#endif


#if defined(MI_OSX_INTERPOSE) && defined(MI_SHARED_LIB_EXPORT)

// ------------------------------------------------------
// Override malloc_xxx and malloc_zone_xxx api's to use only
// our mimalloc zone. Since even the loader uses malloc
// on macOS, this ensures that all allocations go through
// mimalloc (as all calls are interposed).
// The main `malloc`, `free`, etc calls are interposed in `alloc-override.c`,
// Here, we also override macOS specific API's like
// `malloc_zone_calloc` etc. see <https://github.com/aosm/libmalloc/blob/master/man/malloc_zone_malloc.3>
// ------------------------------------------------------

static inline malloc_zone_t* mi_get_default_zone(void)
{
  static bool init;
  if mi_unlikely(!init) {
    init = true;
    malloc_zone_register(&mi_malloc_zone);  // by calling register we avoid a zone error on free (see <http://eatmyrandom.blogspot.com/2010/03/mallocfree-interception-on-mac-os-x.html>)
  }
  return &mi_malloc_zone;
}

mi_decl_externc int  malloc_jumpstart(uintptr_t cookie);
mi_decl_externc void _malloc_fork_prepare(void);
mi_decl_externc void _malloc_fork_parent(void);
mi_decl_externc void _malloc_fork_child(void);


static malloc_zone_t* mi_malloc_create_zone(vm_size_t size, unsigned flags) {
  MI_UNUSED(size); MI_UNUSED(flags);
  return mi_get_default_zone();
}

static malloc_zone_t* mi_malloc_default_zone (void) {
  return mi_get_default_zone();
}

static malloc_zone_t* mi_malloc_default_purgeable_zone(void) {
  return mi_get_default_zone();
}

static void mi_malloc_destroy_zone(malloc_zone_t* zone) {
  MI_UNUSED(zone);
  // nothing.
}

static kern_return_t mi_malloc_get_all_zones (task_t task, memory_reader_t mr, vm_address_t** addresses, unsigned* count) {
  MI_UNUSED(task); MI_UNUSED(mr);
  if (addresses != NULL) *addresses = NULL;
  if (count != NULL) *count = 0;
  return KERN_SUCCESS;
}

static const char* mi_malloc_get_zone_name(malloc_zone_t* zone) {
  return (zone == NULL ? mi_malloc_zone.zone_name : zone->zone_name);
}

static void mi_malloc_set_zone_name(malloc_zone_t* zone, const char* name) {
  MI_UNUSED(zone); MI_UNUSED(name);
}

static int mi_malloc_jumpstart(uintptr_t cookie) {
  MI_UNUSED(cookie);
  return 1; // or 0 for no error?
}

static void mi__malloc_fork_prepare(void) {
  // nothing
}
static void mi__malloc_fork_parent(void) {
  // nothing
}
static void mi__malloc_fork_child(void) {
  // nothing
}

static void mi_malloc_printf(const char* fmt, ...) {
  MI_UNUSED(fmt);
}

static bool zone_check(malloc_zone_t* zone) {
  MI_UNUSED(zone);
  return true;
}

static malloc_zone_t* zone_from_ptr(const void* p) {
  MI_UNUSED(p);
  return mi_get_default_zone();
}

static void zone_log(malloc_zone_t* zone, void* p) {
  MI_UNUSED(zone); MI_UNUSED(p);
}

static void zone_print(malloc_zone_t* zone, bool b) {
  MI_UNUSED(zone); MI_UNUSED(b);
}

static void zone_print_ptr_info(void* p) {
  MI_UNUSED(p);
}

static void zone_register(malloc_zone_t* zone) {
  MI_UNUSED(zone);
}

static void zone_unregister(malloc_zone_t* zone) {
  MI_UNUSED(zone);
}

// use interposing so `DYLD_INSERT_LIBRARIES` works without `DYLD_FORCE_FLAT_NAMESPACE=1`
// See: <https://books.google.com/books?id=K8vUkpOXhN4C&pg=PA73>
struct mi_interpose_s {
  const void* replacement;
  const void* target;
};
#define MI_INTERPOSE_FUN(oldfun,newfun) { (const void*)&newfun, (const void*)&oldfun }
#define MI_INTERPOSE_MI(fun)            MI_INTERPOSE_FUN(fun,mi_##fun)
#define MI_INTERPOSE_ZONE(fun)          MI_INTERPOSE_FUN(malloc_##fun,fun)
__attribute__((used)) static const struct mi_interpose_s _mi_zone_interposes[]  __attribute__((section("__DATA, __interpose"))) =
{

  MI_INTERPOSE_MI(malloc_create_zone),
  MI_INTERPOSE_MI(malloc_default_purgeable_zone),
  MI_INTERPOSE_MI(malloc_default_zone),
  MI_INTERPOSE_MI(malloc_destroy_zone),
  MI_INTERPOSE_MI(malloc_get_all_zones),
  MI_INTERPOSE_MI(malloc_get_zone_name),
  MI_INTERPOSE_MI(malloc_jumpstart),
  MI_INTERPOSE_MI(malloc_printf),
  MI_INTERPOSE_MI(malloc_set_zone_name),
  MI_INTERPOSE_MI(_malloc_fork_child),
  MI_INTERPOSE_MI(_malloc_fork_parent),
  MI_INTERPOSE_MI(_malloc_fork_prepare),

  MI_INTERPOSE_ZONE(zone_batch_free),
  MI_INTERPOSE_ZONE(zone_batch_malloc),
  MI_INTERPOSE_ZONE(zone_calloc),
  MI_INTERPOSE_ZONE(zone_check),
  MI_INTERPOSE_ZONE(zone_free),
  MI_INTERPOSE_ZONE(zone_from_ptr),
  MI_INTERPOSE_ZONE(zone_log),
  MI_INTERPOSE_ZONE(zone_malloc),
  MI_INTERPOSE_ZONE(zone_memalign),
  MI_INTERPOSE_ZONE(zone_print),
  MI_INTERPOSE_ZONE(zone_print_ptr_info),
  MI_INTERPOSE_ZONE(zone_realloc),
  MI_INTERPOSE_ZONE(zone_register),
  MI_INTERPOSE_ZONE(zone_unregister),
  MI_INTERPOSE_ZONE(zone_valloc)
};


#else

// ------------------------------------------------------
// hook into the zone api's without interposing
// This is the official way of adding an allocator but
// it seems less robust than using interpose.
// ------------------------------------------------------

static inline malloc_zone_t* mi_get_default_zone(void)
{
  // The first returned zone is the real default
  malloc_zone_t** zones = NULL;
  unsigned count = 0;
  kern_return_t ret = malloc_get_all_zones(0, NULL, (vm_address_t**)&zones, &count);
  if (ret == KERN_SUCCESS && count > 0) {
    return zones[0];
  }
  else {
    // fallback
    return malloc_default_zone();
  }
}

#if defined(__clang__)
__attribute__((constructor(0)))
#else
__attribute__((constructor))      // seems not supported by g++-11 on the M1
#endif
__attribute__((used))
static void _mi_macos_override_malloc(void) {
  malloc_zone_t* purgeable_zone = NULL;

  #if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
  // force the purgeable zone to exist to avoid strange bugs
  if (malloc_default_purgeable_zone) {
    purgeable_zone = malloc_default_purgeable_zone();
  }
  #endif

  // Register our zone.
  // thomcc: I think this is still needed to put us in the zone list.
  malloc_zone_register(&mi_malloc_zone);
  // Unregister the default zone, this makes our zone the new default
  // as that was the last registered.
  malloc_zone_t *default_zone = mi_get_default_zone();
  // thomcc: Unsure if the next test is *always* false or just false in the
  // cases I've tried. I'm also unsure if the code inside is needed. at all
  if (default_zone != &mi_malloc_zone) {
    malloc_zone_unregister(default_zone);

    // Reregister the default zone so free and realloc in that zone keep working.
    malloc_zone_register(default_zone);
  }

  // Unregister, and re-register the purgeable_zone to avoid bugs if it occurs
  // earlier than the default zone.
  if (purgeable_zone != NULL) {
    malloc_zone_unregister(purgeable_zone);
    malloc_zone_register(purgeable_zone);
  }

}
#endif  // MI_OSX_INTERPOSE

#endif // MI_MALLOC_OVERRIDE
