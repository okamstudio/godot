/**************************************************************************/
/*  editor_run_native.cpp                                                 */
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

#include "editor_run_native.h"

#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/export/editor_export.h"
#include "editor/export/editor_export_platform.h"
#include "editor/themes/editor_scale.h"

void EditorRunNative::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			remote_debug->set_button_icon(get_editor_theme_icon(SNAME("PlayRemote")));
		} break;

		case NOTIFICATION_PROCESS: {
			bool changed = EditorExport::get_singleton()->poll_export_platforms() || first;

			if (changed) {
				PopupMenu *popup = remote_debug->get_popup();
				popup->clear();
				int device_shortcut_id = 1;
				for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
					Ref<EditorExportPreset> preset = EditorExport::get_singleton()->get_export_preset(i);
					Ref<EditorExportPlatform> eep = preset->get_platform();
					if (eep.is_null()) {
						continue;
					}
					int platform_idx = -1;
					for (int j = 0; j < EditorExport::get_singleton()->get_export_platform_count(); j++) {
						if (eep->get_name() == EditorExport::get_singleton()->get_export_platform(j)->get_name()) {
							platform_idx = j;
							break;
						}
					}
					int dc = MIN(eep->get_options_count(), 9000);
					String error;
					if (dc > 0 && preset->is_runnable()) {
						popup->add_icon_item(eep->get_run_icon(), eep->get_name(), -1);
						popup->set_item_disabled(-1, true);
						for (int j = 0; j < dc; j++) {
							popup->add_icon_item(eep->get_option_icon(j), eep->get_option_label(j), 10000 * platform_idx + j);
							popup->set_item_tooltip(-1, eep->get_option_tooltip(j));
							popup->set_item_indent(-1, 2);
							if (device_shortcut_id <= 4) {
								// Assign shortcuts for the first 4 devices added in the list.
								popup->set_item_shortcut(-1, ED_GET_SHORTCUT(vformat("remote_deploy/deploy_to_device_%d", device_shortcut_id)), true);
								device_shortcut_id += 1;
							}
						}
					}
				}
				if (popup->get_item_count() == 0) {
					remote_debug->set_disabled(true);
					remote_debug->set_tooltip_text(TTR("No Remote Deploy export presets configured."));
				} else {
					remote_debug->set_disabled(false);
					remote_debug->set_tooltip_text(TTR("Remote Deploy"));
				}

				first = false;
			}
		} break;
	}
}

void EditorRunNative::_confirm_run_native() {
	run_confirmed = true;
	resume_run_native();
}

