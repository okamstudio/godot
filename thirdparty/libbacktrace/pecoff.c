/* pecoff.c -- Get debug data from a PE/COFFF file for backtraces.
   Copyright (C) 2015-2024 Free Software Foundation, Inc.
   Adapted from elf.c by Tristan Gingold, AdaCore.

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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "backtrace.h"
#include "internal.h"

#ifdef HAVE_WINDOWS_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#ifdef HAVE_TLHELP32_H
#include <tlhelp32.h>

#ifdef UNICODE
/* If UNICODE is defined, all the symbols are replaced by a macro to use the
   wide variant. But we need the ansi variant, so undef the macros. */
#undef MODULEENTRY32
#undef Module32First
#undef Module32Next
#endif
#endif

#if defined(_ARM_)
#define NTAPI
#else
#define NTAPI __stdcall
#endif

/* This is a simplified (but binary compatible) version of what Microsoft
   defines in their documentation. */
struct dll_notification_data
{
  ULONG reserved;
  /* The name as UNICODE_STRING struct. */
  PVOID full_dll_name;
  PVOID base_dll_name;
  PVOID dll_base;
  ULONG size_of_image;
};

#define LDR_DLL_NOTIFICATION_REASON_LOADED 1

typedef LONG NTSTATUS;
typedef VOID (CALLBACK *LDR_DLL_NOTIFICATION)(ULONG,
					      struct dll_notification_data*,
					      PVOID);
typedef NTSTATUS (NTAPI *LDR_REGISTER_FUNCTION)(ULONG,
						LDR_DLL_NOTIFICATION, PVOID,
						PVOID*);
#endif

/* Coff file header.  */

typedef struct {
  uint16_t machine;
  uint16_t number_of_sections;
  uint32_t time_date_stamp;
  uint32_t pointer_to_symbol_table;
  uint32_t number_of_symbols;
  uint16_t size_of_optional_header;
  uint16_t characteristics;
} b_coff_file_header;

/* Coff optional header.  */

typedef struct {
  uint16_t magic;
  uint8_t  major_linker_version;
  uint8_t  minor_linker_version;
  uint32_t size_of_code;
  uint32_t size_of_initialized_data;
  uint32_t size_of_uninitialized_data;
  uint32_t address_of_entry_point;
  uint32_t base_of_code;
  union {
    struct {
      uint32_t base_of_data;
      uint32_t image_base;
    } pe;
    struct {
      uint64_t image_base;
    } pep;
  } u;
} b_coff_optional_header;

/* Values of magic in optional header.  */

#define PE_MAGIC 0x10b		/* PE32 executable.  */
#define PEP_MAGIC 0x20b		/* PE32+ executable (for 64bit targets).  */

/* Coff section header.  */

typedef struct {
  char name[8];
  uint32_t virtual_size;
  uint32_t virtual_address;
  uint32_t size_of_raw_data;
  uint32_t pointer_to_raw_data;
  uint32_t pointer_to_relocations;
  uint32_t pointer_to_line_numbers;
  uint16_t number_of_relocations;
  uint16_t number_of_line_numbers;
  uint32_t characteristics;
} b_coff_section_header;

/* Coff symbol name.  */

typedef union {
  char short_name[8];
  struct {
    unsigned char zeroes[4];
    unsigned char off[4];
  } long_name;
} b_coff_name;

/* Coff symbol (external representation which is unaligned).  */

typedef struct {
  b_coff_name name;
  unsigned char value[4];
  unsigned char section_number[2];
  unsigned char type[2];
  unsigned char storage_class;
  unsigned char number_of_aux_symbols;
} b_coff_external_symbol;

/* Symbol types.  */

#define N_TBSHFT 4			/* Shift for the derived type.  */
#define IMAGE_SYM_DTYPE_FUNCTION 2	/* Function derived type.  */

/* Size of a coff symbol.  */

#define SYM_SZ 18

/* Coff symbol, internal representation (aligned).  */

