//
//  m3_api_uvwasi.c
//
//  Created by Colin J. Ihrig on 4/20/20.
//  Copyright © 2020 Colin J. Ihrig, Volodymyr Shymanskyy. All rights reserved.
//

#define _POSIX_C_SOURCE 200809L

#include "m3_api_wasi.h"

#include "m3_env.h"
#include "m3_exception.h"

#if defined(d_m3HasUVWASI)

#include <stdio.h>
#include <string.h>

#ifdef __APPLE__
# include <crt_externs.h>
# define environ (*_NSGetEnviron())
#elif !defined(_MSC_VER)
extern char** environ;
#endif

static m3_wasi_context_t* wasi_context;
static uvwasi_t uvwasi;

typedef struct wasi_iovec_t
{
    uvwasi_size_t buf;
    uvwasi_size_t buf_len;
} wasi_iovec_t;

#if d_m3EnableWasiTracing

const char* wasi_errno2str(uvwasi_errno_t err)
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

const char* wasi_whence2str(uvwasi_whence_t whence)
{
    switch (whence) {
    case UVWASI_WHENCE_SET: return "SET";
    case UVWASI_WHENCE_CUR: return "CUR";
    case UVWASI_WHENCE_END: return "END";
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

    if (context == NULL) { m3ApiReturn(UVWASI_EINVAL); }

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

    m3ApiReturn(UVWASI_ESUCCESS);
}

m3ApiRawFunction(m3_wasi_generic_args_sizes_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArgMem   (uvwasi_size_t *      , argc)
    m3ApiGetArgMem   (uvwasi_size_t *      , argv_buf_size)

    m3ApiCheckMem(argc,             sizeof(uvwasi_size_t));
    m3ApiCheckMem(argv_buf_size,    sizeof(uvwasi_size_t));

    m3_wasi_context_t* context = (m3_wasi_context_t*)(_ctx->userdata);

    if (context == NULL) { m3ApiReturn(UVWASI_EINVAL); }

    uvwasi_size_t buf_len = 0;
    for (u32 i = 0; i < context->argc; ++i)
    {
        buf_len += strlen (context->argv[i]) + 1;
    }

    m3ApiWriteMem32(argc, context->argc);
    m3ApiWriteMem32(argv_buf_size, buf_len);

    m3ApiReturn(UVWASI_ESUCCESS);
}

m3ApiRawFunction(m3_wasi_generic_environ_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArgMem   (uint32_t *           , env)
    m3ApiGetArgMem   (char *               , env_buf)

    char **environment;
    uvwasi_errno_t ret;
    uvwasi_size_t env_count, env_buf_size;

    ret = uvwasi_environ_sizes_get(&uvwasi, &env_count, &env_buf_size);
    if (ret != UVWASI_ESUCCESS) {
        m3ApiReturn(ret);
    }

    m3ApiCheckMem(env,      env_count * sizeof(uint32_t));
    m3ApiCheckMem(env_buf,  env_buf_size);

    environment = calloc(env_count, sizeof(char *));
    if (environment == NULL) {
        m3ApiReturn(UVWASI_ENOMEM);
    }

    ret = uvwasi_environ_get(&uvwasi, environment, env_buf);
    if (ret != UVWASI_ESUCCESS) {
        free(environment);
        m3ApiReturn(ret);
    }

    uint32_t environ_buf_offset = m3ApiPtrToOffset(env_buf);

    for (u32 i = 0; i < env_count; ++i)
    {
        uint32_t offset = environ_buf_offset +
                          (environment[i] - environment[0]);
        m3ApiWriteMem32(&env[i], offset);
    }

    free(environment);
    m3ApiReturn(UVWASI_ESUCCESS);
}

m3ApiRawFunction(m3_wasi_generic_environ_sizes_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArgMem   (uvwasi_size_t *      , env_count)
    m3ApiGetArgMem   (uvwasi_size_t *      , env_buf_size)

    m3ApiCheckMem(env_count,    sizeof(uvwasi_size_t));
    m3ApiCheckMem(env_buf_size, sizeof(uvwasi_size_t));

    uvwasi_size_t count;
    uvwasi_size_t buf_size;

    uvwasi_errno_t ret = uvwasi_environ_sizes_get(&uvwasi, &count, &buf_size);

    m3ApiWriteMem32(env_count,    count);
    m3ApiWriteMem32(env_buf_size, buf_size);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_prestat_dir_name)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArgMem   (char *               , path)
    m3ApiGetArg      (uvwasi_size_t        , path_len)

    m3ApiCheckMem(path, path_len);

    uvwasi_errno_t ret = uvwasi_fd_prestat_dir_name(&uvwasi, fd, path, path_len);

    WASI_TRACE("fd:%d, len:%d | path:%s", fd, path_len, path);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_prestat_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArgMem   (uint8_t *            , buf)

    m3ApiCheckMem(buf, 8);

    uvwasi_prestat_t prestat;

    uvwasi_errno_t ret = uvwasi_fd_prestat_get(&uvwasi, fd, &prestat);

    WASI_TRACE("fd:%d | type:%d, name_len:%d", fd, prestat.pr_type, prestat.u.dir.pr_name_len);

    if (ret != UVWASI_ESUCCESS) {
        m3ApiReturn(ret);
    }

    m3ApiWriteMem32(buf+0, prestat.pr_type);
    m3ApiWriteMem32(buf+4, prestat.u.dir.pr_name_len);
    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_fdstat_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArgMem   (uint8_t *            , buf)

    m3ApiCheckMem(buf, 24);

    uvwasi_fdstat_t stat;
    uvwasi_errno_t ret = uvwasi_fd_fdstat_get(&uvwasi, fd, &stat);

    WASI_TRACE("fd:%d", fd);

    if (ret != UVWASI_ESUCCESS) {
        m3ApiReturn(ret);
    }

    memset(buf, 0, 24);
    m3ApiWriteMem8 (buf+0, stat.fs_filetype);
    m3ApiWriteMem16(buf+2, stat.fs_flags);
    m3ApiWriteMem64(buf+8, stat.fs_rights_base);
    m3ApiWriteMem64(buf+16, stat.fs_rights_inheriting);
    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_fdstat_set_flags)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArg      (uvwasi_fdflags_t     , flags)

    uvwasi_errno_t ret = uvwasi_fd_fdstat_set_flags(&uvwasi, fd, flags);

    WASI_TRACE("fd:%d, flags:0x%x", fd, flags);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_fdstat_set_rights)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArg      (uvwasi_rights_t      , rights_base)
    m3ApiGetArg      (uvwasi_rights_t      , rights_inheriting)

    uvwasi_errno_t ret = uvwasi_fd_fdstat_set_rights(&uvwasi, fd, rights_base, rights_inheriting);

    WASI_TRACE("fd:%d, base:0x%" PRIx64 ", inheriting:0x%" PRIx64, fd, rights_base, rights_inheriting);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_filestat_set_size)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArg      (uvwasi_filesize_t    , size)

    uvwasi_errno_t ret = uvwasi_fd_filestat_set_size(&uvwasi, fd, size);

    WASI_TRACE("fd:%d, size:%" PRIu64, fd, size);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_filestat_set_times)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArg      (uvwasi_timestamp_t   , atim)
    m3ApiGetArg      (uvwasi_timestamp_t   , mtim)
    m3ApiGetArg      (uvwasi_fstflags_t    , fst_flags)

    uvwasi_errno_t ret = uvwasi_fd_filestat_set_times(&uvwasi, fd, atim, mtim, fst_flags);

    WASI_TRACE("fd:%d, atim:%" PRIu64 ", mtim:%" PRIu64 ", flags:%d", fd, atim, mtim, fst_flags);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_unstable_fd_filestat_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArgMem   (uint8_t *            , buf)

    m3ApiCheckMem(buf, 56); // wasi_filestat_t

    uvwasi_filestat_t stat;

    uvwasi_errno_t ret = uvwasi_fd_filestat_get(&uvwasi, fd, &stat);

    WASI_TRACE("fd:%d | fs.size:%" PRIu64, fd, stat.st_size);

    if (ret != UVWASI_ESUCCESS) {
        m3ApiReturn(ret);
    }

    memset(buf, 0, 56);
    m3ApiWriteMem64(buf+0,  stat.st_dev);
    m3ApiWriteMem64(buf+8,  stat.st_ino);
    m3ApiWriteMem8 (buf+16, stat.st_filetype);
    m3ApiWriteMem32(buf+20, stat.st_nlink);
    m3ApiWriteMem64(buf+24, stat.st_size);
    m3ApiWriteMem64(buf+32, stat.st_atim);
    m3ApiWriteMem64(buf+40, stat.st_mtim);
    m3ApiWriteMem64(buf+48, stat.st_ctim);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_snapshot_preview1_fd_filestat_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArgMem   (uint8_t *            , buf)

    m3ApiCheckMem(buf, 64); // wasi_filestat_t

    uvwasi_filestat_t stat;

    uvwasi_errno_t ret = uvwasi_fd_filestat_get(&uvwasi, fd, &stat);

    WASI_TRACE("fd:%d | fs.size:%" PRIu64, fd, stat.st_size);

    if (ret != UVWASI_ESUCCESS) {
        m3ApiReturn(ret);
    }

    memset(buf, 0, 64);
    m3ApiWriteMem64(buf+0,  stat.st_dev);
    m3ApiWriteMem64(buf+8,  stat.st_ino);
    m3ApiWriteMem8 (buf+16, stat.st_filetype);
    m3ApiWriteMem64(buf+24, stat.st_nlink);
    m3ApiWriteMem64(buf+32, stat.st_size);
    m3ApiWriteMem64(buf+40, stat.st_atim);
    m3ApiWriteMem64(buf+48, stat.st_mtim);
    m3ApiWriteMem64(buf+56, stat.st_ctim);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_unstable_fd_seek)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArg      (uvwasi_filedelta_t   , offset)
    m3ApiGetArg      (uint32_t             , wasi_whence)
    m3ApiGetArgMem   (uvwasi_filesize_t *  , result)

    m3ApiCheckMem(result, sizeof(uvwasi_filesize_t));

    uvwasi_whence_t whence = -1;
    switch (wasi_whence) {
    case 0: whence = UVWASI_WHENCE_CUR; break;
    case 1: whence = UVWASI_WHENCE_END; break;
    case 2: whence = UVWASI_WHENCE_SET; break;
    }

    uvwasi_filesize_t pos;
    uvwasi_errno_t ret = uvwasi_fd_seek(&uvwasi, fd, offset, whence, &pos);

    WASI_TRACE("fd:%d, offset:%" PRIu64 ", whence:%s | result:%" PRIu64,
               fd, offset, wasi_whence2str(whence), pos);

    m3ApiWriteMem64(result, pos);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_snapshot_preview1_fd_seek)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArg      (uvwasi_filedelta_t   , offset)
    m3ApiGetArg      (uint32_t             , wasi_whence)
    m3ApiGetArgMem   (uvwasi_filesize_t *  , result)

    m3ApiCheckMem(result, sizeof(uvwasi_filesize_t));

    uvwasi_whence_t whence = -1;
    switch (wasi_whence) {
    case 0: whence = UVWASI_WHENCE_SET; break;
    case 1: whence = UVWASI_WHENCE_CUR; break;
    case 2: whence = UVWASI_WHENCE_END; break;
    }

    uvwasi_filesize_t pos;
    uvwasi_errno_t ret = uvwasi_fd_seek(&uvwasi, fd, offset, whence, &pos);

    WASI_TRACE("fd:%d, offset:%" PRIu64 ", whence:%s | result:%" PRIu64,
               fd, offset, wasi_whence2str(whence), pos);

    m3ApiWriteMem64(result, pos);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_renumber)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , from)
    m3ApiGetArg      (uvwasi_fd_t          , to)

    uvwasi_errno_t ret = uvwasi_fd_renumber(&uvwasi, from, to);

    WASI_TRACE("from:%d, to:%d", from, to);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_sync)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)

    uvwasi_errno_t ret = uvwasi_fd_sync(&uvwasi, fd);

    WASI_TRACE("fd:%d", fd);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_tell)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArgMem   (uvwasi_filesize_t *  , result)

    m3ApiCheckMem(result, sizeof(uvwasi_filesize_t));

    uvwasi_filesize_t pos;
    uvwasi_errno_t ret = uvwasi_fd_tell(&uvwasi, fd, &pos);

    WASI_TRACE("fd:%d | result:%" PRIu64, fd, pos);

    m3ApiWriteMem64(result, pos);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_path_create_directory)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArgMem   (const char *         , path)
    m3ApiGetArg      (uvwasi_size_t        , path_len)

    m3ApiCheckMem(path, path_len);

    uvwasi_errno_t ret = uvwasi_path_create_directory(&uvwasi, fd, path, path_len);

    WASI_TRACE("fd:%d, path:%s", fd, path);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_path_readlink)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArgMem   (const char *         , path)
    m3ApiGetArg      (uvwasi_size_t        , path_len)
    m3ApiGetArgMem   (char *               , buf)
    m3ApiGetArg      (uvwasi_size_t        , buf_len)
    m3ApiGetArgMem   (uvwasi_size_t *      , bufused)

    m3ApiCheckMem(path, path_len);
    m3ApiCheckMem(buf, buf_len);
    m3ApiCheckMem(bufused, sizeof(uvwasi_size_t));

    uvwasi_size_t uvbufused;

    uvwasi_errno_t ret = uvwasi_path_readlink(&uvwasi, fd, path, path_len, buf, buf_len, &uvbufused);

    WASI_TRACE("fd:%d, path:%s | buf:%s, bufused:%d", fd, path, buf, uvbufused);

    m3ApiWriteMem32(bufused, uvbufused);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_path_remove_directory)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArgMem   (const char *         , path)
    m3ApiGetArg      (uvwasi_size_t        , path_len)

    m3ApiCheckMem(path, path_len);

    uvwasi_errno_t ret = uvwasi_path_remove_directory(&uvwasi, fd, path, path_len);

    WASI_TRACE("fd:%d, path:%s", fd, path);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_path_rename)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , old_fd)
    m3ApiGetArgMem   (const char *         , old_path)
    m3ApiGetArg      (uvwasi_size_t        , old_path_len)
    m3ApiGetArg      (uvwasi_fd_t          , new_fd)
    m3ApiGetArgMem   (const char *         , new_path)
    m3ApiGetArg      (uvwasi_size_t        , new_path_len)

    m3ApiCheckMem(old_path, old_path_len);
    m3ApiCheckMem(new_path, new_path_len);

    uvwasi_errno_t ret = uvwasi_path_rename(&uvwasi, old_fd, old_path, old_path_len,
                                                     new_fd, new_path, new_path_len);

    WASI_TRACE("old_fd:%d, old_path:%s, new_fd:%d, new_path:%s", old_fd, old_path, new_fd, new_path);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_path_symlink)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArgMem   (const char *         , old_path)
    m3ApiGetArg      (uvwasi_size_t        , old_path_len)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArgMem   (const char *         , new_path)
    m3ApiGetArg      (uvwasi_size_t        , new_path_len)

    m3ApiCheckMem(old_path, old_path_len);
    m3ApiCheckMem(new_path, new_path_len);

    uvwasi_errno_t ret = uvwasi_path_symlink(&uvwasi, old_path, old_path_len,
                                                  fd, new_path, new_path_len);

    WASI_TRACE("old_path:%s, fd:%d, new_path:%s", old_path, fd, new_path);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_path_unlink_file)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArgMem   (const char *         , path)
    m3ApiGetArg      (uvwasi_size_t        , path_len)

    m3ApiCheckMem(path, path_len);

    uvwasi_errno_t ret = uvwasi_path_unlink_file(&uvwasi, fd, path, path_len);

    WASI_TRACE("fd:%d, path:%s", fd, path);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_path_open)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , dirfd)
    m3ApiGetArg      (uvwasi_lookupflags_t , dirflags)
    m3ApiGetArgMem   (const char *         , path)
    m3ApiGetArg      (uvwasi_size_t        , path_len)
    m3ApiGetArg      (uvwasi_oflags_t      , oflags)
    m3ApiGetArg      (uvwasi_rights_t      , fs_rights_base)
    m3ApiGetArg      (uvwasi_rights_t      , fs_rights_inheriting)
    m3ApiGetArg      (uvwasi_fdflags_t     , fs_flags)
    m3ApiGetArgMem   (uvwasi_fd_t *        , fd)

    m3ApiCheckMem(path, path_len);
    m3ApiCheckMem(fd,   sizeof(uvwasi_fd_t));

    uvwasi_fd_t uvfd;

    uvwasi_errno_t ret = uvwasi_path_open(&uvwasi,
                                 dirfd,
                                 dirflags,
                                 path,
                                 path_len,
                                 oflags,
                                 fs_rights_base,
                                 fs_rights_inheriting,
                                 fs_flags,
                                 &uvfd);

    WASI_TRACE("dirfd:%d, dirflags:0x%x, path:%s, oflags:0x%x, fs_flags:0x%x | fd:%d", dirfd, dirflags, path, oflags, fs_flags, uvfd);

    m3ApiWriteMem32(fd, uvfd);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_unstable_path_filestat_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArg      (uvwasi_lookupflags_t , flags)
    m3ApiGetArgMem   (const char *         , path)
    m3ApiGetArg      (uint32_t             , path_len)
    m3ApiGetArgMem   (uint8_t *            , buf)

    m3ApiCheckMem(path, path_len);
    m3ApiCheckMem(buf,  56); // wasi_filestat_t

    uvwasi_filestat_t stat;

    uvwasi_errno_t ret = uvwasi_path_filestat_get(&uvwasi, fd, flags, path, path_len, &stat);

    WASI_TRACE("fd:%d, flags:0x%x, path:%s | fs.size:%" PRIu64, fd, flags, path, stat.st_size);

    if (ret != UVWASI_ESUCCESS) {
        m3ApiReturn(ret);
    }

    memset(buf, 0, 56);
    m3ApiWriteMem64(buf+0,  stat.st_dev);
    m3ApiWriteMem64(buf+8,  stat.st_ino);
    m3ApiWriteMem8 (buf+16, stat.st_filetype);
    m3ApiWriteMem32(buf+20, stat.st_nlink);
    m3ApiWriteMem64(buf+24, stat.st_size);
    m3ApiWriteMem64(buf+32, stat.st_atim);
    m3ApiWriteMem64(buf+40, stat.st_mtim);
    m3ApiWriteMem64(buf+48, stat.st_ctim);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_snapshot_preview1_path_filestat_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArg      (uvwasi_lookupflags_t , flags)
    m3ApiGetArgMem   (const char *         , path)
    m3ApiGetArg      (uint32_t             , path_len)
    m3ApiGetArgMem   (uint8_t *            , buf)

    m3ApiCheckMem(path, path_len);
    m3ApiCheckMem(buf,  64); // wasi_filestat_t

    uvwasi_filestat_t stat;

    uvwasi_errno_t ret = uvwasi_path_filestat_get(&uvwasi, fd, flags, path, path_len, &stat);

    WASI_TRACE("fd:%d, flags:0x%x, path:%s | fs.size:%" PRIu64, fd, flags, path, stat.st_size);

    if (ret != UVWASI_ESUCCESS) {
        m3ApiReturn(ret);
    }

    memset(buf, 0, 64);
    m3ApiWriteMem64(buf+0,  stat.st_dev);
    m3ApiWriteMem64(buf+8,  stat.st_ino);
    m3ApiWriteMem8 (buf+16, stat.st_filetype);
    m3ApiWriteMem64(buf+24, stat.st_nlink);
    m3ApiWriteMem64(buf+32, stat.st_size);
    m3ApiWriteMem64(buf+40, stat.st_atim);
    m3ApiWriteMem64(buf+48, stat.st_mtim);
    m3ApiWriteMem64(buf+56, stat.st_ctim);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_pread)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArgMem   (wasi_iovec_t *       , wasi_iovs)
    m3ApiGetArg      (uvwasi_size_t        , iovs_len)
    m3ApiGetArg      (uvwasi_filesize_t    , offset)
    m3ApiGetArgMem   (uvwasi_size_t *      , nread)

    m3ApiCheckMem(wasi_iovs,    iovs_len * sizeof(wasi_iovec_t));
    m3ApiCheckMem(nread,        sizeof(uvwasi_size_t));

