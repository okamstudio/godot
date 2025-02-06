/**
 * THIS FILE IS AUTO-GENERATED from the following files:
 *   typenames.witx, wasi_snapshot_preview1.witx
 *
 * @file
 * This file describes the [WASI] interface, consisting of functions, types,
 * and defined values (macros).
 *
 * The interface described here is greatly inspired by [CloudABI]'s clean,
 * thoughtfully-designed, capability-oriented, POSIX-style API.
 *
 * [CloudABI]: https://github.com/NuxiNL/cloudlibc
 * [WASI]: https://github.com/WebAssembly/WASI/
 */

/*
 * File origin:
 *   https://github.com/CraneStation/wasi-libc/blob/master/libc-bottom-half/headers/public/wasi/api.h
 * Revision:
 *   2c2fc9a2fddd0927a66f1c142e65c8dab6f5c5d7
 * License:
 *   CC0 1.0 Universal (CC0 1.0) Public Domain Dedication
 *   https://creativecommons.org/publicdomain/zero/1.0/
 */

#ifndef __wasi_api_h
#define __wasi_api_h

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
// Hacks alert
#undef _Static_assert
#define _Static_assert(...)
#undef _Noreturn
#define _Noreturn
#endif

_Static_assert(_Alignof(int8_t) == 1, "non-wasi data layout");
_Static_assert(_Alignof(uint8_t) == 1, "non-wasi data layout");
_Static_assert(_Alignof(int16_t) == 2, "non-wasi data layout");
_Static_assert(_Alignof(uint16_t) == 2, "non-wasi data layout");
_Static_assert(_Alignof(int32_t) == 4, "non-wasi data layout");
_Static_assert(_Alignof(uint32_t) == 4, "non-wasi data layout");
_Static_assert(_Alignof(int64_t) == 8, "non-wasi data layout");
_Static_assert(_Alignof(uint64_t) == 8, "non-wasi data layout");
// _Static_assert(_Alignof(void*) == 4, "non-wasi data layout");

