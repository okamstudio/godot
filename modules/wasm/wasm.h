/**************************************************************************/
/*  wasm.h                                                                */
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

#ifndef WASM_H
#define WASM_H

#include "core/io/stream_peer.h"
#include "core/object/ref_counted.h"
#include "core/os/memory.h"
#include "core/variant/variant_utility.h"

class WasmMemory : public StreamPeer {
	GDCLASS(WasmMemory, StreamPeer);
	friend class Wasm;

public:
	Dictionary inspect() const;
	Error grow(uint32_t pages);
	Ref<WasmMemory> seek(int p_pos);
	uint32_t get_position() const;

	Error get_data(uint8_t *buffer, int32_t bytes) override;
	Error get_partial_data(uint8_t *buffer, int32_t bytes, int32_t &received) override;
	Error put_data(const uint8_t *buffer, int32_t bytes) override;
	Error put_partial_data(const uint8_t *buffer, int bytes, int32_t &sent) override;
	int32_t get_available_bytes() const override;

protected:
	static void _bind_methods();

private:
	struct M3Runtime *runtime;
	uint32_t offset;
	WasmMemory() :
			runtime(nullptr), offset(0) {}
	WasmMemory(M3Runtime *_runtime) :
			runtime(_runtime), offset(0) {}
};

class Wasm : public RefCounted {
	GDCLASS(Wasm, RefCounted);

public:
	Wasm();
	~Wasm();

	Error compile(PackedByteArray bytecode);
	Error instantiate(const Dictionary import_map);
	Error load(PackedByteArray bytecode, const Dictionary import_map);
	Dictionary inspect() const;
	Variant function(String name, Array args) const;
	Variant global(String name) const;
	Ref<WasmMemory> get_memory() const;

protected:
	static void _bind_methods();

private:
	PackedByteArray code;
	List<Callable> callables;
	struct M3Environment *env;
	struct M3Runtime *runtime;
	struct M3Module *module;
	Ref<WasmMemory> memiface;
};

#endif // WASM_H
