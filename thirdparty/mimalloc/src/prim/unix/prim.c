/* ----------------------------------------------------------------------------
Copyright (c) 2018-2023, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

// This file is included in `src/prim/prim.c`

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE   // ensure mmap flags and syscall are defined
#endif

#if defined(__sun)
// illumos provides new mman.h api when any of these are defined
// otherwise the old api based on caddr_t which predates the void pointers one.
// stock solaris provides only the former, chose to atomically to discard those
// flags only here rather than project wide tough.
#undef _XOPEN_SOURCE
#undef _POSIX_C_SOURCE
#endif

#include "mimalloc.h"
#include "mimalloc/internal.h"
#include "mimalloc/atomic.h"
#include "mimalloc/prim.h"

#include <sys/mman.h>  // mmap
#include <unistd.h>    // sysconf
#include <fcntl.h>     // open, close, read, access

#if defined(__linux__)
  #include <features.h>
  #if defined(MI_NO_THP)
  #include <sys/prctl.h>
  #endif
  #if defined(__GLIBC__)
  #include <linux/mman.h> // linux mmap flags
  #else
  #include <sys/mman.h>
  #endif
#elif defined(__APPLE__)
  #include <AvailabilityMacros.h>
  #include <TargetConditionals.h>
  #if !defined(TARGET_OS_OSX) || TARGET_OS_OSX   // see issue #879, used to be (!TARGET_IOS_IPHONE && !TARGET_IOS_SIMULATOR)
  #include <mach/vm_statistics.h>    // VM_MAKE_TAG, VM_FLAGS_SUPERPAGE_SIZE_2MB, etc.
  #endif
  #if !defined(MAC_OS_X_VERSION_10_7)
  #define MAC_OS_X_VERSION_10_7   1070
  #endif
#elif defined(__FreeBSD__) || defined(__DragonFly__)
  #include <sys/param.h>
  #if __FreeBSD_version >= 1200000
  #include <sys/cpuset.h>
  #include <sys/domainset.h>
  #endif
  #include <sys/sysctl.h>
#endif

#if defined(__linux__) || defined(__FreeBSD__)
  #define MI_HAS_SYSCALL_H
  #include <sys/syscall.h>
#endif


//------------------------------------------------------------------------------------
// Use syscalls for some primitives to allow for libraries that override open/read/close etc.
// and do allocation themselves; using syscalls prevents recursion when mimalloc is
// still initializing (issue #713)
// Declare inline to avoid unused function warnings.
//------------------------------------------------------------------------------------

#if defined(MI_HAS_SYSCALL_H) && defined(SYS_open) && defined(SYS_close) && defined(SYS_read) && defined(SYS_access)

static inline int mi_prim_open(const char* fpath, int open_flags) {
  return syscall(SYS_open,fpath,open_flags,0);
}
static inline ssize_t mi_prim_read(int fd, void* buf, size_t bufsize) {
  return syscall(SYS_read,fd,buf,bufsize);
}
static inline int mi_prim_close(int fd) {
  return syscall(SYS_close,fd);
}
static inline int mi_prim_access(const char *fpath, int mode) {
  return syscall(SYS_access,fpath,mode);
}

#else

static inline int mi_prim_open(const char* fpath, int open_flags) {
  return open(fpath,open_flags);
}
static inline ssize_t mi_prim_read(int fd, void* buf, size_t bufsize) {
  return read(fd,buf,bufsize);
}
static inline int mi_prim_close(int fd) {
  return close(fd);
}
static inline int mi_prim_access(const char *fpath, int mode) {
  return access(fpath,mode);
}

#endif



//---------------------------------------------
// init
//---------------------------------------------

static bool unix_detect_overcommit(void) {
  bool os_overcommit = true;
#if defined(__linux__)
  int fd = mi_prim_open("/proc/sys/vm/overcommit_memory", O_RDONLY);
	if (fd >= 0) {
    char buf[32];
    ssize_t nread = mi_prim_read(fd, &buf, sizeof(buf));
    mi_prim_close(fd);
    // <https://www.kernel.org/doc/Documentation/vm/overcommit-accounting>
    // 0: heuristic overcommit, 1: always overcommit, 2: never overcommit (ignore NORESERVE)
    if (nread >= 1) {
      os_overcommit = (buf[0] == '0' || buf[0] == '1');
    }
  }
#elif defined(__FreeBSD__)
  int val = 0;
  size_t olen = sizeof(val);
  if (sysctlbyname("vm.overcommit", &val, &olen, NULL, 0) == 0) {
    os_overcommit = (val != 0);
  }
#else
  // default: overcommit is true
#endif
  return os_overcommit;
}

void _mi_prim_mem_init( mi_os_mem_config_t* config )
{
  long psize = sysconf(_SC_PAGESIZE);
  if (psize > 0) {
    config->page_size = (size_t)psize;
    config->alloc_granularity = (size_t)psize;
  }
  config->large_page_size = 2*MI_MiB; // TODO: can we query the OS for this?
  config->has_overcommit = unix_detect_overcommit();
  config->has_partial_free = true;    // mmap can free in parts
  config->has_virtual_reserve = true; // todo: check if this true for NetBSD?  (for anonymous mmap with PROT_NONE)

  // disable transparent huge pages for this process?
  #if (defined(__linux__) || defined(__ANDROID__)) && defined(PR_GET_THP_DISABLE)
  #if defined(MI_NO_THP)
  if (true)
  #else
  if (!mi_option_is_enabled(mi_option_allow_large_os_pages)) // disable THP also if large OS pages are not allowed in the options
  #endif
  {
    int val = 0;
    if (prctl(PR_GET_THP_DISABLE, &val, 0, 0, 0) != 0) {
      // Most likely since distros often come with always/madvise settings.
      val = 1;
      // Disabling only for mimalloc process rather than touching system wide settings
      (void)prctl(PR_SET_THP_DISABLE, &val, 0, 0, 0);
    }
  }
  #endif
}


//---------------------------------------------
// free
//---------------------------------------------

int _mi_prim_free(void* addr, size_t size ) {
  bool err = (munmap(addr, size) == -1);
  return (err ? errno : 0);
}


//---------------------------------------------
// mmap
//---------------------------------------------

static int unix_madvise(void* addr, size_t size, int advice) {
  #if defined(__sun)
  return madvise((caddr_t)addr, size, advice);  // Solaris needs cast (issue #520)
  #else
  return madvise(addr, size, advice);
  #endif
}

static void* unix_mmap_prim(void* addr, size_t size, size_t try_alignment, int protect_flags, int flags, int fd) {
  MI_UNUSED(try_alignment);
  void* p = NULL;
  #if defined(MAP_ALIGNED)  // BSD
  if (addr == NULL && try_alignment > 1 && (try_alignment % _mi_os_page_size()) == 0) {
    size_t n = mi_bsr(try_alignment);
    if (((size_t)1 << n) == try_alignment && n >= 12 && n <= 30) {  // alignment is a power of 2 and 4096 <= alignment <= 1GiB
      p = mmap(addr, size, protect_flags, flags | MAP_ALIGNED(n), fd, 0);
      if (p==MAP_FAILED || !_mi_is_aligned(p,try_alignment)) {
        int err = errno;
        _mi_trace_message("unable to directly request aligned OS memory (error: %d (0x%x), size: 0x%zx bytes, alignment: 0x%zx, hint address: %p)\n", err, err, size, try_alignment, addr);
      }
      if (p!=MAP_FAILED) return p;
      // fall back to regular mmap
    }
  }
  #elif defined(MAP_ALIGN)  // Solaris
  if (addr == NULL && try_alignment > 1 && (try_alignment % _mi_os_page_size()) == 0) {
    p = mmap((void*)try_alignment, size, protect_flags, flags | MAP_ALIGN, fd, 0);  // addr parameter is the required alignment
    if (p!=MAP_FAILED) return p;
    // fall back to regular mmap
  }
  #endif
  #if (MI_INTPTR_SIZE >= 8) && !defined(MAP_ALIGNED)
  // on 64-bit systems, use the virtual address area after 2TiB for 4MiB aligned allocations
  if (addr == NULL) {
    void* hint = _mi_os_get_aligned_hint(try_alignment, size);
    if (hint != NULL) {
      p = mmap(hint, size, protect_flags, flags, fd, 0);
      if (p==MAP_FAILED || !_mi_is_aligned(p,try_alignment)) {
        #if MI_TRACK_ENABLED  // asan sometimes does not instrument errno correctly?
        int err = 0;
        #else
        int err = errno;
        #endif
        _mi_trace_message("unable to directly request hinted aligned OS memory (error: %d (0x%x), size: 0x%zx bytes, alignment: 0x%zx, hint address: %p)\n", err, err, size, try_alignment, hint);
      }
      if (p!=MAP_FAILED) return p;
      // fall back to regular mmap
    }
  }
  #endif
  // regular mmap
  p = mmap(addr, size, protect_flags, flags, fd, 0);
  if (p!=MAP_FAILED) return p;
  // failed to allocate
  return NULL;
}

static int unix_mmap_fd(void) {
  #if defined(VM_MAKE_TAG)
  // macOS: tracking anonymous page with a specific ID. (All up to 98 are taken officially but LLVM sanitizers had taken 99)
  int os_tag = (int)mi_option_get(mi_option_os_tag);
  if (os_tag < 100 || os_tag > 255) { os_tag = 100; }
  return VM_MAKE_TAG(os_tag);
  #else
  return -1;
  #endif
}

static void* unix_mmap(void* addr, size_t size, size_t try_alignment, int protect_flags, bool large_only, bool allow_large, bool* is_large) {
  #if !defined(MAP_ANONYMOUS)
  #define MAP_ANONYMOUS  MAP_ANON
  #endif
  #if !defined(MAP_NORESERVE)
  #define MAP_NORESERVE  0
  #endif
  void* p = NULL;
  const int fd = unix_mmap_fd();
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  if (_mi_os_has_overcommit()) {
    flags |= MAP_NORESERVE;
  }
  #if defined(PROT_MAX)
  protect_flags |= PROT_MAX(PROT_READ | PROT_WRITE); // BSD
  #endif
  // huge page allocation
  if ((large_only || _mi_os_use_large_page(size, try_alignment)) && allow_large) {
    static _Atomic(size_t) large_page_try_ok; // = 0;
    size_t try_ok = mi_atomic_load_acquire(&large_page_try_ok);
    if (!large_only && try_ok > 0) {
      // If the OS is not configured for large OS pages, or the user does not have
      // enough permission, the `mmap` will always fail (but it might also fail for other reasons).
      // Therefore, once a large page allocation failed, we don't try again for `large_page_try_ok` times
      // to avoid too many failing calls to mmap.
      mi_atomic_cas_strong_acq_rel(&large_page_try_ok, &try_ok, try_ok - 1);
    }
    else {
      int lflags = flags & ~MAP_NORESERVE;  // using NORESERVE on huge pages seems to fail on Linux
      int lfd = fd;
      #ifdef MAP_ALIGNED_SUPER
      lflags |= MAP_ALIGNED_SUPER;
      #endif
      #ifdef MAP_HUGETLB
      lflags |= MAP_HUGETLB;
      #endif
      #ifdef MAP_HUGE_1GB
      static bool mi_huge_pages_available = true;
      if ((size % MI_GiB) == 0 && mi_huge_pages_available) {
        lflags |= MAP_HUGE_1GB;
      }
      else
      #endif
      {
        #ifdef MAP_HUGE_2MB
        lflags |= MAP_HUGE_2MB;
        #endif
      }
      #ifdef VM_FLAGS_SUPERPAGE_SIZE_2MB
      lfd |= VM_FLAGS_SUPERPAGE_SIZE_2MB;
      #endif
      if (large_only || lflags != flags) {
        // try large OS page allocation
        *is_large = true;
        p = unix_mmap_prim(addr, size, try_alignment, protect_flags, lflags, lfd);
        #ifdef MAP_HUGE_1GB
        if (p == NULL && (lflags & MAP_HUGE_1GB) == MAP_HUGE_1GB) {
          mi_huge_pages_available = false; // don't try huge 1GiB pages again
          _mi_warning_message("unable to allocate huge (1GiB) page, trying large (2MiB) pages instead (errno: %i)\n", errno);
          lflags = ((lflags & ~MAP_HUGE_1GB) | MAP_HUGE_2MB);
          p = unix_mmap_prim(addr, size, try_alignment, protect_flags, lflags, lfd);
        }
        #endif
        if (large_only) return p;
        if (p == NULL) {
          mi_atomic_store_release(&large_page_try_ok, (size_t)8);  // on error, don't try again for the next N allocations
        }
      }
    }
  }
  // regular allocation
  if (p == NULL) {
    *is_large = false;
    p = unix_mmap_prim(addr, size, try_alignment, protect_flags, flags, fd);
    if (p != NULL) {
      #if defined(MADV_HUGEPAGE)
      // Many Linux systems don't allow MAP_HUGETLB but they support instead
      // transparent huge pages (THP). Generally, it is not required to call `madvise` with MADV_HUGE
      // though since properly aligned allocations will already use large pages if available
      // in that case -- in particular for our large regions (in `memory.c`).
      // However, some systems only allow THP if called with explicit `madvise`, so
      // when large OS pages are enabled for mimalloc, we call `madvise` anyways.
      if (allow_large && _mi_os_use_large_page(size, try_alignment)) {
        if (unix_madvise(p, size, MADV_HUGEPAGE) == 0) {
          *is_large = true; // possibly
        };
      }
      #elif defined(__sun)
      if (allow_large && _mi_os_use_large_page(size, try_alignment)) {
        struct memcntl_mha cmd = {0};
        cmd.mha_pagesize = _mi_os_large_page_size();
        cmd.mha_cmd = MHA_MAPSIZE_VA;
        if (memcntl((caddr_t)p, size, MC_HAT_ADVISE, (caddr_t)&cmd, 0, 0) == 0) {
          *is_large = true;
        }
      }
      #endif
    }
  }
  return p;
}

// Note: the `try_alignment` is just a hint and the returned pointer is not guaranteed to be aligned.
int _mi_prim_alloc(size_t size, size_t try_alignment, bool commit, bool allow_large, bool* is_large, bool* is_zero, void** addr) {
  mi_assert_internal(size > 0 && (size % _mi_os_page_size()) == 0);
  mi_assert_internal(commit || !allow_large);
  mi_assert_internal(try_alignment > 0);

  *is_zero = true;
  int protect_flags = (commit ? (PROT_WRITE | PROT_READ) : PROT_NONE);
  *addr = unix_mmap(NULL, size, try_alignment, protect_flags, false, allow_large, is_large);
  return (*addr != NULL ? 0 : errno);
}


//---------------------------------------------
// Commit/Reset
//---------------------------------------------

static void unix_mprotect_hint(int err) {
  #if defined(__linux__) && (MI_SECURE>=2) // guard page around every mimalloc page
  if (err == ENOMEM) {
    _mi_warning_message("The next warning may be caused by a low memory map limit.\n"
                        "  On Linux this is controlled by the vm.max_map_count -- maybe increase it?\n"
                        "  For example: sudo sysctl -w vm.max_map_count=262144\n");
  }
  #else
  MI_UNUSED(err);
  #endif
}

int _mi_prim_commit(void* start, size_t size, bool* is_zero) {
  // commit: ensure we can access the area
  // note: we may think that *is_zero can be true since the memory
  // was either from mmap PROT_NONE, or from decommit MADV_DONTNEED, but
  // we sometimes call commit on a range with still partially committed
  // memory and `mprotect` does not zero the range.
  *is_zero = false;
  int err = mprotect(start, size, (PROT_READ | PROT_WRITE));
  if (err != 0) {
    err = errno;
    unix_mprotect_hint(err);
  }
  return err;
}

int _mi_prim_decommit(void* start, size_t size, bool* needs_recommit) {
  int err = 0;
  // decommit: use MADV_DONTNEED as it decreases rss immediately (unlike MADV_FREE)
  err = unix_madvise(start, size, MADV_DONTNEED);
  #if !MI_DEBUG && !MI_SECURE
    *needs_recommit = false;
  #else
    *needs_recommit = true;
    mprotect(start, size, PROT_NONE);
  #endif
  /*
  // decommit: use mmap with MAP_FIXED and PROT_NONE to discard the existing memory (and reduce rss)
  *needs_recommit = true;
  const int fd = unix_mmap_fd();
  void* p = mmap(start, size, PROT_NONE, (MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE), fd, 0);
  if (p != start) { err = errno; }
  */
  return err;
}

