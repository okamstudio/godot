/**************************************************************************/
/*  editor_export_platform.h                                              */
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

#ifndef EDITOR_EXPORT_PLATFORM_H
#define EDITOR_EXPORT_PLATFORM_H

class EditorFileSystemDirectory;
struct EditorProgress;

#include "core/io/dir_access.h"
#include "core/io/zip_io.h"
#include "core/os/shared_object.h"
#include "editor_export_preset.h"
#include "scene/gui/rich_text_label.h"
#include "scene/main/node.h"
#include "scene/resources/image_texture.h"

class EditorExportPlugin;

const String ENV_SCRIPT_ENCRYPTION_KEY = "GODOT_SCRIPT_ENCRYPTION_KEY";

class EditorExportPlatform : public RefCounted {
	GDCLASS(EditorExportPlatform, RefCounted);

protected:
	static void _bind_methods();

public:
	typedef Error (*EditorExportSaveFunction)(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total, const Vector<String> &p_enc_in_filters, const Vector<String> &p_enc_ex_filters, const Vector<uint8_t> &p_key, uint64_t p_seed);
	typedef Error (*EditorExportRemoveFunction)(void *p_userdata, const String &p_path);
	typedef Error (*EditorExportSaveSharedObject)(void *p_userdata, const SharedObject &p_so);

	enum DebugFlags {
		DEBUG_FLAG_DUMB_CLIENT = 1,
		DEBUG_FLAG_REMOTE_DEBUG = 2,
		DEBUG_FLAG_REMOTE_DEBUG_LOCALHOST = 4,
		DEBUG_FLAG_VIEW_COLLISIONS = 8,
		DEBUG_FLAG_VIEW_NAVIGATION = 16,
	};

	enum ExportMessageType {
		EXPORT_MESSAGE_NONE,
		EXPORT_MESSAGE_INFO,
		EXPORT_MESSAGE_WARNING,
		EXPORT_MESSAGE_ERROR,
	};

	struct ExportMessage {
		ExportMessageType msg_type;
		String category;
		String text;
	};

private:
	struct SavedData {
		uint64_t ofs = 0;
		uint64_t size = 0;
		bool encrypted = false;
		bool removal = false;
		Vector<uint8_t> md5;
		CharString path_utf8;

		bool operator<(const SavedData &p_data) const {
			return path_utf8 < p_data.path_utf8;
		}
	};

	struct PackData {
		Ref<FileAccess> f;
		Vector<SavedData> file_ofs;
		EditorProgress *ep = nullptr;
		Vector<SharedObject> *so_files = nullptr;
	};

	struct ZipData {
		void *zip = nullptr;
		EditorProgress *ep = nullptr;
		Vector<SharedObject> *so_files = nullptr;
		int file_count = 0;
	};

	Vector<ExportMessage> messages;

	void _export_find_resources(EditorFileSystemDirectory *p_dir, HashSet<String> &p_paths);
	void _export_find_customized_resources(const Ref<EditorExportPreset> &p_preset, EditorFileSystemDirectory *p_dir, EditorExportPreset::FileExportMode p_mode, HashSet<String> &p_paths);
	void _export_find_dependencies(const String &p_path, HashSet<String> &p_paths);

	static bool _check_hash(const uint8_t *p_hash, const Vector<uint8_t> &p_data);

	static Error _save_pack_file(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total, const Vector<String> &p_enc_in_filters, const Vector<String> &p_enc_ex_filters, const Vector<uint8_t> &p_key, uint64_t p_seed);
	static Error _save_pack_patch_file(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total, const Vector<String> &p_enc_in_filters, const Vector<String> &p_enc_ex_filters, const Vector<uint8_t> &p_key, uint64_t p_seed);
	static Error _pack_add_shared_object(void *p_userdata, const SharedObject &p_so);

	static Error _remove_pack_file(void *p_userdata, const String &p_path);

	static Error _save_zip_file(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total, const Vector<String> &p_enc_in_filters, const Vector<String> &p_enc_ex_filters, const Vector<uint8_t> &p_key, uint64_t p_seed);
	static Error _save_zip_patch_file(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total, const Vector<String> &p_enc_in_filters, const Vector<String> &p_enc_ex_filters, const Vector<uint8_t> &p_key, uint64_t p_seed);
	static Error _zip_add_shared_object(void *p_userdata, const SharedObject &p_so);

	struct ScriptCallbackData {
		Callable file_cb;
		Callable so_cb;
	};

	static Error _script_save_file(void *p_userdata, const String &p_path, const Vector<uint8_t> &p_data, int p_file, int p_total, const Vector<String> &p_enc_in_filters, const Vector<String> &p_enc_ex_filters, const Vector<uint8_t> &p_key, uint64_t p_seed);
	static Error _script_add_shared_object(void *p_userdata, const SharedObject &p_so);

	void _edit_files_with_filter(Ref<DirAccess> &da, const Vector<String> &p_filters, HashSet<String> &r_list, bool exclude);
	void _edit_filter_list(HashSet<String> &r_list, const String &p_filter, bool exclude);

