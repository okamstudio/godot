#ifndef MONO_WASM_RUNTIME_H
#define MONO_WASM_RUNTIME_H

#include "core/object/object.h"
#include "core/templates/hash_map.h"
#include "core/string/ustring.h"

class MonoWasmRuntime {
    GDCLASS(MonoWasmRuntime, Object);

protected:
    static void _bind_methods();
    static MonoWasmRuntime *singleton;

public:
    static MonoWasmRuntime *get_singleton();

    Error initialize_wasm_runtime();
    Error load_assembly(const String &p_assembly_path);
    Error create_wasm_delegate(const String &p_type_name, const String &p_method_name);
    
    // WebAssembly specific methods
    Error setup_wasm_imports();
    Error setup_wasm_exports();
    Error setup_dotnet_runtime();
    
    // Asset handling
    Error preload_assembly(const String &p_assembly_path);
    Error cache_assembly(const String &p_assembly_name, const Vector<uint8_t> &p_assembly_data);
    
    MonoWasmRuntime();
    ~MonoWasmRuntime();
};

#endif // MONO_WASM_RUNTIME_H
