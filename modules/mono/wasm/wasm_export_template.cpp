#include "wasm_export_template.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"

void WasmExportTemplate::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_dotnet_wasm_runtime_path", "path"), &WasmExportTemplate::set_dotnet_wasm_runtime_path);
    ClassDB::bind_method(D_METHOD("get_dotnet_wasm_runtime_path"), &WasmExportTemplate::get_dotnet_wasm_runtime_path);

    ClassDB::bind_method(D_METHOD("set_wasm_helpers_path", "path"), &WasmExportTemplate::set_wasm_helpers_path);
    ClassDB::bind_method(D_METHOD("get_wasm_helpers_path"), &WasmExportTemplate::get_wasm_helpers_path);

    ClassDB::bind_method(D_METHOD("set_enable_threading", "enable"), &WasmExportTemplate::set_enable_threading);
    ClassDB::bind_method(D_METHOD("get_enable_threading"), &WasmExportTemplate::get_enable_threading);

    ClassDB::bind_method(D_METHOD("set_enable_aot", "enable"), &WasmExportTemplate::set_enable_aot);
    ClassDB::bind_method(D_METHOD("get_enable_aot"), &WasmExportTemplate::get_enable_aot);

    ClassDB::bind_method(D_METHOD("set_heap_size_mb", "size"), &WasmExportTemplate::set_heap_size_mb);
    ClassDB::bind_method(D_METHOD("get_heap_size_mb"), &WasmExportTemplate::get_heap_size_mb);

    ADD_PROPERTY(PropertyInfo(Variant::STRING, "dotnet_wasm_runtime_path"), "set_dotnet_wasm_runtime_path", "get_dotnet_wasm_runtime_path");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "wasm_helpers_path"), "set_wasm_helpers_path", "get_wasm_helpers_path");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_threading"), "set_enable_threading", "get_enable_threading");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enable_aot"), "set_enable_aot", "get_enable_aot");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "heap_size_mb"), "set_heap_size_mb", "get_heap_size_mb");
}

void WasmExportTemplate::set_dotnet_wasm_runtime_path(const String &p_path) {
    dotnet_wasm_runtime_path = p_path;
}

String WasmExportTemplate::get_dotnet_wasm_runtime_path() const {
    return dotnet_wasm_runtime_path;
}

void WasmExportTemplate::set_wasm_helpers_path(const String &p_path) {
    wasm_helpers_path = p_path;
}

String WasmExportTemplate::get_wasm_helpers_path() const {
    return wasm_helpers_path;
}

void WasmExportTemplate::set_enable_threading(bool p_enable) {
    enable_threading = p_enable;
}

bool WasmExportTemplate::get_enable_threading() const {
    return enable_threading;
}

void WasmExportTemplate::set_enable_aot(bool p_enable) {
    enable_aot = p_enable;
}

bool WasmExportTemplate::get_enable_aot() const {
    return enable_aot;
}

void WasmExportTemplate::set_heap_size_mb(int p_size) {
    heap_size_mb = p_size;
}

int WasmExportTemplate::get_heap_size_mb() const {
    return heap_size_mb;
}

Error WasmExportTemplate::validate_configuration() const {
    if (dotnet_wasm_runtime_path.is_empty()) {
        ERR_PRINT("dotnet_wasm_runtime_path is not set");
        return ERR_UNCONFIGURED;
    }

    if (wasm_helpers_path.is_empty()) {
        ERR_PRINT("wasm_helpers_path is not set");
        return ERR_UNCONFIGURED;
    }

    if (heap_size_mb < 32) {
        ERR_PRINT("heap_size_mb must be at least 32MB");
        return ERR_INVALID_PARAMETER;
    }

    return OK;
}

Error WasmExportTemplate::generate_export_files(const String &p_export_path) const {
    Error err = validate_configuration();
    if (err != OK) {
        return err;
    }

    // Create export directory if it doesn't exist
    Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
    if (!dir->dir_exists(p_export_path)) {
        err = dir->make_dir_recursive(p_export_path);
        if (err != OK) {
            return err;
        }
    }

    // TODO: Generate the following files:
    // 1. index.html - Main entry point
    // 2. dotnet.js - .NET WASM runtime loader
    // 3. godot.js - Godot engine loader
    // 4. game.wasm - Compiled game code
    // 5. runtime.js - Runtime configuration

    return OK;
}

WasmExportTemplate::WasmExportTemplate() {
    enable_threading = false;
    enable_aot = true;
    heap_size_mb = 512;
}
