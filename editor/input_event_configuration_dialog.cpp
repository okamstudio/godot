/**************************************************************************/
/*  input_event_configuration_dialog.cpp                                  */
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

#include "editor/input_event_configuration_dialog.h"
#include "core/input/input_map.h"
#include "editor/editor_string_names.h"
#include "editor/event_listener_line_edit.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/check_box.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/option_button.h"
#include "scene/gui/separator.h"
#include "scene/gui/tree.h"

void InputEventConfigurationDialog::_set_event(const Ref<InputEvent> &p_event, const Ref<InputEvent> &p_original_event, bool p_update_input_list_selection) {
	if (p_event.is_valid()) {
		event = p_event;
		original_event = p_original_event;

		// If the event is changed to something which is not the same as the listener,
		// clear out the event from the listener text box to avoid confusion.
		const Ref<InputEvent> listener_event = event_listener->get_event();
		if (listener_event.is_valid() && !listener_event->is_match(p_event)) {
			event_listener->clear_event();
		}

		// Update Label
		event_as_text->set_text(EventListenerLineEdit::get_event_text(event, true));

		Ref<InputEventKey> k = p_event;
		Ref<InputEventMouseButton> mb = p_event;
		Ref<InputEventJoypadButton> joyb = p_event;
		Ref<InputEventJoypadMotion> joym = p_event;
		Ref<InputEventWithModifiers> mod = p_event;

		// Update option values and visibility
		bool show_mods = false;
		bool show_device = false;
		bool show_key = false;
		bool show_location = false;

		if (mod.is_valid()) {
			show_mods = true;
			mod_checkboxes[MOD_ALT]->set_pressed(mod->is_alt_pressed());
			mod_checkboxes[MOD_SHIFT]->set_pressed(mod->is_shift_pressed());
			mod_checkboxes[MOD_CTRL]->set_pressed(mod->is_ctrl_pressed());
			mod_checkboxes[MOD_META]->set_pressed(mod->is_meta_pressed());

			autoremap_command_or_control_checkbox->set_pressed(mod->is_command_or_control_autoremap());
		}

		if (k.is_valid()) {
			show_key = true;
			Key phys_key = k->get_physical_keycode();
			if (k->get_keycode() == Key::NONE && phys_key == Key::NONE && k->get_key_label() != Key::NONE) {
				key_mode->select(KEYMODE_UNICODE);
			} else if (k->get_keycode() != Key::NONE) {
				key_mode->select(KEYMODE_KEYCODE);
			} else if (phys_key != Key::NONE) {
				key_mode->select(KEYMODE_PHY_KEYCODE);
				if (phys_key == Key::SHIFT || phys_key == Key::CTRL || phys_key == Key::ALT || phys_key == Key::META) {
					key_location->select((int)k->get_location());
					show_location = true;
				}
			} else {
				// Invalid key.
				event = Ref<InputEvent>();
				original_event = Ref<InputEvent>();
				event_listener->clear_event();
				event_as_text->set_text(TTR("No Event Configured"));

				additional_options_container->hide();
				input_list_tree->deselect_all();
				_update_input_list();
				return;
			}
		} else if (joyb.is_valid() || joym.is_valid() || mb.is_valid()) {
			show_device = true;
			_set_current_device(event->get_device());
		}

		mod_container->set_visible(show_mods);
		device_container->set_visible(show_device);
		key_mode->set_visible(show_key);
		location_container->set_visible(show_location);
		additional_options_container->show();

		// Update mode selector based on original key event.
		Ref<InputEventKey> ko = p_original_event;
		if (ko.is_valid()) {
			if (ko->get_keycode() == Key::NONE) {
				if (ko->get_physical_keycode() != Key::NONE) {
					ko->set_keycode(ko->get_physical_keycode());
				}
				if (ko->get_key_label() != Key::NONE) {
					ko->set_keycode(fix_keycode((char32_t)ko->get_key_label(), Key::NONE));
				}
			}

			if (ko->get_physical_keycode() == Key::NONE) {
				if (ko->get_keycode() != Key::NONE) {
					ko->set_physical_keycode(ko->get_keycode());
				}
				if (ko->get_key_label() != Key::NONE) {
					ko->set_physical_keycode(fix_keycode((char32_t)ko->get_key_label(), Key::NONE));
				}
			}

			if (ko->get_key_label() == Key::NONE) {
				if (ko->get_keycode() != Key::NONE) {
					ko->set_key_label(fix_key_label((char32_t)ko->get_keycode(), Key::NONE));
				}
				if (ko->get_physical_keycode() != Key::NONE) {
					ko->set_key_label(fix_key_label((char32_t)ko->get_physical_keycode(), Key::NONE));
				}
			}

			key_mode->set_item_disabled(KEYMODE_KEYCODE, ko->get_keycode() == Key::NONE);
			key_mode->set_item_disabled(KEYMODE_PHY_KEYCODE, ko->get_physical_keycode() == Key::NONE);
			key_mode->set_item_disabled(KEYMODE_UNICODE, ko->get_key_label() == Key::NONE);
		}

		// Update selected item in input list.
		if (p_update_input_list_selection && (k.is_valid() || joyb.is_valid() || joym.is_valid() || mb.is_valid())) {
			in_tree_update = true;
			TreeItem *category = input_list_tree->get_root()->get_first_child();
			while (category) {
				TreeItem *input_item = category->get_first_child();

				if (input_item != nullptr) {
					// input_type should always be > 0, unless the tree structure has been misconfigured.
					int input_type = input_item->get_parent()->get_meta("__type", 0);
					if (input_type == 0) {
						in_tree_update = false;
						return;
					}

					// If event type matches input types of this category.
					if ((k.is_valid() && input_type == INPUT_KEY) || (joyb.is_valid() && input_type == INPUT_JOY_BUTTON) || (joym.is_valid() && input_type == INPUT_JOY_MOTION) || (mb.is_valid() && input_type == INPUT_MOUSE_BUTTON)) {
						// Loop through all items of this category until one matches.
						while (input_item) {
							bool key_match = k.is_valid() && (Variant(k->get_keycode()) == input_item->get_meta("__keycode") || Variant(k->get_physical_keycode()) == input_item->get_meta("__keycode"));
							bool joyb_match = joyb.is_valid() && Variant(joyb->get_button_index()) == input_item->get_meta("__index");
							bool joym_match = joym.is_valid() && Variant(joym->get_axis()) == input_item->get_meta("__axis") && joym->get_axis_value() == (float)input_item->get_meta("__value");
							bool mb_match = mb.is_valid() && Variant(mb->get_button_index()) == input_item->get_meta("__index");
							if (key_match || joyb_match || joym_match || mb_match) {
								category->set_collapsed(false);
								input_item->select(0);
								input_list_tree->ensure_cursor_is_visible();
								in_tree_update = false;
								return;
							}
							input_item = input_item->get_next();
						}
					}
				}

				category->set_collapsed(true); // Event not in this category, so collapse;
				category = category->get_next();
			}
			in_tree_update = false;
		}
	} else {
		// Event is not valid, reset dialog
		event = Ref<InputEvent>();
		original_event = Ref<InputEvent>();
		event_listener->clear_event();
		event_as_text->set_text(TTR("No Event Configured"));

		additional_options_container->hide();
		input_list_tree->deselect_all();
		_update_input_list();
	}
}

