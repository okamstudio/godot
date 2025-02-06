//
//  m3_api_meta_wasi.c
//
//  Created by Volodymyr Shymanskyy on 01/08/20.
//  Copyright © 2020 Volodymyr Shymanskyy. All rights reserved.
//

#include "m3_api_wasi.h"

#include "m3_env.h"
#include "m3_exception.h"

#if defined(d_m3HasMetaWASI)

// NOTE: MetaWASI mostly redirects WASI calls to the host WASI environment

#if !defined(__wasi__)
# error "MetaWASI is only supported on WASI target"
#endif

#if __has_include("wasi/api.h")
# include <wasi/api.h>
# define USE_NEW_WASI
# define WASI_STAT_FIELD(f) f

#elif __has_include("wasi/core.h")
# warning "Using legacy WASI headers"
# include <wasi/core.h>
# define __WASI_ERRNO_SUCCESS   __WASI_ESUCCESS
# define __WASI_ERRNO_INVAL     __WASI_EINVAL
# define WASI_STAT_FIELD(f) st_##f

#else
# error "Missing WASI headers"
#endif

static m3_wasi_context_t* wasi_context;

typedef size_t __wasi_size_t;

static inline
const void* copy_iov_to_host(IM3Runtime runtime, void* _mem, __wasi_iovec_t* host_iov, __wasi_iovec_t* wasi_iov, int32_t iovs_len)
{
    // Convert wasi memory offsets to host addresses
    for (int i = 0; i < iovs_len; i++) {
        host_iov[i].buf = m3ApiOffsetToPtr(wasi_iov[i].buf);
        host_iov[i].buf_len  = wasi_iov[i].buf_len;
        m3ApiCheckMem(host_iov[i].buf,     host_iov[i].buf_len);
    }
    m3ApiSuccess();
}

#if d_m3EnableWasiTracing

const char* wasi_errno2str(__wasi_errno_t err)
{
    switch (err) {
    case  0: return "ESUCCESS";
    case  1: return "E2BIG";
    case  2: return "EACCES";
    case  3: return "EADDRINUSE";
    case  4: return "EADDRNOTAVAIL";
    case  5: return "EAFNOSUPPORT";
    case  6: return "EAGAIN";
    case  7: return "EALREADY";
    case  8: return "EBADF";
    case  9: return "EBADMSG";
    case 10: return "EBUSY";
    case 11: return "ECANCELED";
    case 12: return "ECHILD";
    case 13: return "ECONNABORTED";
    case 14: return "ECONNREFUSED";
    case 15: return "ECONNRESET";
    case 16: return "EDEADLK";
    case 17: return "EDESTADDRREQ";
    case 18: return "EDOM";
    case 19: return "EDQUOT";
    case 20: return "EEXIST";
    case 21: return "EFAULT";
    case 22: return "EFBIG";
    case 23: return "EHOSTUNREACH";
    case 24: return "EIDRM";
    case 25: return "EILSEQ";
    case 26: return "EINPROGRESS";
    case 27: return "EINTR";
    case 28: return "EINVAL";
    case 29: return "EIO";
    case 30: return "EISCONN";
    case 31: return "EISDIR";
    case 32: return "ELOOP";
    case 33: return "EMFILE";
    case 34: return "EMLINK";
    case 35: return "EMSGSIZE";
    case 36: return "EMULTIHOP";
    case 37: return "ENAMETOOLONG";
    case 38: return "ENETDOWN";
    case 39: return "ENETRESET";
    case 40: return "ENETUNREACH";
    case 41: return "ENFILE";
    case 42: return "ENOBUFS";
    case 43: return "ENODEV";
    case 44: return "ENOENT";
    case 45: return "ENOEXEC";
    case 46: return "ENOLCK";
    case 47: return "ENOLINK";
    case 48: return "ENOMEM";
    case 49: return "ENOMSG";
    case 50: return "ENOPROTOOPT";
    case 51: return "ENOSPC";
    case 52: return "ENOSYS";
    case 53: return "ENOTCONN";
    case 54: return "ENOTDIR";
    case 55: return "ENOTEMPTY";
    case 56: return "ENOTRECOVERABLE";
    case 57: return "ENOTSOCK";
    case 58: return "ENOTSUP";
    case 59: return "ENOTTY";
    case 60: return "ENXIO";
    case 61: return "EOVERFLOW";
    case 62: return "EOWNERDEAD";
    case 63: return "EPERM";
    case 64: return "EPIPE";
    case 65: return "EPROTO";
    case 66: return "EPROTONOSUPPORT";
    case 67: return "EPROTOTYPE";
    case 68: return "ERANGE";
    case 69: return "EROFS";
    case 70: return "ESPIPE";
    case 71: return "ESRCH";
    case 72: return "ESTALE";
    case 73: return "ETIMEDOUT";
    case 74: return "ETXTBSY";
    case 75: return "EXDEV";
    case 76: return "ENOTCAPABLE";
    default: return "<unknown>";
    }
}

