/* fileline.c -- Get file and line number information in a backtrace.
   Copyright (C) 2012-2024 Free Software Foundation, Inc.
   Written by Ian Lance Taylor, Google.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    (1) Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

    (2) Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

    (3) The name of the author may not be used to
    endorse or promote products derived from this software without
    specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.  */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#if defined (HAVE_KERN_PROC_ARGS) || defined (HAVE_KERN_PROC)
#include <sys/sysctl.h>
#endif

#ifdef HAVE_MACH_O_DYLD_H
#include <mach-o/dyld.h>
#endif

#ifdef HAVE_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#endif

#include "backtrace.h"
#include "internal.h"

#ifndef HAVE_GETEXECNAME
#define getexecname() NULL
#endif

#if !defined (HAVE_KERN_PROC_ARGS) && !defined (HAVE_KERN_PROC)

#define sysctl_exec_name1(state, error_callback, data) NULL
#define sysctl_exec_name2(state, error_callback, data) NULL

#else /* defined (HAVE_KERN_PROC_ARGS) || |defined (HAVE_KERN_PROC) */

static char *
sysctl_exec_name (struct backtrace_state *state,
		  int mib0, int mib1, int mib2, int mib3,
		  backtrace_error_callback error_callback, void *data)
{
  int mib[4];
  size_t len;
  char *name;
  size_t rlen;

  mib[0] = mib0;
  mib[1] = mib1;
  mib[2] = mib2;
  mib[3] = mib3;

  if (sysctl (mib, 4, NULL, &len, NULL, 0) < 0)
    return NULL;
  name = (char *) backtrace_alloc (state, len, error_callback, data);
  if (name == NULL)
    return NULL;
  rlen = len;
  if (sysctl (mib, 4, name, &rlen, NULL, 0) < 0)
    {
      backtrace_free (state, name, len, error_callback, data);
      return NULL;
    }
  return name;
}

#ifdef HAVE_KERN_PROC_ARGS

static char *
sysctl_exec_name1 (struct backtrace_state *state,
		   backtrace_error_callback error_callback, void *data)
{
  /* This variant is used on NetBSD.  */
  return sysctl_exec_name (state, CTL_KERN, KERN_PROC_ARGS, -1,
			   KERN_PROC_PATHNAME, error_callback, data);
}

#else

#define sysctl_exec_name1(state, error_callback, data) NULL

#endif

#ifdef HAVE_KERN_PROC

static char *
sysctl_exec_name2 (struct backtrace_state *state,
		   backtrace_error_callback error_callback, void *data)
{
  /* This variant is used on FreeBSD.  */
  return sysctl_exec_name (state, CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1,
			   error_callback, data);
}

#else

#define sysctl_exec_name2(state, error_callback, data) NULL

#endif

#endif /* defined (HAVE_KERN_PROC_ARGS) || |defined (HAVE_KERN_PROC) */

#ifdef HAVE_MACH_O_DYLD_H

static char *
macho_get_executable_path (struct backtrace_state *state,
			   backtrace_error_callback error_callback, void *data)
{
  uint32_t len;
  char *name;

  len = 0;
  if (_NSGetExecutablePath (NULL, &len) == 0)
    return NULL;
  name = (char *) backtrace_alloc (state, len, error_callback, data);
  if (name == NULL)
    return NULL;
  if (_NSGetExecutablePath (name, &len) != 0)
    {
      backtrace_free (state, name, len, error_callback, data);
      return NULL;
    }
  return name;
}

#else /* !defined (HAVE_MACH_O_DYLD_H) */

#define macho_get_executable_path(state, error_callback, data) NULL

#endif /* !defined (HAVE_MACH_O_DYLD_H) */

#if HAVE_DECL__PGMPTR

#define windows_executable_filename() _pgmptr

#else /* !HAVE_DECL__PGMPTR */

#define windows_executable_filename() NULL

#endif /* !HAVE_DECL__PGMPTR */

#ifdef HAVE_WINDOWS_H

#define FILENAME_BUF_SIZE (MAX_PATH)

static char *
windows_get_executable_path (char *buf, backtrace_error_callback error_callback,
			     void *data)
{
  size_t got;
  int error;

  got = GetModuleFileNameA (NULL, buf, FILENAME_BUF_SIZE - 1);
  error = GetLastError ();
  if (got == 0
      || (got == FILENAME_BUF_SIZE - 1 && error == ERROR_INSUFFICIENT_BUFFER))
    {
      error_callback (data,
		      "could not get the filename of the current executable",
		      error);
      return NULL;
    }
  return buf;
}

#else /* !defined (HAVE_WINDOWS_H) */

#define windows_get_executable_path(buf, error_callback, data) NULL
#define FILENAME_BUF_SIZE 64

#endif /* !defined (HAVE_WINDOWS_H) */

/* Initialize the fileline information from the executable.  Returns 1
   on success, 0 on failure.  */