int _mi_prim_reset(void* start, size_t size) {
  // We try to use `MADV_FREE` as that is the fastest. A drawback though is that it
  // will not reduce the `rss` stats in tools like `top` even though the memory is available
  // to other processes. With the default `MIMALLOC_PURGE_DECOMMITS=1` we ensure that by
  // default `MADV_DONTNEED` is used though.
  #if defined(MADV_FREE)
  static _Atomic(size_t) advice = MI_ATOMIC_VAR_INIT(MADV_FREE);
  int oadvice = (int)mi_atomic_load_relaxed(&advice);
  int err;
  while ((err = unix_madvise(start, size, oadvice)) != 0 && errno == EAGAIN) { errno = 0;  };
  if (err != 0 && errno == EINVAL && oadvice == MADV_FREE) {
    // if MADV_FREE is not supported, fall back to MADV_DONTNEED from now on
    mi_atomic_store_release(&advice, (size_t)MADV_DONTNEED);
    err = unix_madvise(start, size, MADV_DONTNEED);
  }
  #else
  int err = unix_madvise(start, size, MADV_DONTNEED);
  #endif
  return err;
}

int _mi_prim_protect(void* start, size_t size, bool protect) {
  int err = mprotect(start, size, protect ? PROT_NONE : (PROT_READ | PROT_WRITE));
  if (err != 0) { err = errno; }
  unix_mprotect_hint(err);
  return err;
}