const char* wasi_whence2str(__wasi_whence_t whence)
{
    switch (whence) {
    case __WASI_WHENCE_SET: return "SET";
    case __WASI_WHENCE_CUR: return "CUR";
    case __WASI_WHENCE_END: return "END";
    default:                return "<unknown>";
    }
}

#  define WASI_TRACE(fmt, ...)    { fprintf(stderr, "%s " fmt, __FUNCTION__+16, ##__VA_ARGS__); fprintf(stderr, " => %s\n", wasi_errno2str(ret)); }
#else
#  define WASI_TRACE(fmt, ...)
#endif

/*
 * WASI API implementation
 */

m3ApiRawFunction(m3_wasi_generic_args_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArgMem   (uint32_t *           , argv)
    m3ApiGetArgMem   (char *               , argv_buf)

    m3_wasi_context_t* context = (m3_wasi_context_t*)(_ctx->userdata);

    if (context == NULL) { m3ApiReturn(__WASI_ERRNO_INVAL); }

    m3ApiCheckMem(argv, context->argc * sizeof(uint32_t));

    for (u32 i = 0; i < context->argc; ++i)
    {
        m3ApiWriteMem32(&argv[i], m3ApiPtrToOffset(argv_buf));

        size_t len = strlen (context->argv[i]);

        m3ApiCheckMem(argv_buf, len);
        memcpy (argv_buf, context->argv[i], len);
        argv_buf += len;
        * argv_buf++ = 0;
    }

    m3ApiReturn(__WASI_ERRNO_SUCCESS);
}

m3ApiRawFunction(m3_wasi_generic_args_sizes_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArgMem   (__wasi_size_t *      , argc)
    m3ApiGetArgMem   (__wasi_size_t *      , argv_buf_size)

    m3ApiCheckMem(argc,             sizeof(__wasi_size_t));
    m3ApiCheckMem(argv_buf_size,    sizeof(__wasi_size_t));

    m3_wasi_context_t* context = (m3_wasi_context_t*)(_ctx->userdata);

    if (context == NULL) { m3ApiReturn(__WASI_ERRNO_INVAL); }

    __wasi_size_t buf_len = 0;
    for (u32 i = 0; i < context->argc; ++i)
    {
        buf_len += strlen (context->argv[i]) + 1;
    }

    m3ApiWriteMem32(argc, context->argc);
    m3ApiWriteMem32(argv_buf_size, buf_len);

    m3ApiReturn(__WASI_ERRNO_SUCCESS);
}

m3ApiRawFunction(m3_wasi_generic_environ_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArgMem   (uint32_t *           , env)
    m3ApiGetArgMem   (char *               , env_buf)

    __wasi_errno_t ret;
    __wasi_size_t env_count, env_buf_size;

    ret = __wasi_environ_sizes_get(&env_count, &env_buf_size);
    if (ret != __WASI_ERRNO_SUCCESS) m3ApiReturn(ret);

    m3ApiCheckMem(env,      env_count * sizeof(uint32_t));
    m3ApiCheckMem(env_buf,  env_buf_size);

    ret = __wasi_environ_get(env, env_buf);
    if (ret != __WASI_ERRNO_SUCCESS) m3ApiReturn(ret);

    for (u32 i = 0; i < env_count; ++i) {
        env[i] = m3ApiPtrToOffset (env[i]);
    }

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_environ_sizes_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArgMem   (__wasi_size_t *      , env_count)
    m3ApiGetArgMem   (__wasi_size_t *      , env_buf_size)

    m3ApiCheckMem(env_count,    sizeof(__wasi_size_t));
    m3ApiCheckMem(env_buf_size, sizeof(__wasi_size_t));

    __wasi_errno_t ret = __wasi_environ_sizes_get(env_count, env_buf_size);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_prestat_dir_name)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArgMem   (char *               , path)
    m3ApiGetArg      (__wasi_size_t        , path_len)

    m3ApiCheckMem(path, path_len);

    __wasi_errno_t ret = __wasi_fd_prestat_dir_name(fd, path, path_len);

    WASI_TRACE("fd:%d, len:%d | path:%s", fd, path_len, path);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_prestat_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArgMem   (__wasi_prestat_t *   , buf)

    m3ApiCheckMem(buf, sizeof(__wasi_prestat_t));

    __wasi_errno_t ret = __wasi_fd_prestat_get(fd, buf);

    WASI_TRACE("fd:%d | type:%d, name_len:%d", fd, buf->pr_type, buf->u.dir.pr_name_len);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_fdstat_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArgMem   (__wasi_fdstat_t *    , fdstat)

    m3ApiCheckMem(fdstat, sizeof(__wasi_fdstat_t));

    __wasi_errno_t ret = __wasi_fd_fdstat_get(fd, fdstat);

    WASI_TRACE("fd:%d", fd);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_fdstat_set_flags)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArg      (__wasi_fdflags_t     , flags)

    __wasi_errno_t ret = __wasi_fd_fdstat_set_flags(fd, flags);

    WASI_TRACE("fd:%d, flags:0x%x", fd, flags);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_fdstat_set_rights)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArg      (__wasi_rights_t      , rights_base)
    m3ApiGetArg      (__wasi_rights_t      , rights_inheriting)