	static Vector<uint8_t> _filter_extension_list_config_file(const String &p_config_path, const HashSet<String> &p_paths);

	struct FileExportCache {
		uint64_t source_modified_time = 0;
		String source_md5;
		String saved_path;
		bool used = false;
	};

	bool _export_customize_dictionary(Dictionary &dict, LocalVector<Ref<EditorExportPlugin>> &customize_resources_plugins);
	bool _export_customize_array(Array &array, LocalVector<Ref<EditorExportPlugin>> &customize_resources_plugins);
	bool _export_customize_object(Object *p_object, LocalVector<Ref<EditorExportPlugin>> &customize_resources_plugins);
	bool _export_customize_scene_resources(Node *p_root, Node *p_node, LocalVector<Ref<EditorExportPlugin>> &customize_resources_plugins);
	bool _is_editable_ancestor(Node *p_root, Node *p_node);

	String _export_customize(const String &p_path, LocalVector<Ref<EditorExportPlugin>> &customize_resources_plugins, LocalVector<Ref<EditorExportPlugin>> &customize_scenes_plugins, HashMap<String, FileExportCache> &export_cache, const String &export_base_path, bool p_force_save);
	String _get_script_encryption_key(const Ref<EditorExportPreset> &p_preset) const;

protected:
	struct ExportNotifier {
		ExportNotifier(EditorExportPlatform &p_platform, const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags);
		~ExportNotifier();
	};

	HashSet<String> get_features(const Ref<EditorExportPreset> &p_preset, bool p_debug) const;

	Dictionary _find_export_template(const String &p_template_file_name) const {
		Dictionary ret;
		String err;

		String path = find_export_template(p_template_file_name, &err);
		ret["result"] = (err.is_empty() && !path.is_empty()) ? OK : FAILED;
		ret["path"] = path;
		ret["error_string"] = err;

		return ret;
	}

	bool exists_export_template(const String &p_template_file_name, String *r_err) const;
	String find_export_template(const String &p_template_file_name, String *r_err = nullptr) const;
	Vector<String> gen_export_flags(BitField<EditorExportPlatform::DebugFlags> p_flags);

	virtual void zip_folder_recursive(zipFile &p_zip, const String &p_root_path, const String &p_folder, const String &p_pkg_name);

	Error _ssh_run_on_remote(const String &p_host, const String &p_port, const Vector<String> &p_ssh_args, const String &p_cmd_args, Array r_output = Array(), int p_port_fwd = -1) const {
		String pipe;
		Error err = ssh_run_on_remote(p_host, p_port, p_ssh_args, p_cmd_args, &pipe, p_port_fwd);
		r_output.push_back(pipe);
		return err;
	}
	OS::ProcessID _ssh_run_on_remote_no_wait(const String &p_host, const String &p_port, const Vector<String> &p_ssh_args, const String &p_cmd_args, int p_port_fwd = -1) const {
		OS::ProcessID pid = 0;
		Error err = ssh_run_on_remote_no_wait(p_host, p_port, p_ssh_args, p_cmd_args, &pid, p_port_fwd);
		if (err != OK) {
			return -1;
		} else {
			return pid;
		}
	}

	Error ssh_run_on_remote(const String &p_host, const String &p_port, const Vector<String> &p_ssh_args, const String &p_cmd_args, String *r_out = nullptr, int p_port_fwd = -1) const;
	Error ssh_run_on_remote_no_wait(const String &p_host, const String &p_port, const Vector<String> &p_ssh_args, const String &p_cmd_args, OS::ProcessID *r_pid = nullptr, int p_port_fwd = -1) const;
	Error ssh_push_to_remote(const String &p_host, const String &p_port, const Vector<String> &p_scp_args, const String &p_src_file, const String &p_dst_file) const;

	Error _load_patches(const Vector<String> &p_patches);
	void _unload_patches();

public:
	virtual void get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const = 0;

	struct ExportOption {
		PropertyInfo option;
		Variant default_value;
		bool update_visibility = false;
		bool required = false;

		ExportOption(const PropertyInfo &p_info, const Variant &p_default, bool p_update_visibility = false, bool p_required = false) :
				option(p_info),
				default_value(p_default),
				update_visibility(p_update_visibility),
				required(p_required) {
		}
		ExportOption() {}
	};

	virtual Ref<EditorExportPreset> create_preset();
	virtual bool is_executable(const String &p_path) const { return false; }

	virtual void clear_messages() { messages.clear(); }
	virtual void add_message(ExportMessageType p_type, const String &p_category, const String &p_message) {
		ExportMessage msg;
		msg.category = p_category;
		msg.text = p_message;
		msg.msg_type = p_type;
		messages.push_back(msg);
		switch (p_type) {
			case EXPORT_MESSAGE_INFO: {
				print_line(vformat("%s: %s", msg.category, msg.text));
			} break;
			case EXPORT_MESSAGE_WARNING: {
				WARN_PRINT(vformat("%s: %s", msg.category, msg.text));
			} break;
			case EXPORT_MESSAGE_ERROR: {
				ERR_PRINT(vformat("%s: %s", msg.category, msg.text));
			} break;
			default:
				break;
		}
	}

