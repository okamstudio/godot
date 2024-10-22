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

#ifndef ANDROID_EXPORT_PLUGIN_H
#define ANDROID_EXPORT_PLUGIN_H

#ifndef DISABLE_DEPRECATED
#include "godot_plugin_config.h"
#endif // DISABLE_DEPRECATED

#include "core/io/image.h"
#include "core/io/zip_io.h"
#include "core/os/os.h"
#include "editor/export/editor_export_platform.h"

// Optional environment variables for defining confidential information. If any
// of these is set, they will override the values set in the credentials file.
const String ENV_ANDROID_KEYSTORE_DEBUG_PATH = "GODOT_ANDROID_KEYSTORE_DEBUG_PATH";
const String ENV_ANDROID_KEYSTORE_DEBUG_USER = "GODOT_ANDROID_KEYSTORE_DEBUG_USER";
const String ENV_ANDROID_KEYSTORE_DEBUG_PASS = "GODOT_ANDROID_KEYSTORE_DEBUG_PASSWORD";
const String ENV_ANDROID_KEYSTORE_RELEASE_PATH = "GODOT_ANDROID_KEYSTORE_RELEASE_PATH";
const String ENV_ANDROID_KEYSTORE_RELEASE_USER = "GODOT_ANDROID_KEYSTORE_RELEASE_USER";
const String ENV_ANDROID_KEYSTORE_RELEASE_PASS = "GODOT_ANDROID_KEYSTORE_RELEASE_PASSWORD";

const String DEFAULT_ANDROID_KEYSTORE_DEBUG_USER = "androiddebugkey";
const String DEFAULT_ANDROID_KEYSTORE_DEBUG_PASSWORD = "android";

struct LauncherIcon {
	const char *export_path;
	int dimensions = 0;
};

class EditorExportPlatformAndroid : public EditorExportPlatform {
	GDCLASS(EditorExportPlatformAndroid, EditorExportPlatform);

	Ref<ImageTexture> logo;
	Ref<ImageTexture> run_icon;

	struct Device {
		String id;
		String name;
		String description;
		int api_level = 0;
		String architecture;
	};

	struct APKExportData {
		zipFile apk;
		EditorProgress *ep = nullptr;
	};

#ifndef DISABLE_DEPRECATED
	mutable Vector<PluginConfigAndroid> android_plugins;
	mutable SafeFlag android_plugins_changed;
	Mutex android_plugins_lock;
#endif // DISABLE_DEPRECATED
	String last_plugin_names;
	uint64_t last_gradle_build_time = 0;
	String last_gradle_build_dir;

	Vector<Device> devices;
	SafeFlag devices_changed;
	Mutex device_lock;
#ifndef ANDROID_ENABLED
	Thread check_for_changes_thread;
	SafeFlag quit_request;
	SafeFlag has_runnable_preset;

	static void _check_for_changes_poll_thread(void *ud);
	void _update_preset_status();
#endif

	String get_project_name(const String &p_name) const;

	String get_package_name(const String &p_package) const;

	String get_valid_basename() const;

	String get_assets_directory(const Ref<EditorExportPreset> &p_preset, int p_export_format) const;

	bool is_package_name_valid(const String &p_package, String *r_error = nullptr) const;
	bool is_project_name_valid() const;

	static bool _should_compress_asset(const String &p_path, const Vector<uint8_t> &p_data);

	static zip_fileinfo get_zip_fileinfo();

	struct ABI {
		String abi;
		String arch;

		bool operator==(const ABI &p_a) const {
			return p_a.abi == abi;
		}

		ABI(const String &p_abi, const String &p_arch) {
			abi = p_abi;
			arch = p_arch;
		}
		ABI() {}
	};

	static Vector<ABI> get_abis();

#ifndef DISABLE_DEPRECATED
	/// List the gdap files in the directory specified by the p_path parameter.
	static Vector<String> list_gdap_files(const String &p_path);

	static Vector<PluginConfigAndroid> get_plugins();

	static Vector<PluginConfigAndroid> get_enabled_plugins(const Ref<EditorExportPreset> &p_presets);
#endif // DISABLE_DEPRECATED

	static Error store_in_apk(APKExportData *ed, const String &p_path, const Vector<uint8_t> &p_data, int compression_method = Z_DEFLATED);

	static Error save_apk_so(void *p_userdata, const SharedObject &p_so);

	static Error save_apk_file(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total, const Vector<String> &p_enc_in_filters, const Vector<String> &p_enc_ex_filters, const Vector<uint8_t> &p_key);

	static Error ignore_apk_file(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total, const Vector<String> &p_enc_in_filters, const Vector<String> &p_enc_ex_filters, const Vector<uint8_t> &p_key);