#if 0
    __wasi_errno_t ret = __wasi_fd_fdstat_set_rights(fd, rights_base, rights_inheriting);
#else
    __wasi_errno_t ret = __WASI_ERRNO_INVAL;
#endif

    WASI_TRACE("fd:%d, base:0x%" PRIx64 ", inheriting:0x%" PRIx64, fd, rights_base, rights_inheriting);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_filestat_set_size)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArg      (__wasi_filesize_t    , size)

    __wasi_errno_t ret = __wasi_fd_filestat_set_size(fd, size);

    WASI_TRACE("fd:%d, size:%" PRIu64, fd, size);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_filestat_set_times)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArg      (__wasi_timestamp_t   , atim)
    m3ApiGetArg      (__wasi_timestamp_t   , mtim)
    m3ApiGetArg      (__wasi_fstflags_t    , fst_flags)

    __wasi_errno_t ret = __wasi_fd_filestat_set_times(fd, atim, mtim, fst_flags);

    WASI_TRACE("fd:%d, atim:%" PRIu64 ", mtim:%" PRIu64 ", flags:%d", fd, atim, mtim, fst_flags);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_unstable_fd_filestat_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArgMem   (uint8_t *            , buf)

    m3ApiCheckMem(buf, 56); // wasi_filestat_t

    __wasi_filestat_t stat;

    __wasi_errno_t ret = __wasi_fd_filestat_get(fd, &stat);

    WASI_TRACE("fd:%d | fs.size:%" PRIu64, fd, stat.WASI_STAT_FIELD(size));

    if (ret != __WASI_ERRNO_SUCCESS) {
        m3ApiReturn(ret);
    }

    memset(buf, 0, 56);
    m3ApiWriteMem64(buf+0,  stat.WASI_STAT_FIELD(dev));
    m3ApiWriteMem64(buf+8,  stat.WASI_STAT_FIELD(ino));
    m3ApiWriteMem8 (buf+16, stat.WASI_STAT_FIELD(filetype));
    m3ApiWriteMem32(buf+20, stat.WASI_STAT_FIELD(nlink));
    m3ApiWriteMem64(buf+24, stat.WASI_STAT_FIELD(size));
    m3ApiWriteMem64(buf+32, stat.WASI_STAT_FIELD(atim));
    m3ApiWriteMem64(buf+40, stat.WASI_STAT_FIELD(mtim));
    m3ApiWriteMem64(buf+48, stat.WASI_STAT_FIELD(ctim));

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_snapshot_preview1_fd_filestat_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArgMem   (uint8_t *            , buf)

    m3ApiCheckMem(buf, 64); // wasi_filestat_t

    __wasi_filestat_t stat;

    __wasi_errno_t ret = __wasi_fd_filestat_get(fd, &stat);

    WASI_TRACE("fd:%d | fs.size:%" PRIu64, fd, stat.WASI_STAT_FIELD(size));

    if (ret != __WASI_ERRNO_SUCCESS) {
        m3ApiReturn(ret);
    }

    memset(buf, 0, 64);
    m3ApiWriteMem64(buf+0,  stat.WASI_STAT_FIELD(dev));
    m3ApiWriteMem64(buf+8,  stat.WASI_STAT_FIELD(ino));
    m3ApiWriteMem8 (buf+16, stat.WASI_STAT_FIELD(filetype));
    m3ApiWriteMem64(buf+24, stat.WASI_STAT_FIELD(nlink));
    m3ApiWriteMem64(buf+32, stat.WASI_STAT_FIELD(size));
    m3ApiWriteMem64(buf+40, stat.WASI_STAT_FIELD(atim));
    m3ApiWriteMem64(buf+48, stat.WASI_STAT_FIELD(mtim));
    m3ApiWriteMem64(buf+56, stat.WASI_STAT_FIELD(ctim));

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_unstable_fd_seek)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArg      (__wasi_filedelta_t   , offset)
    m3ApiGetArg      (uint32_t             , wasi_whence)
    m3ApiGetArgMem   (__wasi_filesize_t *  , result)

    m3ApiCheckMem(result, sizeof(__wasi_filesize_t));

    __wasi_whence_t whence = -1;
    switch (wasi_whence) {
    case 0: whence = __WASI_WHENCE_CUR; break;
    case 1: whence = __WASI_WHENCE_END; break;
    case 2: whence = __WASI_WHENCE_SET; break;
    }

    __wasi_filesize_t pos;
    __wasi_errno_t ret = __wasi_fd_seek(fd, offset, whence, &pos);

    WASI_TRACE("fd:%d, offset:%" PRIu64 ", whence:%s | result:%" PRIu64,
               fd, offset, wasi_whence2str(whence), pos);

    *result = pos;

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_snapshot_preview1_fd_seek)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArg      (__wasi_filedelta_t   , offset)
    m3ApiGetArg      (uint32_t             , wasi_whence)
    m3ApiGetArgMem   (__wasi_filesize_t *  , result)

    m3ApiCheckMem(result, sizeof(__wasi_filesize_t));

    __wasi_whence_t whence = -1;
    switch (wasi_whence) {
    case 0: whence = __WASI_WHENCE_SET; break;
    case 1: whence = __WASI_WHENCE_CUR; break;
    case 2: whence = __WASI_WHENCE_END; break;
    }

    __wasi_filesize_t pos;
    __wasi_errno_t ret = __wasi_fd_seek(fd, offset, whence, &pos);

    WASI_TRACE("fd:%d, offset:%" PRIu64 ", whence:%s | result:%" PRIu64,
               fd, offset, wasi_whence2str(whence), pos);

    *result = pos;

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_renumber)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , from)
    m3ApiGetArg      (__wasi_fd_t          , to)

    __wasi_errno_t ret = __wasi_fd_renumber(from, to);

    WASI_TRACE("from:%d, to:%d", from, to);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_sync)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)

    __wasi_errno_t ret = __wasi_fd_sync(fd);

    WASI_TRACE("fd:%d", fd);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_tell)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArgMem   (__wasi_filesize_t *  , result)

    m3ApiCheckMem(result, sizeof(__wasi_filesize_t));

    __wasi_filesize_t pos;
    __wasi_errno_t ret = __wasi_fd_tell(fd, &pos);

    WASI_TRACE("fd:%d | result:%" PRIu64, fd, pos);

    *result = pos;

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_path_create_directory)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArgMem   (const char *         , path)
    m3ApiGetArg      (__wasi_size_t        , path_len)

    m3ApiCheckMem(path, path_len);

    __wasi_errno_t ret = __wasi_path_create_directory(fd, path, path_len);

    WASI_TRACE("fd:%d, path:%s", fd, path);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_path_readlink)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArgMem   (const char *         , path)
    m3ApiGetArg      (__wasi_size_t        , path_len)
    m3ApiGetArgMem   (char *               , buf)
    m3ApiGetArg      (__wasi_size_t        , buf_len)
    m3ApiGetArgMem   (__wasi_size_t *      , bufused)

    m3ApiCheckMem(path, path_len);
    m3ApiCheckMem(buf, buf_len);
    m3ApiCheckMem(bufused, sizeof(__wasi_size_t));

    __wasi_errno_t ret = __wasi_path_readlink(fd, path, path_len, buf, buf_len, bufused);

    WASI_TRACE("fd:%d, path:%s | buf:%s, bufused:%d", fd, path, buf, *bufused);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_path_remove_directory)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArgMem   (const char *         , path)
    m3ApiGetArg      (__wasi_size_t        , path_len)

    m3ApiCheckMem(path, path_len);

    __wasi_errno_t ret = __wasi_path_remove_directory(fd, path, path_len);

    WASI_TRACE("fd:%d, path:%s", fd, path);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_path_rename)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , old_fd)
    m3ApiGetArgMem   (const char *         , old_path)
    m3ApiGetArg      (__wasi_size_t        , old_path_len)
    m3ApiGetArg      (__wasi_fd_t          , new_fd)
    m3ApiGetArgMem   (const char *         , new_path)
    m3ApiGetArg      (__wasi_size_t        , new_path_len)

    m3ApiCheckMem(old_path, old_path_len);
    m3ApiCheckMem(new_path, new_path_len);

    __wasi_errno_t ret = __wasi_path_rename(old_fd, old_path, old_path_len,
                                            new_fd, new_path, new_path_len);

    WASI_TRACE("old_fd:%d, old_path:%s, new_fd:%d, new_path:%s", old_fd, old_path, new_fd, new_path);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_path_symlink)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArgMem   (const char *         , old_path)
    m3ApiGetArg      (__wasi_size_t        , old_path_len)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArgMem   (const char *         , new_path)
    m3ApiGetArg      (__wasi_size_t        , new_path_len)

    m3ApiCheckMem(old_path, old_path_len);
    m3ApiCheckMem(new_path, new_path_len);

    __wasi_errno_t ret = __wasi_path_symlink(old_path, old_path_len,
                                                  fd, new_path, new_path_len);

    WASI_TRACE("old_path:%s, fd:%d, new_path:%s", old_path, fd, new_path);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_path_unlink_file)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArgMem   (const char *         , path)
    m3ApiGetArg      (__wasi_size_t        , path_len)

    m3ApiCheckMem(path, path_len);

    __wasi_errno_t ret = __wasi_path_unlink_file(fd, path, path_len);

    WASI_TRACE("fd:%d, path:%s", fd, path);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_path_open)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , dirfd)
    m3ApiGetArg      (__wasi_lookupflags_t , dirflags)
    m3ApiGetArgMem   (const char *         , path)
    m3ApiGetArg      (__wasi_size_t        , path_len)
    m3ApiGetArg      (__wasi_oflags_t      , oflags)
    m3ApiGetArg      (__wasi_rights_t      , fs_rights_base)
    m3ApiGetArg      (__wasi_rights_t      , fs_rights_inheriting)
    m3ApiGetArg      (__wasi_fdflags_t     , fs_flags)
    m3ApiGetArgMem   (__wasi_fd_t *        , fd)

    m3ApiCheckMem(path, path_len);
    m3ApiCheckMem(fd,   sizeof(__wasi_fd_t));

    __wasi_errno_t ret = __wasi_path_open(dirfd,
                                 dirflags,
                                 path,
                                 path_len,
                                 oflags,
                                 fs_rights_base,
                                 fs_rights_inheriting,
                                 fs_flags,
                                 fd);

    WASI_TRACE("dirfd:%d, dirflags:0x%x, path:%s, oflags:0x%x, fs_flags:0x%x | fd:%d", dirfd, dirflags, path, oflags, fs_flags, *fd);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_unstable_path_filestat_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArg      (__wasi_lookupflags_t , flags)
    m3ApiGetArgMem   (const char *         , path)
    m3ApiGetArg      (uint32_t             , path_len)
    m3ApiGetArgMem   (uint8_t *            , buf)

    m3ApiCheckMem(path, path_len);
    m3ApiCheckMem(buf,  56); // wasi_filestat_t

    __wasi_filestat_t stat;

    __wasi_errno_t ret = __wasi_path_filestat_get(fd, flags, path, path_len, &stat);

    WASI_TRACE("fd:%d, flags:0x%x, path:%s | fs.size:%" PRIu64, fd, flags, path, stat.WASI_STAT_FIELD(size));

    if (ret != __WASI_ERRNO_SUCCESS) {
        m3ApiReturn(ret);
    }

    memset(buf, 0, 56);
    m3ApiWriteMem64(buf+0,  stat.WASI_STAT_FIELD(dev));
    m3ApiWriteMem64(buf+8,  stat.WASI_STAT_FIELD(ino));
    m3ApiWriteMem8 (buf+16, stat.WASI_STAT_FIELD(filetype));
    m3ApiWriteMem32(buf+20, stat.WASI_STAT_FIELD(nlink));
    m3ApiWriteMem64(buf+24, stat.WASI_STAT_FIELD(size));
    m3ApiWriteMem64(buf+32, stat.WASI_STAT_FIELD(atim));
    m3ApiWriteMem64(buf+40, stat.WASI_STAT_FIELD(mtim));
    m3ApiWriteMem64(buf+48, stat.WASI_STAT_FIELD(ctim));

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_snapshot_preview1_path_filestat_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArg      (__wasi_lookupflags_t , flags)
    m3ApiGetArgMem   (const char *         , path)
    m3ApiGetArg      (uint32_t             , path_len)
    m3ApiGetArgMem   (uint8_t *            , buf)

    m3ApiCheckMem(path, path_len);
    m3ApiCheckMem(buf,  64); // wasi_filestat_t

    __wasi_filestat_t stat;

    __wasi_errno_t ret = __wasi_path_filestat_get(fd, flags, path, path_len, &stat);

    WASI_TRACE("fd:%d, flags:0x%x, path:%s | fs.size:%" PRIu64, fd, flags, path, stat.WASI_STAT_FIELD(size));

    if (ret != __WASI_ERRNO_SUCCESS) {
        m3ApiReturn(ret);
    }

    memset(buf, 0, 64);
    m3ApiWriteMem64(buf+0,  stat.WASI_STAT_FIELD(dev));
    m3ApiWriteMem64(buf+8,  stat.WASI_STAT_FIELD(ino));
    m3ApiWriteMem8 (buf+16, stat.WASI_STAT_FIELD(filetype));
    m3ApiWriteMem64(buf+24, stat.WASI_STAT_FIELD(nlink));
    m3ApiWriteMem64(buf+32, stat.WASI_STAT_FIELD(size));
    m3ApiWriteMem64(buf+40, stat.WASI_STAT_FIELD(atim));
    m3ApiWriteMem64(buf+48, stat.WASI_STAT_FIELD(mtim));
    m3ApiWriteMem64(buf+56, stat.WASI_STAT_FIELD(ctim));

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_pread)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArgMem   (__wasi_iovec_t *     , wasi_iovs)
    m3ApiGetArg      (__wasi_size_t        , iovs_len)
    m3ApiGetArg      (__wasi_filesize_t    , offset)
    m3ApiGetArgMem   (__wasi_size_t *      , nread)

    m3ApiCheckMem(wasi_iovs,    iovs_len * sizeof(__wasi_iovec_t));
    m3ApiCheckMem(nread,        sizeof(__wasi_size_t));

    __wasi_iovec_t iovs[iovs_len];
    const void* mem_check = copy_iov_to_host(runtime, _mem, iovs, wasi_iovs, iovs_len);
    if (mem_check != m3Err_none) {
        return mem_check;
    }

    __wasi_errno_t ret = __wasi_fd_pread(fd, iovs, iovs_len, offset, nread);

    WASI_TRACE("fd:%d | nread:%d", fd, *nread);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_read)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArgMem   (__wasi_iovec_t *     , wasi_iovs)
    m3ApiGetArg      (__wasi_size_t        , iovs_len)
    m3ApiGetArgMem   (__wasi_size_t *      , nread)

    m3ApiCheckMem(wasi_iovs,    iovs_len * sizeof(__wasi_iovec_t));
    m3ApiCheckMem(nread,        sizeof(__wasi_size_t));

    __wasi_iovec_t iovs[iovs_len];
    const void* mem_check = copy_iov_to_host(runtime, _mem, iovs, wasi_iovs, iovs_len);
    if (mem_check != m3Err_none) {
        return mem_check;
    }

    __wasi_errno_t ret = __wasi_fd_read(fd, iovs, iovs_len, nread);

    WASI_TRACE("fd:%d | nread:%d", fd, *nread);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_write)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArgMem   (__wasi_iovec_t *     , wasi_iovs)
    m3ApiGetArg      (__wasi_size_t        , iovs_len)
    m3ApiGetArgMem   (__wasi_size_t *      , nwritten)

    m3ApiCheckMem(wasi_iovs,    iovs_len * sizeof(__wasi_iovec_t));
    m3ApiCheckMem(nwritten,     sizeof(__wasi_size_t));

    __wasi_iovec_t iovs[iovs_len];
    const void* mem_check = copy_iov_to_host(runtime, _mem, iovs, wasi_iovs, iovs_len);
    if (mem_check != m3Err_none) {
        return mem_check;
    }

    __wasi_errno_t ret = __wasi_fd_write(fd, (__wasi_ciovec_t*)iovs, iovs_len, nwritten);

    WASI_TRACE("fd:%d | nwritten:%d", fd, *nwritten);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_pwrite)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArgMem   (__wasi_iovec_t *     , wasi_iovs)
    m3ApiGetArg      (__wasi_size_t        , iovs_len)
    m3ApiGetArg      (__wasi_filesize_t    , offset)
    m3ApiGetArgMem   (__wasi_size_t *      , nwritten)

    m3ApiCheckMem(wasi_iovs,    iovs_len * sizeof(__wasi_iovec_t));
    m3ApiCheckMem(nwritten,     sizeof(__wasi_size_t));

    __wasi_iovec_t iovs[iovs_len];
    const void* mem_check = copy_iov_to_host(runtime, _mem, iovs, wasi_iovs, iovs_len);
    if (mem_check != m3Err_none) {
        return mem_check;
    }
    
    __wasi_errno_t ret = __wasi_fd_pwrite(fd, (__wasi_ciovec_t*)iovs, iovs_len, offset, nwritten);

    WASI_TRACE("fd:%d | nwritten:%d", fd, *nwritten);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_readdir)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArgMem   (void *               , buf)
    m3ApiGetArg      (__wasi_size_t        , buf_len)
    m3ApiGetArg      (__wasi_dircookie_t   , cookie)
    m3ApiGetArgMem   (__wasi_size_t *      , bufused)

    m3ApiCheckMem(buf,      buf_len);
    m3ApiCheckMem(bufused,  sizeof(__wasi_size_t));

    __wasi_errno_t ret = __wasi_fd_readdir(fd, buf, buf_len, cookie, bufused);

    WASI_TRACE("fd:%d | bufused:%d", fd, *bufused);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_advise)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArg      (__wasi_filesize_t    , offset)
    m3ApiGetArg      (__wasi_filesize_t    , length)
    m3ApiGetArg      (__wasi_advice_t      , advice)

    __wasi_errno_t ret = __wasi_fd_advise(fd, offset, length, advice);

    WASI_TRACE("fd:%d, offset:%" PRIu64 ", length:%" PRIu64 ", advice:%d", fd, offset, length, advice);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_allocate)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t          , fd)
    m3ApiGetArg      (__wasi_filesize_t    , offset)
    m3ApiGetArg      (__wasi_filesize_t    , length)

    __wasi_errno_t ret = __wasi_fd_allocate(fd, offset, length);

    WASI_TRACE("fd:%d, offset:%" PRIu64 ", length:%" PRIu64, fd, offset, length);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_close)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t, fd)

    __wasi_errno_t ret = __wasi_fd_close(fd);

    WASI_TRACE("fd:%d", fd);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_datasync)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_fd_t, fd)

    __wasi_errno_t ret = __wasi_fd_datasync(fd);

    WASI_TRACE("fd:%d", fd);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_random_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArgMem   (uint8_t *            , buf)
    m3ApiGetArg      (__wasi_size_t        , buf_len)

    m3ApiCheckMem(buf, buf_len);

    __wasi_errno_t ret = __wasi_random_get(buf, buf_len);

    WASI_TRACE("len:%d", buf_len);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_clock_res_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_clockid_t     , wasi_clk_id)
    m3ApiGetArgMem   (__wasi_timestamp_t * , resolution)

    m3ApiCheckMem(resolution, sizeof(__wasi_timestamp_t));

    __wasi_timestamp_t t;
    __wasi_errno_t ret = __wasi_clock_res_get(wasi_clk_id, &t);

    WASI_TRACE("clk_id:%d | res:%" PRIu64, wasi_clk_id, t);

    *resolution = t;

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_clock_time_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_clockid_t     , wasi_clk_id)
    m3ApiGetArg      (__wasi_timestamp_t   , precision)
    m3ApiGetArgMem   (__wasi_timestamp_t * , time)

    m3ApiCheckMem(time, sizeof(__wasi_timestamp_t));

    __wasi_timestamp_t t;
    __wasi_errno_t ret = __wasi_clock_time_get(wasi_clk_id, precision, &t);

    WASI_TRACE("clk_id:%d | res:%" PRIu64, wasi_clk_id, t);

    *time = t;

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_poll_oneoff)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArgMem   (const __wasi_subscription_t * , in)
    m3ApiGetArgMem   (__wasi_event_t *              , out)
    m3ApiGetArg      (__wasi_size_t                 , nsubscriptions)
    m3ApiGetArgMem   (__wasi_size_t *               , nevents)

    m3ApiCheckMem(in,       nsubscriptions * sizeof(__wasi_subscription_t));
    m3ApiCheckMem(out,      nsubscriptions * sizeof(__wasi_event_t));
    m3ApiCheckMem(nevents,  sizeof(__wasi_size_t));

    // TODO: unstable/snapshot_preview1 compatibility

    __wasi_errno_t ret = __wasi_poll_oneoff(in, out, nsubscriptions, nevents);

    WASI_TRACE("nsubscriptions:%d | nevents:%d", nsubscriptions, *nevents);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_proc_exit)
{
    m3ApiGetArg      (uint32_t, code)

    m3_wasi_context_t* context = (m3_wasi_context_t*)(_ctx->userdata);

    if (context) {
        context->exit_code = code;
    }

    m3ApiTrap(m3Err_trapExit);
}

