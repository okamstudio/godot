/**************************************************************************/
/*  export_template_manager.h                                             */
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

#include "scene/gui/dialogs.h"

class EditorExportPreset;
class ExportTemplateVersion;
class FileDialog;
class HTTPRequest;
class MenuButton;
class OptionButton;
class ProgressBar;
class Tree;

class ExportTemplateManager : public AcceptDialog {
	GDCLASS(ExportTemplateManager, AcceptDialog);

	bool current_version_exists = false;
	bool mirrors_available = false;
	bool is_refreshing_mirrors = false;
	bool is_downloading_templates = false;
	float update_countdown = 0;

	Label *current_value = nullptr;
	Label *current_missing_label = nullptr;
	Label *current_installed_label = nullptr;

	HBoxContainer *current_installed_hb = nullptr;
	LineEdit *current_installed_path = nullptr;
	Button *current_uninstall_button = nullptr;

	VBoxContainer *install_options_vb = nullptr;
	OptionButton *mirrors_list = nullptr;

	enum MirrorAction {
		VISIT_WEB_MIRROR,
		COPY_MIRROR_URL,
	};

	MenuButton *mirror_options_button = nullptr;
	HBoxContainer *download_progress_hb = nullptr;
	ProgressBar *download_progress_bar = nullptr;
	Label *download_progress_label = nullptr;
	HTTPRequest *download_templates = nullptr;
	Button *install_file_button = nullptr;
	Button *download_current_button = nullptr;
	HTTPRequest *request_mirrors = nullptr;

	enum TemplatesAction {
		OPEN_TEMPLATE_FOLDER,
		UNINSTALL_TEMPLATE,
	};

	Tree *installed_table = nullptr;

	ConfirmationDialog *uninstall_confirm = nullptr;
	String uninstall_version;
	FileDialog *install_file_dialog = nullptr;
	AcceptDialog *hide_dialog_accept = nullptr;

	void _update_template_status();

	void _download_current();
	void _download_template(const String &p_url, bool p_skip_check = false);
	void _download_template_completed(int p_status, int p_code, const PackedStringArray &headers, const PackedByteArray &p_data);
	void _cancel_template_download();
	void _refresh_mirrors();
	void _refresh_mirrors_completed(int p_status, int p_code, const PackedStringArray &headers, const PackedByteArray &p_data);

	bool _humanize_http_status(HTTPRequest *p_request, String *r_status, int *r_downloaded_bytes, int *r_total_bytes);
	void _set_current_progress_status(const String &p_status, bool p_error = false);
	void _set_current_progress_value(float p_value, const String &p_status);

	void _install_file();
	bool _install_file_selected(const String &p_file, bool p_skip_progress = false);

	void _uninstall_template(const String &p_version);
	void _uninstall_template_confirmed();

	String _get_selected_mirror() const;
	void _mirror_options_button_cbk(int p_id);
	void _installed_table_button_cbk(Object *p_item, int p_column, int p_id, MouseButton p_button);

	void _open_template_folder(const String &p_version);

	virtual void ok_pressed() override;
	void _hide_dialog();

protected:
	void _notification(int p_what);

public:
	static String get_android_build_directory(const Ref<EditorExportPreset> &p_preset);
	static String get_android_source_zip(const Ref<EditorExportPreset> &p_preset);
	static String get_android_template_identifier(const Ref<EditorExportPreset> &p_preset);

	bool is_android_template_installed(const Ref<EditorExportPreset> &p_preset);
	bool can_install_android_template(const Ref<EditorExportPreset> &p_preset);
	Error install_android_template(const Ref<EditorExportPreset> &p_preset);

	Error install_android_template_from_file(const String &p_file, const Ref<EditorExportPreset> &p_preset);

	void popup_manager();

	ExportTemplateManager();
};
