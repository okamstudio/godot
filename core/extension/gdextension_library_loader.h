/**************************************************************************/
/*  gdextension_library_loader.h                                          */
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

#pragma once

#include <functional>

#include "core/extension/gdextension_loader.h"
#include "core/io/config_file.h"
#include "core/os/shared_object.h"

class GDExtensionLibraryLoader : public GDExtensionLoader {
	friend class GDExtensionManager;
	friend class GDExtension;

private:
	String resource_path;

	void *library = nullptr; // pointer if valid.
	String library_path;
	String entry_symbol;

	bool is_static_library = false;

#ifdef TOOLS_ENABLED
	bool is_reloadable = false;
#endif

	Vector<SharedObject> library_dependencies;

	HashMap<String, String> class_icon_paths;

#ifdef TOOLS_ENABLED
	uint64_t resource_last_modified_time = 0;
	uint64_t library_last_modified_time = 0;

	void update_last_modified_time(uint64_t p_resource_last_modified_time, uint64_t p_library_last_modified_time) {
		resource_last_modified_time = p_resource_last_modified_time;
		library_last_modified_time = p_library_last_modified_time;
	}
#endif

public:
	static String find_extension_library(const String &p_path, Ref<ConfigFile> p_config, std::function<bool(String)> p_has_feature, PackedStringArray *r_tags = nullptr);
	static Vector<SharedObject> find_extension_dependencies(const String &p_path, Ref<ConfigFile> p_config, std::function<bool(String)> p_has_feature);

	virtual Error open_library(const String &p_path) override;
	virtual Error initialize(GDExtensionInterfaceGetProcAddress p_get_proc_address, const Ref<GDExtension> &p_extension, GDExtensionInitialization *r_initialization) override;
	virtual void close_library() override;
	virtual bool is_library_open() const override;
	virtual bool has_library_changed() const override;
	virtual bool library_exists() const override;

	Error parse_gdextension_file(const String &p_path);
};
