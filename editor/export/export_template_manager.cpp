/**************************************************************************/
/*  export_template_manager.cpp                                           */
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

#include "export_template_manager.h"

#include "core/io/dir_access.h"
#include "core/io/json.h"
#include "core/io/zip_io.h"
#include "core/version.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/export/editor_export_preset.h"
#include "editor/progress_dialog.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/separator.h"
#include "scene/gui/tree.h"
#include "scene/main/http_request.h"

enum DownloadsAvailability {
	DOWNLOADS_AVAILABLE,
	DOWNLOADS_NOT_AVAILABLE_IN_OFFLINE_MODE,
	DOWNLOADS_NOT_AVAILABLE_FOR_DEV_BUILDS,
};

static DownloadsAvailability _get_downloads_availability() {
	const int network_mode = EDITOR_GET("network/connection/network_mode");
	if (network_mode == EditorSettings::NETWORK_OFFLINE) {
		return DOWNLOADS_NOT_AVAILABLE_IN_OFFLINE_MODE;
	}

	// Downloadable export templates are only available for stable and official alpha/beta/RC builds
	// (which always have a number following their status, e.g. "alpha1").
	// Therefore, don't display download-related features when using a development version
	// (whose builds aren't numbered).
	if (String(VERSION_STATUS) == String("dev") ||
			String(VERSION_STATUS) == String("alpha") ||
			String(VERSION_STATUS) == String("beta") ||
			String(VERSION_STATUS) == String("rc")) {
		return DOWNLOADS_NOT_AVAILABLE_FOR_DEV_BUILDS;
	}

	return DOWNLOADS_AVAILABLE;
}

void ExportTemplateManager::_update_template_status() {
	// Fetch installed templates from the file system.
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	const String &templates_dir = EditorPaths::get_singleton()->get_export_templates_dir();

	Error err = da->change_dir(templates_dir);
	ERR_FAIL_COND_MSG(err != OK, "Could not access templates directory at '" + templates_dir + "'.");

	RBSet<String> templates;
	da->list_dir_begin();
	if (err == OK) {
		String c = da->get_next();
		while (!c.is_empty()) {
			if (da->current_is_dir() && !c.begins_with(".")) {
				templates.insert(c);
			}
			c = da->get_next();
		}
	}
	da->list_dir_end();

	// Update the state of the current version.
	String current_version = VERSION_FULL_CONFIG;
	current_value->set_text(current_version);

	if (templates.has(current_version)) {
		current_missing_label->hide();
		current_installed_label->show();

		current_installed_hb->show();
		current_version_exists = true;
	} else {
		current_installed_label->hide();
		current_missing_label->show();

		current_installed_hb->hide();
		current_version_exists = false;
	}

	if (is_downloading_templates) {
		install_options_vb->hide();
		download_progress_hb->show();
	} else {
		download_progress_hb->hide();
		install_options_vb->show();

		if (templates.has(current_version)) {
			current_installed_path->set_text(templates_dir.path_join(current_version));
		}
	}

	// Update the list of other installed versions.
	installed_table->clear();
	TreeItem *installed_root = installed_table->create_item();

	for (RBSet<String>::Element *E = templates.back(); E; E = E->prev()) {
		String version_string = E->get();
		if (version_string == current_version) {
			continue;
		}

		TreeItem *ti = installed_table->create_item(installed_root);
		ti->set_text(0, version_string);

#ifndef ANDROID_ENABLED
		ti->add_button(0, get_editor_theme_icon(SNAME("Folder")), OPEN_TEMPLATE_FOLDER, false, TTR("Open the folder containing these templates."));
#endif
		ti->add_button(0, get_editor_theme_icon(SNAME("Remove")), UNINSTALL_TEMPLATE, false, TTR("Uninstall these templates."));
	}
}

void ExportTemplateManager::_download_current() {
	if (is_downloading_templates) {
		return;
	}
	is_downloading_templates = true;

	install_options_vb->hide();
	download_progress_hb->show();

	if (mirrors_available) {
		String mirror_url = _get_selected_mirror();
		if (mirror_url.is_empty()) {
			_set_current_progress_status(TTR("There are no mirrors available."), true);
			return;
		}

		_download_template(mirror_url, true);
	} else if (!is_refreshing_mirrors) {
		_set_current_progress_status(TTR("Retrieving the mirror list..."));
		_refresh_mirrors();
	}
}

void ExportTemplateManager::_download_template(const String &p_url, bool p_skip_check) {
	if (!p_skip_check && is_downloading_templates) {
		return;
	}
	is_downloading_templates = true;

	install_options_vb->hide();
	download_progress_hb->show();
	download_progress_bar->show();
	download_progress_bar->set_indeterminate(true);

	_set_current_progress_status(TTR("Starting the download..."));

	download_templates->set_download_file(EditorPaths::get_singleton()->get_cache_dir().path_join("tmp_templates.tpz"));
	download_templates->set_use_threads(true);

	const String proxy_host = EDITOR_GET("network/http_proxy/host");
	const int proxy_port = EDITOR_GET("network/http_proxy/port");
	download_templates->set_http_proxy(proxy_host, proxy_port);
	download_templates->set_https_proxy(proxy_host, proxy_port);

	Error err = download_templates->request(p_url);
	if (err != OK) {
		_set_current_progress_status(TTR("Error requesting URL:") + " " + p_url, true);
		download_progress_hb->hide();
		return;
	}

	set_process(true);
	_set_current_progress_status(TTR("Connecting to the mirror..."));
}