#if defined(M3_COMPILER_MSVC)
    if (iovs_len > 32) m3ApiReturn(UVWASI_EINVAL);
    uvwasi_iovec_t  iovs[32];
#else
    if (iovs_len > 128) m3ApiReturn(UVWASI_EINVAL);
    uvwasi_iovec_t  iovs[iovs_len];
#endif

    for (uvwasi_size_t i = 0; i < iovs_len; ++i) {
        iovs[i].buf = m3ApiOffsetToPtr(m3ApiReadMem32(&wasi_iovs[i].buf));
        iovs[i].buf_len = m3ApiReadMem32(&wasi_iovs[i].buf_len);
        m3ApiCheckMem(iovs[i].buf,     iovs[i].buf_len);
        //fprintf(stderr, "> fd_pread fd:%d iov%d.len:%d\n", fd, i, iovs[i].buf_len);
    }

    uvwasi_size_t num_read;

    uvwasi_errno_t ret = uvwasi_fd_pread(&uvwasi, fd, iovs, iovs_len, offset, &num_read);

    WASI_TRACE("fd:%d | nread:%d", fd, num_read);

    m3ApiWriteMem32(nread, num_read);
    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_read)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArgMem   (wasi_iovec_t *       , wasi_iovs)
    m3ApiGetArg      (uvwasi_size_t        , iovs_len)
    m3ApiGetArgMem   (uvwasi_size_t *      , nread)

    m3ApiCheckMem(wasi_iovs,    iovs_len * sizeof(wasi_iovec_t));
    m3ApiCheckMem(nread,        sizeof(uvwasi_size_t));