#ifdef __cplusplus
extern "C" {
#endif

// TODO: Encoding this in witx.
#define __WASI_DIRCOOKIE_START (UINT64_C(0))
// typedef __SIZE_TYPE__ __wasi_size_t;
typedef uint32_t __wasi_size_t; // fixme for wasm64

// _Static_assert(sizeof(__wasi_size_t) == 4, "witx calculated size");
// _Static_assert(_Alignof(__wasi_size_t) == 4, "witx calculated align");

/**
 * Non-negative file size or length of a region within a file.
 */
typedef uint64_t __wasi_filesize_t;

_Static_assert(sizeof(__wasi_filesize_t) == 8, "witx calculated size");
_Static_assert(_Alignof(__wasi_filesize_t) == 8, "witx calculated align");

/**
 * Timestamp in nanoseconds.
 */
typedef uint64_t __wasi_timestamp_t;

_Static_assert(sizeof(__wasi_timestamp_t) == 8, "witx calculated size");
_Static_assert(_Alignof(__wasi_timestamp_t) == 8, "witx calculated align");

/**
 * Identifiers for clocks.
 */
typedef uint32_t __wasi_clockid_t;

/**
 * The clock measuring real time. Time value zero corresponds with
 * 1970-01-01T00:00:00Z.
 */
#define __WASI_CLOCKID_REALTIME (UINT32_C(0))

/**
 * The store-wide monotonic clock, which is defined as a clock measuring
 * real time, whose value cannot be adjusted and which cannot have negative
 * clock jumps. The epoch of this clock is undefined. The absolute time
 * value of this clock therefore has no meaning.
 */
#define __WASI_CLOCKID_MONOTONIC (UINT32_C(1))

/**
 * The CPU-time clock associated with the current process.
 */
#define __WASI_CLOCKID_PROCESS_CPUTIME_ID (UINT32_C(2))

/**
 * The CPU-time clock associated with the current thread.
 */
#define __WASI_CLOCKID_THREAD_CPUTIME_ID (UINT32_C(3))

_Static_assert(sizeof(__wasi_clockid_t) == 4, "witx calculated size");
_Static_assert(_Alignof(__wasi_clockid_t) == 4, "witx calculated align");

/**
 * Error codes returned by functions.
 * Not all of these error codes are returned by the functions provided by this
 * API; some are used in higher-level library layers, and others are provided
 * merely for alignment with POSIX.
 */
typedef uint16_t __wasi_errno_t;

/**
 * No error occurred. System call completed successfully.
 */
#define __WASI_ERRNO_SUCCESS (UINT16_C(0))

/**
 * Argument list too long.
 */
#define __WASI_ERRNO_2BIG (UINT16_C(1))

/**
 * Permission denied.
 */
#define __WASI_ERRNO_ACCES (UINT16_C(2))

/**
 * Address in use.
 */
#define __WASI_ERRNO_ADDRINUSE (UINT16_C(3))

/**
 * Address not available.
 */
#define __WASI_ERRNO_ADDRNOTAVAIL (UINT16_C(4))

/**
 * Address family not supported.
 */
#define __WASI_ERRNO_AFNOSUPPORT (UINT16_C(5))

/**
 * Resource unavailable, or operation would block.
 */
#define __WASI_ERRNO_AGAIN (UINT16_C(6))

/**
 * Connection already in progress.
 */
#define __WASI_ERRNO_ALREADY (UINT16_C(7))

/**
 * Bad file descriptor.
 */
#define __WASI_ERRNO_BADF (UINT16_C(8))

/**
 * Bad message.
 */
#define __WASI_ERRNO_BADMSG (UINT16_C(9))

/**
 * Device or resource busy.
 */
#define __WASI_ERRNO_BUSY (UINT16_C(10))

/**
 * Operation canceled.
 */
#define __WASI_ERRNO_CANCELED (UINT16_C(11))

/**
 * No child processes.
 */
#define __WASI_ERRNO_CHILD (UINT16_C(12))

/**
 * Connection aborted.
 */
#define __WASI_ERRNO_CONNABORTED (UINT16_C(13))

/**
 * Connection refused.
 */
#define __WASI_ERRNO_CONNREFUSED (UINT16_C(14))

/**
 * Connection reset.
 */
#define __WASI_ERRNO_CONNRESET (UINT16_C(15))

/**
 * Resource deadlock would occur.
 */
#define __WASI_ERRNO_DEADLK (UINT16_C(16))

/**
 * Destination address required.
 */
#define __WASI_ERRNO_DESTADDRREQ (UINT16_C(17))

/**
 * Mathematics argument out of domain of function.
 */
#define __WASI_ERRNO_DOM (UINT16_C(18))

/**
 * Reserved.
 */
#define __WASI_ERRNO_DQUOT (UINT16_C(19))

/**
 * File exists.
 */
#define __WASI_ERRNO_EXIST (UINT16_C(20))

/**
 * Bad address.
 */
#define __WASI_ERRNO_FAULT (UINT16_C(21))

/**
 * File too large.
 */
#define __WASI_ERRNO_FBIG (UINT16_C(22))

/**
 * Host is unreachable.
 */
#define __WASI_ERRNO_HOSTUNREACH (UINT16_C(23))

/**
 * Identifier removed.
 */
#define __WASI_ERRNO_IDRM (UINT16_C(24))

/**
 * Illegal byte sequence.
 */
#define __WASI_ERRNO_ILSEQ (UINT16_C(25))

/**
 * Operation in progress.
 */
#define __WASI_ERRNO_INPROGRESS (UINT16_C(26))

/**
 * Interrupted function.
 */
#define __WASI_ERRNO_INTR (UINT16_C(27))

/**
 * Invalid argument.
 */
#define __WASI_ERRNO_INVAL (UINT16_C(28))

/**
 * I/O error.
 */
#define __WASI_ERRNO_IO (UINT16_C(29))

/**
 * Socket is connected.
 */
#define __WASI_ERRNO_ISCONN (UINT16_C(30))

/**
 * Is a directory.
 */
#define __WASI_ERRNO_ISDIR (UINT16_C(31))

/**
 * Too many levels of symbolic links.
 */
#define __WASI_ERRNO_LOOP (UINT16_C(32))

/**
 * File descriptor value too large.
 */
#define __WASI_ERRNO_MFILE (UINT16_C(33))

/**
 * Too many links.
 */
#define __WASI_ERRNO_MLINK (UINT16_C(34))

/**
 * Message too large.
 */
#define __WASI_ERRNO_MSGSIZE (UINT16_C(35))

/**
 * Reserved.
 */
#define __WASI_ERRNO_MULTIHOP (UINT16_C(36))

/**
 * Filename too long.
 */
#define __WASI_ERRNO_NAMETOOLONG (UINT16_C(37))

/**
 * Network is down.
 */
#define __WASI_ERRNO_NETDOWN (UINT16_C(38))

/**
 * Connection aborted by network.
 */
#define __WASI_ERRNO_NETRESET (UINT16_C(39))

/**
 * Network unreachable.
 */
#define __WASI_ERRNO_NETUNREACH (UINT16_C(40))

/**
 * Too many files open in system.
 */
#define __WASI_ERRNO_NFILE (UINT16_C(41))

/**
 * No buffer space available.
 */
#define __WASI_ERRNO_NOBUFS (UINT16_C(42))

/**
 * No such device.
 */
#define __WASI_ERRNO_NODEV (UINT16_C(43))

/**
 * No such file or directory.
 */
#define __WASI_ERRNO_NOENT (UINT16_C(44))

/**
 * Executable file format error.
 */
#define __WASI_ERRNO_NOEXEC (UINT16_C(45))

/**
 * No locks available.
 */
#define __WASI_ERRNO_NOLCK (UINT16_C(46))

/**
 * Reserved.
 */
#define __WASI_ERRNO_NOLINK (UINT16_C(47))

/**
 * Not enough space.
 */
#define __WASI_ERRNO_NOMEM (UINT16_C(48))

/**
 * No message of the desired type.
 */
#define __WASI_ERRNO_NOMSG (UINT16_C(49))

/**
 * Protocol not available.
 */
#define __WASI_ERRNO_NOPROTOOPT (UINT16_C(50))

/**
 * No space left on device.
 */
#define __WASI_ERRNO_NOSPC (UINT16_C(51))

/**
 * Function not supported.
 */
#define __WASI_ERRNO_NOSYS (UINT16_C(52))

/**
 * The socket is not connected.
 */
#define __WASI_ERRNO_NOTCONN (UINT16_C(53))

/**
 * Not a directory or a symbolic link to a directory.
 */
#define __WASI_ERRNO_NOTDIR (UINT16_C(54))

/**
 * Directory not empty.
 */
#define __WASI_ERRNO_NOTEMPTY (UINT16_C(55))

/**
 * State not recoverable.
 */
#define __WASI_ERRNO_NOTRECOVERABLE (UINT16_C(56))

/**
 * Not a socket.
 */
#define __WASI_ERRNO_NOTSOCK (UINT16_C(57))

/**
 * Not supported, or operation not supported on socket.
 */
#define __WASI_ERRNO_NOTSUP (UINT16_C(58))

/**
 * Inappropriate I/O control operation.
 */
#define __WASI_ERRNO_NOTTY (UINT16_C(59))

/**
 * No such device or address.
 */
#define __WASI_ERRNO_NXIO (UINT16_C(60))

/**
 * Value too large to be stored in data type.
 */
#define __WASI_ERRNO_OVERFLOW (UINT16_C(61))

/**
 * Previous owner died.
 */
#define __WASI_ERRNO_OWNERDEAD (UINT16_C(62))

/**
 * Operation not permitted.
 */
#define __WASI_ERRNO_PERM (UINT16_C(63))

/**
 * Broken pipe.
 */
#define __WASI_ERRNO_PIPE (UINT16_C(64))

/**
 * Protocol error.
 */
#define __WASI_ERRNO_PROTO (UINT16_C(65))

/**
 * Protocol not supported.
 */
#define __WASI_ERRNO_PROTONOSUPPORT (UINT16_C(66))

/**
 * Protocol wrong type for socket.
 */
#define __WASI_ERRNO_PROTOTYPE (UINT16_C(67))

/**
 * Result too large.
 */
#define __WASI_ERRNO_RANGE (UINT16_C(68))

/**
 * Read-only file system.
 */
#define __WASI_ERRNO_ROFS (UINT16_C(69))

/**
 * Invalid seek.
 */
#define __WASI_ERRNO_SPIPE (UINT16_C(70))

/**
 * No such process.
 */
#define __WASI_ERRNO_SRCH (UINT16_C(71))

/**
 * Reserved.
 */
#define __WASI_ERRNO_STALE (UINT16_C(72))

/**
 * Connection timed out.
 */
#define __WASI_ERRNO_TIMEDOUT (UINT16_C(73))

/**
 * Text file busy.
 */
#define __WASI_ERRNO_TXTBSY (UINT16_C(74))

/**
 * Cross-device link.
 */
#define __WASI_ERRNO_XDEV (UINT16_C(75))

/**
 * Extension: Capabilities insufficient.
 */
#define __WASI_ERRNO_NOTCAPABLE (UINT16_C(76))

_Static_assert(sizeof(__wasi_errno_t) == 2, "witx calculated size");
_Static_assert(_Alignof(__wasi_errno_t) == 2, "witx calculated align");

/**
 * File descriptor rights, determining which actions may be performed.
 */
typedef uint64_t __wasi_rights_t;

/**
 * The right to invoke `fd_datasync`.
 * If `path_open` is set, includes the right to invoke
 * `path_open` with `fdflags::dsync`.
 */
#define __WASI_RIGHTS_FD_DATASYNC (UINT64_C(1))

/**
 * The right to invoke `fd_read` and `sock_recv`.
 * If `rights::fd_seek` is set, includes the right to invoke `fd_pread`.
 */
#define __WASI_RIGHTS_FD_READ (UINT64_C(2))

/**
 * The right to invoke `fd_seek`. This flag implies `rights::fd_tell`.
 */
#define __WASI_RIGHTS_FD_SEEK (UINT64_C(4))

/**
 * The right to invoke `fd_fdstat_set_flags`.
 */
#define __WASI_RIGHTS_FD_FDSTAT_SET_FLAGS (UINT64_C(8))

/**
 * The right to invoke `fd_sync`.
 * If `path_open` is set, includes the right to invoke
 * `path_open` with `fdflags::rsync` and `fdflags::dsync`.
 */
#define __WASI_RIGHTS_FD_SYNC (UINT64_C(16))

/**
 * The right to invoke `fd_seek` in such a way that the file offset
 * remains unaltered (i.e., `whence::cur` with offset zero), or to
 * invoke `fd_tell`.
 */
#define __WASI_RIGHTS_FD_TELL (UINT64_C(32))

/**
 * The right to invoke `fd_write` and `sock_send`.
 * If `rights::fd_seek` is set, includes the right to invoke `fd_pwrite`.
 */
#define __WASI_RIGHTS_FD_WRITE (UINT64_C(64))

/**
 * The right to invoke `fd_advise`.
 */
#define __WASI_RIGHTS_FD_ADVISE (UINT64_C(128))

/**
 * The right to invoke `fd_allocate`.
 */
#define __WASI_RIGHTS_FD_ALLOCATE (UINT64_C(256))

/**
 * The right to invoke `path_create_directory`.
 */
#define __WASI_RIGHTS_PATH_CREATE_DIRECTORY (UINT64_C(512))

/**
 * If `path_open` is set, the right to invoke `path_open` with `oflags::creat`.
 */
#define __WASI_RIGHTS_PATH_CREATE_FILE (UINT64_C(1024))

/**
 * The right to invoke `path_link` with the file descriptor as the
 * source directory.
 */
#define __WASI_RIGHTS_PATH_LINK_SOURCE (UINT64_C(2048))

/**
 * The right to invoke `path_link` with the file descriptor as the
 * target directory.
 */
#define __WASI_RIGHTS_PATH_LINK_TARGET (UINT64_C(4096))

/**
 * The right to invoke `path_open`.
 */
#define __WASI_RIGHTS_PATH_OPEN (UINT64_C(8192))

/**
 * The right to invoke `fd_readdir`.
 */
#define __WASI_RIGHTS_FD_READDIR (UINT64_C(16384))

/**
 * The right to invoke `path_readlink`.
 */
#define __WASI_RIGHTS_PATH_READLINK (UINT64_C(32768))

/**
 * The right to invoke `path_rename` with the file descriptor as the source directory.
 */
#define __WASI_RIGHTS_PATH_RENAME_SOURCE (UINT64_C(65536))

/**
 * The right to invoke `path_rename` with the file descriptor as the target directory.
 */
#define __WASI_RIGHTS_PATH_RENAME_TARGET (UINT64_C(131072))

/**
 * The right to invoke `path_filestat_get`.
 */
#define __WASI_RIGHTS_PATH_FILESTAT_GET (UINT64_C(262144))

/**
 * The right to change a file's size (there is no `path_filestat_set_size`).
 * If `path_open` is set, includes the right to invoke `path_open` with `oflags::trunc`.
 */
#define __WASI_RIGHTS_PATH_FILESTAT_SET_SIZE (UINT64_C(524288))

/**
 * The right to invoke `path_filestat_set_times`.
 */
#define __WASI_RIGHTS_PATH_FILESTAT_SET_TIMES (UINT64_C(1048576))

/**
 * The right to invoke `fd_filestat_get`.
 */
#define __WASI_RIGHTS_FD_FILESTAT_GET (UINT64_C(2097152))

/**
 * The right to invoke `fd_filestat_set_size`.
 */
#define __WASI_RIGHTS_FD_FILESTAT_SET_SIZE (UINT64_C(4194304))

/**
 * The right to invoke `fd_filestat_set_times`.
 */
#define __WASI_RIGHTS_FD_FILESTAT_SET_TIMES (UINT64_C(8388608))

/**
 * The right to invoke `path_symlink`.
 */
#define __WASI_RIGHTS_PATH_SYMLINK (UINT64_C(16777216))

/**
 * The right to invoke `path_remove_directory`.
 */
#define __WASI_RIGHTS_PATH_REMOVE_DIRECTORY (UINT64_C(33554432))

/**
 * The right to invoke `path_unlink_file`.
 */
#define __WASI_RIGHTS_PATH_UNLINK_FILE (UINT64_C(67108864))

/**
 * If `rights::fd_read` is set, includes the right to invoke `poll_oneoff` to subscribe to `eventtype::fd_read`.
 * If `rights::fd_write` is set, includes the right to invoke `poll_oneoff` to subscribe to `eventtype::fd_write`.
 */
#define __WASI_RIGHTS_POLL_FD_READWRITE (UINT64_C(134217728))

/**
 * The right to invoke `sock_shutdown`.
 */
#define __WASI_RIGHTS_SOCK_SHUTDOWN (UINT64_C(268435456))

_Static_assert(sizeof(__wasi_rights_t) == 8, "witx calculated size");
_Static_assert(_Alignof(__wasi_rights_t) == 8, "witx calculated align");

/**
 * A file descriptor index.
 */
typedef uint32_t __wasi_fd_t;

_Static_assert(sizeof(__wasi_fd_t) == 4, "witx calculated size");
_Static_assert(_Alignof(__wasi_fd_t) == 4, "witx calculated align");

/**
 * A region of memory for scatter/gather reads.
 */
typedef struct __wasi_iovec_t {
    /**
     * The address of the buffer to be filled.
     */
    uint8_t * buf;

    /**
     * The length of the buffer to be filled.
     */
    __wasi_size_t buf_len;

} __wasi_iovec_t;

// _Static_assert(sizeof(__wasi_iovec_t) == 8, "witx calculated size");
// _Static_assert(_Alignof(__wasi_iovec_t) == 4, "witx calculated align");
// _Static_assert(offsetof(__wasi_iovec_t, buf) == 0, "witx calculated offset");
// _Static_assert(offsetof(__wasi_iovec_t, buf_len) == 4, "witx calculated offset");

/**
 * A region of memory for scatter/gather writes.
 */
typedef struct __wasi_ciovec_t {
    /**
     * The address of the buffer to be written.
     */
    const uint8_t * buf;

    /**
     * The length of the buffer to be written.
     */
    __wasi_size_t buf_len;

} __wasi_ciovec_t;

// _Static_assert(sizeof(__wasi_ciovec_t) == 8, "witx calculated size");
// _Static_assert(_Alignof(__wasi_ciovec_t) == 4, "witx calculated align");
// _Static_assert(offsetof(__wasi_ciovec_t, buf) == 0, "witx calculated offset");
// _Static_assert(offsetof(__wasi_ciovec_t, buf_len) == 4, "witx calculated offset");

/**
 * Relative offset within a file.
 */
typedef int64_t __wasi_filedelta_t;

_Static_assert(sizeof(__wasi_filedelta_t) == 8, "witx calculated size");
_Static_assert(_Alignof(__wasi_filedelta_t) == 8, "witx calculated align");

/**
 * The position relative to which to set the offset of the file descriptor.
 */
typedef uint8_t __wasi_whence_t;

/**
 * Seek relative to start-of-file.
 */
#define __WASI_WHENCE_SET (UINT8_C(0))

/**
 * Seek relative to current position.
 */
#define __WASI_WHENCE_CUR (UINT8_C(1))

/**
 * Seek relative to end-of-file.
 */
#define __WASI_WHENCE_END (UINT8_C(2))

_Static_assert(sizeof(__wasi_whence_t) == 1, "witx calculated size");
_Static_assert(_Alignof(__wasi_whence_t) == 1, "witx calculated align");

/**
 * A reference to the offset of a directory entry.
 *
 * The value 0 signifies the start of the directory.
 */
typedef uint64_t __wasi_dircookie_t;

_Static_assert(sizeof(__wasi_dircookie_t) == 8, "witx calculated size");
_Static_assert(_Alignof(__wasi_dircookie_t) == 8, "witx calculated align");

/**
 * The type for the $d_namlen field of $dirent.
 */
typedef uint32_t __wasi_dirnamlen_t;

_Static_assert(sizeof(__wasi_dirnamlen_t) == 4, "witx calculated size");
_Static_assert(_Alignof(__wasi_dirnamlen_t) == 4, "witx calculated align");

/**
 * File serial number that is unique within its file system.
 */
typedef uint64_t __wasi_inode_t;

_Static_assert(sizeof(__wasi_inode_t) == 8, "witx calculated size");
_Static_assert(_Alignof(__wasi_inode_t) == 8, "witx calculated align");

/**
 * The type of a file descriptor or file.
 */
typedef uint8_t __wasi_filetype_t;

/**
 * The type of the file descriptor or file is unknown or is different from any of the other types specified.
 */
#define __WASI_FILETYPE_UNKNOWN (UINT8_C(0))

/**
 * The file descriptor or file refers to a block device inode.
 */
#define __WASI_FILETYPE_BLOCK_DEVICE (UINT8_C(1))

/**
 * The file descriptor or file refers to a character device inode.
 */
#define __WASI_FILETYPE_CHARACTER_DEVICE (UINT8_C(2))

/**
 * The file descriptor or file refers to a directory inode.
 */
#define __WASI_FILETYPE_DIRECTORY (UINT8_C(3))

/**
 * The file descriptor or file refers to a regular file inode.
 */
#define __WASI_FILETYPE_REGULAR_FILE (UINT8_C(4))

/**
 * The file descriptor or file refers to a datagram socket.
 */
#define __WASI_FILETYPE_SOCKET_DGRAM (UINT8_C(5))

/**
 * The file descriptor or file refers to a byte-stream socket.
 */
#define __WASI_FILETYPE_SOCKET_STREAM (UINT8_C(6))

/**
 * The file refers to a symbolic link inode.
 */
#define __WASI_FILETYPE_SYMBOLIC_LINK (UINT8_C(7))

_Static_assert(sizeof(__wasi_filetype_t) == 1, "witx calculated size");
_Static_assert(_Alignof(__wasi_filetype_t) == 1, "witx calculated align");

/**
 * A directory entry.
 */
typedef struct __wasi_dirent_t {
    /**
     * The offset of the next directory entry stored in this directory.
     */
    __wasi_dircookie_t d_next;

    /**
     * The serial number of the file referred to by this directory entry.
     */
    __wasi_inode_t d_ino;

    /**
     * The length of the name of the directory entry.
     */
    __wasi_dirnamlen_t d_namlen;

    /**
     * The type of the file referred to by this directory entry.
     */
    __wasi_filetype_t d_type;

} __wasi_dirent_t;

_Static_assert(sizeof(__wasi_dirent_t) == 24, "witx calculated size");
_Static_assert(_Alignof(__wasi_dirent_t) == 8, "witx calculated align");
_Static_assert(offsetof(__wasi_dirent_t, d_next) == 0, "witx calculated offset");
_Static_assert(offsetof(__wasi_dirent_t, d_ino) == 8, "witx calculated offset");
_Static_assert(offsetof(__wasi_dirent_t, d_namlen) == 16, "witx calculated offset");
_Static_assert(offsetof(__wasi_dirent_t, d_type) == 20, "witx calculated offset");

/**
 * File or memory access pattern advisory information.
 */
typedef uint8_t __wasi_advice_t;

/**
 * The application has no advice to give on its behavior with respect to the specified data.
 */
#define __WASI_ADVICE_NORMAL (UINT8_C(0))

/**
 * The application expects to access the specified data sequentially from lower offsets to higher offsets.
 */
#define __WASI_ADVICE_SEQUENTIAL (UINT8_C(1))

/**
 * The application expects to access the specified data in a random order.
 */
#define __WASI_ADVICE_RANDOM (UINT8_C(2))

/**
 * The application expects to access the specified data in the near future.
 */
#define __WASI_ADVICE_WILLNEED (UINT8_C(3))

/**
 * The application expects that it will not access the specified data in the near future.
 */
#define __WASI_ADVICE_DONTNEED (UINT8_C(4))

/**
 * The application expects to access the specified data once and then not reuse it thereafter.
 */
#define __WASI_ADVICE_NOREUSE (UINT8_C(5))

_Static_assert(sizeof(__wasi_advice_t) == 1, "witx calculated size");
_Static_assert(_Alignof(__wasi_advice_t) == 1, "witx calculated align");

/**
 * File descriptor flags.
 */
typedef uint16_t __wasi_fdflags_t;

/**
 * Append mode: Data written to the file is always appended to the file's end.
 */
#define __WASI_FDFLAGS_APPEND (UINT16_C(1))

/**
 * Write according to synchronized I/O data integrity completion. Only the data stored in the file is synchronized.
 */
#define __WASI_FDFLAGS_DSYNC (UINT16_C(2))

/**
 * Non-blocking mode.
 */
#define __WASI_FDFLAGS_NONBLOCK (UINT16_C(4))

/**
 * Synchronized read I/O operations.
 */
#define __WASI_FDFLAGS_RSYNC (UINT16_C(8))

/**
 * Write according to synchronized I/O file integrity completion. In
 * addition to synchronizing the data stored in the file, the implementation
 * may also synchronously update the file's metadata.
 */
#define __WASI_FDFLAGS_SYNC (UINT16_C(16))

_Static_assert(sizeof(__wasi_fdflags_t) == 2, "witx calculated size");
_Static_assert(_Alignof(__wasi_fdflags_t) == 2, "witx calculated align");

/**
 * File descriptor attributes.
 */
typedef struct __wasi_fdstat_t {
    /**
     * File type.
     */
    __wasi_filetype_t fs_filetype;

    /**
     * File descriptor flags.
     */
    __wasi_fdflags_t fs_flags;

    /**
     * Rights that apply to this file descriptor.
     */
    __wasi_rights_t fs_rights_base;

    /**
     * Maximum set of rights that may be installed on new file descriptors that
     * are created through this file descriptor, e.g., through `path_open`.
     */
    __wasi_rights_t fs_rights_inheriting;

} __wasi_fdstat_t;

_Static_assert(sizeof(__wasi_fdstat_t) == 24, "witx calculated size");
_Static_assert(_Alignof(__wasi_fdstat_t) == 8, "witx calculated align");
_Static_assert(offsetof(__wasi_fdstat_t, fs_filetype) == 0, "witx calculated offset");
_Static_assert(offsetof(__wasi_fdstat_t, fs_flags) == 2, "witx calculated offset");
_Static_assert(offsetof(__wasi_fdstat_t, fs_rights_base) == 8, "witx calculated offset");
_Static_assert(offsetof(__wasi_fdstat_t, fs_rights_inheriting) == 16, "witx calculated offset");

/**
 * Identifier for a device containing a file system. Can be used in combination
 * with `inode` to uniquely identify a file or directory in the filesystem.
 */
typedef uint64_t __wasi_device_t;

_Static_assert(sizeof(__wasi_device_t) == 8, "witx calculated size");
_Static_assert(_Alignof(__wasi_device_t) == 8, "witx calculated align");

/**
 * Which file time attributes to adjust.
 */
typedef uint16_t __wasi_fstflags_t;

/**
 * Adjust the last data access timestamp to the value stored in `filestat::atim`.
 */
#define __WASI_FSTFLAGS_ATIM (UINT16_C(1))

/**
 * Adjust the last data access timestamp to the time of clock `clockid::realtime`.
 */
#define __WASI_FSTFLAGS_ATIM_NOW (UINT16_C(2))

/**
 * Adjust the last data modification timestamp to the value stored in `filestat::mtim`.
 */
#define __WASI_FSTFLAGS_MTIM (UINT16_C(4))

/**
 * Adjust the last data modification timestamp to the time of clock `clockid::realtime`.
 */
#define __WASI_FSTFLAGS_MTIM_NOW (UINT16_C(8))

_Static_assert(sizeof(__wasi_fstflags_t) == 2, "witx calculated size");
_Static_assert(_Alignof(__wasi_fstflags_t) == 2, "witx calculated align");

/**
 * Flags determining the method of how paths are resolved.
 */
typedef uint32_t __wasi_lookupflags_t;

/**
 * As long as the resolved path corresponds to a symbolic link, it is expanded.
 */
#define __WASI_LOOKUPFLAGS_SYMLINK_FOLLOW (UINT32_C(1))

_Static_assert(sizeof(__wasi_lookupflags_t) == 4, "witx calculated size");
_Static_assert(_Alignof(__wasi_lookupflags_t) == 4, "witx calculated align");

/**
 * Open flags used by `path_open`.
 */
typedef uint16_t __wasi_oflags_t;

/**
 * Create file if it does not exist.
 */
#define __WASI_OFLAGS_CREAT (UINT16_C(1))

/**
 * Fail if not a directory.
 */
#define __WASI_OFLAGS_DIRECTORY (UINT16_C(2))

/**
 * Fail if file already exists.
 */
#define __WASI_OFLAGS_EXCL (UINT16_C(4))

/**
 * Truncate file to size 0.
 */
#define __WASI_OFLAGS_TRUNC (UINT16_C(8))

_Static_assert(sizeof(__wasi_oflags_t) == 2, "witx calculated size");
_Static_assert(_Alignof(__wasi_oflags_t) == 2, "witx calculated align");

/**
 * Number of hard links to an inode.
 */
typedef uint64_t __wasi_linkcount_t;

_Static_assert(sizeof(__wasi_linkcount_t) == 8, "witx calculated size");
_Static_assert(_Alignof(__wasi_linkcount_t) == 8, "witx calculated align");

/**
 * File attributes.
 */
typedef struct __wasi_filestat_t {
    /**
     * Device ID of device containing the file.
     */
    __wasi_device_t dev;

    /**
     * File serial number.
     */
    __wasi_inode_t ino;

    /**
     * File type.
     */
    __wasi_filetype_t filetype;

    /**
     * Number of hard links to the file.
     */
    __wasi_linkcount_t nlink;

    /**
     * For regular files, the file size in bytes. For symbolic links, the length in bytes of the pathname contained in the symbolic link.
     */
    __wasi_filesize_t size;

    /**
     * Last data access timestamp.
     */
    __wasi_timestamp_t atim;

    /**
     * Last data modification timestamp.
     */
    __wasi_timestamp_t mtim;

    /**
     * Last file status change timestamp.
     */
    __wasi_timestamp_t ctim;

} __wasi_filestat_t;

_Static_assert(sizeof(__wasi_filestat_t) == 64, "witx calculated size");
_Static_assert(_Alignof(__wasi_filestat_t) == 8, "witx calculated align");
_Static_assert(offsetof(__wasi_filestat_t, dev) == 0, "witx calculated offset");
_Static_assert(offsetof(__wasi_filestat_t, ino) == 8, "witx calculated offset");
_Static_assert(offsetof(__wasi_filestat_t, filetype) == 16, "witx calculated offset");
_Static_assert(offsetof(__wasi_filestat_t, nlink) == 24, "witx calculated offset");
_Static_assert(offsetof(__wasi_filestat_t, size) == 32, "witx calculated offset");
_Static_assert(offsetof(__wasi_filestat_t, atim) == 40, "witx calculated offset");
_Static_assert(offsetof(__wasi_filestat_t, mtim) == 48, "witx calculated offset");
_Static_assert(offsetof(__wasi_filestat_t, ctim) == 56, "witx calculated offset");

/**
 * User-provided value that may be attached to objects that is retained when
 * extracted from the implementation.
 */
typedef uint64_t __wasi_userdata_t;

_Static_assert(sizeof(__wasi_userdata_t) == 8, "witx calculated size");
_Static_assert(_Alignof(__wasi_userdata_t) == 8, "witx calculated align");

/**
 * Type of a subscription to an event or its occurrence.
 */
typedef uint8_t __wasi_eventtype_t;

/**
 * The time value of clock `subscription_clock::id` has
 * reached timestamp `subscription_clock::timeout`.
 */
#define __WASI_EVENTTYPE_CLOCK (UINT8_C(0))

/**
 * File descriptor `subscription_fd_readwrite::file_descriptor` has data
 * available for reading. This event always triggers for regular files.
 */
#define __WASI_EVENTTYPE_FD_READ (UINT8_C(1))

/**
 * File descriptor `subscription_fd_readwrite::file_descriptor` has capacity
 * available for writing. This event always triggers for regular files.
 */
#define __WASI_EVENTTYPE_FD_WRITE (UINT8_C(2))

_Static_assert(sizeof(__wasi_eventtype_t) == 1, "witx calculated size");
_Static_assert(_Alignof(__wasi_eventtype_t) == 1, "witx calculated align");

/**
 * The state of the file descriptor subscribed to with
 * `eventtype::fd_read` or `eventtype::fd_write`.
 */
typedef uint16_t __wasi_eventrwflags_t;

/**
 * The peer of this socket has closed or disconnected.
 */
#define __WASI_EVENTRWFLAGS_FD_READWRITE_HANGUP (UINT16_C(1))

_Static_assert(sizeof(__wasi_eventrwflags_t) == 2, "witx calculated size");
_Static_assert(_Alignof(__wasi_eventrwflags_t) == 2, "witx calculated align");

/**
 * The contents of an $event when type is `eventtype::fd_read` or
 * `eventtype::fd_write`.
 */
typedef struct __wasi_event_fd_readwrite_t {
    /**
     * The number of bytes available for reading or writing.
     */
    __wasi_filesize_t nbytes;

    /**
     * The state of the file descriptor.
     */
    __wasi_eventrwflags_t flags;

} __wasi_event_fd_readwrite_t;

_Static_assert(sizeof(__wasi_event_fd_readwrite_t) == 16, "witx calculated size");
_Static_assert(_Alignof(__wasi_event_fd_readwrite_t) == 8, "witx calculated align");
_Static_assert(offsetof(__wasi_event_fd_readwrite_t, nbytes) == 0, "witx calculated offset");
_Static_assert(offsetof(__wasi_event_fd_readwrite_t, flags) == 8, "witx calculated offset");

/**
 * The contents of an $event.
 */
typedef union __wasi_event_u_t {
    /**
     * When type is `eventtype::fd_read` or `eventtype::fd_write`:
     */
    __wasi_event_fd_readwrite_t fd_readwrite;

} __wasi_event_u_t;

_Static_assert(sizeof(__wasi_event_u_t) == 16, "witx calculated size");
_Static_assert(_Alignof(__wasi_event_u_t) == 8, "witx calculated align");

/**
 * An event that occurred.
 */
typedef struct __wasi_event_t {
    /**
     * User-provided value that got attached to `subscription::userdata`.
     */
    __wasi_userdata_t userdata;

    /**
     * If non-zero, an error that occurred while processing the subscription request.
     */
    __wasi_errno_t error;

    /**
     * The type of the event that occurred.
     */
    __wasi_eventtype_t type;

    /**
     * The contents of the event.
     */
    __wasi_event_u_t u;

} __wasi_event_t;

_Static_assert(sizeof(__wasi_event_t) == 32, "witx calculated size");
_Static_assert(_Alignof(__wasi_event_t) == 8, "witx calculated align");
_Static_assert(offsetof(__wasi_event_t, userdata) == 0, "witx calculated offset");
_Static_assert(offsetof(__wasi_event_t, error) == 8, "witx calculated offset");
_Static_assert(offsetof(__wasi_event_t, type) == 10, "witx calculated offset");
_Static_assert(offsetof(__wasi_event_t, u) == 16, "witx calculated offset");

/**
 * Flags determining how to interpret the timestamp provided in
 * `subscription_clock::timeout`.
 */
typedef uint16_t __wasi_subclockflags_t;

/**
 * If set, treat the timestamp provided in
 * `subscription_clock::timeout` as an absolute timestamp of clock
 * `subscription_clock::id`. If clear, treat the timestamp
 * provided in `subscription_clock::timeout` relative to the
 * current time value of clock `subscription_clock::id`.
 */
#define __WASI_SUBCLOCKFLAGS_SUBSCRIPTION_CLOCK_ABSTIME (UINT16_C(1))

_Static_assert(sizeof(__wasi_subclockflags_t) == 2, "witx calculated size");
_Static_assert(_Alignof(__wasi_subclockflags_t) == 2, "witx calculated align");

/**
 * The contents of a $subscription when type is `eventtype::clock`.
 */
typedef struct __wasi_subscription_clock_t {
    /**
     * The clock against which to compare the timestamp.
     */
    __wasi_clockid_t id;

    /**
     * The absolute or relative timestamp.
     */
    __wasi_timestamp_t timeout;

    /**
     * The amount of time that the implementation may wait additionally
     * to coalesce with other events.
     */
    __wasi_timestamp_t precision;

    /**
     * Flags specifying whether the timeout is absolute or relative
     */
    __wasi_subclockflags_t flags;

} __wasi_subscription_clock_t;

_Static_assert(sizeof(__wasi_subscription_clock_t) == 32, "witx calculated size");
_Static_assert(_Alignof(__wasi_subscription_clock_t) == 8, "witx calculated align");
_Static_assert(offsetof(__wasi_subscription_clock_t, id) == 0, "witx calculated offset");
_Static_assert(offsetof(__wasi_subscription_clock_t, timeout) == 8, "witx calculated offset");
_Static_assert(offsetof(__wasi_subscription_clock_t, precision) == 16, "witx calculated offset");
_Static_assert(offsetof(__wasi_subscription_clock_t, flags) == 24, "witx calculated offset");

/**
 * The contents of a $subscription when type is type is
 * `eventtype::fd_read` or `eventtype::fd_write`.
 */
typedef struct __wasi_subscription_fd_readwrite_t {
    /**
     * The file descriptor on which to wait for it to become ready for reading or writing.
     */
    __wasi_fd_t file_descriptor;

} __wasi_subscription_fd_readwrite_t;

_Static_assert(sizeof(__wasi_subscription_fd_readwrite_t) == 4, "witx calculated size");
_Static_assert(_Alignof(__wasi_subscription_fd_readwrite_t) == 4, "witx calculated align");
_Static_assert(offsetof(__wasi_subscription_fd_readwrite_t, file_descriptor) == 0, "witx calculated offset");

/**
 * The contents of a $subscription.
 */
typedef union __wasi_subscription_u_t {
    /**
     * When type is `eventtype::clock`:
     */
    __wasi_subscription_clock_t clock;

    /**
     * When type is `eventtype::fd_read` or `eventtype::fd_write`:
     */
    __wasi_subscription_fd_readwrite_t fd_readwrite;

} __wasi_subscription_u_t;

_Static_assert(sizeof(__wasi_subscription_u_t) == 32, "witx calculated size");
_Static_assert(_Alignof(__wasi_subscription_u_t) == 8, "witx calculated align");

/**
 * Subscription to an event.
 */
typedef struct __wasi_subscription_t {
    /**
     * User-provided value that is attached to the subscription in the
     * implementation and returned through `event::userdata`.
     */
    __wasi_userdata_t userdata;

    /**
     * The type of the event to which to subscribe.
     */
    __wasi_eventtype_t type;

    /**
     * The contents of the subscription.
     */
    __wasi_subscription_u_t u;

} __wasi_subscription_t;

_Static_assert(sizeof(__wasi_subscription_t) == 48, "witx calculated size");
_Static_assert(_Alignof(__wasi_subscription_t) == 8, "witx calculated align");
_Static_assert(offsetof(__wasi_subscription_t, userdata) == 0, "witx calculated offset");
_Static_assert(offsetof(__wasi_subscription_t, type) == 8, "witx calculated offset");
_Static_assert(offsetof(__wasi_subscription_t, u) == 16, "witx calculated offset");

/**
 * Exit code generated by a process when exiting.
 */
typedef uint32_t __wasi_exitcode_t;

_Static_assert(sizeof(__wasi_exitcode_t) == 4, "witx calculated size");
_Static_assert(_Alignof(__wasi_exitcode_t) == 4, "witx calculated align");

/**
 * Signal condition.
 */
typedef uint8_t __wasi_signal_t;

/**
 * No signal. Note that POSIX has special semantics for `kill(pid, 0)`,
 * so this value is reserved.
 */
#define __WASI_SIGNAL_NONE (UINT8_C(0))

/**
 * Hangup.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_HUP (UINT8_C(1))

/**
 * Terminate interrupt signal.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_INT (UINT8_C(2))

/**
 * Terminal quit signal.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_QUIT (UINT8_C(3))

/**
 * Illegal instruction.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_ILL (UINT8_C(4))

/**
 * Trace/breakpoint trap.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_TRAP (UINT8_C(5))

/**
 * Process abort signal.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_ABRT (UINT8_C(6))

/**
 * Access to an undefined portion of a memory object.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_BUS (UINT8_C(7))

/**
 * Erroneous arithmetic operation.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_FPE (UINT8_C(8))

/**
 * Kill.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_KILL (UINT8_C(9))

/**
 * User-defined signal 1.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_USR1 (UINT8_C(10))

/**
 * Invalid memory reference.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_SEGV (UINT8_C(11))

/**
 * User-defined signal 2.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_USR2 (UINT8_C(12))

/**
 * Write on a pipe with no one to read it.
 * Action: Ignored.
 */
#define __WASI_SIGNAL_PIPE (UINT8_C(13))

/**
 * Alarm clock.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_ALRM (UINT8_C(14))

/**
 * Termination signal.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_TERM (UINT8_C(15))

/**
 * Child process terminated, stopped, or continued.
 * Action: Ignored.
 */
#define __WASI_SIGNAL_CHLD (UINT8_C(16))

/**
 * Continue executing, if stopped.
 * Action: Continues executing, if stopped.
 */
#define __WASI_SIGNAL_CONT (UINT8_C(17))

/**
 * Stop executing.
 * Action: Stops executing.
 */
#define __WASI_SIGNAL_STOP (UINT8_C(18))

/**
 * Terminal stop signal.
 * Action: Stops executing.
 */
#define __WASI_SIGNAL_TSTP (UINT8_C(19))

/**
 * Background process attempting read.
 * Action: Stops executing.
 */
#define __WASI_SIGNAL_TTIN (UINT8_C(20))

/**
 * Background process attempting write.
 * Action: Stops executing.
 */
#define __WASI_SIGNAL_TTOU (UINT8_C(21))

/**
 * High bandwidth data is available at a socket.
 * Action: Ignored.
 */
#define __WASI_SIGNAL_URG (UINT8_C(22))

/**
 * CPU time limit exceeded.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_XCPU (UINT8_C(23))

/**
 * File size limit exceeded.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_XFSZ (UINT8_C(24))

/**
 * Virtual timer expired.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_VTALRM (UINT8_C(25))

/**
 * Profiling timer expired.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_PROF (UINT8_C(26))

/**
 * Window changed.
 * Action: Ignored.
 */
#define __WASI_SIGNAL_WINCH (UINT8_C(27))

/**
 * I/O possible.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_POLL (UINT8_C(28))

/**
 * Power failure.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_PWR (UINT8_C(29))

/**
 * Bad system call.
 * Action: Terminates the process.
 */
#define __WASI_SIGNAL_SYS (UINT8_C(30))

_Static_assert(sizeof(__wasi_signal_t) == 1, "witx calculated size");
_Static_assert(_Alignof(__wasi_signal_t) == 1, "witx calculated align");

/**
 * Flags provided to `sock_recv`.
 */
typedef uint16_t __wasi_riflags_t;

/**
 * Returns the message without removing it from the socket's receive queue.
 */
#define __WASI_RIFLAGS_RECV_PEEK (UINT16_C(1))

/**
 * On byte-stream sockets, block until the full amount of data can be returned.
 */
#define __WASI_RIFLAGS_RECV_WAITALL (UINT16_C(2))

_Static_assert(sizeof(__wasi_riflags_t) == 2, "witx calculated size");
_Static_assert(_Alignof(__wasi_riflags_t) == 2, "witx calculated align");

/**
 * Flags returned by `sock_recv`.
 */
typedef uint16_t __wasi_roflags_t;

/**
 * Returned by `sock_recv`: Message data has been truncated.
 */
#define __WASI_ROFLAGS_RECV_DATA_TRUNCATED (UINT16_C(1))

_Static_assert(sizeof(__wasi_roflags_t) == 2, "witx calculated size");
_Static_assert(_Alignof(__wasi_roflags_t) == 2, "witx calculated align");

/**
 * Flags provided to `sock_send`. As there are currently no flags
 * defined, it must be set to zero.
 */
typedef uint16_t __wasi_siflags_t;

_Static_assert(sizeof(__wasi_siflags_t) == 2, "witx calculated size");
_Static_assert(_Alignof(__wasi_siflags_t) == 2, "witx calculated align");

/**
 * Which channels on a socket to shut down.
 */
typedef uint8_t __wasi_sdflags_t;

/**
 * Disables further receive operations.
 */
#define __WASI_SDFLAGS_RD (UINT8_C(1))

/**
 * Disables further send operations.
 */
#define __WASI_SDFLAGS_WR (UINT8_C(2))

_Static_assert(sizeof(__wasi_sdflags_t) == 1, "witx calculated size");
_Static_assert(_Alignof(__wasi_sdflags_t) == 1, "witx calculated align");

/**
 * Identifiers for preopened capabilities.
 */
typedef uint8_t __wasi_preopentype_t;

/**
 * A pre-opened directory.
 */
#define __WASI_PREOPENTYPE_DIR (UINT8_C(0))

_Static_assert(sizeof(__wasi_preopentype_t) == 1, "witx calculated size");
_Static_assert(_Alignof(__wasi_preopentype_t) == 1, "witx calculated align");

/**
 * The contents of a $prestat when type is `preopentype::dir`.
 */
typedef struct __wasi_prestat_dir_t {
    /**
     * The length of the directory name for use with `fd_prestat_dir_name`.
     */
    __wasi_size_t pr_name_len;

} __wasi_prestat_dir_t;

_Static_assert(sizeof(__wasi_prestat_dir_t) == 4, "witx calculated size");
_Static_assert(_Alignof(__wasi_prestat_dir_t) == 4, "witx calculated align");
_Static_assert(offsetof(__wasi_prestat_dir_t, pr_name_len) == 0, "witx calculated offset");

/**
 * The contents of an $prestat.
 */
typedef union __wasi_prestat_u_t {
    /**
     * When type is `preopentype::dir`:
     */
    __wasi_prestat_dir_t dir;

} __wasi_prestat_u_t;

_Static_assert(sizeof(__wasi_prestat_u_t) == 4, "witx calculated size");
_Static_assert(_Alignof(__wasi_prestat_u_t) == 4, "witx calculated align");

/**
 * Information about a pre-opened capability.
 */
typedef struct __wasi_prestat_t {
    /**
     * The type of the pre-opened capability.
     */
    __wasi_preopentype_t pr_type;

    /**
     * The contents of the information.
     */
    __wasi_prestat_u_t u;

} __wasi_prestat_t;

_Static_assert(sizeof(__wasi_prestat_t) == 8, "witx calculated size");
_Static_assert(_Alignof(__wasi_prestat_t) == 4, "witx calculated align");
_Static_assert(offsetof(__wasi_prestat_t, pr_type) == 0, "witx calculated offset");
_Static_assert(offsetof(__wasi_prestat_t, u) == 4, "witx calculated offset");

/**
 * @defgroup wasi_snapshot_preview1
 * @{
 */

/**
 * Read command-line argument data.
 * The size of the array should match that returned by `args_sizes_get`
 */
__wasi_errno_t __wasi_args_get(
    uint8_t * * argv,

    uint8_t * argv_buf
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("args_get"),
    __warn_unused_result__
));