void InputEventConfigurationDialog::_on_listen_input_changed(const Ref<InputEvent> &p_event) {
	// Ignore if invalid, echo or not pressed
	if (p_event.is_null() || p_event->is_echo() || !p_event->is_pressed()) {
		return;
	}

	// Create an editable reference and a copy of full event.
	Ref<InputEvent> received_event = p_event;
	Ref<InputEvent> received_original_event = received_event->duplicate();

	// Check what the type is and if it is allowed.
	Ref<InputEventKey> k = received_event;
	Ref<InputEventJoypadButton> joyb = received_event;
	Ref<InputEventJoypadMotion> joym = received_event;
	Ref<InputEventMouseButton> mb = received_event;

	int type = 0;
	if (k.is_valid()) {
		type = INPUT_KEY;
	} else if (joyb.is_valid()) {
		type = INPUT_JOY_BUTTON;
	} else if (joym.is_valid()) {
		type = INPUT_JOY_MOTION;
	} else if (mb.is_valid()) {
		type = INPUT_MOUSE_BUTTON;
	}

	if (!(allowed_input_types & type)) {
		return;
	}

	if (joym.is_valid()) {
		joym->set_axis_value(SIGN(joym->get_axis_value()));
	}

	if (k.is_valid()) {
		k->set_pressed(false); // To avoid serialization of 'pressed' property - doesn't matter for actions anyway.
		if (key_mode->get_selected_id() == KEYMODE_KEYCODE) {
			k->set_physical_keycode(Key::NONE);
			k->set_key_label(Key::NONE);
		} else if (key_mode->get_selected_id() == KEYMODE_PHY_KEYCODE) {
			k->set_keycode(Key::NONE);
			k->set_key_label(Key::NONE);
		} else if (key_mode->get_selected_id() == KEYMODE_UNICODE) {
			k->set_physical_keycode(Key::NONE);
			k->set_keycode(Key::NONE);
		}
		if (key_location->get_selected_id() == (int)KeyLocation::UNSPECIFIED) {
			k->set_location(KeyLocation::UNSPECIFIED);
		}
	}

	Ref<InputEventWithModifiers> mod = received_event;
	if (mod.is_valid()) {
		mod->set_window_id(0);
	}

	// Maintain device selection.
	received_event->set_device(_get_current_device());

	_set_event(received_event, received_original_event);
}