#if defined(M3_COMPILER_MSVC)
    if (iovs_len > 32) m3ApiReturn(UVWASI_EINVAL);
    uvwasi_iovec_t  iovs[32];
#else
    if (iovs_len > 128) m3ApiReturn(UVWASI_EINVAL);
    uvwasi_iovec_t  iovs[iovs_len];
#endif
    uvwasi_size_t num_read;
    uvwasi_errno_t ret;

    for (uvwasi_size_t i = 0; i < iovs_len; ++i) {
        iovs[i].buf = m3ApiOffsetToPtr(m3ApiReadMem32(&wasi_iovs[i].buf));
        iovs[i].buf_len = m3ApiReadMem32(&wasi_iovs[i].buf_len);
        m3ApiCheckMem(iovs[i].buf,     iovs[i].buf_len);
        //fprintf(stderr, "> fd_read fd:%d iov%d.len:%d\n", fd, i, iovs[i].buf_len);
    }

    ret = uvwasi_fd_read(&uvwasi, fd, iovs, iovs_len, &num_read);

    WASI_TRACE("fd:%d | nread:%d", fd, num_read);

    m3ApiWriteMem32(nread, num_read);
    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_write)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArgMem   (wasi_iovec_t *       , wasi_iovs)
    m3ApiGetArg      (uvwasi_size_t        , iovs_len)
    m3ApiGetArgMem   (uvwasi_size_t *      , nwritten)

    m3ApiCheckMem(wasi_iovs,    iovs_len * sizeof(wasi_iovec_t));
    m3ApiCheckMem(nwritten,     sizeof(uvwasi_size_t));