/**
 * Return command-line argument data sizes.
 */
__wasi_errno_t __wasi_args_sizes_get(
    /**
     * The number of arguments.
     */
    __wasi_size_t *argc,
    /**
     * The size of the argument string data.
     */
    __wasi_size_t *argv_buf_size
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("args_sizes_get"),
    __warn_unused_result__
));

/**
 * Read environment variable data.
 * The sizes of the buffers should match that returned by `environ_sizes_get`.
 */
__wasi_errno_t __wasi_environ_get(
    uint8_t * * environ,

    uint8_t * environ_buf
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("environ_get"),
    __warn_unused_result__
));

/**
 * Return command-line argument data sizes.
 */
__wasi_errno_t __wasi_environ_sizes_get(
    /**
     * The number of arguments.
     */
    __wasi_size_t *argc,
    /**
     * The size of the argument string data.
     */
    __wasi_size_t *argv_buf_size
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("environ_sizes_get"),
    __warn_unused_result__
));

/**
 * Return the resolution of a clock.
 * Implementations are required to provide a non-zero value for supported clocks. For unsupported clocks,
 * return `errno::inval`.
 * Note: This is similar to `clock_getres` in POSIX.
 */
__wasi_errno_t __wasi_clock_res_get(
    /**
     * The clock for which to return the resolution.
     */
    __wasi_clockid_t id,

    /**
     * The resolution of the clock.
     */
    __wasi_timestamp_t *resolution
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("clock_res_get"),
    __warn_unused_result__
));

