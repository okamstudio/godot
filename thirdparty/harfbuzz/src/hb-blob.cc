/*
 * Copyright © 2009  Red Hat, Inc.
 * Copyright © 2018  Ebrahim Byagowi
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Behdad Esfahbod
 */

#include "hb.hh"
#include "hb-blob.hh"

#ifdef HAVE_SYS_MMAN_H
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <sys/mman.h>
#endif /* HAVE_SYS_MMAN_H */


/**
 * SECTION: hb-blob
 * @title: hb-blob
 * @short_description: Binary data containers
 * @include: hb.h
 *
 * Blobs wrap a chunk of binary data to handle lifecycle management of data
 * while it is passed between client and HarfBuzz.  Blobs are primarily used
 * to create font faces, but also to access font face tables, as well as
 * pass around other binary data.
 **/


/**
 * hb_blob_create: (skip)
 * @data: Pointer to blob data.
 * @length: Length of @data in bytes.
 * @mode: Memory mode for @data.
 * @user_data: Data parameter to pass to @destroy.
 * @destroy: (nullable): Callback to call when @data is not needed anymore.
 *
 * Creates a new "blob" object wrapping @data.  The @mode parameter is used
 * to negotiate ownership and lifecycle of @data.
 *
 * Return value: New blob, or the empty blob if something failed or if @length is
 * zero.  Destroy with hb_blob_destroy().
 *
 * Since: 0.9.2
 **/
hb_blob_t *
hb_blob_create (const char        *data,
		unsigned int       length,
		hb_memory_mode_t   mode,
		void              *user_data,
		hb_destroy_func_t  destroy)
{
  if (!length)
  {
    if (destroy)
      destroy (user_data);
    return hb_blob_get_empty ();
  }

  hb_blob_t *blob = hb_blob_create_or_fail (data, length, mode,
					    user_data, destroy);
  return likely (blob) ? blob : hb_blob_get_empty ();
}

/**
 * hb_blob_create_or_fail: (skip)
 * @data: Pointer to blob data.
 * @length: Length of @data in bytes.
 * @mode: Memory mode for @data.
 * @user_data: Data parameter to pass to @destroy.
 * @destroy: (nullable): Callback to call when @data is not needed anymore.
 *
 * Creates a new "blob" object wrapping @data.  The @mode parameter is used
 * to negotiate ownership and lifecycle of @data.
 *
 * Note that this function returns a freshly-allocated empty blob even if @length
 * is zero. This is in contrast to hb_blob_create(), which returns the singleton
 * empty blob (as returned by hb_blob_get_empty()) if @length is zero.
 *
 * Return value: New blob, or `NULL` if failed.  Destroy with hb_blob_destroy().
 *
 * Since: 2.8.2
 **/
hb_blob_t *
hb_blob_create_or_fail (const char        *data,
			unsigned int       length,
			hb_memory_mode_t   mode,
			void              *user_data,
			hb_destroy_func_t  destroy)
{
  hb_blob_t *blob;

  if (length >= 1u << 31 ||
      !(blob = hb_object_create<hb_blob_t> ()))
  {
    if (destroy)
      destroy (user_data);
    return nullptr;
  }

  blob->data = data;
  blob->length = length;
  blob->mode = mode;

  blob->user_data = user_data;
  blob->destroy = destroy;

  if (blob->mode == HB_MEMORY_MODE_DUPLICATE) {
    blob->mode = HB_MEMORY_MODE_READONLY;
    if (!blob->try_make_writable ())
    {
      hb_blob_destroy (blob);
      return nullptr;
    }
  }

  return blob;
}

static void
_hb_blob_destroy (void *data)
{
  hb_blob_destroy ((hb_blob_t *) data);
}

/**
 * hb_blob_create_sub_blob:
 * @parent: Parent blob.
 * @offset: Start offset of sub-blob within @parent, in bytes.
 * @length: Length of sub-blob.
 *
 * Returns a blob that represents a range of bytes in @parent.  The new
 * blob is always created with #HB_MEMORY_MODE_READONLY, meaning that it
 * will never modify data in the parent blob.  The parent data is not
 * expected to be modified, and will result in undefined behavior if it
 * is.
 *
 * Makes @parent immutable.
 *
 * Return value: New blob, or the empty blob if something failed or if
 * @length is zero or @offset is beyond the end of @parent's data.  Destroy
 * with hb_blob_destroy().
 *
 * Since: 0.9.2
 **/