m3ApiRawFunction(m3_wasi_generic_proc_raise)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (__wasi_signal_t, sig)

    __wasi_errno_t ret = __WASI_ERRNO_INVAL;
#if 0
    ret = __wasi_proc_raise(sig);
#endif

    WASI_TRACE("sig:%d", sig);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_sched_yield)
{
    m3ApiReturnType  (uint32_t)
    __wasi_errno_t ret = __wasi_sched_yield();

    WASI_TRACE("");

    m3ApiReturn(ret);
}


static
M3Result SuppressLookupFailure(M3Result i_result)
{
    if (i_result == m3Err_functionLookupFailed)
        return m3Err_none;
    else
        return i_result;
}

m3_wasi_context_t* m3_GetWasiContext()
{
    return wasi_context;
}


M3Result  m3_LinkWASI  (IM3Module module)
{
    M3Result result = m3Err_none;

    if (!wasi_context) {
        wasi_context = (m3_wasi_context_t*)malloc(sizeof(m3_wasi_context_t));
        wasi_context->exit_code = 0;
        wasi_context->argc = 0;
        wasi_context->argv = 0;
    }

    static const char* namespaces[2] = { "wasi_unstable", "wasi_snapshot_preview1" };

    // Some functions are incompatible between WASI versions
_   (SuppressLookupFailure (m3_LinkRawFunction (module, "wasi_unstable",          "fd_seek",           "i(iIi*)",   &m3_wasi_unstable_fd_seek)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, "wasi_snapshot_preview1", "fd_seek",           "i(iIi*)",   &m3_wasi_snapshot_preview1_fd_seek)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, "wasi_unstable",          "fd_filestat_get",   "i(i*)",     &m3_wasi_unstable_fd_filestat_get)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, "wasi_snapshot_preview1", "fd_filestat_get",   "i(i*)",     &m3_wasi_snapshot_preview1_fd_filestat_get)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, "wasi_unstable",          "path_filestat_get", "i(ii*i*)",  &m3_wasi_unstable_path_filestat_get)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, "wasi_snapshot_preview1", "path_filestat_get", "i(ii*i*)",  &m3_wasi_snapshot_preview1_path_filestat_get)));

    for (int i=0; i<2; i++)
    {
        const char* wasi = namespaces[i];

_       (SuppressLookupFailure (m3_LinkRawFunctionEx (module, wasi, "args_get",           "i(**)",   &m3_wasi_generic_args_get, wasi_context)));
_       (SuppressLookupFailure (m3_LinkRawFunctionEx (module, wasi, "args_sizes_get",     "i(**)",   &m3_wasi_generic_args_sizes_get, wasi_context)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "clock_res_get",        "i(i*)",   &m3_wasi_generic_clock_res_get)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "clock_time_get",       "i(iI*)",  &m3_wasi_generic_clock_time_get)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "environ_get",          "i(**)",   &m3_wasi_generic_environ_get)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "environ_sizes_get",    "i(**)",   &m3_wasi_generic_environ_sizes_get)));

