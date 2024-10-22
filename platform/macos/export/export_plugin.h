/**************************************************************************/
/*  export_plugin.h                                                       */
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

#ifndef MACOS_EXPORT_PLUGIN_H
#define MACOS_EXPORT_PLUGIN_H

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/marshalls.h"
#include "core/io/resource_saver.h"
#include "core/os/os.h"
#include "core/version.h"
#include "editor/editor_settings.h"
#include "editor/export/editor_export.h"

#include <sys/stat.h>

// Optional environment variables for defining confidential information. If any
// of these is set, they will override the values set in the credentials file.
const String ENV_MAC_CODESIGN_CERT_FILE = "GODOT_MACOS_CODESIGN_CERTIFICATE_FILE";
const String ENV_MAC_CODESIGN_CERT_PASS = "GODOT_MACOS_CODESIGN_CERTIFICATE_PASSWORD";
const String ENV_MAC_CODESIGN_PROFILE = "GODOT_MACOS_CODESIGN_PROVISIONING_PROFILE";
const String ENV_MAC_NOTARIZATION_UUID = "GODOT_MACOS_NOTARIZATION_API_UUID";
const String ENV_MAC_NOTARIZATION_KEY = "GODOT_MACOS_NOTARIZATION_API_KEY";
const String ENV_MAC_NOTARIZATION_KEY_ID = "GODOT_MACOS_NOTARIZATION_API_KEY_ID";
const String ENV_MAC_NOTARIZATION_APPLE_ID = "GODOT_MACOS_NOTARIZATION_APPLE_ID_NAME";
const String ENV_MAC_NOTARIZATION_APPLE_PASS = "GODOT_MACOS_NOTARIZATION_APPLE_ID_PASSWORD";

class EditorExportPlatformMacOS : public EditorExportPlatform {
	GDCLASS(EditorExportPlatformMacOS, EditorExportPlatform);

	int version_code = 0;

	Ref<ImageTexture> logo;

	struct SSHCleanupCommand {
		String host;
		String port;
		Vector<String> ssh_args;
		String cmd_args;
		bool wait = false;

		SSHCleanupCommand() {}
		SSHCleanupCommand(const String &p_host, const String &p_port, const Vector<String> &p_ssh_arg, const String &p_cmd_args, bool p_wait = false) {
			host = p_host;
			port = p_port;
			ssh_args = p_ssh_arg;
			cmd_args = p_cmd_args;
			wait = p_wait;
		};
	};

	Ref<ImageTexture> run_icon;
	Ref<ImageTexture> stop_icon;

	Vector<SSHCleanupCommand> cleanup_commands;
	OS::ProcessID ssh_pid = 0;
	int menu_options = 0;

	void _fix_privacy_manifest(const Ref<EditorExportPreset> &p_preset, Vector<uint8_t> &plist);
	void _fix_plist(const Ref<EditorExportPreset> &p_preset, Vector<uint8_t> &plist, const String &p_binary);
	void _make_icon(const Ref<EditorExportPreset> &p_preset, const Ref<Image> &p_icon, Vector<uint8_t> &p_data);

	Error _notarize(const Ref<EditorExportPreset> &p_preset, const String &p_path);
	void _code_sign(const Ref<EditorExportPreset> &p_preset, const String &p_path, const String &p_ent_path, bool p_warn = true, bool p_set_id = false);
	void _code_sign_directory(const Ref<EditorExportPreset> &p_preset, const String &p_path, const String &p_ent_path, const String &p_helper_ent_path, bool p_should_error_on_non_code = true);
	Error _copy_and_sign_files(Ref<DirAccess> &dir_access, const String &p_src_path, const String &p_in_app_path,
			bool p_sign_enabled, const Ref<EditorExportPreset> &p_preset, const String &p_ent_path, const String &p_helper_ent_path,
			bool p_should_error_on_non_code_sign, bool p_sandbox);
	Error _export_macos_plugins_for(Ref<EditorExportPlugin> p_editor_export_plugin, const String &p_app_path_name,
			Ref<DirAccess> &dir_access, bool p_sign_enabled, const Ref<EditorExportPreset> &p_preset,
			const String &p_ent_path, const String &p_helper_ent_path, bool p_sandbox);
	Error _create_dmg(const String &p_dmg_path, const String &p_pkg_name, const String &p_app_path_name);
	Error _create_pkg(const Ref<EditorExportPreset> &p_preset, const String &p_pkg_path, const String &p_app_path_name);
	Error _export_debug_script(const Ref<EditorExportPreset> &p_preset, const String &p_app_name, const String &p_pkg_name, const String &p_path);

	bool use_codesign() const { return true; }

	bool is_package_name_valid(const String &p_package, String *r_error = nullptr) const {
		String pname = p_package;

		if (pname.length() == 0) {
			if (r_error) {
				*r_error = TTR("Identifier is missing.");
			}
			return false;
		}

		for (int i = 0; i < pname.length(); i++) {
			char32_t c = pname[i];
			if (!(is_ascii_alphanumeric_char(c) || c == '-' || c == '.')) {
				if (r_error) {
					*r_error = vformat(TTR("The character '%s' is not allowed in Identifier."), String::chr(c));
				}
				return false;
			}
		}

		return true;
	}
	bool is_shebang(const String &p_path) const;

protected:
	virtual void get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const override;
	virtual void get_export_options(List<ExportOption> *r_options) const override;
	virtual bool get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const override;
	virtual String get_export_option_warning(const EditorExportPreset *p_preset, const StringName &p_name) const override;

public:
	virtual String get_name() const override {
		return "macOS";
	}
	virtual String get_os_name() const override {
		return "macOS";
	}
	virtual Ref<Texture2D> get_logo() const override {
		return logo;
	}

	virtual bool is_executable(const String &p_path) const override;
	virtual List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override;
	virtual Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0) override;

	virtual bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug = false) const override;
	virtual bool has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const override;

	virtual void get_platform_features(List<String> *r_features) const override {
		r_features->push_back("pc");
		r_features->push_back("s3tc");
		r_features->push_back("macos");
	}

	virtual void resolve_platform_feature_priorities(const Ref<EditorExportPreset> &p_preset, HashSet<String> &p_features) override {
	}

	virtual Ref<Texture2D> get_run_icon() const override;
	virtual bool poll_export() override;
	virtual Ref<ImageTexture> get_option_icon(int p_index) const override;
	virtual int get_options_count() const override;
	virtual String get_option_label(int p_index) const override;
	virtual String get_option_tooltip(int p_index) const override;
	virtual Error run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) override;
	virtual void cleanup() override;

	EditorExportPlatformMacOS();
};

#endif // MACOS_EXPORT_PLUGIN_H