typedef struct {
  const char *name;
  uint32_t value;
  int16_t sec;
  uint16_t type;
  uint16_t sc;
} b_coff_internal_symbol;

/* Names of sections, indexed by enum dwarf_section in internal.h.  */

static const char * const debug_section_names[DEBUG_MAX] =
{
  ".debug_info",
  ".debug_line",
  ".debug_abbrev",
  ".debug_ranges",
  ".debug_str",
  ".debug_addr",
  ".debug_str_offsets",
  ".debug_line_str",
  ".debug_rnglists"
};

/* Information we gather for the sections we care about.  */

struct debug_section_info
{
  /* Section file offset.  */
  off_t offset;
  /* Section size.  */
  size_t size;
};

/* Information we keep for an coff symbol.  */

struct coff_symbol
{
  /* The name of the symbol.  */
  const char *name;
  /* The address of the symbol.  */
  uintptr_t address;
};

/* Information to pass to coff_syminfo.  */

struct coff_syminfo_data
{
  /* Symbols for the next module.  */
  struct coff_syminfo_data *next;
  /* The COFF symbols, sorted by address.  */
  struct coff_symbol *symbols;
  /* The number of symbols.  */
  size_t count;
};

/* A dummy callback function used when we can't find any debug info.  */

static int
coff_nodebug (struct backtrace_state *state ATTRIBUTE_UNUSED,
	      uintptr_t pc ATTRIBUTE_UNUSED,
	      backtrace_full_callback callback ATTRIBUTE_UNUSED,
	      backtrace_error_callback error_callback, void *data)
{
  error_callback (data, "no debug info in PE/COFF executable (make sure to compile with -g)", -1);
  return 0;
}

/* A dummy callback function used when we can't find a symbol
   table.  */

static void
coff_nosyms (struct backtrace_state *state ATTRIBUTE_UNUSED,
	     uintptr_t addr ATTRIBUTE_UNUSED,
	     backtrace_syminfo_callback callback ATTRIBUTE_UNUSED,
	     backtrace_error_callback error_callback, void *data)
{
  error_callback (data, "no symbol table in PE/COFF executable", -1);
}

/* Read a potentially unaligned 4 byte word at P, using native endianness.  */

static uint32_t
coff_read4 (const unsigned char *p)
{
  uint32_t res;

  memcpy (&res, p, 4);
  return res;
}

/* Read a potentially unaligned 2 byte word at P, using native endianness.
   All 2 byte word in symbols are always aligned, but for coherency all
   fields are declared as char arrays.  */

static uint16_t
coff_read2 (const unsigned char *p)
{
  uint16_t res;

  memcpy (&res, p, sizeof (res));
  return res;
}

/* Return the length (without the trailing 0) of a COFF short name.  */

static size_t
coff_short_name_len (const char *name)
{
  int i;

  for (i = 0; i < 8; i++)
    if (name[i] == 0)
      return i;
  return 8;
}

/* Return true iff COFF short name CNAME is the same as NAME (a NUL-terminated
   string).  */

static int
coff_short_name_eq (const char *name, const char *cname)
{
  int i;

  for (i = 0; i < 8; i++)
    {
      if (name[i] != cname[i])
	return 0;
      if (name[i] == 0)
	return 1;
    }
  return name[8] == 0;
}

/* Return true iff NAME is the same as string at offset OFF.  */

static int
coff_long_name_eq (const char *name, unsigned int off,
		   struct backtrace_view *str_view)
{
  if (off >= str_view->len)
    return 0;
  return strcmp (name, (const char *)str_view->data + off) == 0;
}

/* Compare struct coff_symbol for qsort.  */

static int
coff_symbol_compare (const void *v1, const void *v2)
{
  const struct coff_symbol *e1 = (const struct coff_symbol *) v1;
  const struct coff_symbol *e2 = (const struct coff_symbol *) v2;

  if (e1->address < e2->address)
    return -1;
  else if (e1->address > e2->address)
    return 1;
  else
    return 0;
}

