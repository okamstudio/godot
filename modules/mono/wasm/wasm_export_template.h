#ifndef MONO_WASM_EXPORT_TEMPLATE_H
#define MONO_WASM_EXPORT_TEMPLATE_H

#include "core/object/object.h"
#include "core/io/resource.h"
#include "core/string/ustring.h"

class WasmExportTemplate : public Resource {
    GDCLASS(WasmExportTemplate, Resource);

protected:
    static void _bind_methods();

private:
    String dotnet_wasm_runtime_path;
    String wasm_helpers_path;
    bool enable_threading;
    bool enable_aot;
    int heap_size_mb;

public:
    void set_dotnet_wasm_runtime_path(const String &p_path);
    String get_dotnet_wasm_runtime_path() const;

    void set_wasm_helpers_path(const String &p_path);
    String get_wasm_helpers_path() const;

    void set_enable_threading(bool p_enable);
    bool get_enable_threading() const;

    void set_enable_aot(bool p_enable);
    bool get_enable_aot() const;

    void set_heap_size_mb(int p_size);
    int get_heap_size_mb() const;

    Error validate_configuration() const;
    Error generate_export_files(const String &p_export_path) const;

    WasmExportTemplate();
};

#endif // MONO_WASM_EXPORT_TEMPLATE_H