void InputEventConfigurationDialog::_search_term_updated(const String &) {
	_update_input_list();
}

void InputEventConfigurationDialog::_update_input_list() {
	input_list_tree->clear();

	TreeItem *root = input_list_tree->create_item();
	String search_term = input_list_search->get_text();

	bool collapse = input_list_search->get_text().is_empty();

	if (allowed_input_types & INPUT_KEY) {
		TreeItem *kb_root = input_list_tree->create_item(root);
		kb_root->set_text(0, TTR("Keyboard Keys"));
		kb_root->set_icon(0, icon_cache.keyboard);
		kb_root->set_collapsed(collapse);
		kb_root->set_meta("__type", INPUT_KEY);

		for (int i = 0; i < keycode_get_count(); i++) {
			String name = keycode_get_name_by_index(i);

			if (!search_term.is_empty() && !name.containsn(search_term)) {
				continue;
			}

			TreeItem *item = input_list_tree->create_item(kb_root);
			item->set_text(0, name);
			item->set_meta("__keycode", keycode_get_value_by_index(i));
		}
	}

	if (allowed_input_types & INPUT_MOUSE_BUTTON) {
		TreeItem *mouse_root = input_list_tree->create_item(root);
		mouse_root->set_text(0, TTR("Mouse Buttons"));
		mouse_root->set_icon(0, icon_cache.mouse);
		mouse_root->set_collapsed(collapse);
		mouse_root->set_meta("__type", INPUT_MOUSE_BUTTON);

		MouseButton mouse_buttons[9] = { MouseButton::LEFT, MouseButton::RIGHT, MouseButton::MIDDLE, MouseButton::WHEEL_UP, MouseButton::WHEEL_DOWN, MouseButton::WHEEL_LEFT, MouseButton::WHEEL_RIGHT, MouseButton::MB_XBUTTON1, MouseButton::MB_XBUTTON2 };
		for (int i = 0; i < 9; i++) {
			Ref<InputEventMouseButton> mb;
			mb.instantiate();
			mb->set_button_index(mouse_buttons[i]);
			String desc = EventListenerLineEdit::get_event_text(mb, false);

			if (!search_term.is_empty() && !desc.containsn(search_term)) {
				continue;
			}

			TreeItem *item = input_list_tree->create_item(mouse_root);
			item->set_text(0, desc);
			item->set_meta("__index", mouse_buttons[i]);
		}
	}

	if (allowed_input_types & INPUT_JOY_BUTTON) {
		TreeItem *joyb_root = input_list_tree->create_item(root);
		joyb_root->set_text(0, TTR("Joypad Buttons"));
		joyb_root->set_icon(0, icon_cache.joypad_button);
		joyb_root->set_collapsed(collapse);
		joyb_root->set_meta("__type", INPUT_JOY_BUTTON);

		for (int i = 0; i < (int)JoyButton::MAX; i++) {
			Ref<InputEventJoypadButton> joyb;
			joyb.instantiate();
			joyb->set_button_index((JoyButton)i);
			String desc = EventListenerLineEdit::get_event_text(joyb, false);

			if (!search_term.is_empty() && !desc.containsn(search_term)) {
				continue;
			}

			TreeItem *item = input_list_tree->create_item(joyb_root);
			item->set_text(0, desc);
			item->set_meta("__index", i);
		}
	}

	if (allowed_input_types & INPUT_JOY_MOTION) {
		TreeItem *joya_root = input_list_tree->create_item(root);
		joya_root->set_text(0, TTR("Joypad Axes"));
		joya_root->set_icon(0, icon_cache.joypad_axis);
		joya_root->set_collapsed(collapse);
		joya_root->set_meta("__type", INPUT_JOY_MOTION);

		for (int i = 0; i < (int)JoyAxis::MAX * 2; i++) {
			int axis = i / 2;
			int direction = (i & 1) ? 1 : -1;
			Ref<InputEventJoypadMotion> joym;
			joym.instantiate();
			joym->set_axis((JoyAxis)axis);
			joym->set_axis_value(direction);
			String desc = EventListenerLineEdit::get_event_text(joym, false);

			if (!search_term.is_empty() && !desc.containsn(search_term)) {
				continue;
			}

			TreeItem *item = input_list_tree->create_item(joya_root);
			item->set_text(0, desc);
			item->set_meta("__axis", i >> 1);
			item->set_meta("__value", (i & 1) ? 1 : -1);
		}
	}
}

