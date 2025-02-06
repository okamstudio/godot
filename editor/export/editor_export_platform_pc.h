/**************************************************************************/
/*  editor_export_platform_pc.h                                           */
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

#include "editor_export_platform.h"

class EditorExportPlatformPC : public EditorExportPlatform {
	GDCLASS(EditorExportPlatformPC, EditorExportPlatform);

private:
	Ref<ImageTexture> logo;
	String name;
	String os_name;

	int chmod_flags = -1;

public:
	virtual void get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const override;

	virtual void get_export_options(List<ExportOption> *r_options) const override;

	virtual String get_name() const override;
	virtual String get_os_name() const override;
	virtual Ref<Texture2D> get_logo() const override;

	virtual bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug = false) const override;
	virtual bool has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const override;
	virtual Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0) override;
	virtual Error sign_shared_object(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path);
	virtual String get_template_file_name(const String &p_target, const String &p_arch) const = 0;

	virtual Error prepare_template(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags);
	virtual Error modify_template(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) { return OK; }
	virtual Error export_project_data(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags);

	void set_name(const String &p_name);
	void set_os_name(const String &p_name);

	void set_logo(const Ref<Texture2D> &p_logo);

	void add_platform_feature(const String &p_feature);
	virtual void get_platform_features(List<String> *r_features) const override;
	virtual void resolve_platform_feature_priorities(const Ref<EditorExportPreset> &p_preset, HashSet<String> &p_features) override;

	int get_chmod_flags() const;
	void set_chmod_flags(int p_flags);

	virtual Error fixup_embedded_pck(const String &p_path, int64_t p_embedded_start, int64_t p_embedded_size) {
		return Error::OK;
	}
};
