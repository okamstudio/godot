/**************************************************************************/
/*  dedicated_server_export_plugin.h                                      */
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

#include "editor/export/editor_export_plugin.h"

class DedicatedServerExportPlugin : public EditorExportPlugin {
private:
	EditorExportPreset::FileExportMode current_export_mode;

	EditorExportPreset::FileExportMode _get_export_mode_for_path(const String &p_path);

protected:
	String get_name() const override { return "DedicatedServer"; }

	PackedStringArray _get_export_features(const Ref<EditorExportPlatform> &p_platform, bool p_debug) const override;
	uint64_t _get_customization_configuration_hash() const override;

	bool _begin_customize_scenes(const Ref<EditorExportPlatform> &p_platform, const Vector<String> &p_features) override;
	bool _begin_customize_resources(const Ref<EditorExportPlatform> &p_platform, const Vector<String> &p_features) override;

	Node *_customize_scene(Node *p_root, const String &p_path) override;
	Ref<Resource> _customize_resource(const Ref<Resource> &p_resource, const String &p_path) override;

	void _end_customize_scenes() override;
	void _end_customize_resources() override;
};