void InputEventConfigurationDialog::_mod_toggled(bool p_checked, int p_index) {
	Ref<InputEventWithModifiers> ie = event;

	// Not event with modifiers
	if (ie.is_null()) {
		return;
	}

	if (p_index == 0) {
		ie->set_alt_pressed(p_checked);
	} else if (p_index == 1) {
		ie->set_shift_pressed(p_checked);
	} else if (p_index == 2) {
		if (!autoremap_command_or_control_checkbox->is_pressed()) {
			ie->set_ctrl_pressed(p_checked);
		}
	} else if (p_index == 3) {
		if (!autoremap_command_or_control_checkbox->is_pressed()) {
			ie->set_meta_pressed(p_checked);
		}
	}

	_set_event(ie, original_event);
}

void InputEventConfigurationDialog::_autoremap_command_or_control_toggled(bool p_checked) {
	Ref<InputEventWithModifiers> ie = event;
	if (ie.is_valid()) {
		ie->set_command_or_control_autoremap(p_checked);
		_set_event(ie, original_event);
	}

	if (p_checked) {
		mod_checkboxes[MOD_META]->hide();
		mod_checkboxes[MOD_CTRL]->hide();
	} else {
		mod_checkboxes[MOD_META]->show();
		mod_checkboxes[MOD_CTRL]->show();
	}
}

void InputEventConfigurationDialog::_key_mode_selected(int p_mode) {
	Ref<InputEventKey> k = event;
	Ref<InputEventKey> ko = original_event;
	if (k.is_null() || ko.is_null()) {
		return;
	}

	if (key_mode->get_selected_id() == KEYMODE_KEYCODE) {
		k->set_keycode(ko->get_keycode());
		k->set_physical_keycode(Key::NONE);
		k->set_key_label(Key::NONE);
	} else if (key_mode->get_selected_id() == KEYMODE_PHY_KEYCODE) {
		k->set_keycode(Key::NONE);
		k->set_physical_keycode(ko->get_physical_keycode());
		k->set_key_label(Key::NONE);
	} else if (key_mode->get_selected_id() == KEYMODE_UNICODE) {
		k->set_physical_keycode(Key::NONE);
		k->set_keycode(Key::NONE);
		k->set_key_label(ko->get_key_label());
	}

	_set_event(k, original_event);
}

void InputEventConfigurationDialog::_key_location_selected(int p_location) {
	Ref<InputEventKey> k = event;
	if (k.is_null()) {
		return;
	}

	k->set_location((KeyLocation)p_location);

	_set_event(k, original_event);
}

void InputEventConfigurationDialog::_input_list_item_activated() {
	TreeItem *selected = input_list_tree->get_selected();
	selected->set_collapsed(!selected->is_collapsed());
}