/* Convert SYM to internal (and aligned) format ISYM, using string table
   from STRTAB and STRTAB_SIZE, and number of sections SECTS_NUM.
   Return -1 in case of error (invalid section number or string index).  */

static int
coff_expand_symbol (b_coff_internal_symbol *isym,
		    const b_coff_external_symbol *sym,
		    uint16_t sects_num,
		    const unsigned char *strtab, size_t strtab_size)
{
  isym->type = coff_read2 (sym->type);
  isym->sec = coff_read2 (sym->section_number);
  isym->sc = sym->storage_class;

  if (isym->sec > 0 && (uint16_t) isym->sec > sects_num)
    return -1;
  if (sym->name.short_name[0] != 0)
    isym->name = sym->name.short_name;
  else
    {
      uint32_t off = coff_read4 (sym->name.long_name.off);

      if (off >= strtab_size)
	return -1;
      isym->name = (const char *) strtab + off;
    }
  return 0;
}

/* Return true iff SYM is a defined symbol for a function.  Data symbols
   aren't considered because they aren't easily identified (same type as
   section names, presence of symbols defined by the linker script).  */

static int
coff_is_function_symbol (const b_coff_internal_symbol *isym)
{
  return (isym->type >> N_TBSHFT) == IMAGE_SYM_DTYPE_FUNCTION
    && isym->sec > 0;
}

/* Initialize the symbol table info for coff_syminfo.  */

static int
coff_initialize_syminfo (struct backtrace_state *state,
			 struct libbacktrace_base_address base_address,
			 int is_64, const b_coff_section_header *sects,
			 size_t sects_num, const b_coff_external_symbol *syms,
			 size_t syms_size, const unsigned char *strtab,
			 size_t strtab_size,
			 backtrace_error_callback error_callback,
			 void *data, struct coff_syminfo_data *sdata)
{
  size_t syms_count;
  char *coff_symstr;
  size_t coff_symstr_len;
  size_t coff_symbol_count;
  size_t coff_symbol_size;
  struct coff_symbol *coff_symbols;
  struct coff_symbol *coff_sym;
  char *coff_str;
  size_t i;

  syms_count = syms_size / SYM_SZ;

  /* We only care about function symbols.  Count them.  Also count size of
     strings for in-symbol names.  */
  coff_symbol_count = 0;
  coff_symstr_len = 0;
  for (i = 0; i < syms_count; ++i)
    {
      const b_coff_external_symbol *asym = &syms[i];
      b_coff_internal_symbol isym;

      if (coff_expand_symbol (&isym, asym, sects_num, strtab, strtab_size) < 0)
	{
	  error_callback (data, "invalid section or offset in coff symbol", 0);
	  return 0;
	}
      if (coff_is_function_symbol (&isym))
	{
	  ++coff_symbol_count;
	  if (asym->name.short_name[0] != 0)
	    coff_symstr_len += coff_short_name_len (asym->name.short_name) + 1;
	}

      i += asym->number_of_aux_symbols;
    }

  coff_symbol_size = (coff_symbol_count + 1) * sizeof (struct coff_symbol);
  coff_symbols = ((struct coff_symbol *)
		  backtrace_alloc (state, coff_symbol_size, error_callback,
				   data));
  if (coff_symbols == NULL)
    return 0;

  /* Allocate memory for symbols strings.  */
  if (coff_symstr_len > 0)
    {
      coff_symstr = ((char *)
		     backtrace_alloc (state, coff_symstr_len, error_callback,
				      data));
      if (coff_symstr == NULL)
	{
	  backtrace_free (state, coff_symbols, coff_symbol_size,
			  error_callback, data);
	  return 0;
	}
    }
  else
    coff_symstr = NULL;

  /* Copy symbols.  */
  coff_sym = coff_symbols;
  coff_str = coff_symstr;
  for (i = 0; i < syms_count; ++i)
    {
      const b_coff_external_symbol *asym = &syms[i];
      b_coff_internal_symbol isym;

      if (coff_expand_symbol (&isym, asym, sects_num, strtab, strtab_size))
	{
	  /* Should not fail, as it was already tested in the previous
	     loop.  */
	  abort ();
	}
      if (coff_is_function_symbol (&isym))
	{
	  const char *name;
	  int16_t secnum;

	  if (asym->name.short_name[0] != 0)
	    {
	      size_t len = coff_short_name_len (isym.name);
	      name = coff_str;
	      memcpy (coff_str, isym.name, len);
	      coff_str[len] = 0;
	      coff_str += len + 1;
	    }
	  else
	    name = isym.name;

	  if (!is_64)
	    {
	      /* Strip leading '_'.  */
	      if (name[0] == '_')
		name++;
	    }

	  /* Symbol value is section relative, so we need to read the address
	     of its section.  */
	  secnum = coff_read2 (asym->section_number);

	  coff_sym->name = name;
	  coff_sym->address =
	    libbacktrace_add_base ((coff_read4 (asym->value)
				    + sects[secnum - 1].virtual_address),
				   base_address);
	  coff_sym++;
	}

      i += asym->number_of_aux_symbols;
    }

  /* End of symbols marker.  */
  coff_sym->name = NULL;
  coff_sym->address = -1;

  backtrace_qsort (coff_symbols, coff_symbol_count,
		   sizeof (struct coff_symbol), coff_symbol_compare);

  sdata->next = NULL;
  sdata->symbols = coff_symbols;
  sdata->count = coff_symbol_count;

  return 1;
}