//---------------------------------------------
// Huge page allocation
//---------------------------------------------

#if (MI_INTPTR_SIZE >= 8) && !defined(__HAIKU__) && !defined(__CYGWIN__)

#ifndef MPOL_PREFERRED
#define MPOL_PREFERRED 1
#endif

#if defined(MI_HAS_SYSCALL_H) && defined(SYS_mbind)
static long mi_prim_mbind(void* start, unsigned long len, unsigned long mode, const unsigned long* nmask, unsigned long maxnode, unsigned flags) {
  return syscall(SYS_mbind, start, len, mode, nmask, maxnode, flags);
}
#else
static long mi_prim_mbind(void* start, unsigned long len, unsigned long mode, const unsigned long* nmask, unsigned long maxnode, unsigned flags) {
  MI_UNUSED(start); MI_UNUSED(len); MI_UNUSED(mode); MI_UNUSED(nmask); MI_UNUSED(maxnode); MI_UNUSED(flags);
  return 0;
}
#endif

int _mi_prim_alloc_huge_os_pages(void* hint_addr, size_t size, int numa_node, bool* is_zero, void** addr) {
  bool is_large = true;
  *is_zero = true;
  *addr = unix_mmap(hint_addr, size, MI_SEGMENT_SIZE, PROT_READ | PROT_WRITE, true, true, &is_large);
  if (*addr != NULL && numa_node >= 0 && numa_node < 8*MI_INTPTR_SIZE) { // at most 64 nodes
    unsigned long numa_mask = (1UL << numa_node);
    // TODO: does `mbind` work correctly for huge OS pages? should we
    // use `set_mempolicy` before calling mmap instead?
    // see: <https://lkml.org/lkml/2017/2/9/875>
    long err = mi_prim_mbind(*addr, size, MPOL_PREFERRED, &numa_mask, 8*MI_INTPTR_SIZE, 0);
    if (err != 0) {
      err = errno;
      _mi_warning_message("failed to bind huge (1GiB) pages to numa node %d (error: %d (0x%x))\n", numa_node, err, err);
    }
  }
  return (*addr != NULL ? 0 : errno);
}