void ExportTemplateManager::_download_template_completed(int p_status, int p_code, const PackedStringArray &headers, const PackedByteArray &p_data) {
	switch (p_status) {
		case HTTPRequest::RESULT_CANT_RESOLVE: {
			_set_current_progress_status(TTR("Can't resolve the requested address."), true);
		} break;
		case HTTPRequest::RESULT_BODY_SIZE_LIMIT_EXCEEDED:
		case HTTPRequest::RESULT_CONNECTION_ERROR:
		case HTTPRequest::RESULT_CHUNKED_BODY_SIZE_MISMATCH:
		case HTTPRequest::RESULT_TLS_HANDSHAKE_ERROR:
		case HTTPRequest::RESULT_CANT_CONNECT: {
			_set_current_progress_status(TTR("Can't connect to the mirror."), true);
		} break;
		case HTTPRequest::RESULT_NO_RESPONSE: {
			_set_current_progress_status(TTR("No response from the mirror."), true);
		} break;
		case HTTPRequest::RESULT_REQUEST_FAILED: {
			_set_current_progress_status(TTR("Request failed."), true);
		} break;
		case HTTPRequest::RESULT_REDIRECT_LIMIT_REACHED: {
			_set_current_progress_status(TTR("Request ended up in a redirect loop."), true);
		} break;
		default: {
			if (p_code != 200) {
				_set_current_progress_status(TTR("Request failed:") + " " + itos(p_code), true);
			} else {
				_set_current_progress_status(TTR("Download complete; extracting templates..."));
				String path = download_templates->get_download_file();

				is_downloading_templates = false;
				bool ret = _install_file_selected(path, true);
				if (ret) {
					// Clean up downloaded file.
					Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
					Error err = da->remove(path);
					if (err != OK) {
						EditorNode::get_singleton()->add_io_error(TTR("Cannot remove temporary file:") + "\n" + path + "\n");
					}
				} else {
					EditorNode::get_singleton()->add_io_error(vformat(TTR("Templates installation failed.\nThe problematic templates archives can be found at '%s'."), path));
				}
			}
		} break;
	}

	set_process(false);
}

void ExportTemplateManager::_cancel_template_download() {
	if (!is_downloading_templates) {
		return;
	}

	download_templates->cancel_request();
	download_progress_hb->hide();
	install_options_vb->show();
	is_downloading_templates = false;
}

void ExportTemplateManager::_refresh_mirrors() {
	if (is_refreshing_mirrors) {
		return;
	}
	is_refreshing_mirrors = true;

	String current_version = VERSION_FULL_CONFIG;
	const String mirrors_metadata_url = "https://godotengine.org/mirrorlist/" + current_version + ".json";
	request_mirrors->request(mirrors_metadata_url);
}

void ExportTemplateManager::_refresh_mirrors_completed(int p_status, int p_code, const PackedStringArray &headers, const PackedByteArray &p_data) {
	if (p_status != HTTPRequest::RESULT_SUCCESS || p_code != 200) {
		EditorNode::get_singleton()->show_warning(TTR("Error getting the list of mirrors."));
		is_refreshing_mirrors = false;
		if (is_downloading_templates) {
			_cancel_template_download();
		}
		return;
	}

	String response_json;
	{
		const uint8_t *r = p_data.ptr();
		response_json.parse_utf8((const char *)r, p_data.size());
	}

	JSON json;
	Error err = json.parse(response_json);
	if (err != OK) {
		EditorNode::get_singleton()->show_warning(TTR("Error parsing JSON with the list of mirrors. Please report this issue!"));
		is_refreshing_mirrors = false;
		if (is_downloading_templates) {
			_cancel_template_download();
		}
		return;
	}

	mirrors_list->clear();
	mirrors_list->add_item(TTR("Best available mirror"), 0);

	mirrors_available = false;

	Dictionary mirror_data = json.get_data();
	if (mirror_data.has("mirrors")) {
		Array mirrors = mirror_data["mirrors"];

		for (int i = 0; i < mirrors.size(); i++) {
			Dictionary m = mirrors[i];
			ERR_CONTINUE(!m.has("url") || !m.has("name"));

			mirrors_list->add_item(m["name"]);
			mirrors_list->set_item_metadata(i + 1, m["url"]);

			mirrors_available = true;
		}
	}
	if (!mirrors_available) {
		EditorNode::get_singleton()->show_warning(TTR("No download links found for this version. Direct download is only available for official releases."));
		if (is_downloading_templates) {
			_cancel_template_download();
		}
	}

	is_refreshing_mirrors = false;

	if (is_downloading_templates) {
		String mirror_url = _get_selected_mirror();
		if (mirror_url.is_empty()) {
			_set_current_progress_status(TTR("There are no mirrors available."), true);
			return;
		}

		_download_template(mirror_url, true);
	}
}