/* Add EDATA to the list in STATE.  */

static void
coff_add_syminfo_data (struct backtrace_state *state,
		       struct coff_syminfo_data *sdata)
{
  if (!state->threaded)
    {
      struct coff_syminfo_data **pp;

      for (pp = (struct coff_syminfo_data **) (void *) &state->syminfo_data;
	   *pp != NULL;
	   pp = &(*pp)->next)
	;
      *pp = sdata;
    }
  else
    {
      while (1)
	{
	  struct coff_syminfo_data **pp;

	  pp = (struct coff_syminfo_data **) (void *) &state->syminfo_data;

	  while (1)
	    {
	      struct coff_syminfo_data *p;

	      p = backtrace_atomic_load_pointer (pp);

	      if (p == NULL)
		break;

	      pp = &p->next;
	    }

	  if (__sync_bool_compare_and_swap (pp, NULL, sdata))
	    break;
	}
    }
}

/* Compare an ADDR against an elf_symbol for bsearch.  We allocate one
   extra entry in the array so that this can look safely at the next
   entry.  */

static int
coff_symbol_search (const void *vkey, const void *ventry)
{
  const uintptr_t *key = (const uintptr_t *) vkey;
  const struct coff_symbol *entry = (const struct coff_symbol *) ventry;
  uintptr_t addr;

  addr = *key;
  if (addr < entry->address)
    return -1;
  else if (addr >= entry[1].address)
    return 1;
  else
    return 0;
}

/* Return the symbol name and value for an ADDR.  */

static void
coff_syminfo (struct backtrace_state *state, uintptr_t addr,
	      backtrace_syminfo_callback callback,
	      backtrace_error_callback error_callback ATTRIBUTE_UNUSED,
	      void *data)
{
  struct coff_syminfo_data *sdata;
  struct coff_symbol *sym = NULL;

  if (!state->threaded)
    {
      for (sdata = (struct coff_syminfo_data *) state->syminfo_data;
	   sdata != NULL;
	   sdata = sdata->next)
	{
	  sym = ((struct coff_symbol *)
		 bsearch (&addr, sdata->symbols, sdata->count,
			  sizeof (struct coff_symbol), coff_symbol_search));
	  if (sym != NULL)
	    break;
	}
    }
  else
    {
      struct coff_syminfo_data **pp;

      pp = (struct coff_syminfo_data **) (void *) &state->syminfo_data;
      while (1)
	{
	  sdata = backtrace_atomic_load_pointer (pp);
	  if (sdata == NULL)
	    break;

	  sym = ((struct coff_symbol *)
		 bsearch (&addr, sdata->symbols, sdata->count,
			  sizeof (struct coff_symbol), coff_symbol_search));
	  if (sym != NULL)
	    break;

	  pp = &sdata->next;
	}
    }

  if (sym == NULL)
    callback (data, addr, NULL, 0, 0);
  else
    callback (data, addr, sym->name, sym->address, 0);
}