#else

int _mi_prim_alloc_huge_os_pages(void* hint_addr, size_t size, int numa_node, bool* is_zero, void** addr) {
  MI_UNUSED(hint_addr); MI_UNUSED(size); MI_UNUSED(numa_node);
  *is_zero = false;
  *addr = NULL;
  return ENOMEM;
}

#endif

//---------------------------------------------
// NUMA nodes
//---------------------------------------------

#if defined(__linux__)

size_t _mi_prim_numa_node(void) {
  #if defined(MI_HAS_SYSCALL_H) && defined(SYS_getcpu)
    unsigned long node = 0;
    unsigned long ncpu = 0;
    long err = syscall(SYS_getcpu, &ncpu, &node, NULL);
    if (err != 0) return 0;
    return node;
  #else
    return 0;
  #endif
}

size_t _mi_prim_numa_node_count(void) {
  char buf[128];
  unsigned node = 0;
  for(node = 0; node < 256; node++) {
    // enumerate node entries -- todo: it there a more efficient way to do this? (but ensure there is no allocation)
    _mi_snprintf(buf, 127, "/sys/devices/system/node/node%u", node + 1);
    if (mi_prim_access(buf,R_OK) != 0) break;
  }
  return (node+1);
}

#elif defined(__FreeBSD__) && __FreeBSD_version >= 1200000