_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_advise",            "i(iIIi)", &m3_wasi_generic_fd_advise)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_allocate",          "i(iII)",  &m3_wasi_generic_fd_allocate)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_close",             "i(i)",    &m3_wasi_generic_fd_close)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_datasync",          "i(i)",    &m3_wasi_generic_fd_datasync)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_fdstat_get",        "i(i*)",   &m3_wasi_generic_fd_fdstat_get)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_fdstat_set_flags",  "i(ii)",   &m3_wasi_generic_fd_fdstat_set_flags)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_fdstat_set_rights", "i(iII)",  &m3_wasi_generic_fd_fdstat_set_rights)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_filestat_set_size", "i(iI)",   &m3_wasi_generic_fd_filestat_set_size)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_filestat_set_times","i(iIIi)", &m3_wasi_generic_fd_filestat_set_times)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_pread",             "i(i*iI*)",&m3_wasi_generic_fd_pread)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_prestat_get",       "i(i*)",   &m3_wasi_generic_fd_prestat_get)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_prestat_dir_name",  "i(i*i)",  &m3_wasi_generic_fd_prestat_dir_name)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_pwrite",            "i(i*iI*)",&m3_wasi_generic_fd_pwrite)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_read",              "i(i*i*)", &m3_wasi_generic_fd_read)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_readdir",           "i(i*iI*)",&m3_wasi_generic_fd_readdir)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_renumber",          "i(ii)",   &m3_wasi_generic_fd_renumber)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_sync",              "i(i)",    &m3_wasi_generic_fd_sync)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_tell",              "i(i*)",   &m3_wasi_generic_fd_tell)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "fd_write",             "i(i*i*)", &m3_wasi_generic_fd_write)));