/* Add the backtrace data for one PE/COFF file.  Returns 1 on success,
   0 on failure (in both cases descriptor is closed).  */

static int
coff_add (struct backtrace_state *state, int descriptor,
	  backtrace_error_callback error_callback, void *data,
	  fileline *fileline_fn, int *found_sym, int *found_dwarf,
	  uintptr_t module_handle ATTRIBUTE_UNUSED)
{
  struct backtrace_view fhdr_view;
  off_t fhdr_off;
  int magic_ok;
  b_coff_file_header fhdr;
  off_t opt_sects_off;
  size_t opt_sects_size;
  unsigned int sects_num;
  struct backtrace_view sects_view;
  int sects_view_valid;
  const b_coff_optional_header *opt_hdr;
  const b_coff_section_header *sects;
  struct backtrace_view str_view;
  int str_view_valid;
  size_t str_size;
  off_t str_off;
  struct backtrace_view syms_view;
  off_t syms_off;
  size_t syms_size;
  int syms_view_valid;
  unsigned int syms_num;
  unsigned int i;
  struct debug_section_info sections[DEBUG_MAX];
  off_t min_offset;
  off_t max_offset;
  struct backtrace_view debug_view;
  int debug_view_valid;
  int is_64;
  struct libbacktrace_base_address image_base;
  struct libbacktrace_base_address base_address;
  struct dwarf_sections dwarf_sections;

  *found_sym = 0;
  *found_dwarf = 0;

  sects_view_valid = 0;
  syms_view_valid = 0;
  str_view_valid = 0;
  debug_view_valid = 0;

  /* Map the MS-DOS stub (if any) and extract file header offset.  */
  if (!backtrace_get_view (state, descriptor, 0, 0x40, error_callback,
			   data, &fhdr_view))
    goto fail;

  {
    const unsigned char *vptr = fhdr_view.data;

    if (vptr[0] == 'M' && vptr[1] == 'Z')
      fhdr_off = coff_read4 (vptr + 0x3c);
    else
      fhdr_off = 0;
  }

  backtrace_release_view (state, &fhdr_view, error_callback, data);

  /* Map the coff file header.  */
  if (!backtrace_get_view (state, descriptor, fhdr_off,
			   sizeof (b_coff_file_header) + 4,
			   error_callback, data, &fhdr_view))
    goto fail;

  if (fhdr_off != 0)
    {
      const char *magic = (const char *) fhdr_view.data;
      magic_ok = memcmp (magic, "PE\0", 4) == 0;
      fhdr_off += 4;

      memcpy (&fhdr, (const unsigned char *) fhdr_view.data + 4, sizeof fhdr);
    }
  else
    {
      memcpy (&fhdr, fhdr_view.data, sizeof fhdr);
      /* TODO: test fhdr.machine for coff but non-PE platforms.  */
      magic_ok = 0;
    }
  backtrace_release_view (state, &fhdr_view, error_callback, data);

  if (!magic_ok)
    {
      error_callback (data, "executable file is not COFF", 0);
      goto fail;
    }

  sects_num = fhdr.number_of_sections;
  syms_num = fhdr.number_of_symbols;

  opt_sects_off = fhdr_off + sizeof (fhdr);
  opt_sects_size = (fhdr.size_of_optional_header
		    + sects_num * sizeof (b_coff_section_header));

  /* To translate PC to file/line when using DWARF, we need to find
     the .debug_info and .debug_line sections.  */

  /* Read the optional header and the section headers.  */

  if (!backtrace_get_view (state, descriptor, opt_sects_off, opt_sects_size,
			   error_callback, data, &sects_view))
    goto fail;
  sects_view_valid = 1;
  opt_hdr = (const b_coff_optional_header *) sects_view.data;
  sects = (const b_coff_section_header *)
    ((const unsigned char *) sects_view.data + fhdr.size_of_optional_header);

  is_64 = 0;
  memset (&image_base, 0, sizeof image_base);
  if (fhdr.size_of_optional_header > sizeof (*opt_hdr))
    {
      if (opt_hdr->magic == PE_MAGIC)
	image_base.m = opt_hdr->u.pe.image_base;
      else if (opt_hdr->magic == PEP_MAGIC)
	{
	  image_base.m = opt_hdr->u.pep.image_base;
	  is_64 = 1;
	}
      else
	{
	  error_callback (data, "bad magic in PE optional header", 0);
	  goto fail;
	}
    }

  /* Read the symbol table and the string table.  */

  if (fhdr.pointer_to_symbol_table == 0)
    {
      /* No symbol table, no string table.  */
      str_off = 0;
      str_size = 0;
      syms_num = 0;
      syms_size = 0;
    }
  else
    {
      /* Symbol table is followed by the string table.  The string table
	 starts with its length (on 4 bytes).
	 Map the symbol table and the length of the string table.  */
      syms_off = fhdr.pointer_to_symbol_table;
      syms_size = syms_num * SYM_SZ;

      if (!backtrace_get_view (state, descriptor, syms_off, syms_size + 4,
			       error_callback, data, &syms_view))
	goto fail;
      syms_view_valid = 1;

      str_size = coff_read4 ((const unsigned char *) syms_view.data
			     + syms_size);

      str_off = syms_off + syms_size;

      if (str_size > 4)
	{
	  /* Map string table (including the length word).  */

	  if (!backtrace_get_view (state, descriptor, str_off, str_size,
				   error_callback, data, &str_view))
	    goto fail;
	  str_view_valid = 1;
	}
    }

  memset (sections, 0, sizeof sections);

  /* Look for the symbol table.  */
  for (i = 0; i < sects_num; ++i)
    {
      const b_coff_section_header *s = sects + i;
      unsigned int str_off;
      int j;

      if (s->name[0] == '/')
	{
	  /* Extended section name.  */
	  str_off = atoi (s->name + 1);
	}
      else
	str_off = 0;

      for (j = 0; j < (int) DEBUG_MAX; ++j)
	{
	  const char *dbg_name = debug_section_names[j];
	  int match;

	  if (str_off != 0)
	    match = coff_long_name_eq (dbg_name, str_off, &str_view);
	  else
	    match = coff_short_name_eq (dbg_name, s->name);
	  if (match)
	    {
	      sections[j].offset = s->pointer_to_raw_data;
	      sections[j].size = s->virtual_size <= s->size_of_raw_data ?
		s->virtual_size : s->size_of_raw_data;
	      break;
	    }
	}
    }

  if (syms_num != 0)
    {
      struct coff_syminfo_data *sdata;

      sdata = ((struct coff_syminfo_data *)
	       backtrace_alloc (state, sizeof *sdata, error_callback, data));
      if (sdata == NULL)
	goto fail;

      if (!coff_initialize_syminfo (state, image_base, is_64,
				    sects, sects_num,
				    syms_view.data, syms_size,
				    str_view.data, str_size,
				    error_callback, data, sdata))
	{
	  backtrace_free (state, sdata, sizeof *sdata, error_callback, data);
	  goto fail;
	}

      *found_sym = 1;

      coff_add_syminfo_data (state, sdata);
    }

  backtrace_release_view (state, &sects_view, error_callback, data);
  sects_view_valid = 0;
  if (syms_view_valid)
    {
      backtrace_release_view (state, &syms_view, error_callback, data);
      syms_view_valid = 0;
    }

  /* Read all the debug sections in a single view, since they are
     probably adjacent in the file.  We never release this view.  */

  min_offset = 0;
  max_offset = 0;
  for (i = 0; i < (int) DEBUG_MAX; ++i)
    {
      off_t end;

      if (sections[i].size == 0)
	continue;
      if (min_offset == 0 || sections[i].offset < min_offset)
	min_offset = sections[i].offset;
      end = sections[i].offset + sections[i].size;
      if (end > max_offset)
	max_offset = end;
    }
  if (min_offset == 0 || max_offset == 0)
    {
      if (!backtrace_close (descriptor, error_callback, data))
	goto fail;
      *fileline_fn = coff_nodebug;
      return 1;
    }

  if (!backtrace_get_view (state, descriptor, min_offset,
			   max_offset - min_offset,
			   error_callback, data, &debug_view))
    goto fail;
  debug_view_valid = 1;

  /* We've read all we need from the executable.  */
  if (!backtrace_close (descriptor, error_callback, data))
    goto fail;
  descriptor = -1;

  for (i = 0; i < (int) DEBUG_MAX; ++i)
    {
      size_t size = sections[i].size;
      dwarf_sections.size[i] = size;
      if (size == 0)
	dwarf_sections.data[i] = NULL;
      else
	dwarf_sections.data[i] = ((const unsigned char *) debug_view.data
				  + (sections[i].offset - min_offset));
    }

  memset (&base_address, 0, sizeof base_address);
#ifdef HAVE_WINDOWS_H
  base_address.m = module_handle - image_base.m;
#endif

  if (!backtrace_dwarf_add (state, base_address, &dwarf_sections,
			    0, /* FIXME: is_bigendian */
			    NULL, /* altlink */
			    error_callback, data, fileline_fn,
			    NULL /* returned fileline_entry */))
    goto fail;

  *found_dwarf = 1;

  return 1;

 fail:
  if (sects_view_valid)
    backtrace_release_view (state, &sects_view, error_callback, data);
  if (str_view_valid)
    backtrace_release_view (state, &str_view, error_callback, data);
  if (syms_view_valid)
    backtrace_release_view (state, &syms_view, error_callback, data);
  if (debug_view_valid)
    backtrace_release_view (state, &debug_view, error_callback, data);
  if (descriptor != -1)
    backtrace_close (descriptor, error_callback, data);
  return 0;
}

