/**************************************************************************/
/*  resource_saver.h                                                      */
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

#include "core/io/resource.h"
#include "core/object/gdvirtual.gen.inc"

class ResourceFormatSaver : public RefCounted {
	GDCLASS(ResourceFormatSaver, RefCounted);

protected:
	static void _bind_methods();

	GDVIRTUAL3R(Error, _save, Ref<Resource>, String, uint32_t)
	GDVIRTUAL2R(Error, _set_uid, String, ResourceUID::ID)
	GDVIRTUAL1RC(bool, _recognize, Ref<Resource>)
	GDVIRTUAL1RC(Vector<String>, _get_recognized_extensions, Ref<Resource>)
	GDVIRTUAL2RC(bool, _recognize_path, Ref<Resource>, String)

public:
	virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0);
	virtual Error set_uid(const String &p_path, ResourceUID::ID p_uid);
	virtual bool recognize(const Ref<Resource> &p_resource) const;
	virtual void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const;
	virtual bool recognize_path(const Ref<Resource> &p_resource, const String &p_path) const;

	virtual ~ResourceFormatSaver() {}
};

typedef void (*ResourceSavedCallback)(Ref<Resource> p_resource, const String &p_path);
typedef ResourceUID::ID (*ResourceSaverGetResourceIDForPath)(const String &p_path, bool p_generate);

class ResourceSaver {
	enum {
		MAX_SAVERS = 64
	};

	static Ref<ResourceFormatSaver> saver[MAX_SAVERS];
	static int saver_count;
	static bool timestamp_on_save;
	static ResourceSavedCallback save_callback;
	static ResourceSaverGetResourceIDForPath save_get_id_for_path;

	static Ref<ResourceFormatSaver> _find_custom_resource_format_saver(const String &path);

public:
	enum SaverFlags {
		FLAG_NONE = 0,
		FLAG_RELATIVE_PATHS = 1,
		FLAG_BUNDLE_RESOURCES = 2,
		FLAG_CHANGE_PATH = 4,
		FLAG_OMIT_EDITOR_PROPERTIES = 8,
		FLAG_SAVE_BIG_ENDIAN = 16,
		FLAG_COMPRESS = 32,
		FLAG_REPLACE_SUBRESOURCE_PATHS = 64,
	};

	static Error save(const Ref<Resource> &p_resource, const String &p_path = "", uint32_t p_flags = (uint32_t)FLAG_NONE);
	static void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions);
	static void add_resource_format_saver(Ref<ResourceFormatSaver> p_format_saver, bool p_at_front = false);
	static void remove_resource_format_saver(Ref<ResourceFormatSaver> p_format_saver);

	static Error set_uid(const String &p_path, ResourceUID::ID p_uid);

	static void set_timestamp_on_save(bool p_timestamp) { timestamp_on_save = p_timestamp; }
	static bool get_timestamp_on_save() { return timestamp_on_save; }

	static ResourceUID::ID get_resource_id_for_path(const String &p_path, bool p_generate = false);

	static void set_save_callback(ResourceSavedCallback p_callback);
	static void set_get_resource_id_for_path(ResourceSaverGetResourceIDForPath p_callback);

	static bool add_custom_resource_format_saver(const String &script_path);
	static void add_custom_savers();
	static void remove_custom_savers();
};