hb_blob_t *
hb_blob_create_sub_blob (hb_blob_t    *parent,
			 unsigned int  offset,
			 unsigned int  length)
{
  hb_blob_t *blob;

  if (!length || !parent || offset >= parent->length)
    return hb_blob_get_empty ();

  hb_blob_make_immutable (parent);

  blob = hb_blob_create (parent->data + offset,
			 hb_min (length, parent->length - offset),
			 HB_MEMORY_MODE_READONLY,
			 hb_blob_reference (parent),
			 _hb_blob_destroy);

  return blob;
}

/**
 * hb_blob_copy_writable_or_fail:
 * @blob: A blob.
 *
 * Makes a writable copy of @blob.
 *
 * Return value: The new blob, or nullptr if allocation failed
 *
 * Since: 1.8.0
 **/
hb_blob_t *
hb_blob_copy_writable_or_fail (hb_blob_t *blob)
{
  blob = hb_blob_create (blob->data,
			 blob->length,
			 HB_MEMORY_MODE_DUPLICATE,
			 nullptr,
			 nullptr);

  if (unlikely (blob == hb_blob_get_empty ()))
    blob = nullptr;

  return blob;
}

/**
 * hb_blob_get_empty:
 *
 * Returns the singleton empty blob.
 *
 * See TODO:link object types for more information.
 *
 * Return value: (transfer full): The empty blob.
 *
 * Since: 0.9.2
 **/
hb_blob_t *
hb_blob_get_empty ()
{
  return const_cast<hb_blob_t *> (&Null (hb_blob_t));
}

/**
 * hb_blob_reference: (skip)
 * @blob: a blob.
 *
 * Increases the reference count on @blob.
 *
 * See TODO:link object types for more information.
 *
 * Return value: @blob.
 *
 * Since: 0.9.2
 **/
hb_blob_t *
hb_blob_reference (hb_blob_t *blob)
{
  return hb_object_reference (blob);
}

/**
 * hb_blob_destroy: (skip)
 * @blob: a blob.
 *
 * Decreases the reference count on @blob, and if it reaches zero, destroys
 * @blob, freeing all memory, possibly calling the destroy-callback the blob
 * was created for if it has not been called already.
 *
 * See TODO:link object types for more information.
 *
 * Since: 0.9.2
 **/
void
hb_blob_destroy (hb_blob_t *blob)
{
  if (!hb_object_destroy (blob)) return;

  hb_free (blob);
}

/**
 * hb_blob_set_user_data: (skip)
 * @blob: An #hb_blob_t
 * @key: The user-data key to set
 * @data: A pointer to the user data to set
 * @destroy: (nullable): A callback to call when @data is not needed anymore
 * @replace: Whether to replace an existing data with the same key
 *
 * Attaches a user-data key/data pair to the specified blob.
 *
 * Return value: `true` if success, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_blob_set_user_data (hb_blob_t          *blob,
		       hb_user_data_key_t *key,
		       void *              data,
		       hb_destroy_func_t   destroy,
		       hb_bool_t           replace)
{
  return hb_object_set_user_data (blob, key, data, destroy, replace);
}

/**
 * hb_blob_get_user_data: (skip)
 * @blob: a blob
 * @key: The user-data key to query
 *
 * Fetches the user data associated with the specified key,
 * attached to the specified font-functions structure.
 *
 * Return value: (transfer none): A pointer to the user data
 *
 * Since: 0.9.2
 **/
void *
hb_blob_get_user_data (const hb_blob_t    *blob,
		       hb_user_data_key_t *key)
{
  return hb_object_get_user_data (blob, key);
}


/**
 * hb_blob_make_immutable:
 * @blob: a blob
 *
 * Makes a blob immutable.
 *
 * Since: 0.9.2
 **/
void
hb_blob_make_immutable (hb_blob_t *blob)
{
  if (hb_object_is_immutable (blob))
    return;

  hb_object_make_immutable (blob);
}

/**
 * hb_blob_is_immutable:
 * @blob: a blob.
 *
 * Tests whether a blob is immutable.
 *
 * Return value: `true` if @blob is immutable, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_blob_is_immutable (hb_blob_t *blob)
{
  return hb_object_is_immutable (blob);
}


/**
 * hb_blob_get_length:
 * @blob: a blob.
 *
 * Fetches the length of a blob's data.
 *
 * Return value: the length of @blob data in bytes.
 *
 * Since: 0.9.2
 **/
unsigned int
hb_blob_get_length (hb_blob_t *blob)
{
  return blob->length;
}

/**
 * hb_blob_get_data:
 * @blob: a blob.
 * @length: (out): The length in bytes of the data retrieved
 *
 * Fetches the data from a blob.
 *
 * Returns: (nullable) (transfer none) (array length=length): the byte data of @blob.
 *
 * Since: 0.9.2
 **/