#ifdef HAVE_WINDOWS_H
struct dll_notification_context
{
  struct backtrace_state *state;
  backtrace_error_callback error_callback;
  void *data;
};

static VOID CALLBACK
dll_notification (ULONG reason,
		  struct dll_notification_data *notification_data,
		  PVOID context)
{
  char module_name[MAX_PATH];
  int descriptor;
  struct dll_notification_context* dll_context =
    (struct dll_notification_context*) context;
  struct backtrace_state *state = dll_context->state;
  void *data = dll_context->data;
  backtrace_error_callback error_callback = dll_context->data;
  fileline fileline;
  int found_sym;
  int found_dwarf;
  HMODULE module_handle;

  if (reason != LDR_DLL_NOTIFICATION_REASON_LOADED)
    return;

  if (!GetModuleHandleExW ((GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
			    | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT),
			   (wchar_t*) notification_data->dll_base,
			   &module_handle))
    return;

  if (!GetModuleFileNameA ((HMODULE) module_handle, module_name, MAX_PATH - 1))
    return;

  descriptor = backtrace_open (module_name, error_callback, data, NULL);

  if (descriptor < 0)
    return;

  coff_add (state, descriptor, error_callback, data, &fileline, &found_sym,
	    &found_dwarf, (uintptr_t) module_handle);
}
#endif /* defined(HAVE_WINDOWS_H) */