Error EditorRunNative::start_run_native(int p_id) {
	if (p_id < 0) {
		return OK;
	}

	int platform = p_id / 10000;
	int idx = p_id % 10000;
	resume_id = p_id;

	if (!EditorNode::get_singleton()->ensure_main_scene(true)) {
		return OK;
	}

	Ref<EditorExportPlatform> eep = EditorExport::get_singleton()->get_export_platform(platform);
	ERR_FAIL_COND_V(eep.is_null(), ERR_UNAVAILABLE);

	Ref<EditorExportPreset> preset;

	for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
		Ref<EditorExportPreset> ep = EditorExport::get_singleton()->get_export_preset(i);
		if (ep->is_runnable() && ep->get_platform() == eep) {
			preset = ep;
			break;
		}
	}

	if (preset.is_null()) {
		EditorNode::get_singleton()->show_warning(TTR("No runnable export preset found for this platform.\nPlease add a runnable preset in the Export menu or define an existing preset as runnable."));
		return ERR_UNAVAILABLE;
	}

	String architecture = eep->get_device_architecture(idx);
	if (!run_confirmed && !architecture.is_empty()) {
		String preset_arch = "architectures/" + architecture;
		bool is_arch_enabled = preset->get(preset_arch);

		if (!is_arch_enabled) {
			String warning_message = vformat(TTR("Warning: The CPU architecture \"%s\" is not active in your export preset.\n\n"), Variant(architecture));
			warning_message += TTR("Run \"Remote Deploy\" anyway?");

			run_native_confirm->set_text(warning_message);
			run_native_confirm->popup_centered();
			return OK;
		}
	}
	run_confirmed = false;

	preset->update_value_overrides();

	emit_signal(SNAME("native_run"), preset);

	BitField<EditorExportPlatform::DebugFlags> flags = 0;

	bool deploy_debug_remote = is_deploy_debug_remote_enabled();
	bool deploy_dumb = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_file_server", false);
	bool debug_collisions = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_debug_collisions", false);
	bool debug_navigation = EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_debug_navigation", false);

	if (deploy_debug_remote) {
		flags.set_flag(EditorExportPlatform::DEBUG_FLAG_REMOTE_DEBUG);
	}
	if (deploy_dumb) {
		flags.set_flag(EditorExportPlatform::DEBUG_FLAG_DUMB_CLIENT);
	}
	if (debug_collisions) {
		flags.set_flag(EditorExportPlatform::DEBUG_FLAG_VIEW_COLLISIONS);
	}
	if (debug_navigation) {
		flags.set_flag(EditorExportPlatform::DEBUG_FLAG_VIEW_NAVIGATION);
	}

	eep->clear_messages();
	Error err = eep->run(preset, idx, flags);
	result_dialog_log->clear();
	if (eep->fill_log_messages(result_dialog_log, err)) {
		if (eep->get_worst_message_type() >= EditorExportPlatform::EXPORT_MESSAGE_ERROR) {
			result_dialog->popup_centered_ratio(0.5);
		}
	}
	return err;
}

void EditorRunNative::resume_run_native() {
	start_run_native(resume_id);
}

void EditorRunNative::_bind_methods() {
	ADD_SIGNAL(MethodInfo("native_run", PropertyInfo(Variant::OBJECT, "preset", PROPERTY_HINT_RESOURCE_TYPE, "EditorExportPreset")));
}

bool EditorRunNative::is_deploy_debug_remote_enabled() const {
	return EditorSettings::get_singleton()->get_project_metadata("debug_options", "run_deploy_remote_debug", true);
}

EditorRunNative::EditorRunNative() {
	ED_SHORTCUT("remote_deploy/deploy_to_device_1", TTRC("Deploy to First Device in List"), KeyModifierMask::SHIFT | Key::F5);
	ED_SHORTCUT_OVERRIDE("remote_deploy/deploy_to_device_1", "macos", KeyModifierMask::META | KeyModifierMask::SHIFT | Key::B);
	ED_SHORTCUT("remote_deploy/deploy_to_device_2", TTRC("Deploy to Second Device in List"));
	ED_SHORTCUT("remote_deploy/deploy_to_device_3", TTRC("Deploy to Third Device in List"));
	ED_SHORTCUT("remote_deploy/deploy_to_device_4", TTRC("Deploy to Fourth Device in List"));

	remote_debug = memnew(MenuButton);
	remote_debug->set_flat(false);
	remote_debug->set_theme_type_variation("RunBarButton");
	remote_debug->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &EditorRunNative::start_run_native));
	remote_debug->set_tooltip_text(TTR("Remote Deploy"));
	remote_debug->set_disabled(true);

	add_child(remote_debug);

	result_dialog = memnew(AcceptDialog);
	result_dialog->set_title(TTR("Project Run"));
	result_dialog_log = memnew(RichTextLabel);
	result_dialog_log->set_custom_minimum_size(Size2(300, 80) * EDSCALE);
	result_dialog->add_child(result_dialog_log);

	add_child(result_dialog);
	result_dialog->hide();

	run_native_confirm = memnew(ConfirmationDialog);
	add_child(run_native_confirm);
	run_native_confirm->connect(SceneStringName(confirmed), callable_mp(this, &EditorRunNative::_confirm_run_native));

	set_process(true);
}
