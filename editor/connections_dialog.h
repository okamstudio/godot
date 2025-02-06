/**************************************************************************/
/*  connections_dialog.h                                                  */
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

#include "scene/gui/check_button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/tree.h"

class Button;
class CheckBox;
class ConnectDialogBinds;
class EditorInspector;
class Label;
class LineEdit;
class OptionButton;
class PopupMenu;
class SceneTreeEditor;
class SpinBox;

class ConnectDialog : public ConfirmationDialog {
	GDCLASS(ConnectDialog, ConfirmationDialog);

public:
	struct ConnectionData {
		Node *source = nullptr;
		Node *target = nullptr;
		StringName signal;
		StringName method;
		uint32_t flags = 0;
		int unbinds = 0;
		Vector<Variant> binds;

		ConnectionData() {}

		ConnectionData(const Connection &p_connection) {
			source = Object::cast_to<Node>(p_connection.signal.get_object());
			signal = p_connection.signal.get_name();
			target = Object::cast_to<Node>(p_connection.callable.get_object());
			flags = p_connection.flags;

			Callable base_callable;
			if (p_connection.callable.is_custom()) {
				CallableCustomBind *ccb = dynamic_cast<CallableCustomBind *>(p_connection.callable.get_custom());
				if (ccb) {
					binds = ccb->get_binds();
					base_callable = ccb->get_callable();
				}

				CallableCustomUnbind *ccu = dynamic_cast<CallableCustomUnbind *>(p_connection.callable.get_custom());
				if (ccu) {
					unbinds = ccu->get_unbinds();
					base_callable = ccu->get_callable();
				}
			} else {
				base_callable = p_connection.callable;
			}
			method = base_callable.get_method();
		}

		Callable get_callable() const {
			if (unbinds > 0) {
				return Callable(target, method).unbind(unbinds);
			} else if (!binds.is_empty()) {
				const Variant **argptrs = (const Variant **)alloca(sizeof(Variant *) * binds.size());
				for (int i = 0; i < binds.size(); i++) {
					argptrs[i] = &binds[i];
				}
				return Callable(target, method).bindp(argptrs, binds.size());
			} else {
				return Callable(target, method);
			}
		}
	};

private:
	Label *connect_to_label = nullptr;
	LineEdit *from_signal = nullptr;
	LineEdit *filter_nodes = nullptr;
	Node *source = nullptr;
	ConnectionData source_connection_data;
	StringName signal;
	PackedStringArray signal_args;
	LineEdit *dst_method = nullptr;
	ConnectDialogBinds *cdbinds = nullptr;
	bool edit_mode = false;
	bool first_popup = true;
	NodePath dst_path;
	VBoxContainer *vbc_right = nullptr;
	SceneTreeEditor *tree = nullptr;
	AcceptDialog *error = nullptr;

	Button *open_method_tree = nullptr;
	AcceptDialog *method_popup = nullptr;
	Tree *method_tree = nullptr;
	Label *empty_tree_label = nullptr;
	LineEdit *method_search = nullptr;
	CheckButton *script_methods_only = nullptr;
	CheckButton *compatible_methods_only = nullptr;

	SpinBox *unbind_count = nullptr;
	EditorInspector *bind_editor = nullptr;
	OptionButton *type_list = nullptr;
	CheckBox *deferred = nullptr;
	CheckBox *one_shot = nullptr;
	CheckButton *advanced = nullptr;
	Vector<Control *> bind_controls;

	Label *warning_label = nullptr;
	Label *error_label = nullptr;

	void ok_pressed() override;
	void _cancel_pressed();
	void _item_activated();
	void _tree_node_selected();
	void _focus_currently_connected();

	void _method_selected();
	void _create_method_tree_items(const List<MethodInfo> &p_methods, TreeItem *p_parent_item);
	List<MethodInfo> _filter_method_list(const List<MethodInfo> &p_methods, const MethodInfo &p_signal, const String &p_search_string) const;
	void _update_method_tree();
	void _method_check_button_pressed(const CheckButton *p_button);
	void _open_method_popup();