/**
 * Return the time value of a clock.
 * Note: This is similar to `clock_gettime` in POSIX.
 */
__wasi_errno_t __wasi_clock_time_get(
    /**
     * The clock for which to return the time.
     */
    __wasi_clockid_t id,

    /**
     * The maximum lag (exclusive) that the returned time value may have, compared to its actual value.
     */
    __wasi_timestamp_t precision,

    /**
     * The time value of the clock.
     */
    __wasi_timestamp_t *time
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("clock_time_get"),
    __warn_unused_result__
));

/**
 * Provide file advisory information on a file descriptor.
 * Note: This is similar to `posix_fadvise` in POSIX.
 */
__wasi_errno_t __wasi_fd_advise(
    __wasi_fd_t fd,

    /**
     * The offset within the file to which the advisory applies.
     */
    __wasi_filesize_t offset,

    /**
     * The length of the region to which the advisory applies.
     */
    __wasi_filesize_t len,

    /**
     * The advice.
     */
    __wasi_advice_t advice
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_advise"),
    __warn_unused_result__
));

/**
 * Force the allocation of space in a file.
 * Note: This is similar to `posix_fallocate` in POSIX.
 */
__wasi_errno_t __wasi_fd_allocate(
    __wasi_fd_t fd,

    /**
     * The offset at which to start the allocation.
     */
    __wasi_filesize_t offset,

    /**
     * The length of the area that is allocated.
     */
    __wasi_filesize_t len
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_allocate"),
    __warn_unused_result__
));