size_t _mi_prim_numa_node(void) {
  domainset_t dom;
  size_t node;
  int policy;
  if (cpuset_getdomain(CPU_LEVEL_CPUSET, CPU_WHICH_PID, -1, sizeof(dom), &dom, &policy) == -1) return 0ul;
  for (node = 0; node < MAXMEMDOM; node++) {
    if (DOMAINSET_ISSET(node, &dom)) return node;
  }
  return 0ul;
}

size_t _mi_prim_numa_node_count(void) {
  size_t ndomains = 0;
  size_t len = sizeof(ndomains);
  if (sysctlbyname("vm.ndomains", &ndomains, &len, NULL, 0) == -1) return 0ul;
  return ndomains;
}

#elif defined(__DragonFly__)

size_t _mi_prim_numa_node(void) {
  // TODO: DragonFly does not seem to provide any userland means to get this information.
  return 0ul;
}

size_t _mi_prim_numa_node_count(void) {
  size_t ncpus = 0, nvirtcoresperphys = 0;
  size_t len = sizeof(size_t);
  if (sysctlbyname("hw.ncpu", &ncpus, &len, NULL, 0) == -1) return 0ul;
  if (sysctlbyname("hw.cpu_topology_ht_ids", &nvirtcoresperphys, &len, NULL, 0) == -1) return 0ul;
  return nvirtcoresperphys * ncpus;
}