	static Error copy_gradle_so(void *p_userdata, const SharedObject &p_so);

	bool _has_read_write_storage_permission(const Vector<String> &p_permissions);

	bool _has_manage_external_storage_permission(const Vector<String> &p_permissions);

	void _get_permissions(const Ref<EditorExportPreset> &p_preset, bool p_give_internet, Vector<String> &r_permissions);

	void _write_tmp_manifest(const Ref<EditorExportPreset> &p_preset, bool p_give_internet, bool p_debug);

	void _fix_manifest(const Ref<EditorExportPreset> &p_preset, Vector<uint8_t> &p_manifest, bool p_give_internet);

	static String _get_keystore_path(const Ref<EditorExportPreset> &p_preset, bool p_debug);

	static String _parse_string(const uint8_t *p_bytes, bool p_utf8);

	void _fix_resources(const Ref<EditorExportPreset> &p_preset, Vector<uint8_t> &r_manifest);

	void _load_image_data(const Ref<Image> &p_splash_image, Vector<uint8_t> &p_data);

	void _process_launcher_icons(const String &p_file_name, const Ref<Image> &p_source_image, int dimension, Vector<uint8_t> &p_data);

	void load_icon_refs(const Ref<EditorExportPreset> &p_preset, Ref<Image> &icon, Ref<Image> &foreground, Ref<Image> &background, Ref<Image> &monochrome);

	void _copy_icons_to_gradle_project(const Ref<EditorExportPreset> &p_preset,
			const Ref<Image> &p_main_image,
			const Ref<Image> &p_foreground,
			const Ref<Image> &p_background,
			const Ref<Image> &p_monochrome);

	static void _create_editor_debug_keystore_if_needed();

	static Vector<ABI> get_enabled_abis(const Ref<EditorExportPreset> &p_preset);

	static bool _uses_vulkan();

protected:
	void _notification(int p_what);

public:
	typedef Error (*EditorExportSaveFunction)(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total, const Vector<String> &p_enc_in_filters, const Vector<String> &p_enc_ex_filters, const Vector<uint8_t> &p_key);

	virtual void get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const override;

	virtual void get_export_options(List<ExportOption> *r_options) const override;

	virtual bool get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const override;

	virtual String get_export_option_warning(const EditorExportPreset *p_preset, const StringName &p_name) const override;

	virtual String get_name() const override;

	virtual String get_os_name() const override;

	virtual Ref<Texture2D> get_logo() const override;

	virtual bool should_update_export_options() override;

	virtual bool poll_export() override;

	virtual int get_options_count() const override;

	virtual String get_options_tooltip() const override;

	virtual String get_option_label(int p_index) const override;

	virtual String get_option_tooltip(int p_index) const override;

	virtual String get_device_architecture(int p_index) const override;

	virtual Error run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) override;

	virtual Ref<Texture2D> get_run_icon() const override;

	static String get_adb_path();

	static String get_apksigner_path(int p_target_sdk = -1, bool p_check_executes = false);

	static String get_java_path();

	static String get_keytool_path();

	virtual bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug = false) const override;
	virtual bool has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const override;
	static bool has_valid_username_and_password(const Ref<EditorExportPreset> &p_preset, String &r_error);

	virtual List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const override;

	String _get_plugins_names(const Ref<EditorExportPreset> &p_preset) const;

	String _resolve_export_plugin_android_library_path(const String &p_android_library_path) const;

	bool _is_clean_build_required(const Ref<EditorExportPreset> &p_preset);

	String get_apk_expansion_fullpath(const Ref<EditorExportPreset> &p_preset, const String &p_path);

	Error save_apk_expansion_file(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path);

	void get_command_line_flags(const Ref<EditorExportPreset> &p_preset, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags, Vector<uint8_t> &r_command_line_flags);

	Error sign_apk(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &export_path, EditorProgress &ep);

	void _clear_assets_directory(const Ref<EditorExportPreset> &p_preset);

	void _remove_copied_libs(String p_gdextension_libs_path);

	static String join_list(const List<String> &p_parts, const String &p_separator);
	static String join_abis(const Vector<ABI> &p_parts, const String &p_separator, bool p_use_arch);

	virtual Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0) override;

	Error export_project_helper(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, int export_format, bool should_sign, BitField<EditorExportPlatform::DebugFlags> p_flags);

	virtual void get_platform_features(List<String> *r_features) const override;

	virtual void resolve_platform_feature_priorities(const Ref<EditorExportPreset> &p_preset, HashSet<String> &p_features) override;

	EditorExportPlatformAndroid();

	~EditorExportPlatformAndroid();
};

#endif // ANDROID_EXPORT_PLUGIN_H