/**
 * Close a file descriptor.
 * Note: This is similar to `close` in POSIX.
 */
__wasi_errno_t __wasi_fd_close(
    __wasi_fd_t fd
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_close"),
    __warn_unused_result__
));

/**
 * Synchronize the data of a file to disk.
 * Note: This is similar to `fdatasync` in POSIX.
 */
__wasi_errno_t __wasi_fd_datasync(
    __wasi_fd_t fd
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_datasync"),
    __warn_unused_result__
));

/**
 * Get the attributes of a file descriptor.
 * Note: This returns similar flags to `fsync(fd, F_GETFL)` in POSIX, as well as additional fields.
 */
__wasi_errno_t __wasi_fd_fdstat_get(
    __wasi_fd_t fd,

    /**
     * The buffer where the file descriptor's attributes are stored.
     */
    __wasi_fdstat_t *stat
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_fdstat_get"),
    __warn_unused_result__
));

/**
 * Adjust the flags associated with a file descriptor.
 * Note: This is similar to `fcntl(fd, F_SETFL, flags)` in POSIX.
 */
__wasi_errno_t __wasi_fd_fdstat_set_flags(
    __wasi_fd_t fd,

    /**
     * The desired values of the file descriptor flags.
     */
    __wasi_fdflags_t flags
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_fdstat_set_flags"),
    __warn_unused_result__
));