bool ExportTemplateManager::_humanize_http_status(HTTPRequest *p_request, String *r_status, int *r_downloaded_bytes, int *r_total_bytes) {
	*r_status = "";
	*r_downloaded_bytes = -1;
	*r_total_bytes = -1;
	bool success = true;

	switch (p_request->get_http_client_status()) {
		case HTTPClient::STATUS_DISCONNECTED:
			*r_status = TTR("Disconnected");
			success = false;
			break;
		case HTTPClient::STATUS_RESOLVING:
			*r_status = TTR("Resolving");
			break;
		case HTTPClient::STATUS_CANT_RESOLVE:
			*r_status = TTR("Can't Resolve");
			success = false;
			break;
		case HTTPClient::STATUS_CONNECTING:
			*r_status = TTR("Connecting...");
			break;
		case HTTPClient::STATUS_CANT_CONNECT:
			*r_status = TTR("Can't Connect");
			success = false;
			break;
		case HTTPClient::STATUS_CONNECTED:
			*r_status = TTR("Connected");
			break;
		case HTTPClient::STATUS_REQUESTING:
			*r_status = TTR("Requesting...");
			break;
		case HTTPClient::STATUS_BODY:
			*r_status = TTR("Downloading");
			*r_downloaded_bytes = p_request->get_downloaded_bytes();
			*r_total_bytes = p_request->get_body_size();

			if (p_request->get_body_size() > 0) {
				*r_status += " " + String::humanize_size(p_request->get_downloaded_bytes()) + "/" + String::humanize_size(p_request->get_body_size());
			} else {
				*r_status += " " + String::humanize_size(p_request->get_downloaded_bytes());
			}
			break;
		case HTTPClient::STATUS_CONNECTION_ERROR:
			*r_status = TTR("Connection Error");
			success = false;
			break;
		case HTTPClient::STATUS_TLS_HANDSHAKE_ERROR:
			*r_status = TTR("TLS Handshake Error");
			success = false;
			break;
	}

	return success;
}

void ExportTemplateManager::_set_current_progress_status(const String &p_status, bool p_error) {
	download_progress_label->set_text(p_status);

	if (p_error) {
		download_progress_bar->hide();
		download_progress_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
	} else {
		download_progress_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SceneStringName(font_color), SNAME("Label")));
	}
}

void ExportTemplateManager::_set_current_progress_value(float p_value, const String &p_status) {
	download_progress_bar->show();
	download_progress_bar->set_indeterminate(false);
	download_progress_bar->set_value(p_value);
	download_progress_label->set_text(p_status);
}

void ExportTemplateManager::_install_file() {
	install_file_dialog->popup_file_dialog();
}

