/**************************************************************************/
/*  editor_scene_exporter_gltf_settings.h                                 */
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

#ifdef TOOLS_ENABLED

#include "../gltf_document.h"

class EditorSceneExporterGLTFSettings : public RefCounted {
	GDCLASS(EditorSceneExporterGLTFSettings, RefCounted);
	List<PropertyInfo> _property_list;
	Ref<GLTFDocument> _document;
	HashMap<String, Ref<GLTFDocumentExtension>> _config_name_to_extension_map;

	String _copyright;
	double _bake_fps = 30.0;

protected:
	static void _bind_methods();
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;
	void _on_extension_property_list_changed();

	bool _set_extension_setting(const String &p_name_str, const Variant &p_value);
	bool _get_extension_setting(const String &p_name_str, Variant &r_ret) const;

public:
	void generate_property_list(Ref<GLTFDocument> p_document, Node *p_root = nullptr);

	String get_copyright() const;
	void set_copyright(const String &p_copyright);

	double get_bake_fps() const;
	void set_bake_fps(const double p_bake_fps);
};

#endif // TOOLS_ENABLED