_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "path_create_directory",    "i(i*i)",       &m3_wasi_generic_path_create_directory)));
//_     (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "path_filestat_set_times",  "i(ii*iIIi)",   )));
//_     (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "path_link",                "i(ii*ii*i)",   )));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "path_open",                "i(ii*iiIIi*)", &m3_wasi_generic_path_open)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "path_readlink",            "i(i*i*i*)",    &m3_wasi_generic_path_readlink)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "path_remove_directory",    "i(i*i)",       &m3_wasi_generic_path_remove_directory)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "path_rename",              "i(i*ii*i)",    &m3_wasi_generic_path_rename)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "path_symlink",             "i(*ii*i)",     &m3_wasi_generic_path_symlink)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "path_unlink_file",         "i(i*i)",       &m3_wasi_generic_path_unlink_file)));

_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "poll_oneoff",          "i(**i*)", &m3_wasi_generic_poll_oneoff)));
_       (SuppressLookupFailure (m3_LinkRawFunctionEx (module, wasi, "proc_exit",          "v(i)",    &m3_wasi_generic_proc_exit, wasi_context)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "proc_raise",           "i(i)",    &m3_wasi_generic_proc_raise)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "random_get",           "i(*i)",   &m3_wasi_generic_random_get)));
_       (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "sched_yield",          "i()",     &m3_wasi_generic_sched_yield)));

//_     (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "sock_recv",            "i(i*ii**)",        )));
//_     (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "sock_send",            "i(i*ii*)",         )));
//_     (SuppressLookupFailure (m3_LinkRawFunction (module, wasi, "sock_shutdown",        "i(ii)",            )));
    }

_catch:
    return result;
}

#endif // d_m3HasMetaWASI