bool ExportTemplateManager::_install_file_selected(const String &p_file, bool p_skip_progress) {
	Ref<FileAccess> io_fa;
	zlib_filefunc_def io = zipio_create_io(&io_fa);

	unzFile pkg = unzOpen2(p_file.utf8().get_data(), &io);
	if (!pkg) {
		EditorNode::get_singleton()->show_warning(TTR("Can't open the export templates file."));
		return false;
	}
	int ret = unzGoToFirstFile(pkg);

	// Count them and find version.
	int fc = 0;
	String version;
	String contents_dir;

	while (ret == UNZ_OK) {
		unz_file_info info;
		char fname[16384];
		ret = unzGetCurrentFileInfo(pkg, &info, fname, 16384, nullptr, 0, nullptr, 0);
		if (ret != UNZ_OK) {
			break;
		}

		String file = String::utf8(fname);

		// Skip the __MACOSX directory created by macOS's built-in file zipper.
		if (file.begins_with("__MACOSX")) {
			ret = unzGoToNextFile(pkg);
			continue;
		}

		if (file.ends_with("version.txt")) {
			Vector<uint8_t> uncomp_data;
			uncomp_data.resize(info.uncompressed_size);

			// Read.
			unzOpenCurrentFile(pkg);
			ret = unzReadCurrentFile(pkg, uncomp_data.ptrw(), uncomp_data.size());
			ERR_BREAK_MSG(ret < 0, vformat("An error occurred while attempting to read from file: %s. This file will not be used.", file));
			unzCloseCurrentFile(pkg);

			String data_str;
			data_str.parse_utf8((const char *)uncomp_data.ptr(), uncomp_data.size());
			data_str = data_str.strip_edges();

			// Version number should be of the form major.minor[.patch].status[.module_config]
			// so it can in theory have 3 or more slices.
			if (data_str.get_slice_count(".") < 3) {
				EditorNode::get_singleton()->show_warning(vformat(TTR("Invalid version.txt format inside the export templates file: %s."), data_str));
				unzClose(pkg);
				return false;
			}

			version = data_str;
			contents_dir = file.get_base_dir().trim_suffix("/").trim_suffix("\\");
		}

		if (file.get_file().size() != 0) {
			fc++;
		}

		ret = unzGoToNextFile(pkg);
	}

	if (version.is_empty()) {
		EditorNode::get_singleton()->show_warning(TTR("No version.txt found inside the export templates file."));
		unzClose(pkg);
		return false;
	}

	Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String template_path = EditorPaths::get_singleton()->get_export_templates_dir().path_join(version);
	Error err = d->make_dir_recursive(template_path);
	if (err != OK) {
		EditorNode::get_singleton()->show_warning(TTR("Error creating path for extracting templates:") + "\n" + template_path);
		unzClose(pkg);
		return false;
	}

	EditorProgress *p = nullptr;
	if (!p_skip_progress) {
		p = memnew(EditorProgress("ltask", TTR("Extracting Export Templates"), fc));
	}

	fc = 0;
	ret = unzGoToFirstFile(pkg);
	while (ret == UNZ_OK) {
		// Get filename.
		unz_file_info info;
		char fname[16384];
		ret = unzGetCurrentFileInfo(pkg, &info, fname, 16384, nullptr, 0, nullptr, 0);
		if (ret != UNZ_OK) {
			break;
		}

		if (String::utf8(fname).ends_with("/")) {
			// File is a directory, ignore it.
			// Directories will be created when extracting each file.
			ret = unzGoToNextFile(pkg);
			continue;
		}

		String file_path(String::utf8(fname).simplify_path());

		String file = file_path.get_file();

		// Skip the __MACOSX directory created by macOS's built-in file zipper.
		if (file.is_empty() || file.begins_with("__MACOSX")) {
			ret = unzGoToNextFile(pkg);
			continue;
		}

		Vector<uint8_t> uncomp_data;
		uncomp_data.resize(info.uncompressed_size);

		// Read
		unzOpenCurrentFile(pkg);
		ret = unzReadCurrentFile(pkg, uncomp_data.ptrw(), uncomp_data.size());
		ERR_BREAK_MSG(ret < 0, vformat("An error occurred while attempting to read from file: %s. This file will not be used.", file));
		unzCloseCurrentFile(pkg);

		String base_dir = file_path.get_base_dir().trim_suffix("/");

		if (base_dir != contents_dir && base_dir.begins_with(contents_dir)) {
			base_dir = base_dir.substr(contents_dir.length(), file_path.length()).trim_prefix("/");
			file = base_dir.path_join(file);

			Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			ERR_CONTINUE(da.is_null());

			String output_dir = template_path.path_join(base_dir);

			if (!DirAccess::exists(output_dir)) {
				Error mkdir_err = da->make_dir_recursive(output_dir);
				ERR_CONTINUE(mkdir_err != OK);
			}
		}

		if (p) {
			p->step(TTR("Importing:") + " " + file, fc);
		}

		String to_write = template_path.path_join(file);
		Ref<FileAccess> f = FileAccess::open(to_write, FileAccess::WRITE);

		if (f.is_null()) {
			ret = unzGoToNextFile(pkg);
			fc++;
			ERR_CONTINUE_MSG(true, "Can't open file from path '" + String(to_write) + "'.");
		}

		f->store_buffer(uncomp_data.ptr(), uncomp_data.size());
		f.unref(); // close file.
#ifndef WINDOWS_ENABLED
		FileAccess::set_unix_permissions(to_write, (info.external_fa >> 16) & 0x01FF);
#endif

		ret = unzGoToNextFile(pkg);
		fc++;
	}

	if (p) {
		memdelete(p);
	}
	unzClose(pkg);

	_update_template_status();
	EditorSettings::get_singleton()->set("_export_template_download_directory", p_file.get_base_dir());
	return true;
}

void ExportTemplateManager::_uninstall_template(const String &p_version) {
	uninstall_confirm->set_text(vformat(TTR("Remove templates for the version '%s'?"), p_version));
	uninstall_confirm->popup_centered();
	uninstall_version = p_version;
}

void ExportTemplateManager::_uninstall_template_confirmed() {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	const String &templates_dir = EditorPaths::get_singleton()->get_export_templates_dir();

	Error err = da->change_dir(templates_dir);
	ERR_FAIL_COND_MSG(err != OK, "Could not access templates directory at '" + templates_dir + "'.");
	err = da->change_dir(uninstall_version);
	ERR_FAIL_COND_MSG(err != OK, "Could not access templates directory at '" + templates_dir.path_join(uninstall_version) + "'.");

	err = da->erase_contents_recursive();
	ERR_FAIL_COND_MSG(err != OK, "Could not remove all templates in '" + templates_dir.path_join(uninstall_version) + "'.");

	da->change_dir("..");
	err = da->remove(uninstall_version);
	ERR_FAIL_COND_MSG(err != OK, "Could not remove templates directory at '" + templates_dir.path_join(uninstall_version) + "'.");

	_update_template_status();
}