/**
 * Adjust the rights associated with a file descriptor.
 * This can only be used to remove rights, and returns `errno::notcapable` if called in a way that would attempt to add rights
 */
__wasi_errno_t __wasi_fd_fdstat_set_rights(
    __wasi_fd_t fd,

    /**
     * The desired rights of the file descriptor.
     */
    __wasi_rights_t fs_rights_base,

    __wasi_rights_t fs_rights_inheriting
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_fdstat_set_rights"),
    __warn_unused_result__
));

/**
 * Return the attributes of an open file.
 */
__wasi_errno_t __wasi_fd_filestat_get(
    __wasi_fd_t fd,

    /**
     * The buffer where the file's attributes are stored.
     */
    __wasi_filestat_t *buf
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_filestat_get"),
    __warn_unused_result__
));

/**
 * Adjust the size of an open file. If this increases the file's size, the extra bytes are filled with zeros.
 * Note: This is similar to `ftruncate` in POSIX.
 */
__wasi_errno_t __wasi_fd_filestat_set_size(
    __wasi_fd_t fd,

    /**
     * The desired file size.
     */
    __wasi_filesize_t size
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_filestat_set_size"),
    __warn_unused_result__
));

/**
 * Adjust the timestamps of an open file or directory.
 * Note: This is similar to `futimens` in POSIX.
 */