	void _unbind_count_changed(double p_count);
	void _add_bind();
	void _remove_bind();
	void _advanced_pressed();
	void _update_ok_enabled();
	void _update_warning_label();

protected:
	virtual void _post_popup() override;
	void _notification(int p_what);
	static void _bind_methods();

public:
	static StringName generate_method_callback_name(Node *p_source, const String &p_signal_name, Node *p_target);
	Node *get_source() const;
	ConnectionData get_source_connection_data() const;
	StringName get_signal_name() const;
	PackedStringArray get_signal_args() const;
	NodePath get_dst_path() const;
	void set_dst_node(Node *p_node);
	StringName get_dst_method_name() const;
	void set_dst_method(const StringName &p_method);
	int get_unbinds() const;
	Vector<Variant> get_binds() const;
	String get_signature(const MethodInfo &p_method, PackedStringArray *r_arg_names = nullptr);

	bool get_deferred() const;
	bool get_one_shot() const;
	bool is_editing() const;

	virtual void shortcut_input(const Ref<InputEvent> &p_event) override;

	void init(const ConnectionData &p_cd, const PackedStringArray &p_signal_args, bool p_edit = false);

	void popup_dialog(const String &p_for_signal);
	ConnectDialog();
	~ConnectDialog();
};

//////////////////////////////////////////

// Custom `Tree` needed to use `EditorHelpBit` to display signal documentation.
class ConnectionsDockTree : public Tree {
	virtual Control *make_custom_tooltip(const String &p_text) const;
};

class ConnectionsDock : public VBoxContainer {
	GDCLASS(ConnectionsDock, VBoxContainer);

	enum TreeItemType {
		TREE_ITEM_TYPE_ROOT,
		TREE_ITEM_TYPE_CLASS,
		TREE_ITEM_TYPE_SIGNAL,
		TREE_ITEM_TYPE_CONNECTION,
	};

	// Right-click context menu options.
	enum ClassMenuOption {
		CLASS_MENU_OPEN_DOCS,
	};
	enum SignalMenuOption {
		SIGNAL_MENU_CONNECT,
		SIGNAL_MENU_DISCONNECT_ALL,
		SIGNAL_MENU_COPY_NAME,
		SIGNAL_MENU_OPEN_DOCS,
	};
	enum SlotMenuOption {
		SLOT_MENU_EDIT,
		SLOT_MENU_GO_TO_METHOD,
		SLOT_MENU_DISCONNECT,
	};

	Node *selected_node = nullptr;
	ConnectionsDockTree *tree = nullptr;

	ConfirmationDialog *disconnect_all_dialog = nullptr;
	ConnectDialog *connect_dialog = nullptr;
	Button *connect_button = nullptr;
	PopupMenu *class_menu = nullptr;
	String class_menu_doc_class_name;
	PopupMenu *signal_menu = nullptr;
	PopupMenu *slot_menu = nullptr;
	LineEdit *search_box = nullptr;

	void _filter_changed(const String &p_text);

	void _make_or_edit_connection();
	void _connect(const ConnectDialog::ConnectionData &p_cd);
	void _disconnect(const ConnectDialog::ConnectionData &p_cd);
	void _disconnect_all();

	void _tree_item_selected();
	void _tree_item_activated();
	TreeItemType _get_item_type(const TreeItem &p_item) const;
	bool _is_connection_inherited(Connection &p_connection);

	void _open_connection_dialog(TreeItem &p_item);
	void _open_edit_connection_dialog(TreeItem &p_item);
	void _go_to_method(TreeItem &p_item);

	void _handle_class_menu_option(int p_option);
	void _class_menu_about_to_popup();
	void _handle_signal_menu_option(int p_option);
	void _signal_menu_about_to_popup();
	void _handle_slot_menu_option(int p_option);
	void _slot_menu_about_to_popup();
	void _tree_gui_input(const Ref<InputEvent> &p_event);
	void _close();

protected:
	void _connect_pressed();
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_node(Node *p_node);
	void update_tree();

	ConnectionsDock();
	~ConnectionsDock();
};