String ExportTemplateManager::_get_selected_mirror() const {
	if (mirrors_list->get_item_count() == 1) {
		return "";
	}

	int selected = mirrors_list->get_selected_id();
	if (selected == 0) {
		// This is a special "best available" value; so pick the first available mirror from the rest of the list.
		selected = 1;
	}

	return mirrors_list->get_item_metadata(selected);
}

void ExportTemplateManager::_mirror_options_button_cbk(int p_id) {
	switch (p_id) {
		case VISIT_WEB_MIRROR: {
			String mirror_url = _get_selected_mirror();
			if (mirror_url.is_empty()) {
				EditorNode::get_singleton()->show_warning(TTR("There are no mirrors available."));
				return;
			}

			OS::get_singleton()->shell_open(mirror_url);
		} break;

		case COPY_MIRROR_URL: {
			String mirror_url = _get_selected_mirror();
			if (mirror_url.is_empty()) {
				EditorNode::get_singleton()->show_warning(TTR("There are no mirrors available."));
				return;
			}

			DisplayServer::get_singleton()->clipboard_set(mirror_url);
		} break;
	}
}

void ExportTemplateManager::_installed_table_button_cbk(Object *p_item, int p_column, int p_id, MouseButton p_button) {
	if (p_button != MouseButton::LEFT) {
		return;
	}
	TreeItem *ti = Object::cast_to<TreeItem>(p_item);
	if (!ti) {
		return;
	}

	switch (p_id) {
		case OPEN_TEMPLATE_FOLDER: {
			String version_string = ti->get_text(0);
			_open_template_folder(version_string);
		} break;

		case UNINSTALL_TEMPLATE: {
			String version_string = ti->get_text(0);
			_uninstall_template(version_string);
		} break;
	}
}

void ExportTemplateManager::_open_template_folder(const String &p_version) {
	const String &templates_dir = EditorPaths::get_singleton()->get_export_templates_dir();
	OS::get_singleton()->shell_show_in_file_manager(templates_dir.path_join(p_version), true);
}

void ExportTemplateManager::popup_manager() {
	_update_template_status();

	switch (_get_downloads_availability()) {
		case DOWNLOADS_AVAILABLE: {
			current_missing_label->set_text(TTR("Export templates are missing. Download them or install from a file."));

			mirrors_list->clear();
			mirrors_list->add_item(TTR("Best available mirror"), 0);
			mirrors_list->set_disabled(false);
			mirrors_list->set_tooltip_text("");

			mirror_options_button->set_disabled(false);

			download_current_button->set_disabled(false);
			download_current_button->set_tooltip_text("");

			if (!is_downloading_templates) {
				_refresh_mirrors();
			}
		} break;

		case DOWNLOADS_NOT_AVAILABLE_IN_OFFLINE_MODE: {
			current_missing_label->set_text(TTR("Export templates are missing. Install them from a file."));

			mirrors_list->clear();
			mirrors_list->add_item(TTR("Not available in offline mode"), 0);
			mirrors_list->set_disabled(true);
			mirrors_list->set_tooltip_text(TTR("Template downloading is disabled in offline mode."));

			mirror_options_button->set_disabled(true);

			download_current_button->set_disabled(true);
			download_current_button->set_tooltip_text(TTR("Template downloading is disabled in offline mode."));
		} break;

		case DOWNLOADS_NOT_AVAILABLE_FOR_DEV_BUILDS: {
			current_missing_label->set_text(TTR("Export templates are missing. Install them from a file."));

			mirrors_list->clear();
			mirrors_list->add_item(TTR("No templates for development builds"), 0);
			mirrors_list->set_disabled(true);
			mirrors_list->set_tooltip_text(TTR("Official export templates aren't available for development builds."));

			mirror_options_button->set_disabled(true);

			download_current_button->set_disabled(true);
			download_current_button->set_tooltip_text(TTR("Official export templates aren't available for development builds."));
		} break;
	}

	popup_centered(Size2(720, 280) * EDSCALE);
}

void ExportTemplateManager::ok_pressed() {
	if (!is_downloading_templates) {
		hide();
		return;
	}

	hide_dialog_accept->popup_centered();
}

void ExportTemplateManager::_hide_dialog() {
	hide();
}

String ExportTemplateManager::get_android_build_directory(const Ref<EditorExportPreset> &p_preset) {
	if (p_preset.is_valid()) {
		String gradle_build_dir = p_preset->get("gradle_build/gradle_build_directory");
		if (!gradle_build_dir.is_empty()) {
			return gradle_build_dir.path_join("build");
		}
	}
	return "res://android/build";
}

String ExportTemplateManager::get_android_source_zip(const Ref<EditorExportPreset> &p_preset) {
	if (p_preset.is_valid()) {
		String android_source_zip = p_preset->get("gradle_build/android_source_template");
		if (!android_source_zip.is_empty()) {
			return android_source_zip;
		}
	}

	const String templates_dir = EditorPaths::get_singleton()->get_export_templates_dir().path_join(VERSION_FULL_CONFIG);
	return templates_dir.path_join("android_source.zip");
}