void InputEventConfigurationDialog::_input_list_item_selected() {
	TreeItem *selected = input_list_tree->get_selected();

	// Called form _set_event, do not update for a second time.
	if (in_tree_update) {
		return;
	}

	// Invalid tree selection - type only exists on the "category" items, which are not a valid selection.
	if (selected->has_meta("__type")) {
		return;
	}

	InputType input_type = (InputType)(int)selected->get_parent()->get_meta("__type");

	switch (input_type) {
		case INPUT_KEY: {
			Key keycode = (Key)(int)selected->get_meta("__keycode");
			Ref<InputEventKey> k;
			k.instantiate();

			k->set_physical_keycode(keycode);
			k->set_keycode(keycode);
			k->set_key_label(keycode);

			// Maintain modifier state from checkboxes.
			k->set_alt_pressed(mod_checkboxes[MOD_ALT]->is_pressed());
			k->set_shift_pressed(mod_checkboxes[MOD_SHIFT]->is_pressed());
			if (autoremap_command_or_control_checkbox->is_pressed()) {
				k->set_command_or_control_autoremap(true);
			} else {
				k->set_ctrl_pressed(mod_checkboxes[MOD_CTRL]->is_pressed());
				k->set_meta_pressed(mod_checkboxes[MOD_META]->is_pressed());
			}

			Ref<InputEventKey> ko = k->duplicate();

			if (key_mode->get_selected_id() == KEYMODE_UNICODE) {
				key_mode->select(KEYMODE_PHY_KEYCODE);
			}

			if (key_mode->get_selected_id() == KEYMODE_KEYCODE) {
				k->set_physical_keycode(Key::NONE);
				k->set_keycode(keycode);
				k->set_key_label(Key::NONE);
			} else if (key_mode->get_selected_id() == KEYMODE_PHY_KEYCODE) {
				k->set_physical_keycode(keycode);
				k->set_keycode(Key::NONE);
				k->set_key_label(Key::NONE);
			}

			_set_event(k, ko, false);
		} break;
		case INPUT_MOUSE_BUTTON: {
			MouseButton idx = (MouseButton)(int)selected->get_meta("__index");
			Ref<InputEventMouseButton> mb;
			mb.instantiate();
			mb->set_button_index(idx);
			// Maintain modifier state from checkboxes
			mb->set_alt_pressed(mod_checkboxes[MOD_ALT]->is_pressed());
			mb->set_shift_pressed(mod_checkboxes[MOD_SHIFT]->is_pressed());
			if (autoremap_command_or_control_checkbox->is_pressed()) {
				mb->set_command_or_control_autoremap(true);
			} else {
				mb->set_ctrl_pressed(mod_checkboxes[MOD_CTRL]->is_pressed());
				mb->set_meta_pressed(mod_checkboxes[MOD_META]->is_pressed());
			}

			// Maintain selected device
			mb->set_device(_get_current_device());

			_set_event(mb, mb, false);
		} break;
		case INPUT_JOY_BUTTON: {
			JoyButton idx = (JoyButton)(int)selected->get_meta("__index");
			Ref<InputEventJoypadButton> jb = InputEventJoypadButton::create_reference(idx);

			// Maintain selected device
			jb->set_device(_get_current_device());

			_set_event(jb, jb, false);
		} break;
		case INPUT_JOY_MOTION: {
			JoyAxis axis = (JoyAxis)(int)selected->get_meta("__axis");
			int value = selected->get_meta("__value");

			Ref<InputEventJoypadMotion> jm;
			jm.instantiate();
			jm->set_axis(axis);
			jm->set_axis_value(value);

			// Maintain selected device
			jm->set_device(_get_current_device());

			_set_event(jm, jm, false);
		} break;
	}
}

void InputEventConfigurationDialog::_device_selection_changed(int p_option_button_index) {
	// Subtract 1 as option index 0 corresponds to "All Devices" (value of -1)
	// and option index 1 corresponds to device 0, etc...
	event->set_device(p_option_button_index - 1);
	event_as_text->set_text(EventListenerLineEdit::get_event_text(event, true));
}

void InputEventConfigurationDialog::_set_current_device(int p_device) {
	device_id_option->select(p_device + 1);
}

int InputEventConfigurationDialog::_get_current_device() const {
	return device_id_option->get_selected() - 1;
}

void InputEventConfigurationDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_VISIBILITY_CHANGED: {
			event_listener->grab_focus();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			input_list_search->set_right_icon(get_editor_theme_icon(SNAME("Search")));

			key_mode->set_item_icon(KEYMODE_KEYCODE, get_editor_theme_icon(SNAME("Keyboard")));
			key_mode->set_item_icon(KEYMODE_PHY_KEYCODE, get_editor_theme_icon(SNAME("KeyboardPhysical")));
			key_mode->set_item_icon(KEYMODE_UNICODE, get_editor_theme_icon(SNAME("KeyboardLabel")));

			icon_cache.keyboard = get_editor_theme_icon(SNAME("Keyboard"));
			icon_cache.mouse = get_editor_theme_icon(SNAME("Mouse"));
			icon_cache.joypad_button = get_editor_theme_icon(SNAME("JoyButton"));
			icon_cache.joypad_axis = get_editor_theme_icon(SNAME("JoyAxis"));

			event_as_text->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("bold"), EditorStringName(EditorFonts)));

			_update_input_list();
		} break;
	}
}

void InputEventConfigurationDialog::popup_and_configure(const Ref<InputEvent> &p_event, const String &p_current_action_name) {
	if (p_event.is_valid()) {
		_set_event(p_event->duplicate(), p_event->duplicate());
	} else {
		// Clear Event
		_set_event(Ref<InputEvent>(), Ref<InputEvent>());

		// Clear Checkbox Values
		for (int i = 0; i < MOD_MAX; i++) {
			mod_checkboxes[i]->set_pressed(false);
		}

		// Enable the Physical Key by default to encourage its use.
		// Physical Key should be used for most game inputs as it allows keys to work
		// on non-QWERTY layouts out of the box.
		// This is especially important for WASD movement layouts.

		key_mode->select(KEYMODE_PHY_KEYCODE);
		autoremap_command_or_control_checkbox->set_pressed(false);

		// Select "All Devices" by default.
		device_id_option->select(0);
		// Also "all locations".
		key_location->select(0);
	}

	if (!p_current_action_name.is_empty()) {
		set_title(vformat(TTR("Event Configuration for \"%s\""), p_current_action_name));
	} else {
		set_title(TTR("Event Configuration"));
	}

	popup_centered(Size2(0, 400) * EDSCALE);
}

Ref<InputEvent> InputEventConfigurationDialog::get_event() const {
	return event;
}

void InputEventConfigurationDialog::set_allowed_input_types(int p_type_masks) {
	allowed_input_types = p_type_masks;
	event_listener->set_allowed_input_types(p_type_masks);
}