#if defined(M3_COMPILER_MSVC)
    if (iovs_len > 32) m3ApiReturn(UVWASI_EINVAL);
    uvwasi_ciovec_t  iovs[32];
#else
    if (iovs_len > 128) m3ApiReturn(UVWASI_EINVAL);
    uvwasi_ciovec_t  iovs[iovs_len];
#endif
    uvwasi_size_t num_written;
    uvwasi_errno_t ret;

    for (uvwasi_size_t i = 0; i < iovs_len; ++i) {
        iovs[i].buf = m3ApiOffsetToPtr(m3ApiReadMem32(&wasi_iovs[i].buf));
        iovs[i].buf_len = m3ApiReadMem32(&wasi_iovs[i].buf_len);
        m3ApiCheckMem(iovs[i].buf,     iovs[i].buf_len);
    }

    ret = uvwasi_fd_write(&uvwasi, fd, iovs, iovs_len, &num_written);

    WASI_TRACE("fd:%d | nwritten:%d", fd, num_written);

    m3ApiWriteMem32(nwritten, num_written);
    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_pwrite)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArgMem   (wasi_iovec_t *       , wasi_iovs)
    m3ApiGetArg      (uvwasi_size_t        , iovs_len)
    m3ApiGetArg      (uvwasi_filesize_t    , offset)
    m3ApiGetArgMem   (uvwasi_size_t *      , nwritten)

    m3ApiCheckMem(wasi_iovs,    iovs_len * sizeof(wasi_iovec_t));
    m3ApiCheckMem(nwritten,     sizeof(uvwasi_size_t));