String ExportTemplateManager::get_android_template_identifier(const Ref<EditorExportPreset> &p_preset) {
	// The template identifier is the Godot version for the default template, and the full path plus md5 hash for custom templates.
	if (p_preset.is_valid()) {
		String android_source_zip = p_preset->get("gradle_build/android_source_template");
		if (!android_source_zip.is_empty()) {
			return android_source_zip + String(" [") + FileAccess::get_md5(android_source_zip) + String("]");
		}
	}
	return VERSION_FULL_CONFIG;
}

bool ExportTemplateManager::is_android_template_installed(const Ref<EditorExportPreset> &p_preset) {
	return DirAccess::exists(get_android_build_directory(p_preset));
}

bool ExportTemplateManager::can_install_android_template(const Ref<EditorExportPreset> &p_preset) {
	return FileAccess::exists(get_android_source_zip(p_preset));
}

Error ExportTemplateManager::install_android_template(const Ref<EditorExportPreset> &p_preset) {
	const String source_zip = get_android_source_zip(p_preset);
	ERR_FAIL_COND_V(!FileAccess::exists(source_zip), ERR_CANT_OPEN);
	return install_android_template_from_file(source_zip, p_preset);
}

Error ExportTemplateManager::install_android_template_from_file(const String &p_file, const Ref<EditorExportPreset> &p_preset) {
	// To support custom Android builds, we install the Java source code and buildsystem
	// from android_source.zip to the project's res://android folder.

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	ERR_FAIL_COND_V(da.is_null(), ERR_CANT_CREATE);

	String build_dir = get_android_build_directory(p_preset);
	String parent_dir = build_dir.get_base_dir();

	// Make parent of the build dir (if it does not exist).
	da->make_dir_recursive(parent_dir);
	{
		// Add identifier, to ensure building won't work if the current template doesn't match.
		Ref<FileAccess> f = FileAccess::open(parent_dir.path_join(".build_version"), FileAccess::WRITE);
		ERR_FAIL_COND_V(f.is_null(), ERR_CANT_CREATE);
		f->store_line(get_android_template_identifier(p_preset));
	}

	// Create the android build directory.
	Error err = da->make_dir_recursive(build_dir);
	ERR_FAIL_COND_V(err != OK, err);
	{
		// Add an empty .gdignore file to avoid scan.
		Ref<FileAccess> f = FileAccess::open(build_dir.path_join(".gdignore"), FileAccess::WRITE);
		ERR_FAIL_COND_V(f.is_null(), ERR_CANT_CREATE);
		f->store_line("");
	}

	// Uncompress source template.

	Ref<FileAccess> io_fa;
	zlib_filefunc_def io = zipio_create_io(&io_fa);

	unzFile pkg = unzOpen2(p_file.utf8().get_data(), &io);
	ERR_FAIL_NULL_V_MSG(pkg, ERR_CANT_OPEN, "Android sources not in ZIP format.");

	int ret = unzGoToFirstFile(pkg);
	int total_files = 0;
	// Count files to unzip.
	while (ret == UNZ_OK) {
		total_files++;
		ret = unzGoToNextFile(pkg);
	}
	ret = unzGoToFirstFile(pkg);

	ProgressDialog::get_singleton()->add_task("uncompress_src", TTR("Uncompressing Android Build Sources"), total_files);

	HashSet<String> dirs_tested;
	int idx = 0;
	while (ret == UNZ_OK) {
		// Get file path.
		unz_file_info info;
		char fpath[16384];
		ret = unzGetCurrentFileInfo(pkg, &info, fpath, 16384, nullptr, 0, nullptr, 0);
		if (ret != UNZ_OK) {
			break;
		}

		String path = String::utf8(fpath);
		String base_dir = path.get_base_dir();

		if (!path.ends_with("/")) {
			Vector<uint8_t> uncomp_data;
			uncomp_data.resize(info.uncompressed_size);

			// Read.
			unzOpenCurrentFile(pkg);
			unzReadCurrentFile(pkg, uncomp_data.ptrw(), uncomp_data.size());
			unzCloseCurrentFile(pkg);

			if (!dirs_tested.has(base_dir)) {
				da->make_dir_recursive(build_dir.path_join(base_dir));
				dirs_tested.insert(base_dir);
			}

			String to_write = build_dir.path_join(path);
			Ref<FileAccess> f = FileAccess::open(to_write, FileAccess::WRITE);
			if (f.is_valid()) {
				f->store_buffer(uncomp_data.ptr(), uncomp_data.size());
				f.unref(); // close file.
#ifndef WINDOWS_ENABLED
				FileAccess::set_unix_permissions(to_write, (info.external_fa >> 16) & 0x01FF);
#endif
			} else {
				ERR_PRINT("Can't uncompress file: " + to_write);
			}
		}

		ProgressDialog::get_singleton()->task_step("uncompress_src", path, idx);

		idx++;
		ret = unzGoToNextFile(pkg);
	}

	ProgressDialog::get_singleton()->end_task("uncompress_src");
	unzClose(pkg);
	EditorFileSystem::get_singleton()->scan_changes();
	return OK;
}

