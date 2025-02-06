//
//  m3_api_tracer.c
//
//  Created by Volodymyr Shymanskyy on 02/18/20.
//  Copyright © 2020 Volodymyr Shymanskyy. All rights reserved.
//

#include "m3_api_tracer.h"

#include "m3_env.h"
#include "m3_exception.h"

#if defined(d_m3HasTracer)


static FILE* trace = NULL;

m3ApiRawFunction(m3_env_log_execution)
{
    m3ApiGetArg      (uint32_t, id)
    fprintf(trace, "exec;%d\n", id);
    m3ApiSuccess();
}

m3ApiRawFunction(m3_env_log_exec_enter)
{
    m3ApiGetArg      (uint32_t, id)
    m3ApiGetArg      (uint32_t, func)
    fprintf(trace, "enter;%d;%d\n", id, func);
    m3ApiSuccess();
}

m3ApiRawFunction(m3_env_log_exec_exit)
{
    m3ApiGetArg      (uint32_t, id)
    m3ApiGetArg      (uint32_t, func)
    fprintf(trace, "exit;%d;%d\n", id, func);
    m3ApiSuccess();
}

m3ApiRawFunction(m3_env_log_exec_loop)
{
    m3ApiGetArg      (uint32_t, id)
    fprintf(trace, "loop;%d\n", id);
    m3ApiSuccess();
}

m3ApiRawFunction(m3_env_load_ptr)
{
    m3ApiReturnType (uint32_t)
    m3ApiGetArg      (uint32_t, id)
    m3ApiGetArg      (uint32_t, align)
    m3ApiGetArg      (uint32_t, offset)
    m3ApiGetArg      (uint32_t, address)
    fprintf(trace, "load ptr;%d;%d;%d;%d\n", id, align, offset, address);
    m3ApiReturn(address);
}

m3ApiRawFunction(m3_env_store_ptr)
{
    m3ApiReturnType (uint32_t)
    m3ApiGetArg      (uint32_t, id)
    m3ApiGetArg      (uint32_t, align)
    m3ApiGetArg      (uint32_t, offset)
    m3ApiGetArg      (uint32_t, address)
    fprintf(trace, "store ptr;%d;%d;%d;%d\n", id, align, offset, address);
    m3ApiReturn(address);
}


#define d_m3TraceMemory(FUNC, NAME, TYPE, FMT)                \
m3ApiRawFunction(m3_env_##FUNC)                               \
{                                                             \
    m3ApiReturnType (TYPE)                                    \
    m3ApiGetArg      (uint32_t, id)                           \
    m3ApiGetArg      (TYPE,     val)                          \
    fprintf(trace, NAME ";%d;" FMT "\n", id, val);            \
    m3ApiReturn(val);                                         \
}

d_m3TraceMemory( load_val_i32,  "load i32", int32_t, "%" PRIi32)
d_m3TraceMemory(store_val_i32, "store i32", int32_t, "%" PRIi32)
d_m3TraceMemory( load_val_i64,  "load i64", int64_t, "%" PRIi64)
d_m3TraceMemory(store_val_i64, "store i64", int64_t, "%" PRIi64)
d_m3TraceMemory( load_val_f32,  "load f32", float,   "%" PRIf32)
d_m3TraceMemory(store_val_f32, "store f32", float,   "%" PRIf32)
d_m3TraceMemory( load_val_f64,  "load f64", double,  "%" PRIf64)
d_m3TraceMemory(store_val_f64, "store f64", double,  "%" PRIf64)


#define d_m3TraceLocal(FUNC, NAME, TYPE, FMT)                 \
m3ApiRawFunction(m3_env_##FUNC)                               \
{                                                             \
    m3ApiReturnType (TYPE)                                    \
    m3ApiGetArg      (uint32_t, id)                           \
    m3ApiGetArg      (uint32_t, local)                        \
    m3ApiGetArg      (TYPE,     val)                          \
    fprintf(trace, NAME ";%d;%d;" FMT "\n", id, local, val); \
    m3ApiReturn(val);                                         \
}


d_m3TraceLocal(get_i32, "get i32", int32_t, "%" PRIi32)
d_m3TraceLocal(set_i32, "set i32", int32_t, "%" PRIi32)
d_m3TraceLocal(get_i64, "get i64", int64_t, "%" PRIi64)
d_m3TraceLocal(set_i64, "set i64", int64_t, "%" PRIi64)
d_m3TraceLocal(get_f32, "get f32", float,   "%" PRIf32)
d_m3TraceLocal(set_f32, "set f32", float,   "%" PRIf32)
d_m3TraceLocal(get_f64, "get f64", double,  "%" PRIf64)
d_m3TraceLocal(set_f64, "set f64", double,  "%" PRIf64)


static
M3Result SuppressLookupFailure(M3Result i_result)
{
    if (i_result == m3Err_none) {
        // If any trace function is found in the module, open the trace file
        if (!trace) {
            trace = fopen ("wasm3_trace.csv","w");
        }
    } else if (i_result == m3Err_functionLookupFailed) {
        i_result = m3Err_none;
    }
    return i_result;
}


M3Result  m3_LinkTracer  (IM3Module module)
{
    M3Result result = m3Err_none;

    const char* env  = "env";

_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "log_execution",       "v(i)",     &m3_env_log_execution)));

_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "log_exec_enter",      "v(ii)",    &m3_env_log_exec_enter)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "log_exec_exit",       "v(ii)",    &m3_env_log_exec_exit)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "log_exec_loop",       "v(i)",     &m3_env_log_exec_loop)));

_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "load_ptr",            "i(iiii)",  &m3_env_load_ptr)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "store_ptr",           "i(iiii)",  &m3_env_store_ptr)));

_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "load_val_i32",        "i(ii)",    &m3_env_load_val_i32)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "load_val_i64",        "I(iI)",    &m3_env_load_val_i64)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "load_val_f32",        "f(if)",    &m3_env_load_val_f32)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "load_val_f64",        "F(iF)",    &m3_env_load_val_f64)));

_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "store_val_i32",       "i(ii)",    &m3_env_store_val_i32)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "store_val_i64",       "I(iI)",    &m3_env_store_val_i64)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "store_val_f32",       "f(if)",    &m3_env_store_val_f32)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "store_val_f64",       "F(iF)",    &m3_env_store_val_f64)));

_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "get_i32",             "i(iii)",   &m3_env_get_i32)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "get_i64",             "I(iiI)",   &m3_env_get_i64)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "get_f32",             "f(iif)",   &m3_env_get_f32)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "get_f64",             "F(iiF)",   &m3_env_get_f64)));

_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "set_i32",             "i(iii)",   &m3_env_set_i32)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "set_i64",             "I(iiI)",   &m3_env_set_i64)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "set_f32",             "f(iif)",   &m3_env_set_f32)));
_   (SuppressLookupFailure (m3_LinkRawFunction (module, env, "set_f64",             "F(iiF)",   &m3_env_set_f64)));

_catch:
    return result;
}

#endif // d_m3HasTracer