#else

size_t _mi_prim_numa_node(void) {
  return 0;
}

size_t _mi_prim_numa_node_count(void) {
  return 1;
}

#endif

// ----------------------------------------------------------------
// Clock
// ----------------------------------------------------------------

#include <time.h>

#if defined(CLOCK_REALTIME) || defined(CLOCK_MONOTONIC)

mi_msecs_t _mi_prim_clock_now(void) {
  struct timespec t;
  #ifdef CLOCK_MONOTONIC
  clock_gettime(CLOCK_MONOTONIC, &t);
  #else
  clock_gettime(CLOCK_REALTIME, &t);
  #endif
  return ((mi_msecs_t)t.tv_sec * 1000) + ((mi_msecs_t)t.tv_nsec / 1000000);
}

#else

// low resolution timer
mi_msecs_t _mi_prim_clock_now(void) {
  #if !defined(CLOCKS_PER_SEC) || (CLOCKS_PER_SEC == 1000) || (CLOCKS_PER_SEC == 0)
  return (mi_msecs_t)clock();
  #elif (CLOCKS_PER_SEC < 1000)
  return (mi_msecs_t)clock() * (1000 / (mi_msecs_t)CLOCKS_PER_SEC);
  #else
  return (mi_msecs_t)clock() / ((mi_msecs_t)CLOCKS_PER_SEC / 1000);
  #endif
}

#endif




//----------------------------------------------------------------
// Process info
//----------------------------------------------------------------

#if defined(__unix__) || defined(__unix) || defined(unix) || defined(__APPLE__) || defined(__HAIKU__)
#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>

#if defined(__APPLE__)
#include <mach/mach.h>
#endif

#if defined(__HAIKU__)
#include <kernel/OS.h>
#endif

static mi_msecs_t timeval_secs(const struct timeval* tv) {
  return ((mi_msecs_t)tv->tv_sec * 1000L) + ((mi_msecs_t)tv->tv_usec / 1000L);
}