void ExportTemplateManager::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE:
		case NOTIFICATION_THEME_CHANGED: {
			current_value->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("main"), EditorStringName(EditorFonts)));
			current_missing_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("error_color"), EditorStringName(Editor)));
			current_installed_label->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor)));

			mirror_options_button->set_button_icon(get_editor_theme_icon(SNAME("GuiTabMenuHl")));
		} break;

		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (!is_visible()) {
				set_process(false);
			} else if (is_visible() && is_downloading_templates) {
				set_process(true);
			}
		} break;

		case NOTIFICATION_PROCESS: {
			update_countdown -= get_process_delta_time();
			if (update_countdown > 0) {
				return;
			}
			update_countdown = 0.5;

			String status;
			int downloaded_bytes;
			int total_bytes;
			bool success = _humanize_http_status(download_templates, &status, &downloaded_bytes, &total_bytes);

			if (downloaded_bytes >= 0) {
				if (total_bytes > 0) {
					_set_current_progress_value(float(downloaded_bytes) / total_bytes, status);
				} else {
					_set_current_progress_value(0, status);
				}
			} else {
				_set_current_progress_status(status);
			}

			if (!success) {
				set_process(false);
			}
		} break;

		case NOTIFICATION_WM_CLOSE_REQUEST: {
			// This won't stop the window from closing, but will show the alert if the download is active.
			ok_pressed();
		} break;
	}
}