	virtual int get_message_count() const {
		return messages.size();
	}

	virtual ExportMessage get_message(int p_index) const {
		ERR_FAIL_INDEX_V(p_index, messages.size(), ExportMessage());
		return messages[p_index];
	}

	virtual ExportMessageType _get_message_type(int p_index) const {
		ERR_FAIL_INDEX_V(p_index, messages.size(), EXPORT_MESSAGE_NONE);
		return messages[p_index].msg_type;
	}

	virtual String _get_message_category(int p_index) const {
		ERR_FAIL_INDEX_V(p_index, messages.size(), String());
		return messages[p_index].category;
	}

	virtual String _get_message_text(int p_index) const {
		ERR_FAIL_INDEX_V(p_index, messages.size(), String());
		return messages[p_index].text;
	}

	virtual ExportMessageType get_worst_message_type() const {
		ExportMessageType worst_type = EXPORT_MESSAGE_NONE;
		for (int i = 0; i < messages.size(); i++) {
			worst_type = MAX(worst_type, messages[i].msg_type);
		}
		return worst_type;
	}

	Dictionary get_internal_export_files(const Ref<EditorExportPreset> &p_preset, bool p_debug);

	static Vector<String> get_forced_export_files();

	virtual bool fill_log_messages(RichTextLabel *p_log, Error p_err);

	virtual void get_export_options(List<ExportOption> *r_options) const = 0;
	virtual bool should_update_export_options() { return false; }
	virtual bool get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const { return true; }
	virtual String get_export_option_warning(const EditorExportPreset *p_preset, const StringName &p_name) const { return String(); }

	virtual String get_os_name() const = 0;
	virtual String get_name() const = 0;
	virtual Ref<Texture2D> get_logo() const = 0;

	Array get_current_presets() const;

	Error _export_project_files(const Ref<EditorExportPreset> &p_preset, bool p_debug, const Callable &p_save_func, const Callable &p_so_func);
	Error export_project_files(const Ref<EditorExportPreset> &p_preset, bool p_debug, EditorExportSaveFunction p_save_func, EditorExportRemoveFunction p_remove_func, void *p_udata, EditorExportSaveSharedObject p_so_func = nullptr);

	Dictionary _save_pack(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, bool p_embed = false);
	Dictionary _save_zip(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path);

	Dictionary _save_pack_patch(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path);
	Dictionary _save_zip_patch(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path);

	Error save_pack(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, Vector<SharedObject> *p_so_files = nullptr, EditorExportSaveFunction p_save_func = nullptr, EditorExportRemoveFunction p_remove_func = nullptr, bool p_embed = false, int64_t *r_embedded_start = nullptr, int64_t *r_embedded_size = nullptr);
	Error save_zip(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, Vector<SharedObject> *p_so_files = nullptr, EditorExportSaveFunction p_save_func = nullptr);

	Error save_pack_patch(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, Vector<SharedObject> *p_so_files = nullptr, bool p_embed = false, int64_t *r_embedded_start = nullptr, int64_t *r_embedded_size = nullptr);
	Error save_zip_patch(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, Vector<SharedObject> *p_so_files = nullptr);

	virtual bool poll_export() { return false; }
	virtual int get_options_count() const { return 0; }
	virtual String get_options_tooltip() const { return ""; }
	virtual Ref<ImageTexture> get_option_icon(int p_index) const;
	virtual String get_option_label(int p_device) const { return ""; }
	virtual String get_option_tooltip(int p_device) const { return ""; }
	virtual String get_device_architecture(int p_device) const { return ""; }

	virtual void cleanup() {}
	virtual Error run(const Ref<EditorExportPreset> &p_preset, int p_device, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) { return OK; }
	virtual Ref<Texture2D> get_run_icon() const { return get_logo(); }

	virtual bool can_export(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug = false) const;
	virtual bool has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug = false) const = 0;
	virtual bool has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const = 0;

	virtual List<String> get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const = 0;
	virtual Error export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0) = 0;
	virtual Error export_pack(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0);
	virtual Error export_zip(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags = 0);
	virtual Error export_pack_patch(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, const Vector<String> &p_patches = Vector<String>(), BitField<EditorExportPlatform::DebugFlags> p_flags = 0);
	virtual Error export_zip_patch(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, const Vector<String> &p_patches = Vector<String>(), BitField<EditorExportPlatform::DebugFlags> p_flags = 0);
	virtual void get_platform_features(List<String> *r_features) const = 0;
	virtual void resolve_platform_feature_priorities(const Ref<EditorExportPreset> &p_preset, HashSet<String> &p_features) {}
	virtual String get_debug_protocol() const { return "tcp://"; }

	EditorExportPlatform();
};

VARIANT_ENUM_CAST(EditorExportPlatform::ExportMessageType)
VARIANT_BITFIELD_CAST(EditorExportPlatform::DebugFlags);

#endif // EDITOR_EXPORT_PLATFORM_H