__wasi_errno_t __wasi_fd_filestat_set_times(
    __wasi_fd_t fd,

    /**
     * The desired values of the data access timestamp.
     */
    __wasi_timestamp_t atim,

    /**
     * The desired values of the data modification timestamp.
     */
    __wasi_timestamp_t mtim,

    /**
     * A bitmask indicating which timestamps to adjust.
     */
    __wasi_fstflags_t fst_flags
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_filestat_set_times"),
    __warn_unused_result__
));

/**
 * Read from a file descriptor, without using and updating the file descriptor's offset.
 * Note: This is similar to `preadv` in POSIX.
 */
__wasi_errno_t __wasi_fd_pread(
    __wasi_fd_t fd,

    /**
     * List of scatter/gather vectors in which to store data.
     */
    const __wasi_iovec_t *iovs,

    /**
     * The length of the array pointed to by `iovs`.
     */
    size_t iovs_len,

    /**
     * The offset within the file at which to read.
     */
    __wasi_filesize_t offset,

    /**
     * The number of bytes read.
     */
    __wasi_size_t *nread
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_pread"),
    __warn_unused_result__
));

/**
 * Return a description of the given preopened file descriptor.
 */
__wasi_errno_t __wasi_fd_prestat_get(
    __wasi_fd_t fd,

    /**
     * The buffer where the description is stored.
     */
    __wasi_prestat_t *buf
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_prestat_get"),
    __warn_unused_result__
));

/**
 * Return a description of the given preopened file descriptor.
 */
__wasi_errno_t __wasi_fd_prestat_dir_name(
    __wasi_fd_t fd,

    /**
     * A buffer into which to write the preopened directory name.
     */
    uint8_t * path,

    __wasi_size_t path_len
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_prestat_dir_name"),
    __warn_unused_result__
));

/**
 * Write to a file descriptor, without using and updating the file descriptor's offset.
 * Note: This is similar to `pwritev` in POSIX.
 */
__wasi_errno_t __wasi_fd_pwrite(
    __wasi_fd_t fd,

    /**
     * List of scatter/gather vectors from which to retrieve data.
     */
    const __wasi_ciovec_t *iovs,

    /**
     * The length of the array pointed to by `iovs`.
     */
    size_t iovs_len,

    /**
     * The offset within the file at which to write.
     */
    __wasi_filesize_t offset,

    /**
     * The number of bytes written.
     */
    __wasi_size_t *nwritten
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_pwrite"),
    __warn_unused_result__
));

/**
 * Read from a file descriptor.
 * Note: This is similar to `readv` in POSIX.
 */
__wasi_errno_t __wasi_fd_read(
    __wasi_fd_t fd,

    /**
     * List of scatter/gather vectors to which to store data.
     */
    const __wasi_iovec_t *iovs,

    /**
     * The length of the array pointed to by `iovs`.
     */
    size_t iovs_len,

    /**
     * The number of bytes read.
     */
    __wasi_size_t *nread
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_read"),
    __warn_unused_result__
));

/**
 * Read directory entries from a directory.
 * When successful, the contents of the output buffer consist of a sequence of
 * directory entries. Each directory entry consists of a dirent_t object,
 * followed by dirent_t::d_namlen bytes holding the name of the directory
 * entry.
 * This function fills the output buffer as much as possible, potentially
 * truncating the last directory entry. This allows the caller to grow its
 * read buffer size in case it's too small to fit a single large directory
 * entry, or skip the oversized directory entry.
 */
__wasi_errno_t __wasi_fd_readdir(
    __wasi_fd_t fd,

    /**
     * The buffer where directory entries are stored
     */
    uint8_t * buf,

    __wasi_size_t buf_len,

    /**
     * The location within the directory to start reading
     */
    __wasi_dircookie_t cookie,

    /**
     * The number of bytes stored in the read buffer. If less than the size of the read buffer, the end of the directory has been reached.
     */
    __wasi_size_t *bufused
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_readdir"),
    __warn_unused_result__
));

/**
 * Atomically replace a file descriptor by renumbering another file descriptor.
 * Due to the strong focus on thread safety, this environment does not provide
 * a mechanism to duplicate or renumber a file descriptor to an arbitrary
 * number, like `dup2()`. This would be prone to race conditions, as an actual
 * file descriptor with the same number could be allocated by a different
 * thread at the same time.
 * This function provides a way to atomically renumber file descriptors, which
 * would disappear if `dup2()` were to be removed entirely.
 */
__wasi_errno_t __wasi_fd_renumber(
    __wasi_fd_t fd,

    /**
     * The file descriptor to overwrite.
     */
    __wasi_fd_t to
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_renumber"),
    __warn_unused_result__
));

/**
 * Move the offset of a file descriptor.
 * Note: This is similar to `lseek` in POSIX.
 */
__wasi_errno_t __wasi_fd_seek(
    __wasi_fd_t fd,

    /**
     * The number of bytes to move.
     */
    __wasi_filedelta_t offset,

    /**
     * The base from which the offset is relative.
     */
    __wasi_whence_t whence,

    /**
     * The new offset of the file descriptor, relative to the start of the file.
     */
    __wasi_filesize_t *newoffset
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_seek"),
    __warn_unused_result__
));

/**
 * Synchronize the data and metadata of a file to disk.
 * Note: This is similar to `fsync` in POSIX.
 */
__wasi_errno_t __wasi_fd_sync(
    __wasi_fd_t fd
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_sync"),
    __warn_unused_result__
));

/**
 * Return the current offset of a file descriptor.
 * Note: This is similar to `lseek(fd, 0, SEEK_CUR)` in POSIX.
 */
__wasi_errno_t __wasi_fd_tell(
    __wasi_fd_t fd,

    /**
     * The current offset of the file descriptor, relative to the start of the file.
     */
    __wasi_filesize_t *offset
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_tell"),
    __warn_unused_result__
));

/**
 * Write to a file descriptor.
 * Note: This is similar to `writev` in POSIX.
 */
__wasi_errno_t __wasi_fd_write(
    __wasi_fd_t fd,

    /**
     * List of scatter/gather vectors from which to retrieve data.
     */
    const __wasi_ciovec_t *iovs,

    /**
     * The length of the array pointed to by `iovs`.
     */
    size_t iovs_len,

    /**
     * The number of bytes written.
     */
    __wasi_size_t *nwritten
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("fd_write"),
    __warn_unused_result__
));

/**
 * Create a directory.
 * Note: This is similar to `mkdirat` in POSIX.
 */
__wasi_errno_t __wasi_path_create_directory(
    __wasi_fd_t fd,

    /**
     * The path at which to create the directory.
     */
    const char *path,

    /**
     * The length of the buffer pointed to by `path`.
     */
    size_t path_len
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("path_create_directory"),
    __warn_unused_result__
));

/**
 * Return the attributes of a file or directory.
 * Note: This is similar to `stat` in POSIX.
 */
__wasi_errno_t __wasi_path_filestat_get(
    __wasi_fd_t fd,

    /**
     * Flags determining the method of how the path is resolved.
     */
    __wasi_lookupflags_t flags,

    /**
     * The path of the file or directory to inspect.
     */
    const char *path,

    /**
     * The length of the buffer pointed to by `path`.
     */
    size_t path_len,

    /**
     * The buffer where the file's attributes are stored.
     */
    __wasi_filestat_t *buf
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("path_filestat_get"),
    __warn_unused_result__
));

/**
 * Adjust the timestamps of a file or directory.
 * Note: This is similar to `utimensat` in POSIX.
 */
__wasi_errno_t __wasi_path_filestat_set_times(
    __wasi_fd_t fd,

    /**
     * Flags determining the method of how the path is resolved.
     */
    __wasi_lookupflags_t flags,

    /**
     * The path of the file or directory to operate on.
     */
    const char *path,

    /**
     * The length of the buffer pointed to by `path`.
     */
    size_t path_len,

    /**
     * The desired values of the data access timestamp.
     */
    __wasi_timestamp_t atim,

    /**
     * The desired values of the data modification timestamp.
     */
    __wasi_timestamp_t mtim,

    /**
     * A bitmask indicating which timestamps to adjust.
     */
    __wasi_fstflags_t fst_flags
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("path_filestat_set_times"),
    __warn_unused_result__
));

