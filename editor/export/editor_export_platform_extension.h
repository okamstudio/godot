/**************************************************************************/
/*  editor_export_platform_extension.h                                    */
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
#include "editor_export_preset.h"

class EditorExportPlatformExtension : public EditorExportPlatform {
	GDCLASS(EditorExportPlatformExtension, EditorExportPlatform);

	mutable String config_error;
	mutable bool config_missing_templates = false;

protected:
	static void _bind_methods();

public:
	virtual void get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const override;
	GDVIRTUAL1RC_REQUIRED(Vector<String>, _get_preset_features, Ref<EditorExportPreset>);

	virtual bool is_executable(const String &p_path) const override;
	GDVIRTUAL1RC(bool, _is_executable, const String &);

	virtual void get_export_options(List<ExportOption> *r_options) const override;
	GDVIRTUAL0RC(TypedArray<Dictionary>, _get_export_options);

	virtual bool should_update_export_options() override;
	GDVIRTUAL0R(bool, _should_update_export_options);

	virtual bool get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const override;
	GDVIRTUAL2RC(bool, _get_export_option_visibility, Ref<EditorExportPreset>, const String &);

	virtual String get_export_option_warning(const EditorExportPreset *p_preset, const StringName &p_name) const override;
	GDVIRTUAL2RC(String, _get_export_option_warning, Ref<EditorExportPreset>, const StringName &);

	virtual String get_os_name() const override;
	GDVIRTUAL0RC_REQUIRED(String, _get_os_name);

	virtual String get_name() const override;
	GDVIRTUAL0RC_REQUIRED(String, _get_name);

	virtual Ref<Texture2D> get_logo() const override;
	GDVIRTUAL0RC_REQUIRED(Ref<Texture2D>, _get_logo);

	virtual bool poll_export() override;
	GDVIRTUAL0R(bool, _poll_export);

	virtual int get_options_count() const override;
	GDVIRTUAL0RC(int, _get_options_count);

	virtual String get_options_tooltip() const override;
	GDVIRTUAL0RC(String, _get_options_tooltip);

	virtual Ref<ImageTexture> get_option_icon(int p_index) const override;
	GDVIRTUAL1RC(Ref<ImageTexture>, _get_option_icon, int);

	virtual String get_option_label(int p_device) const override;
	GDVIRTUAL1RC(String, _get_option_label, int);

	virtual String get_option_tooltip(int p_device) const override;
	GDVIRTUAL1RC(String, _get_option_tooltip, int);

	virtual String get_device_architecture(int p_device) const override;
	GDVIRTUAL1RC(String, _get_device_architecture, int);

	virtual void cleanup() override;
	GDVIRTUAL0(_cleanup);

	virtual Error run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) override;
	GDVIRTUAL3R(Error, _run, Ref<EditorExportPreset>, int, BitField<EditorExportPlatform::DebugFlags>);

	virtual Ref<Texture2D> get_run_icon() const override;
	GDVIRTUAL0RC(Ref<Texture2D>, _get_run_icon);

	void set_config_error(const String &p_error) const {
		config_error = p_error;
	}
	String get_config_error() const {
		return config_error;
	}

	void set_config_missing_templates(bool p_missing_templates) const {
		config_missing_templates = p_missing_templates;
	}
	bool get_config_missing_templates() const {
		return config_missing_templates;
	}

	virtual bool can_export(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug = false) const override;
	GDVIRTUAL2RC(bool, _can_export, Ref<EditorExportPreset>, bool);

	virtual bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug = false) const override;
	GDVIRTUAL2RC_REQUIRED(bool, _has_valid_export_configuration, Ref<EditorExportPreset>, bool);

	virtual bool has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const override;
	GDVIRTUAL1RC_REQUIRED(bool, _has_valid_project_configuration, Ref<EditorExportPreset>);

	virtual List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override;
	GDVIRTUAL1RC_REQUIRED(Vector<String>, _get_binary_extensions, Ref<EditorExportPreset>);

	virtual Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0) override;
	GDVIRTUAL4R_REQUIRED(Error, _export_project, Ref<EditorExportPreset>, bool, const String &, BitField<EditorExportPlatform::DebugFlags>);

	virtual Error export_pack(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0) override;
	GDVIRTUAL4R(Error, _export_pack, Ref<EditorExportPreset>, bool, const String &, BitField<EditorExportPlatform::DebugFlags>);

	virtual Error export_zip(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0) override;
	GDVIRTUAL4R(Error, _export_zip, Ref<EditorExportPreset>, bool, const String &, BitField<EditorExportPlatform::DebugFlags>);

	virtual Error export_pack_patch(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, const Vector<String> &p_patches = Vector<String>(), BitField<EditorExportPlatform::DebugFlags> p_flags = 0) override;
	GDVIRTUAL5R(Error, _export_pack_patch, Ref<EditorExportPreset>, bool, const String &, const Vector<String> &, BitField<EditorExportPlatform::DebugFlags>);

	virtual Error export_zip_patch(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, const Vector<String> &p_patches = Vector<String>(), BitField<EditorExportPlatform::DebugFlags> p_flags = 0) override;
	GDVIRTUAL5R(Error, _export_zip_patch, Ref<EditorExportPreset>, bool, const String &, const Vector<String> &, BitField<EditorExportPlatform::DebugFlags>);

	virtual void get_platform_features(List<String> *r_features) const override;
	GDVIRTUAL0RC_REQUIRED(Vector<String>, _get_platform_features);

	virtual String get_debug_protocol() const override;
	GDVIRTUAL0RC(String, _get_debug_protocol);

	EditorExportPlatformExtension();
	~EditorExportPlatformExtension();
};