static int
fileline_initialize (struct backtrace_state *state,
		     backtrace_error_callback error_callback, void *data)
{
  int failed;
  fileline fileline_fn;
  int pass;
  int called_error_callback;
  int descriptor;
  const char *filename;
  char buf[FILENAME_BUF_SIZE];

  if (!state->threaded)
    failed = state->fileline_initialization_failed;
  else
    failed = backtrace_atomic_load_int (&state->fileline_initialization_failed);

  if (failed)
    {
      error_callback (data, "failed to read executable information", -1);
      return 0;
    }

  if (!state->threaded)
    fileline_fn = state->fileline_fn;
  else
    fileline_fn = backtrace_atomic_load_pointer (&state->fileline_fn);
  if (fileline_fn != NULL)
    return 1;

  /* We have not initialized the information.  Do it now.  */

  descriptor = -1;
  called_error_callback = 0;
  for (pass = 0; pass < 10; ++pass)
    {
      int does_not_exist;

      switch (pass)
	{
	case 0:
	  filename = state->filename;
	  break;
	case 1:
	  filename = getexecname ();
	  break;
	case 2:
	  /* Test this before /proc/self/exe, as the latter exists but points
	     to the wine binary (and thus doesn't work).  */
	  filename = windows_executable_filename ();
	  break;
	case 3:
	  filename = "/proc/self/exe";
	  break;
	case 4:
	  filename = "/proc/curproc/file";
	  break;
	case 5:
	  snprintf (buf, sizeof (buf), "/proc/%ld/object/a.out",
		    (long) getpid ());
	  filename = buf;
	  break;
	case 6:
	  filename = sysctl_exec_name1 (state, error_callback, data);
	  break;
	case 7:
	  filename = sysctl_exec_name2 (state, error_callback, data);
	  break;
	case 8:
	  filename = macho_get_executable_path (state, error_callback, data);
	  break;
	case 9:
	  filename = windows_get_executable_path (buf, error_callback, data);
	  break;
	default:
	  abort ();
	}

      if (filename == NULL)
	continue;

      descriptor = backtrace_open (filename, error_callback, data,
				   &does_not_exist);
      if (descriptor < 0 && !does_not_exist)
	{
	  called_error_callback = 1;
	  break;
	}
      if (descriptor >= 0)
	break;
    }

  if (descriptor < 0)
    {
      if (!called_error_callback)
	{
	  if (state->filename != NULL)
	    error_callback (data, state->filename, ENOENT);
	  else
	    error_callback (data,
			    "libbacktrace could not find executable to open",
			    0);
	}
      failed = 1;
    }

  if (!failed)
    {
      if (!backtrace_initialize (state, filename, descriptor, error_callback,
				 data, &fileline_fn))
	failed = 1;
    }

  if (failed)
    {
      if (!state->threaded)
	state->fileline_initialization_failed = 1;
      else
	backtrace_atomic_store_int (&state->fileline_initialization_failed, 1);
      return 0;
    }

  if (!state->threaded)
    state->fileline_fn = fileline_fn;
  else
    {
      backtrace_atomic_store_pointer (&state->fileline_fn, fileline_fn);

      /* Note that if two threads initialize at once, one of the data
	 sets may be leaked.  */
    }

  return 1;
}

/* Given a PC, find the file name, line number, and function name.  */

int
backtrace_pcinfo (struct backtrace_state *state, uintptr_t pc,
		  backtrace_full_callback callback,
		  backtrace_error_callback error_callback, void *data)
{
  if (!fileline_initialize (state, error_callback, data))
    return 0;

  if (state->fileline_initialization_failed)
    return 0;

  return state->fileline_fn (state, pc, callback, error_callback, data);
}

/* Given a PC, find the symbol for it, and its value.  */

int
backtrace_syminfo (struct backtrace_state *state, uintptr_t pc,
		   backtrace_syminfo_callback callback,
		   backtrace_error_callback error_callback, void *data)
{
  if (!fileline_initialize (state, error_callback, data))
    return 0;

  if (state->fileline_initialization_failed)
    return 0;

  state->syminfo_fn (state, pc, callback, error_callback, data);
  return 1;
}

/* A backtrace_syminfo_callback that can call into a
   backtrace_full_callback, used when we have a symbol table but no
   debug info.  */

void
backtrace_syminfo_to_full_callback (void *data, uintptr_t pc,
				    const char *symname,
				    uintptr_t symval ATTRIBUTE_UNUSED,
				    uintptr_t symsize ATTRIBUTE_UNUSED)
{
  struct backtrace_call_full *bdata = (struct backtrace_call_full *) data;

  bdata->ret = bdata->full_callback (bdata->full_data, pc, NULL, 0, symname);
}

/* An error callback that corresponds to
   backtrace_syminfo_to_full_callback.  */

void
backtrace_syminfo_to_full_error_callback (void *data, const char *msg,
					  int errnum)
{
  struct backtrace_call_full *bdata = (struct backtrace_call_full *) data;

  bdata->full_error_callback (bdata->full_data, msg, errnum);
}