void _mi_prim_process_info(mi_process_info_t* pinfo)
{
  struct rusage rusage;
  getrusage(RUSAGE_SELF, &rusage);
  pinfo->utime = timeval_secs(&rusage.ru_utime);
  pinfo->stime = timeval_secs(&rusage.ru_stime);
#if !defined(__HAIKU__)
  pinfo->page_faults = rusage.ru_majflt;
#endif
#if defined(__HAIKU__)
  // Haiku does not have (yet?) a way to
  // get these stats per process
  thread_info tid;
  area_info mem;
  ssize_t c;
  get_thread_info(find_thread(0), &tid);
  while (get_next_area_info(tid.team, &c, &mem) == B_OK) {
    pinfo->peak_rss += mem.ram_size;
  }
  pinfo->page_faults = 0;
#elif defined(__APPLE__)
  pinfo->peak_rss = rusage.ru_maxrss;         // macos reports in bytes
  #ifdef MACH_TASK_BASIC_INFO
  struct mach_task_basic_info info;
  mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &infoCount) == KERN_SUCCESS) {
    pinfo->current_rss = (size_t)info.resident_size;
  }
  #else
  struct task_basic_info info;
  mach_msg_type_number_t infoCount = TASK_BASIC_INFO_COUNT;
  if (task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&info, &infoCount) == KERN_SUCCESS) {
    pinfo->current_rss = (size_t)info.resident_size;
  }
  #endif
#else
  pinfo->peak_rss = rusage.ru_maxrss * 1024;  // Linux/BSD report in KiB
#endif
  // use defaults for commit
}

#else

#ifndef __wasi__
// WebAssembly instances are not processes
#pragma message("define a way to get process info")
#endif

void _mi_prim_process_info(mi_process_info_t* pinfo)
{
  // use defaults
  MI_UNUSED(pinfo);
}

#endif


//----------------------------------------------------------------
// Output
//----------------------------------------------------------------

void _mi_prim_out_stderr( const char* msg ) {
  fputs(msg,stderr);
}


//----------------------------------------------------------------
// Environment
//----------------------------------------------------------------

#if !defined(MI_USE_ENVIRON) || (MI_USE_ENVIRON!=0)
// On Posix systemsr use `environ` to access environment variables
// even before the C runtime is initialized.
#if defined(__APPLE__) && defined(__has_include) && __has_include(<crt_externs.h>)
#include <crt_externs.h>
static char** mi_get_environ(void) {
  return (*_NSGetEnviron());
}
#else
extern char** environ;
static char** mi_get_environ(void) {
  return environ;
}
#endif
bool _mi_prim_getenv(const char* name, char* result, size_t result_size) {
  if (name==NULL) return false;
  const size_t len = _mi_strlen(name);
  if (len == 0) return false;
  char** env = mi_get_environ();
  if (env == NULL) return false;
  // compare up to 10000 entries
  for (int i = 0; i < 10000 && env[i] != NULL; i++) {
    const char* s = env[i];
    if (_mi_strnicmp(name, s, len) == 0 && s[len] == '=') { // case insensitive
      // found it
      _mi_strlcpy(result, s + len + 1, result_size);
      return true;
    }
  }
  return false;
}
#else
// fallback: use standard C `getenv` but this cannot be used while initializing the C runtime
bool _mi_prim_getenv(const char* name, char* result, size_t result_size) {
  // cannot call getenv() when still initializing the C runtime.
  if (_mi_preloading()) return false;
  const char* s = getenv(name);
  if (s == NULL) {
    // we check the upper case name too.
    char buf[64+1];
    size_t len = _mi_strnlen(name,sizeof(buf)-1);
    for (size_t i = 0; i < len; i++) {
      buf[i] = _mi_toupper(name[i]);
    }
    buf[len] = 0;
    s = getenv(buf);
  }
  if (s == NULL || _mi_strnlen(s,result_size) >= result_size)  return false;
  _mi_strlcpy(result, s, result_size);
  return true;
}
#endif  // !MI_USE_ENVIRON


//----------------------------------------------------------------
// Random
//----------------------------------------------------------------

#if defined(__APPLE__) && defined(MAC_OS_X_VERSION_10_15) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_15)
#include <CommonCrypto/CommonCryptoError.h>
#include <CommonCrypto/CommonRandom.h>