InputEventConfigurationDialog::InputEventConfigurationDialog() {
	allowed_input_types = INPUT_KEY | INPUT_MOUSE_BUTTON | INPUT_JOY_BUTTON | INPUT_JOY_MOTION;

	set_min_size(Size2i(550, 0) * EDSCALE);

	VBoxContainer *main_vbox = memnew(VBoxContainer);
	add_child(main_vbox);

	event_as_text = memnew(Label);
	event_as_text->set_custom_minimum_size(Size2(500, 0) * EDSCALE);
	event_as_text->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	event_as_text->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	event_as_text->add_theme_font_size_override(SceneStringName(font_size), 18 * EDSCALE);
	main_vbox->add_child(event_as_text);

	event_listener = memnew(EventListenerLineEdit);
	event_listener->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	event_listener->set_stretch_ratio(0.75);
	event_listener->connect("event_changed", callable_mp(this, &InputEventConfigurationDialog::_on_listen_input_changed));
	event_listener->connect(SceneStringName(focus_entered), callable_mp((AcceptDialog *)this, &AcceptDialog::set_close_on_escape).bind(false));
	event_listener->connect(SceneStringName(focus_exited), callable_mp((AcceptDialog *)this, &AcceptDialog::set_close_on_escape).bind(true));
	main_vbox->add_child(event_listener);

	main_vbox->add_child(memnew(HSeparator));

	// List of all input options to manually select from.
	VBoxContainer *manual_vbox = memnew(VBoxContainer);
	manual_vbox->set_name(TTR("Manual Selection"));
	manual_vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_vbox->add_child(manual_vbox);

	input_list_search = memnew(LineEdit);
	input_list_search->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	input_list_search->set_placeholder(TTR("Filter Inputs"));
	input_list_search->set_clear_button_enabled(true);
	input_list_search->connect(SceneStringName(text_changed), callable_mp(this, &InputEventConfigurationDialog::_search_term_updated));
	manual_vbox->add_child(input_list_search);

	input_list_tree = memnew(Tree);
	input_list_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	input_list_tree->set_custom_minimum_size(Size2(0, 100 * EDSCALE)); // Min height for tree
	input_list_tree->connect("item_activated", callable_mp(this, &InputEventConfigurationDialog::_input_list_item_activated));
	input_list_tree->connect(SceneStringName(item_selected), callable_mp(this, &InputEventConfigurationDialog::_input_list_item_selected));
	input_list_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	manual_vbox->add_child(input_list_tree);

	input_list_tree->set_hide_root(true);
	input_list_tree->set_columns(1);

	_update_input_list();

	// Additional Options
	additional_options_container = memnew(VBoxContainer);
	additional_options_container->hide();

	Label *opts_label = memnew(Label);
	opts_label->set_theme_type_variation("HeaderSmall");
	opts_label->set_text(TTR("Additional Options"));
	additional_options_container->add_child(opts_label);

	// Device Selection
	device_container = memnew(HBoxContainer);
	device_container->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	Label *device_label = memnew(Label);
	device_label->set_theme_type_variation("HeaderSmall");
	device_label->set_text(TTR("Device:"));
	device_container->add_child(device_label);

	device_id_option = memnew(OptionButton);
	device_id_option->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	for (int i = -1; i < 8; i++) {
		device_id_option->add_item(EventListenerLineEdit::get_device_string(i));
	}
	device_id_option->connect(SceneStringName(item_selected), callable_mp(this, &InputEventConfigurationDialog::_device_selection_changed));
	_set_current_device(InputMap::ALL_DEVICES);
	device_container->add_child(device_id_option);

	device_container->hide();
	additional_options_container->add_child(device_container);

	// Modifier Selection
	mod_container = memnew(HBoxContainer);
	for (int i = 0; i < MOD_MAX; i++) {
		String name = mods[i];
		mod_checkboxes[i] = memnew(CheckBox);
		mod_checkboxes[i]->connect(SceneStringName(toggled), callable_mp(this, &InputEventConfigurationDialog::_mod_toggled).bind(i));
		mod_checkboxes[i]->set_text(name);
		mod_checkboxes[i]->set_tooltip_text(TTR(mods_tip[i]));
		mod_container->add_child(mod_checkboxes[i]);
	}

	mod_container->add_child(memnew(VSeparator));

	autoremap_command_or_control_checkbox = memnew(CheckBox);
	autoremap_command_or_control_checkbox->connect(SceneStringName(toggled), callable_mp(this, &InputEventConfigurationDialog::_autoremap_command_or_control_toggled));
	autoremap_command_or_control_checkbox->set_pressed(false);
	autoremap_command_or_control_checkbox->set_text(TTR("Command / Control (auto)"));
	autoremap_command_or_control_checkbox->set_tooltip_text(TTR("Automatically remaps between 'Meta' ('Command') and 'Control' depending on current platform."));
	mod_container->add_child(autoremap_command_or_control_checkbox);

	mod_container->hide();
	additional_options_container->add_child(mod_container);

	// Key Mode Selection

	key_mode = memnew(OptionButton);
	key_mode->add_item(TTR("Keycode (Latin Equivalent)"), KEYMODE_KEYCODE);
	key_mode->add_item(TTR("Physical Keycode (Position on US QWERTY Keyboard)"), KEYMODE_PHY_KEYCODE);
	key_mode->add_item(TTR("Key Label (Unicode, Case-Insensitive)"), KEYMODE_UNICODE);
	key_mode->connect(SceneStringName(item_selected), callable_mp(this, &InputEventConfigurationDialog::_key_mode_selected));
	key_mode->hide();
	additional_options_container->add_child(key_mode);

	// Key Location Selection

	location_container = memnew(HBoxContainer);
	location_container->hide();

	Label *location_label = memnew(Label);
	location_label->set_text(TTR("Physical location"));
	location_container->add_child(location_label);

	key_location = memnew(OptionButton);
	key_location->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	key_location->add_item(TTR("Any"), (int)KeyLocation::UNSPECIFIED);
	key_location->add_item(TTR("Left"), (int)KeyLocation::LEFT);
	key_location->add_item(TTR("Right"), (int)KeyLocation::RIGHT);
	key_location->connect(SceneStringName(item_selected), callable_mp(this, &InputEventConfigurationDialog::_key_location_selected));

	location_container->add_child(key_location);
	additional_options_container->add_child(location_container);

	main_vbox->add_child(additional_options_container);
}