ExportTemplateManager::ExportTemplateManager() {
	set_title(TTR("Export Template Manager"));
	set_hide_on_ok(false);
	set_ok_button_text(TTR("Close"));

	VBoxContainer *main_vb = memnew(VBoxContainer);
	add_child(main_vb);

	// Current version controls.
	HBoxContainer *current_hb = memnew(HBoxContainer);
	main_vb->add_child(current_hb);

	Label *current_label = memnew(Label);
	current_label->set_theme_type_variation("HeaderSmall");
	current_label->set_text(TTR("Current Version:"));
	current_hb->add_child(current_label);

	current_value = memnew(Label);
	current_hb->add_child(current_value);

	// Current version statuses.
	// Status: Current version is missing.
	current_missing_label = memnew(Label);
	current_missing_label->set_theme_type_variation("HeaderSmall");

	current_missing_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	current_missing_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	current_hb->add_child(current_missing_label);

	// Status: Current version is installed.
	current_installed_label = memnew(Label);
	current_installed_label->set_theme_type_variation("HeaderSmall");
	current_installed_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	current_installed_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	current_installed_label->set_text(TTR("Export templates are installed and ready to be used."));
	current_hb->add_child(current_installed_label);
	current_installed_label->hide();

	// Currently installed template.
	current_installed_hb = memnew(HBoxContainer);
	main_vb->add_child(current_installed_hb);

	current_installed_path = memnew(LineEdit);
	current_installed_path->set_editable(false);
	current_installed_path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	current_installed_hb->add_child(current_installed_path);

#ifndef ANDROID_ENABLED
	Button *current_open_button = memnew(Button);
	current_open_button->set_text(TTR("Open Folder"));
	current_open_button->set_tooltip_text(TTR("Open the folder containing installed templates for the current version."));
	current_installed_hb->add_child(current_open_button);
	current_open_button->connect(SceneStringName(pressed), callable_mp(this, &ExportTemplateManager::_open_template_folder).bind(VERSION_FULL_CONFIG));
#endif

	current_uninstall_button = memnew(Button);
	current_uninstall_button->set_text(TTR("Uninstall"));
	current_uninstall_button->set_tooltip_text(TTR("Uninstall templates for the current version."));
	current_installed_hb->add_child(current_uninstall_button);
	current_uninstall_button->connect(SceneStringName(pressed), callable_mp(this, &ExportTemplateManager::_uninstall_template).bind(VERSION_FULL_CONFIG));

	main_vb->add_child(memnew(HSeparator));

	// Download and install section.
	HBoxContainer *install_templates_hb = memnew(HBoxContainer);
	main_vb->add_child(install_templates_hb);

	// Download and install buttons are available.
	install_options_vb = memnew(VBoxContainer);
	install_options_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	install_templates_hb->add_child(install_options_vb);

	HBoxContainer *download_install_hb = memnew(HBoxContainer);
	install_options_vb->add_child(download_install_hb);

	Label *mirrors_label = memnew(Label);
	mirrors_label->set_text(TTR("Download from:"));
	download_install_hb->add_child(mirrors_label);

	mirrors_list = memnew(OptionButton);
	mirrors_list->set_custom_minimum_size(Size2(280, 0) * EDSCALE);
	download_install_hb->add_child(mirrors_list);

	request_mirrors = memnew(HTTPRequest);
	mirrors_list->add_child(request_mirrors);
	request_mirrors->connect("request_completed", callable_mp(this, &ExportTemplateManager::_refresh_mirrors_completed));

	mirror_options_button = memnew(MenuButton);
	mirror_options_button->get_popup()->add_item(TTR("Open in Web Browser"), VISIT_WEB_MIRROR);
	mirror_options_button->get_popup()->add_item(TTR("Copy Mirror URL"), COPY_MIRROR_URL);
	download_install_hb->add_child(mirror_options_button);
	mirror_options_button->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &ExportTemplateManager::_mirror_options_button_cbk));

	download_install_hb->add_spacer();

	download_current_button = memnew(Button);
	download_current_button->set_text(TTR("Download and Install"));
	download_current_button->set_tooltip_text(TTR("Download and install templates for the current version from the best possible mirror."));
	download_install_hb->add_child(download_current_button);
	download_current_button->connect(SceneStringName(pressed), callable_mp(this, &ExportTemplateManager::_download_current));

	HBoxContainer *install_file_hb = memnew(HBoxContainer);
	install_file_hb->set_alignment(BoxContainer::ALIGNMENT_END);
	install_options_vb->add_child(install_file_hb);

	install_file_button = memnew(Button);
	install_file_button->set_text(TTR("Install from File"));
	install_file_button->set_tooltip_text(TTR("Install templates from a local file."));
	install_file_hb->add_child(install_file_button);
	install_file_button->connect(SceneStringName(pressed), callable_mp(this, &ExportTemplateManager::_install_file));

	// Templates are being downloaded; buttons unavailable.
	download_progress_hb = memnew(HBoxContainer);
	download_progress_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	install_templates_hb->add_child(download_progress_hb);
	download_progress_hb->hide();

	download_progress_bar = memnew(ProgressBar);
	download_progress_bar->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	download_progress_bar->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	download_progress_bar->set_min(0);
	download_progress_bar->set_max(1);
	download_progress_bar->set_value(0);
	download_progress_bar->set_step(0.01);
	download_progress_bar->set_editor_preview_indeterminate(true);
	download_progress_hb->add_child(download_progress_bar);

	download_progress_label = memnew(Label);
	download_progress_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	download_progress_hb->add_child(download_progress_label);

	Button *download_cancel_button = memnew(Button);
	download_cancel_button->set_text(TTR("Cancel"));
	download_cancel_button->set_tooltip_text(TTR("Cancel the download of the templates."));
	download_progress_hb->add_child(download_cancel_button);
	download_cancel_button->connect(SceneStringName(pressed), callable_mp(this, &ExportTemplateManager::_cancel_template_download));

	download_templates = memnew(HTTPRequest);
	install_templates_hb->add_child(download_templates);
	download_templates->connect("request_completed", callable_mp(this, &ExportTemplateManager::_download_template_completed));

	main_vb->add_child(memnew(HSeparator));

	// Other installed templates table.
	HBoxContainer *installed_versions_hb = memnew(HBoxContainer);
	main_vb->add_child(installed_versions_hb);
	Label *installed_label = memnew(Label);
	installed_label->set_theme_type_variation("HeaderSmall");
	installed_label->set_text(TTR("Other Installed Versions:"));
	installed_versions_hb->add_child(installed_label);

	installed_table = memnew(Tree);
	installed_table->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	installed_table->set_hide_root(true);
	installed_table->set_custom_minimum_size(Size2(0, 100) * EDSCALE);
	installed_table->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_vb->add_child(installed_table);
	installed_table->connect("button_clicked", callable_mp(this, &ExportTemplateManager::_installed_table_button_cbk));

	// Dialogs.
	uninstall_confirm = memnew(ConfirmationDialog);
	uninstall_confirm->set_title(TTR("Uninstall Template"));
	add_child(uninstall_confirm);
	uninstall_confirm->connect(SceneStringName(confirmed), callable_mp(this, &ExportTemplateManager::_uninstall_template_confirmed));

	install_file_dialog = memnew(FileDialog);
	install_file_dialog->set_title(TTR("Select Template File"));
	install_file_dialog->set_access(FileDialog::ACCESS_FILESYSTEM);
	install_file_dialog->set_file_mode(FileDialog::FILE_MODE_OPEN_FILE);
	install_file_dialog->set_current_dir(EDITOR_DEF("_export_template_download_directory", ""));
	install_file_dialog->add_filter("*.tpz", TTR("Godot Export Templates"));
	install_file_dialog->connect("file_selected", callable_mp(this, &ExportTemplateManager::_install_file_selected).bind(false));
	add_child(install_file_dialog);

	hide_dialog_accept = memnew(AcceptDialog);
	hide_dialog_accept->set_text(TTR("The templates will continue to download.\nYou may experience a short editor freeze when they finish."));
	add_child(hide_dialog_accept);
	hide_dialog_accept->connect(SceneStringName(confirmed), callable_mp(this, &ExportTemplateManager::_hide_dialog));
}