/**
 * Create a hard link.
 * Note: This is similar to `linkat` in POSIX.
 */
__wasi_errno_t __wasi_path_link(
    __wasi_fd_t old_fd,

    /**
     * Flags determining the method of how the path is resolved.
     */
    __wasi_lookupflags_t old_flags,

    /**
     * The source path from which to link.
     */
    const char *old_path,

    /**
     * The length of the buffer pointed to by `old_path`.
     */
    size_t old_path_len,

    /**
     * The working directory at which the resolution of the new path starts.
     */
    __wasi_fd_t new_fd,

    /**
     * The destination path at which to create the hard link.
     */
    const char *new_path,

    /**
     * The length of the buffer pointed to by `new_path`.
     */
    size_t new_path_len
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("path_link"),
    __warn_unused_result__
));

/**
 * Open a file or directory.
 * The returned file descriptor is not guaranteed to be the lowest-numbered
 * file descriptor not currently open; it is randomized to prevent
 * applications from depending on making assumptions about indexes, since this
 * is error-prone in multi-threaded contexts. The returned file descriptor is
 * guaranteed to be less than 2**31.
 * Note: This is similar to `openat` in POSIX.
 */
__wasi_errno_t __wasi_path_open(
    __wasi_fd_t fd,

    /**
     * Flags determining the method of how the path is resolved.
     */
    __wasi_lookupflags_t dirflags,

    /**
     * The relative path of the file or directory to open, relative to the
     * `path_open::fd` directory.
     */
    const char *path,

    /**
     * The length of the buffer pointed to by `path`.
     */
    size_t path_len,

    /**
     * The method by which to open the file.
     */
    __wasi_oflags_t oflags,

    /**
     * The initial rights of the newly created file descriptor. The
     * implementation is allowed to return a file descriptor with fewer rights
     * than specified, if and only if those rights do not apply to the type of
     * file being opened.
     * The *base* rights are rights that will apply to operations using the file
     * descriptor itself, while the *inheriting* rights are rights that apply to
     * file descriptors derived from it.
     */
    __wasi_rights_t fs_rights_base,

    __wasi_rights_t fs_rights_inherting,

    __wasi_fdflags_t fdflags,

    /**
     * The file descriptor of the file that has been opened.
     */
    __wasi_fd_t *opened_fd
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("path_open"),
    __warn_unused_result__
));

/**
 * Read the contents of a symbolic link.
 * Note: This is similar to `readlinkat` in POSIX.
 */
__wasi_errno_t __wasi_path_readlink(
    __wasi_fd_t fd,

    /**
     * The path of the symbolic link from which to read.
     */
    const char *path,

    /**
     * The length of the buffer pointed to by `path`.
     */
    size_t path_len,

    /**
     * The buffer to which to write the contents of the symbolic link.
     */
    uint8_t * buf,

    __wasi_size_t buf_len,

    /**
     * The number of bytes placed in the buffer.
     */
    __wasi_size_t *bufused
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("path_readlink"),
    __warn_unused_result__
));

/**
 * Remove a directory.
 * Return `errno::notempty` if the directory is not empty.
 * Note: This is similar to `unlinkat(fd, path, AT_REMOVEDIR)` in POSIX.
 */
__wasi_errno_t __wasi_path_remove_directory(
    __wasi_fd_t fd,

    /**
     * The path to a directory to remove.
     */
    const char *path,

    /**
     * The length of the buffer pointed to by `path`.
     */
    size_t path_len
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("path_remove_directory"),
    __warn_unused_result__
));

/**
 * Rename a file or directory.
 * Note: This is similar to `renameat` in POSIX.
 */
__wasi_errno_t __wasi_path_rename(
    __wasi_fd_t fd,

    /**
     * The source path of the file or directory to rename.
     */
    const char *old_path,

    /**
     * The length of the buffer pointed to by `old_path`.
     */
    size_t old_path_len,

    /**
     * The working directory at which the resolution of the new path starts.
     */
    __wasi_fd_t new_fd,

    /**
     * The destination path to which to rename the file or directory.
     */
    const char *new_path,

    /**
     * The length of the buffer pointed to by `new_path`.
     */
    size_t new_path_len
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("path_rename"),
    __warn_unused_result__
));

/**
 * Create a symbolic link.
 * Note: This is similar to `symlinkat` in POSIX.
 */
__wasi_errno_t __wasi_path_symlink(
    /**
     * The contents of the symbolic link.
     */
    const char *old_path,

    /**
     * The length of the buffer pointed to by `old_path`.
     */
    size_t old_path_len,

    __wasi_fd_t fd,

    /**
     * The destination path at which to create the symbolic link.
     */
    const char *new_path,

    /**
     * The length of the buffer pointed to by `new_path`.
     */
    size_t new_path_len
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("path_symlink"),
    __warn_unused_result__
));

/**
 * Unlink a file.
 * Return `errno::isdir` if the path refers to a directory.
 * Note: This is similar to `unlinkat(fd, path, 0)` in POSIX.
 */
__wasi_errno_t __wasi_path_unlink_file(
    __wasi_fd_t fd,

    /**
     * The path to a file to unlink.
     */
    const char *path,

    /**
     * The length of the buffer pointed to by `path`.
     */
    size_t path_len
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("path_unlink_file"),
    __warn_unused_result__
));

/**
 * Concurrently poll for the occurrence of a set of events.
 */
__wasi_errno_t __wasi_poll_oneoff(
    /**
     * The events to which to subscribe.
     */
    const __wasi_subscription_t * in,

    /**
     * The events that have occurred.
     */
    __wasi_event_t * out,

    /**
     * Both the number of subscriptions and events.
     */
    __wasi_size_t nsubscriptions,

    /**
     * The number of events stored.
     */
    __wasi_size_t *nevents
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("poll_oneoff"),
    __warn_unused_result__
));

/**
 * Terminate the process normally. An exit code of 0 indicates successful
 * termination of the program. The meanings of other values is dependent on
 * the environment.
 */
_Noreturn void __wasi_proc_exit(
    /**
     * The exit code returned by the process.
     */
    __wasi_exitcode_t rval
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("proc_exit"))
));

/**
 * Send a signal to the process of the calling thread.
 * Note: This is similar to `raise` in POSIX.
 */
__wasi_errno_t __wasi_proc_raise(
    /**
     * The signal condition to trigger.
     */
    __wasi_signal_t sig
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("proc_raise"),
    __warn_unused_result__
));

/**
 * Temporarily yield execution of the calling thread.
 * Note: This is similar to `sched_yield` in POSIX.
 */
__wasi_errno_t __wasi_sched_yield(
    void
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("sched_yield"),
    __warn_unused_result__
));

/**
 * Write high-quality random data into a buffer.
 * This function blocks when the implementation is unable to immediately
 * provide sufficient high-quality random data.
 * This function may execute slowly, so when large mounts of random data are
 * required, it's advisable to use this function to seed a pseudo-random
 * number generator, rather than to provide the random data directly.
 */
__wasi_errno_t __wasi_random_get(
    /**
     * The buffer to fill with random data.
     */
    uint8_t * buf,

    __wasi_size_t buf_len
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("random_get"),
    __warn_unused_result__
));

/**
 * Receive a message from a socket.
 * Note: This is similar to `recv` in POSIX, though it also supports reading
 * the data into multiple buffers in the manner of `readv`.
 */
__wasi_errno_t __wasi_sock_recv(
    __wasi_fd_t fd,

    /**
     * List of scatter/gather vectors to which to store data.
     */
    const __wasi_iovec_t *ri_data,

    /**
     * The length of the array pointed to by `ri_data`.
     */
    size_t ri_data_len,

    /**
     * Message flags.
     */
    __wasi_riflags_t ri_flags,

    /**
     * Number of bytes stored in ri_data.
     */
    __wasi_size_t *ro_datalen,
    /**
     * Message flags.
     */
    __wasi_roflags_t *ro_flags
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("sock_recv"),
    __warn_unused_result__
));

/**
 * Send a message on a socket.
 * Note: This is similar to `send` in POSIX, though it also supports writing
 * the data from multiple buffers in the manner of `writev`.
 */
__wasi_errno_t __wasi_sock_send(
    __wasi_fd_t fd,

    /**
     * List of scatter/gather vectors to which to retrieve data
     */
    const __wasi_ciovec_t *si_data,

    /**
     * The length of the array pointed to by `si_data`.
     */
    size_t si_data_len,

    /**
     * Message flags.
     */
    __wasi_siflags_t si_flags,

    /**
     * Number of bytes transmitted.
     */
    __wasi_size_t *so_datalen
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // __import_name__("sock_send"),
    __warn_unused_result__
));

/**
 * Shut down socket send and receive channels.
 * Note: This is similar to `shutdown` in POSIX.
 */
__wasi_errno_t __wasi_sock_shutdown(
    __wasi_fd_t fd,

    /**
     * Which channels on the socket to shut down.
     */
    __wasi_sdflags_t how
) __attribute__((
    // __import_module__("wasi_snapshot_preview1"),
    // // __import_name__("sock_shutdown"),
    __warn_unused_result__
));

/** @} */

#ifdef __cplusplus
}
#endif

#endif