#if defined(M3_COMPILER_MSVC)
    if (iovs_len > 32) m3ApiReturn(UVWASI_EINVAL);
    uvwasi_ciovec_t  iovs[32];
#else
    if (iovs_len > 128) m3ApiReturn(UVWASI_EINVAL);
    uvwasi_ciovec_t  iovs[iovs_len];
#endif
    uvwasi_size_t num_written;
    uvwasi_errno_t ret;

    for (uvwasi_size_t i = 0; i < iovs_len; ++i) {
        iovs[i].buf = m3ApiOffsetToPtr(m3ApiReadMem32(&wasi_iovs[i].buf));
        iovs[i].buf_len = m3ApiReadMem32(&wasi_iovs[i].buf_len);
        m3ApiCheckMem(iovs[i].buf,     iovs[i].buf_len);
    }

    ret = uvwasi_fd_pwrite(&uvwasi, fd, iovs, iovs_len, offset, &num_written);

    WASI_TRACE("fd:%d | nwritten:%d", fd, num_written);

    m3ApiWriteMem32(nwritten, num_written);
    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_readdir)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArgMem   (void *               , buf)
    m3ApiGetArg      (uvwasi_size_t        , buf_len)
    m3ApiGetArg      (uvwasi_dircookie_t   , cookie)
    m3ApiGetArgMem   (uvwasi_size_t *      , bufused)

    m3ApiCheckMem(buf,      buf_len);
    m3ApiCheckMem(bufused,  sizeof(uvwasi_size_t));

    uvwasi_size_t uvbufused;
    uvwasi_errno_t ret = uvwasi_fd_readdir(&uvwasi, fd, buf, buf_len, cookie, &uvbufused);

    WASI_TRACE("fd:%d | bufused:%d", fd, uvbufused);

    m3ApiWriteMem32(bufused, uvbufused);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_advise)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArg      (uvwasi_filesize_t    , offset)
    m3ApiGetArg      (uvwasi_filesize_t    , length)
    m3ApiGetArg      (uvwasi_advice_t      , advice)

    uvwasi_errno_t ret = uvwasi_fd_advise(&uvwasi, fd, offset, length, advice);

    WASI_TRACE("fd:%d, offset:%" PRIu64 ", length:%" PRIu64 ", advice:%d", fd, offset, length, advice);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_allocate)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t          , fd)
    m3ApiGetArg      (uvwasi_filesize_t    , offset)
    m3ApiGetArg      (uvwasi_filesize_t    , length)

    uvwasi_errno_t ret = uvwasi_fd_allocate(&uvwasi, fd, offset, length);

    WASI_TRACE("fd:%d, offset:%" PRIu64 ", length:%" PRIu64, fd, offset, length);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_close)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t, fd)

    uvwasi_errno_t ret = uvwasi_fd_close(&uvwasi, fd);

    WASI_TRACE("fd:%d", fd);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_fd_datasync)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_fd_t, fd)

    uvwasi_errno_t ret = uvwasi_fd_datasync(&uvwasi, fd);

    WASI_TRACE("fd:%d", fd);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_random_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArgMem   (uint8_t *            , buf)
    m3ApiGetArg      (uvwasi_size_t        , buf_len)

    m3ApiCheckMem(buf, buf_len);

    uvwasi_errno_t ret = uvwasi_random_get(&uvwasi, buf, buf_len);

    WASI_TRACE("len:%d", buf_len);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_clock_res_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_clockid_t     , wasi_clk_id)
    m3ApiGetArgMem   (uvwasi_timestamp_t * , resolution)

    m3ApiCheckMem(resolution, sizeof(uvwasi_timestamp_t));

    uvwasi_timestamp_t t;
    uvwasi_errno_t ret = uvwasi_clock_res_get(&uvwasi, wasi_clk_id, &t);

    WASI_TRACE("clk_id:%d | res:%" PRIu64, wasi_clk_id, t);

    m3ApiWriteMem64(resolution, t);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_clock_time_get)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_clockid_t     , wasi_clk_id)
    m3ApiGetArg      (uvwasi_timestamp_t   , precision)
    m3ApiGetArgMem   (uvwasi_timestamp_t * , time)

    m3ApiCheckMem(time, sizeof(uvwasi_timestamp_t));

    uvwasi_timestamp_t t;
    uvwasi_errno_t ret = uvwasi_clock_time_get(&uvwasi, wasi_clk_id, precision, &t);

    WASI_TRACE("clk_id:%d | res:%" PRIu64, wasi_clk_id, t);

    m3ApiWriteMem64(time, t);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_poll_oneoff)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArgMem   (const uvwasi_subscription_t * , in)
    m3ApiGetArgMem   (uvwasi_event_t *              , out)
    m3ApiGetArg      (uvwasi_size_t                 , nsubscriptions)
    m3ApiGetArgMem   (uvwasi_size_t *               , nevents)

    m3ApiCheckMem(in,       nsubscriptions * sizeof(uvwasi_subscription_t));
    m3ApiCheckMem(out,      nsubscriptions * sizeof(uvwasi_event_t));
    m3ApiCheckMem(nevents,  sizeof(uvwasi_size_t));

    // TODO: unstable/snapshot_preview1 compatibility

    uvwasi_errno_t ret = uvwasi_poll_oneoff(&uvwasi, in, out, nsubscriptions, nevents);

    WASI_TRACE("nsubscriptions:%d | nevents:%d", nsubscriptions, *nevents);

    //TODO: m3ApiWriteMem

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_proc_exit)
{
    m3ApiGetArg      (uint32_t, code)

    m3_wasi_context_t* context = (m3_wasi_context_t*)(_ctx->userdata);

    if (context) {
        context->exit_code = code;
    }

    //TODO: fprintf(stderr, "proc_exit code:%d\n", code);

    m3ApiTrap(m3Err_trapExit);
}