/* Initialize the backtrace data we need from an ELF executable.  At
   the ELF level, all we need to do is find the debug info
   sections.  */

int
backtrace_initialize (struct backtrace_state *state,
		      const char *filename ATTRIBUTE_UNUSED, int descriptor,
		      backtrace_error_callback error_callback,
		      void *data, fileline *fileline_fn)
{
  int ret;
  int found_sym;
  int found_dwarf;
  fileline coff_fileline_fn;
  uintptr_t module_handle = 0;
#ifdef HAVE_TLHELP32_H
  fileline module_fileline_fn;
  int module_found_sym;
  HANDLE snapshot;
#endif

#ifdef HAVE_WINDOWS_H
  HMODULE nt_dll_handle;

  module_handle = (uintptr_t) GetModuleHandle (NULL);
#endif

  ret = coff_add (state, descriptor, error_callback, data,
		  &coff_fileline_fn, &found_sym, &found_dwarf, module_handle);
  if (!ret)
    return 0;

#ifdef HAVE_TLHELP32_H
  do
    {
      snapshot = CreateToolhelp32Snapshot (TH32CS_SNAPMODULE, 0);
    }
  while (snapshot == INVALID_HANDLE_VALUE
	 && GetLastError () == ERROR_BAD_LENGTH);

  if (snapshot != INVALID_HANDLE_VALUE)
    {
      MODULEENTRY32 entry;
      BOOL ok;
      entry.dwSize = sizeof (MODULEENTRY32);

      for (ok = Module32First (snapshot, &entry); ok; ok = Module32Next (snapshot, &entry))
	{
	  if (strcmp (filename, entry.szExePath) == 0)
	    continue;

	  module_handle = (uintptr_t) entry.hModule;
	  if (module_handle == 0)
	    continue;

	  descriptor = backtrace_open (entry.szExePath, error_callback, data,
				       NULL);
	  if (descriptor < 0)
	    continue;

	  coff_add (state, descriptor, error_callback, data,
		    &module_fileline_fn, &module_found_sym, &found_dwarf,
		    module_handle);
	  if (module_found_sym)
	    found_sym = 1;
	}

      CloseHandle (snapshot);
    }
#endif

#ifdef HAVE_WINDOWS_H
  nt_dll_handle = GetModuleHandleW (L"ntdll.dll");
  if (nt_dll_handle)
    {
      LDR_REGISTER_FUNCTION register_func;
      const char register_name[] = "LdrRegisterDllNotification";
      register_func = (void*) GetProcAddress (nt_dll_handle,
					      register_name);

      if (register_func)
	{
	  PVOID cookie;
	  struct dll_notification_context *context
	    = backtrace_alloc (state,
			       sizeof (struct dll_notification_context),
			       error_callback, data);

	  if (context)
	    {
	      context->state = state;
	      context->data = data;
	      context->error_callback = error_callback;

	      register_func (0, &dll_notification, context, &cookie);
	    }
	}
    }
#endif /* defined(HAVE_WINDOWS_H) */

  if (!state->threaded)
    {
      if (found_sym)
	state->syminfo_fn = coff_syminfo;
      else if (state->syminfo_fn == NULL)
	state->syminfo_fn = coff_nosyms;
    }
  else
    {
      if (found_sym)
	backtrace_atomic_store_pointer (&state->syminfo_fn, coff_syminfo);
      else
	(void) __sync_bool_compare_and_swap (&state->syminfo_fn, NULL,
					     coff_nosyms);
    }

  if (!state->threaded)
    {
      if (state->fileline_fn == NULL || state->fileline_fn == coff_nodebug)
	*fileline_fn = coff_fileline_fn;
    }
  else
    {
      fileline current_fn;

      current_fn = backtrace_atomic_load_pointer (&state->fileline_fn);
      if (current_fn == NULL || current_fn == coff_nodebug)
	*fileline_fn = coff_fileline_fn;
    }

  return 1;
}