const char *
hb_blob_get_data (hb_blob_t *blob, unsigned int *length)
{
  if (length)
    *length = blob->length;

  return blob->data;
}

/**
 * hb_blob_get_data_writable:
 * @blob: a blob.
 * @length: (out): output length of the writable data.
 *
 * Tries to make blob data writable (possibly copying it) and
 * return pointer to data.
 *
 * Fails if blob has been made immutable, or if memory allocation
 * fails.
 *
 * Returns: (transfer none) (array length=length): Writable blob data,
 * or `NULL` if failed.
 *
 * Since: 0.9.2
 **/
char *
hb_blob_get_data_writable (hb_blob_t *blob, unsigned int *length)
{
  if (hb_object_is_immutable (blob) ||
     !blob->try_make_writable ())
  {
    if (length) *length = 0;
    return nullptr;
  }

  if (length) *length = blob->length;
  return const_cast<char *> (blob->data);
}


bool
hb_blob_t::try_make_writable_inplace_unix ()
{
#if defined(HAVE_SYS_MMAN_H) && defined(HAVE_MPROTECT)
  uintptr_t pagesize = -1, mask, length;
  const char *addr;

#if defined(HAVE_SYSCONF) && defined(_SC_PAGE_SIZE)
  pagesize = (uintptr_t) sysconf (_SC_PAGE_SIZE);
#elif defined(HAVE_SYSCONF) && defined(_SC_PAGESIZE)
  pagesize = (uintptr_t) sysconf (_SC_PAGESIZE);
#elif defined(HAVE_GETPAGESIZE)
  pagesize = (uintptr_t) getpagesize ();
#endif

  if ((uintptr_t) -1L == pagesize) {
    DEBUG_MSG_FUNC (BLOB, this, "failed to get pagesize: %s", strerror (errno));
    return false;
  }
  DEBUG_MSG_FUNC (BLOB, this, "pagesize is %lu", (unsigned long) pagesize);

  mask = ~(pagesize-1);
  addr = (const char *) (((uintptr_t) this->data) & mask);
  length = (const char *) (((uintptr_t) this->data + this->length + pagesize-1) & mask)  - addr;
  DEBUG_MSG_FUNC (BLOB, this,
		  "calling mprotect on [%p..%p] (%lu bytes)",
		  addr, addr+length, (unsigned long) length);
  if (-1 == mprotect ((void *) addr, length, PROT_READ | PROT_WRITE)) {
    DEBUG_MSG_FUNC (BLOB, this, "mprotect failed: %s", strerror (errno));
    return false;
  }

  this->mode = HB_MEMORY_MODE_WRITABLE;

  DEBUG_MSG_FUNC (BLOB, this,
		  "successfully made [%p..%p] (%lu bytes) writable\n",
		  addr, addr+length, (unsigned long) length);
  return true;
#else
  return false;
#endif
}

bool
hb_blob_t::try_make_writable_inplace ()
{
  DEBUG_MSG_FUNC (BLOB, this, "making writable inplace\n");

  if (this->try_make_writable_inplace_unix ())
    return true;

  DEBUG_MSG_FUNC (BLOB, this, "making writable -> FAILED\n");

  /* Failed to make writable inplace, mark that */
  this->mode = HB_MEMORY_MODE_READONLY;
  return false;
}

bool
hb_blob_t::try_make_writable ()
{
  if (unlikely (!length))
    mode = HB_MEMORY_MODE_WRITABLE;

  if (this->mode == HB_MEMORY_MODE_WRITABLE)
    return true;

  if (this->mode == HB_MEMORY_MODE_READONLY_MAY_MAKE_WRITABLE && this->try_make_writable_inplace ())
    return true;

  if (this->mode == HB_MEMORY_MODE_WRITABLE)
    return true;


  DEBUG_MSG_FUNC (BLOB, this, "current data is -> %p\n", this->data);

  char *new_data;

  new_data = (char *) hb_malloc (this->length);
  if (unlikely (!new_data))
    return false;

  DEBUG_MSG_FUNC (BLOB, this, "dupped successfully -> %p\n", this->data);

  hb_memcpy (new_data, this->data, this->length);
  this->destroy_user_data ();
  this->mode = HB_MEMORY_MODE_WRITABLE;
  this->data = new_data;
  this->user_data = new_data;
  this->destroy = hb_free;

  return true;
}

/*
 * Mmap
 */

#ifndef HB_NO_OPEN
#ifdef HAVE_MMAP
# if !defined(HB_NO_RESOURCE_FORK) && defined(__APPLE__)
#  include <sys/paths.h>
# endif
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
#endif