m3ApiRawFunction(m3_wasi_generic_proc_raise)
{
    m3ApiReturnType  (uint32_t)
    m3ApiGetArg      (uvwasi_signal_t, sig)

    uvwasi_errno_t ret = uvwasi_proc_raise(&uvwasi, sig);

    WASI_TRACE("sig:%d", sig);

    m3ApiReturn(ret);
}

m3ApiRawFunction(m3_wasi_generic_sched_yield)
{
    m3ApiReturnType  (uint32_t)
    uvwasi_errno_t ret = uvwasi_sched_yield(&uvwasi);

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
    #define ENV_COUNT       9

    char* env[ENV_COUNT];
    env[0] = "TERM=xterm-256color";
    env[1] = "COLORTERM=truecolor";
    env[2] = "LANG=en_US.UTF-8";
    env[3] = "PWD=/";
    env[4] = "HOME=/";
    env[5] = "PATH=/";
    env[6] = "WASM3=1";
    env[7] = "WASM3_ARCH=" M3_ARCH;
    env[8] = NULL;

    #define PREOPENS_COUNT  2

    uvwasi_preopen_t preopens[PREOPENS_COUNT];
    preopens[0].mapped_path = "/";
    preopens[0].real_path = ".";
    preopens[1].mapped_path = "./";
    preopens[1].real_path = ".";

    uvwasi_options_t init_options;
    uvwasi_options_init(&init_options);
    init_options.argc = 0;      // runtime->argc is not initialized at this point, so we implement args_get directly
    init_options.envp = (const char **) env;
    init_options.preopenc = PREOPENS_COUNT;
    init_options.preopens = preopens;

    return m3_LinkWASIWithOptions(module, init_options);
}

M3Result  m3_LinkWASIWithOptions  (IM3Module module, uvwasi_options_t init_options)
{
    M3Result result = m3Err_none;

    if (!wasi_context) {
        wasi_context = (m3_wasi_context_t*)malloc(sizeof(m3_wasi_context_t));
        wasi_context->exit_code = 0;
        wasi_context->argc = 0;
        wasi_context->argv = 0;

        uvwasi_errno_t ret = uvwasi_init(&uvwasi, &init_options);

        if (ret != UVWASI_ESUCCESS) {
            return "uvwasi_init failed";
        }
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

#endif // d_m3HasUVWASI

