/**************************************************************************/
/*  wasm.cpp                                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "core/os/memory.h"

#include "wasm.h"
#include "wasm3.h"

#include "m3_env.h"

Wasm::Wasm() :
		module(nullptr) {
	env = m3_NewEnvironment();
	runtime = m3_NewRuntime(env, 10000, nullptr);
	memiface = Ref(memnew(WasmMemory));
	memiface->runtime = runtime;
}

Wasm::~Wasm() {
	m3_FreeRuntime(runtime);
	m3_FreeEnvironment(env);
}

Error Wasm::compile(PackedByteArray bytecode) {
	code = bytecode;
	M3Result result = m3_ParseModule(env, &module, code.ptr(), code.size());
	ERR_FAIL_COND_V_EDMSG(result, ERR_COMPILATION_FAILED, result);

	result = m3_LoadModule(runtime, module);
	if (result) {
		m3_FreeModule(module);
		module = nullptr;
		ERR_PRINT_ED(result);
		return ERR_COMPILATION_FAILED;
	}

	return OK;
}

inline Variant dict_safe_get(const Dictionary &d, String k, Variant e) {
	return d.has(k) && d[k].get_type() == e.get_type() ? d[k] : e;
}

const void *handler(IM3Runtime runtime, IM3ImportContext ctx, uint64_t *sp, void *mem) {
	Callable *callable = (Callable *)ctx->userdata;
	Array args;
	for (uint32_t i = 0; i < m3_GetArgCount(ctx->function); ++i) {
		switch (m3_GetArgType(ctx->function, i)) {
			case c_m3Type_none:
				ERR_PRINT_ED("Argument was type none");
				break;
			case c_m3Type_unknown:
				ERR_PRINT_ED("Argument was type unknown");
				break;
			case c_m3Type_i32:
				args.push_back(*((int32_t *)(sp + i)));
				break;
			case c_m3Type_i64:
				args.push_back(*((int64_t *)(sp + i)));
				break;
			case c_m3Type_f32:
				args.push_back(*((float *)(sp + i)));
				break;
			case c_m3Type_f64:
				args.push_back(*((double *)(sp + i)));
				break;
		}
	}

	Variant result = callable->callv(args);

	if (m3_GetRetCount(ctx->function) > 0) {
		switch (m3_GetRetType(ctx->function, 0)) {
			case c_m3Type_none:
				ERR_PRINT_ED("Return was type none");
				break;
			case c_m3Type_unknown:
				ERR_PRINT_ED("Return was type unknown");
				break;
			case c_m3Type_i32:
				*((int32_t *)(sp + 0)) = (int32_t)result;
				break;
			case c_m3Type_i64:
				*((int64_t *)(sp + 0)) = (int64_t)result;
				break;
			case c_m3Type_f32:
				*((float *)(sp + 0)) = (float)result;
				break;
			case c_m3Type_f64:
				*((double *)(sp + 0)) = (double)result;
				break;
		}
	}
	return m3Err_none;
}

Error Wasm::instantiate(const Dictionary import_map) {
	ERR_FAIL_COND_V_MSG(!module, Error::ERR_UNCONFIGURED, "No module loaded");
	const Dictionary &functions = dict_safe_get(import_map, "functions", Dictionary());
	for (const auto &key : functions.keys()) {
		String name = key;
		Vector<String> parts = name.split(".");
		Variant value = functions[key];
		if (value.is_array() && value.operator Array().size() == 2) {
			Array arr = value;
			Object *o = arr[0];
			if (!o || arr[1].get_type() != Variant::STRING) {
				ERR_PRINT_ED("Argument to input isn't that callable");
				continue;
			}
			callables.push_back(Callable(o->get_instance_id(), (String)arr[1]));
		} else if (value.get_type() == Variant::CALLABLE) {
			callables.push_back(value.operator Callable());
		} else {
			ERR_PRINT_ED("Argument to import doesn't look callable");
		}
		M3Result result = m3_LinkRawFunctionEx(module, parts[0].utf8().ptr(), parts[1].utf8().ptr(), nullptr, handler, callables.back());
		if (result) {
			ERR_PRINT_ED(result);
		}
	}
	return OK;
}

Error Wasm::load(PackedByteArray bytecode, const Dictionary import_map) {
	Error err = compile(bytecode);
	if (err != OK)
		return err;
	return instantiate(import_map);
}

Dictionary Wasm::inspect() const {
	Dictionary result;

	if (!module)
		return result;

	Dictionary memory;
	Dictionary import_functions, export_globals, export_functions;

	for (uint32_t func_num = 0; func_num < module->numFunctions; ++func_num) {
		Array args;
		for (uint32_t i = 0; i < m3_GetArgCount(&module->functions[func_num]); ++i) {
			switch (m3_GetArgType(&module->functions[func_num], i)) {
				case c_m3Type_none:
					ERR_PRINT_ED("Argument was type none");
					break;
				case c_m3Type_unknown:
					ERR_PRINT_ED("Argument was type unknown");
					break;
				case c_m3Type_i32:
					args.push_back(Variant::INT);
					break;
				case c_m3Type_i64:
					args.push_back(Variant::INT);
					break;
				case c_m3Type_f32:
					args.push_back(Variant::FLOAT);
					break;
				case c_m3Type_f64:
					args.push_back(Variant::FLOAT);
					break;
			}
		}

		Array ret;
		for (uint32_t i = 0; i < m3_GetRetCount(&module->functions[func_num]); ++i) {
			switch (m3_GetRetType(&module->functions[func_num], i)) {
				case c_m3Type_none:
					ERR_PRINT_ED("Argument was type none");
					break;
				case c_m3Type_unknown:
					ERR_PRINT_ED("Argument was type unknown");
					break;
				case c_m3Type_i32:
					ret.push_back(Variant::INT);
					break;
				case c_m3Type_i64:
					ret.push_back(Variant::INT);
					break;
				case c_m3Type_f32:
					ret.push_back(Variant::FLOAT);
					break;
				case c_m3Type_f64:
					ret.push_back(Variant::FLOAT);
					break;
			}
		}

		Array tuple;
		tuple.push_back(args);
		tuple.push_back(ret);

		export_functions[m3_GetFunctionName(&module->functions[func_num])] = tuple;
	}

	memory["min"] = 0;
	memory["max"] = m3_GetMemorySize(runtime);

	result["memory"] = memory;
	result["import_functions"] = import_functions;
	result["export_globals"] = export_globals;
	result["export_functions"] = export_functions;

	return result;
}

Variant Wasm::function(String name, Array args) const {
	Array ret;
	IM3Function func;
	M3Result result = m3_FindFunction(&func, runtime, name.ascii());
	ERR_FAIL_COND_V_EDMSG(result, Variant(), result);

	uint32_t arg_count = m3_GetArgCount(func);
	uint32_t ret_count = m3_GetRetCount(func);
	if (static_cast<uint32_t>(args.size()) < arg_count) {
		return "not enough arguments";
	} else if (static_cast<uint32_t>(args.size()) > arg_count) {
		return "too many arguments";
	}

	const void **ptrs = (const void **)memalloc(sizeof(void *) * arg_count);
	uint8_t *data = (uint8_t *)memalloc(8 * arg_count);
	uint8_t *write = data;

	for (uint32_t i = 0; i < arg_count; ++i) {
		ptrs[i] = write;
		switch (m3_GetArgType(func, i)) {
			case c_m3Type_none:
				ERR_FAIL_V_EDMSG(Variant(), "Argument had none as argument type");
			case c_m3Type_unknown:
				ERR_FAIL_V_EDMSG(Variant(), "Argument had unknown as argument type");
			case c_m3Type_i32: {
				int32_t vi32 = args[i];
				memcpy(write, &vi32, sizeof(i32));
				write += sizeof(vi32);
				break;
			}
			case c_m3Type_i64: {
				int64_t vi64 = args[i];
				memcpy(write, &vi64, sizeof(i64));
				write += sizeof(vi64);
				break;
			}
			case c_m3Type_f32: {
				float vf32 = args[i];
				memcpy(write, &vf32, sizeof(f32));
				write += sizeof(vf32);
				break;
			}
			case c_m3Type_f64: {
				double vf64 = args[i];
				memcpy(write, &vf64, sizeof(f64));
				write += sizeof(vf64);
				break;
			}
		}
	}

	result = m3_Call(func, arg_count, ptrs);
	if (result)
		goto fail;

	for (uint32_t i = 0; i < ret_count; i++) {
		ptrs[i] = data + i * sizeof(int64_t);
	}

	result = m3_GetResults(func, ret_count, ptrs);
	if (result)
		goto fail;

	for (uint32_t i = 0; i < ret_count; ++i) {
		switch (m3_GetRetType(func, i)) {
			case c_m3Type_i32:
				ret.push_back(*(i32 *)ptrs[i]);
				break;
			case c_m3Type_i64:
				ret.push_back(*(i64 *)ptrs[i]);
				break;
			case c_m3Type_f32:
				ret.push_back(*(f32 *)ptrs[i]);
				break;
			case c_m3Type_f64:
				ret.push_back(*(f64 *)ptrs[i]);
				break;
			default:
				ERR_PRINT_ED("Return value had unknown type");
		}
	}

	memfree(ptrs);
	memfree(data);

	if (ret_count == 0)
		return Variant();
	else if (ret_count == 1)
		return ret[0];
	else
		return ret;
fail:
	memfree(ptrs);
	memfree(data);
	ERR_FAIL_V_EDMSG(Variant(), result);
}

Variant Wasm::global(String name) const {
	ERR_FAIL_COND_V_EDMSG(runtime->modules == nullptr, Variant(), "No modules loaded");
	IM3Global g = m3_FindGlobal(runtime->modules, name.ascii());

	M3TaggedValue tagged;
	M3Result err = m3_GetGlobal(g, &tagged);
	if (err)
		return Variant();

	switch (tagged.type) {
		case c_m3Type_i32:
			return tagged.value.i32;
		case c_m3Type_i64:
			return tagged.value.i64;
		case c_m3Type_f32:
			return tagged.value.f32;
		case c_m3Type_f64:
			return tagged.value.f64;
		default:
			return Variant();
	}
}

Ref<WasmMemory> Wasm::get_memory() const {
	return memiface;
}

void Wasm::_bind_methods() {
	ClassDB::bind_method(D_METHOD("compile", "bytecode"), &Wasm::compile);
	ClassDB::bind_method(D_METHOD("instantiate", "import_map"), &Wasm::instantiate);
	ClassDB::bind_method(D_METHOD("load", "bytecode", "import_map"), &Wasm::load);
	ClassDB::bind_method(D_METHOD("inspect"), &Wasm::inspect);
	ClassDB::bind_method(D_METHOD("global", "name"), &Wasm::global);
	ClassDB::bind_method(D_METHOD("function", "name", "args"), &Wasm::function);
	ClassDB::bind_method(D_METHOD("get_memory"), &Wasm::get_memory);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "memory"), "", "get_memory");
}

/////////////////////

Error WasmMemory::get_data(uint8_t *buffer, int32_t bytes) {
	ERR_FAIL_COND_V_EDMSG(!runtime, Error::ERR_UNCONFIGURED, "No Memory Attached");
	uint32_t size;
	uint8_t *mem = m3_GetMemory(runtime, &size, 0);
	ERR_FAIL_INDEX_V_EDMSG(offset, size, Error::ERR_PARAMETER_RANGE_ERROR, "Access outside memory");
	ERR_FAIL_INDEX_V_EDMSG(offset + bytes - 1, size, Error::ERR_PARAMETER_RANGE_ERROR, "Access outside memory");

	memcpy(buffer, mem + offset, bytes);
	offset += bytes;
	return OK;
}

Error WasmMemory::get_partial_data(uint8_t *buffer, int32_t bytes, int32_t &received) {
	ERR_FAIL_COND_V_EDMSG(!runtime, Error::ERR_UNCONFIGURED, "No Memory Attached");
	uint32_t size;
	uint8_t *mem = m3_GetMemory(runtime, &size, 0);
	ERR_FAIL_INDEX_V_EDMSG(offset, size, Error::ERR_PARAMETER_RANGE_ERROR, "Access outside memory");
	ERR_FAIL_INDEX_V_EDMSG(offset + bytes - 1, size, Error::ERR_PARAMETER_RANGE_ERROR, "Access outside memory");

	memcpy(buffer, mem + offset, bytes);
	offset += bytes;
	received = bytes;
	return OK;
}

Error WasmMemory::put_data(const uint8_t *buffer, int32_t bytes) {
	ERR_FAIL_COND_V_EDMSG(!runtime, Error::ERR_UNCONFIGURED, "No Memory Attached");
	uint32_t size;
	uint8_t *mem = m3_GetMemory(runtime, &size, 0);
	ERR_FAIL_INDEX_V_EDMSG(offset, size, Error::ERR_PARAMETER_RANGE_ERROR, "Access outside memory");
	ERR_FAIL_INDEX_V_EDMSG(offset + bytes - 1, size, Error::ERR_PARAMETER_RANGE_ERROR, "Access outside memory");

	memcpy(mem + offset, buffer, bytes);
	offset += bytes;
	return OK;
}

Error WasmMemory::put_partial_data(const uint8_t *buffer, int bytes, int32_t &sent) {
	ERR_FAIL_COND_V_EDMSG(!runtime, Error::ERR_UNCONFIGURED, "No Memory Attached");
	uint32_t size;
	uint8_t *mem = m3_GetMemory(runtime, &size, 0);
	ERR_FAIL_INDEX_V_EDMSG(offset, size, Error::ERR_PARAMETER_RANGE_ERROR, "Access outside memory");
	ERR_FAIL_INDEX_V_EDMSG(offset + bytes - 1, size, Error::ERR_PARAMETER_RANGE_ERROR, "Access outside memory");

	memcpy(mem + offset, buffer, bytes);
	offset += bytes;
	sent = bytes;
	return OK;
}

Ref<WasmMemory> WasmMemory::seek(int p_pos) {
	Ref<WasmMemory> ref = Ref<WasmMemory>(this);
	ERR_FAIL_COND_V_MSG(p_pos < 0, ref, "Invalid memory position");
	offset = p_pos;
	return ref;
}

Error WasmMemory::grow(uint32_t pages) {
	ERR_FAIL_COND_V_EDMSG(!runtime, Error::ERR_UNCONFIGURED, "No Memory Attached");
	IM3Memory memory = &runtime->memory;
	if (pages >= 0) {
		uint32_t requiredPages = memory->numPages + pages;
		M3Result r = ResizeMemory(runtime, requiredPages);
		ERR_FAIL_COND_V_EDMSG(r, Error::ERR_OUT_OF_MEMORY, r);
	}
	return OK;
}

Dictionary WasmMemory::inspect() const {
	Dictionary result;

	return result;
}

uint32_t WasmMemory::get_position() const {
	return offset;
}

int32_t WasmMemory::get_available_bytes() const {
	return 0;
}

void WasmMemory::_bind_methods() {
	ClassDB::bind_method(D_METHOD("inspect"), &WasmMemory::inspect);
	ClassDB::bind_method(D_METHOD("grow", "pages"), &WasmMemory::grow);
	ClassDB::bind_method(D_METHOD("seek", "p_pos"), &WasmMemory::seek);
	ClassDB::bind_method(D_METHOD("get_position"), &WasmMemory::get_position);
}