#ifdef _WIN32
# include <windows.h>
#else
# ifndef O_BINARY
#  define O_BINARY 0
# endif
#endif

#ifndef MAP_NORESERVE
# define MAP_NORESERVE 0
#endif

struct hb_mapped_file_t
{
  char *contents;
  unsigned long length;
#ifdef _WIN32
  HANDLE mapping;
#endif
};

#if (defined(HAVE_MMAP) || defined(_WIN32)) && !defined(HB_NO_MMAP)
static void
_hb_mapped_file_destroy (void *file_)
{
  hb_mapped_file_t *file = (hb_mapped_file_t *) file_;
#ifdef HAVE_MMAP
  munmap (file->contents, file->length);
#elif defined(_WIN32)
  UnmapViewOfFile (file->contents);
  CloseHandle (file->mapping);
#else
  assert (0); // If we don't have mmap we shouldn't reach here
#endif

  hb_free (file);
}
#endif

#ifdef _PATH_RSRCFORKSPEC
static int
_open_resource_fork (const char *file_name, hb_mapped_file_t *file)
{
  size_t name_len = strlen (file_name);
  size_t len = name_len + sizeof (_PATH_RSRCFORKSPEC);

  char *rsrc_name = (char *) hb_malloc (len);
  if (unlikely (!rsrc_name)) return -1;

  strncpy (rsrc_name, file_name, name_len);
  strncpy (rsrc_name + name_len, _PATH_RSRCFORKSPEC,
	   sizeof (_PATH_RSRCFORKSPEC));

  int fd = open (rsrc_name, O_RDONLY | O_BINARY, 0);
  hb_free (rsrc_name);

  if (fd != -1)
  {
    struct stat st;
    if (fstat (fd, &st) != -1)
      file->length = (unsigned long) st.st_size;
    else
    {
      close (fd);
      fd = -1;
    }
  }

  return fd;
}
#endif

/**
 * hb_blob_create_from_file:
 * @file_name: A font filename
 *
 * Creates a new blob containing the data from the
 * specified binary font file.
 *
 * The filename is passed directly to the system on all platforms,
 * except on Windows, where the filename is interpreted as UTF-8.
 * Only if the filename is not valid UTF-8, it will be interpreted
 * according to the system codepage.
 *
 * Returns: An #hb_blob_t pointer with the content of the file,
 * or hb_blob_get_empty() if failed.
 *
 * Since: 1.7.7
 **/
hb_blob_t *
hb_blob_create_from_file (const char *file_name)
{
  hb_blob_t *blob = hb_blob_create_from_file_or_fail (file_name);
  return likely (blob) ? blob : hb_blob_get_empty ();
}

/**
 * hb_blob_create_from_file_or_fail:
 * @file_name: A filename
 *
 * Creates a new blob containing the data from the specified file.
 *
 * The filename is passed directly to the system on all platforms,
 * except on Windows, where the filename is interpreted as UTF-8.
 * Only if the filename is not valid UTF-8, it will be interpreted
 * according to the system codepage.
 *
 * Returns: An #hb_blob_t pointer with the content of the file,
 * or `NULL` if failed.
 *
 * Since: 2.8.2
 **/
hb_blob_t *
hb_blob_create_from_file_or_fail (const char *file_name)
{
  /* Adopted from glib's gmappedfile.c with Matthias Clasen and
     Allison Lortie permission but changed a lot to suit our need. */
#if defined(HAVE_MMAP) && !defined(HB_NO_MMAP)
  hb_mapped_file_t *file = (hb_mapped_file_t *) hb_calloc (1, sizeof (hb_mapped_file_t));
  if (unlikely (!file)) return nullptr;

  int fd = open (file_name, O_RDONLY | O_BINARY, 0);
  if (unlikely (fd == -1)) goto fail_without_close;

  struct stat st;
  if (unlikely (fstat (fd, &st) == -1)) goto fail;

  file->length = (unsigned long) st.st_size;

#ifdef _PATH_RSRCFORKSPEC
  if (unlikely (file->length == 0))
  {
    int rfd = _open_resource_fork (file_name, file);
    if (rfd != -1)
    {
      close (fd);
      fd = rfd;
    }
  }
#endif

  file->contents = (char *) mmap (nullptr, file->length, PROT_READ,
				  MAP_PRIVATE | MAP_NORESERVE, fd, 0);

  if (unlikely (file->contents == MAP_FAILED)) goto fail;

  close (fd);

  return hb_blob_create_or_fail (file->contents, file->length,
				 HB_MEMORY_MODE_READONLY_MAY_MAKE_WRITABLE, (void *) file,
				 (hb_destroy_func_t) _hb_mapped_file_destroy);

fail:
  close (fd);
fail_without_close:
  hb_free (file);

#elif defined(_WIN32) && !defined(HB_NO_MMAP)
  hb_mapped_file_t *file = (hb_mapped_file_t *) hb_calloc (1, sizeof (hb_mapped_file_t));
  if (unlikely (!file)) return nullptr;

  HANDLE fd;
  int conversion;
  unsigned int size = strlen (file_name) + 1;
  wchar_t * wchar_file_name = (wchar_t *) hb_malloc (sizeof (wchar_t) * size);
  if (unlikely (!wchar_file_name)) goto fail_without_close;

  /* Assume file name is given in UTF-8 encoding */
  conversion = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, file_name, -1, wchar_file_name, size);
  if (conversion <= 0)
  {
    /* Conversion failed due to invalid UTF-8 characters,
       Repeat conversion based on system code page */
    mbstowcs(wchar_file_name, file_name, size);
  }