bool _mi_prim_random_buf(void* buf, size_t buf_len) {
  // We prefere CCRandomGenerateBytes as it returns an error code while arc4random_buf
  // may fail silently on macOS. See PR #390, and <https://opensource.apple.com/source/Libc/Libc-1439.40.11/gen/FreeBSD/arc4random.c.auto.html>
  return (CCRandomGenerateBytes(buf, buf_len) == kCCSuccess);
}

#elif defined(__ANDROID__) || defined(__DragonFly__) || \
      defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || \
      defined(__sun) || \
      (defined(__APPLE__) && (MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_7))

#include <stdlib.h>
bool _mi_prim_random_buf(void* buf, size_t buf_len) {
  arc4random_buf(buf, buf_len);
  return true;
}

#elif defined(__APPLE__) || defined(__linux__) || defined(__HAIKU__)   // also for old apple versions < 10.7 (issue #829)

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

bool _mi_prim_random_buf(void* buf, size_t buf_len) {
  // Modern Linux provides `getrandom` but different distributions either use `sys/random.h` or `linux/random.h`
  // and for the latter the actual `getrandom` call is not always defined.
  // (see <https://stackoverflow.com/questions/45237324/why-doesnt-getrandom-compile>)
  // We therefore use a syscall directly and fall back dynamically to /dev/urandom when needed.
  #if defined(MI_HAS_SYSCALL_H) && defined(SYS_getrandom)
    #ifndef GRND_NONBLOCK
    #define GRND_NONBLOCK (1)
    #endif
    static _Atomic(uintptr_t) no_getrandom; // = 0
    if (mi_atomic_load_acquire(&no_getrandom)==0) {
      ssize_t ret = syscall(SYS_getrandom, buf, buf_len, GRND_NONBLOCK);
      if (ret >= 0) return (buf_len == (size_t)ret);
      if (errno != ENOSYS) return false;
      mi_atomic_store_release(&no_getrandom, (uintptr_t)1); // don't call again, and fall back to /dev/urandom
    }
  #endif
  int flags = O_RDONLY;
  #if defined(O_CLOEXEC)
  flags |= O_CLOEXEC;
  #endif
  int fd = mi_prim_open("/dev/urandom", flags);
  if (fd < 0) return false;
  size_t count = 0;
  while(count < buf_len) {
    ssize_t ret = mi_prim_read(fd, (char*)buf + count, buf_len - count);
    if (ret<=0) {
      if (errno!=EAGAIN && errno!=EINTR) break;
    }
    else {
      count += ret;
    }
  }
  mi_prim_close(fd);
  return (count==buf_len);
}

#else

bool _mi_prim_random_buf(void* buf, size_t buf_len) {
  return false;
}

#endif


//----------------------------------------------------------------
// Thread init/done
//----------------------------------------------------------------

#if defined(MI_USE_PTHREADS)

// use pthread local storage keys to detect thread ending
// (and used with MI_TLS_PTHREADS for the default heap)
pthread_key_t _mi_heap_default_key = (pthread_key_t)(-1);

static void mi_pthread_done(void* value) {
  if (value!=NULL) {
    _mi_thread_done((mi_heap_t*)value);
  }
}

void _mi_prim_thread_init_auto_done(void) {
  mi_assert_internal(_mi_heap_default_key == (pthread_key_t)(-1));
  pthread_key_create(&_mi_heap_default_key, &mi_pthread_done);
}

void _mi_prim_thread_done_auto_done(void) {
  if (_mi_heap_default_key != (pthread_key_t)(-1)) {  // do not leak the key, see issue #809
    pthread_key_delete(_mi_heap_default_key);
  }
}

void _mi_prim_thread_associate_default_heap(mi_heap_t* heap) {
  if (_mi_heap_default_key != (pthread_key_t)(-1)) {  // can happen during recursive invocation on freeBSD
    pthread_setspecific(_mi_heap_default_key, heap);
  }
}

#else

void _mi_prim_thread_init_auto_done(void) {
  // nothing
}

void _mi_prim_thread_done_auto_done(void) {
  // nothing
}

void _mi_prim_thread_associate_default_heap(mi_heap_t* heap) {
  MI_UNUSED(heap);
}

#endif