#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
  {
    CREATEFILE2_EXTENDED_PARAMETERS ceparams = { 0 };
    ceparams.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
    ceparams.dwFileAttributes = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED & 0xFFFF;
    ceparams.dwFileFlags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED & 0xFFF00000;
    ceparams.dwSecurityQosFlags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED & 0x000F0000;
    ceparams.lpSecurityAttributes = nullptr;
    ceparams.hTemplateFile = nullptr;
    fd = CreateFile2 (wchar_file_name, GENERIC_READ, FILE_SHARE_READ,
		      OPEN_EXISTING, &ceparams);
  }
#else
  fd = CreateFileW (wchar_file_name, GENERIC_READ, FILE_SHARE_READ, nullptr,
		    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_OVERLAPPED,
		    nullptr);
#endif
  hb_free (wchar_file_name);

  if (unlikely (fd == INVALID_HANDLE_VALUE)) goto fail_without_close;

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
  {
    LARGE_INTEGER length;
    GetFileSizeEx (fd, &length);
    file->length = length.LowPart;
    file->mapping = CreateFileMappingFromApp (fd, nullptr, PAGE_READONLY, length.QuadPart, nullptr);
  }
#else
  file->length = (unsigned long) GetFileSize (fd, nullptr);
  file->mapping = CreateFileMapping (fd, nullptr, PAGE_READONLY, 0, 0, nullptr);
#endif
  if (unlikely (!file->mapping)) goto fail;

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP) && WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
  file->contents = (char *) MapViewOfFileFromApp (file->mapping, FILE_MAP_READ, 0, 0);
#else
  file->contents = (char *) MapViewOfFile (file->mapping, FILE_MAP_READ, 0, 0, 0);
#endif
  if (unlikely (!file->contents)) goto fail;

  CloseHandle (fd);
  return hb_blob_create_or_fail (file->contents, file->length,
				 HB_MEMORY_MODE_READONLY_MAY_MAKE_WRITABLE, (void *) file,
				 (hb_destroy_func_t) _hb_mapped_file_destroy);

fail:
  CloseHandle (fd);
fail_without_close:
  hb_free (file);

#endif

  /* The following tries to read a file without knowing its size beforehand
     It's used as a fallback for systems without mmap or to read from pipes */
  unsigned long len = 0, allocated = BUFSIZ * 16;
  char *data = (char *) hb_malloc (allocated);
  if (unlikely (!data)) return nullptr;

  FILE *fp = fopen (file_name, "rb");
  if (unlikely (!fp)) goto fread_fail_without_close;

  while (!feof (fp))
  {
    if (allocated - len < BUFSIZ)
    {
      allocated *= 2;
      /* Don't allocate and go more than ~536MB, our mmap reader still
	 can cover files like that but lets limit our fallback reader */
      if (unlikely (allocated > (2 << 28))) goto fread_fail;
      char *new_data = (char *) hb_realloc (data, allocated);
      if (unlikely (!new_data)) goto fread_fail;
      data = new_data;
    }

    unsigned long addition = fread (data + len, 1, allocated - len, fp);

    int err = ferror (fp);
#ifdef EINTR // armcc doesn't have it
    if (unlikely (err == EINTR)) continue;
#endif
    if (unlikely (err)) goto fread_fail;

    len += addition;
  }
	fclose (fp);

  return hb_blob_create_or_fail (data, len, HB_MEMORY_MODE_WRITABLE, data,
				 (hb_destroy_func_t) hb_free);

fread_fail:
  fclose (fp);
fread_fail_without_close:
  hb_free (data);
  return nullptr;
}
#endif /* !HB_NO_OPEN */
