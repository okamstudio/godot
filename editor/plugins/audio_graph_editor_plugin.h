/**************************************************************************/
/*  audio_graph_editor_plugin.h                                           */
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

#ifndef AUDIO_GRAPH_EDITOR_PLUGIN
#define AUDIO_GRAPH_EDITOR_PLUGIN

#include "editor/plugins/editor_plugin.h"
#include "scene/gui/graph_edit.h"
#include "scene/resources/audio_stream_graph.h"

class Button;
class EditorFileDialog;
class ScrollContainer;
class Tree;
class AcceptDialog;
class LineEdit;
class RichTextLabel;
class MenuButton;
class ConfirmationDialog;
class GraphEdit;
class ConfirmationDialog;
class PopupPanel;
class EditorProperty;

class AudioGraphEditorPlugin;
class AudioGraphEditor;

class AudioGraphNodePlugin : public RefCounted {
	GDCLASS(AudioGraphNodePlugin, RefCounted);

protected:
	AudioGraphEditor *ageditor = nullptr;

protected:
	static void _bind_methods();

	GDVIRTUAL2RC(Object *, _create_editor, Ref<Resource>, Ref<AudioGraphNodePlugin>)

public:
	void set_editor(AudioGraphEditor *p_editor);
	virtual Control *create_editor(const Ref<Resource> &p_parent_resource, const Ref<AudioStreamGraphNode> &p_node);
};

class AudioGraphNodePluginDefault : public AudioGraphNodePlugin {
	GDCLASS(AudioGraphNodePluginDefault, AudioGraphNodePlugin);

public:
	virtual Control *create_editor(const Ref<Resource> &p_parent_resource, const Ref<AudioStreamGraphNode> &p_node) override;
};

class AudioGraphEditedProperty : public RefCounted {
	GDCLASS(AudioGraphEditedProperty, RefCounted);

private:
	Variant edited_property;

protected:
	static void _bind_methods();

public:
	void set_edited_property(const Variant &p_variant);
	Variant get_edited_property() const;

	AudioGraphEditedProperty() {}
};

class AudioGraphEditor : public Control {
	GDCLASS(AudioGraphEditor, Control);
	friend class AudioGraphEditorPlugin;

	Vector<String> button_path;
	Vector<String> edited_path;

	void _update_path();
	void _clear_editors();
	ObjectID current_root;

	void _path_button_pressed(int p_path);

	PopupPanel *property_editor_popup = nullptr;
	EditorProperty *property_editor = nullptr;
	int editing_node = -1;
	int editing_port = -1;
	Ref<AudioGraphEditedProperty> edited_property_holder;

	AudioGraphEditorPlugin *graph_plugin = nullptr;

	GraphEdit *graph = nullptr;
	Button *add_node = nullptr;
	Tree *members = nullptr;
	Tree *parameters = nullptr;
	AcceptDialog *alert = nullptr;
	LineEdit *node_filter = nullptr;
	RichTextLabel *node_desc = nullptr;
	MenuButton *tools = nullptr;
	PopupMenu *popup_menu = nullptr;
	PopupMenu *connection_popup_menu = nullptr;

	Point2 saved_node_pos;
	Vector2 menu_point;
	bool saved_node_pos_dirty = false;
	bool connection_node_insert_requested = false;
	bool updating = false;
	bool drag_dirty = false;
	Ref<GraphEdit::Connection> clicked_connection;
	int to_node = -1;
	int to_slot = -1;
	int from_node = -1;
	int from_slot = -1;
	ConfirmationDialog *members_dialog = nullptr;
	AudioStreamGraphNode::PortType members_input_port_type = AudioStreamGraphNode::PORT_TYPE_MAX;
	AudioStreamGraphNode::PortType members_output_port_type = AudioStreamGraphNode::PORT_TYPE_MAX;

	AudioStreamGraph *audio_graph = nullptr;

	Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);
	void _show_members_dialog(bool at_mouse_pos, AudioStreamGraphNode::PortType p_input_port_type = AudioStreamGraphNode::PORT_TYPE_MAX, AudioStreamGraphNode::PortType p_output_port_type = AudioStreamGraphNode::PORT_TYPE_MAX);

	void _member_create();
	void _member_selected();
	void _member_cancel();
	void _update_options_menu();
	void _add_node(int p_idx);
	void _sbox_input(const Ref<InputEvent> &p_event);
	void _graph_gui_input(const Ref<InputEvent> &p_event);
	void _node_menu_id_pressed(int p_idx);
	void _connection_menu_id_pressed(int p_idx);
	void _connection_request(const String &p_from, int p_from_index, const String &p_to, int p_to_index);
	void _disconnection_request(const String &p_from, int p_from_index, const String &p_to, int p_to_index);
	void _update_graph();
	void _node_dragged(const Vector2 &p_from, const Vector2 &p_to, int p_node);
	void _nodes_dragged();
	void _delete_node_request(int p_node);
	void _delete_nodes_request(const TypedArray<StringName> &p_nodes);
	void _delete_nodes(const List<int> &p_nodes);
	void _edit_port_default_input(Object *p_button, int p_node, int p_port);
	void _parameter_line_edit_changed(const String &p_text, int p_node_id);
	void _update_parameter_refs(HashSet<String> &p_deleted_names);
	void _parameter_line_edit_focus_out(Object *line_edit, int p_node_id);
	void _port_edited(const StringName &p_property, const Variant &p_value, const String &p_field, bool p_changing);
	void _add_input_port(int p_node);
	void _remove_input_port(int p_node);
	void _script_created(const Ref<Script> &p_script);
	void _resource_saved(const Ref<Resource> &p_resource);

	void add_plugin(const Ref<AudioGraphNodePlugin> &p_plugin);
	void remove_plugin(const Ref<AudioGraphNodePlugin> &p_plugin);

	Vector<Ref<AudioGraphNodePlugin>> plugins;

	struct CopyItem {
		int id;
		Ref<AudioStreamGraphNode> node;
		Vector2 position;
		bool disabled = false;
	};

	enum ToolsMenuOptions {
		EXPAND_ALL,
		COLLAPSE_ALL
	};

	enum NodeMenuOptions {
		ADD,
		SEPARATOR, // ignore
		CUT,
		COPY,
		PASTE,
		DELETE,
		DUPLICATE,
		CLEAR_COPY_BUFFER,
	};

	enum ConnectionMenuOptions {
		INSERT_NEW_NODE,
		INSERT_NEW_REROUTE,
		DISCONNECT,
	};

	struct AddOption {
		String name;
		String type;
		String description;
		Vector<Variant> ops;
		Ref<Script> script;
		int mode = 0;
		int return_type = 0;
		int func = 0;
		bool is_custom = false;
		bool is_native = false;
		int temp_idx = 0;

		AddOption(const String &p_name = String(), const String &p_type = String(), const String &p_description = String(), const Vector<Variant> &p_ops = Vector<Variant>(), int p_return_type = -1, int p_mode = -1) {
			name = p_name;
			type = p_type;
			description = p_description;
			ops = p_ops;
			return_type = p_return_type;
			mode = p_mode;
		}
	};
	// struct _OptionComparator {
	// 	_FORCE_INLINE_ bool operator()(const AddOption &a, const AddOption &b) const {
	// 		return a.category.count("/") > b.category.count("/") || (a.category + "/" + a.name).naturalnocasecmp_to(b.category + "/" + b.name) < 0;
	// 	}
	// };

	Vector<AddOption> add_options;

	struct DragOp {
		int node = 0;
		Vector2 from;
		Vector2 to;
	};
	List<DragOp> drag_buffer;

	static Vector2 selection_center;
	static List<CopyItem> copy_items_buffer;
	static List<AudioGraphEditor::Connection> copy_connections_buffer;

protected:
	void _notification(int p_what);
	static void _bind_methods();

	static AudioGraphEditor *singleton;

public:
	bool can_edit(const Ref<AudioStreamGraph> &p_node) const;

	void edit_path(const Vector<String> &p_path);
	Vector<String> get_edited_path() const;

	void enter_editor(const String &p_path = "");
	static AudioGraphEditor *get_singleton() { return singleton; }
	void edit(AudioStreamGraph *p_audio_graph);
	Ref<AudioStreamGraph> get_audio_graph() const;
	AudioGraphEditorPlugin *get_graph_plugin() const;
	AudioGraphEditor();
};

class AGGraphNode : public GraphNode {
	GDCLASS(AGGraphNode, GraphNode);

protected:
	void _draw_port(int p_slot_index, Point2i p_pos, bool p_left, const Color &p_color, const Color &p_rim_color);
	virtual void draw_port(int p_slot_index, Point2i p_pos, bool p_left, const Color &p_color) override;
};

class AGRerouteNode : public AGGraphNode {
	GDCLASS(AGRerouteNode, GraphNode);

	const float FADE_ANIMATION_LENGTH_SEC = 0.3;

	float icon_opacity = 0.0;

protected:
	void _notification(int p_what);

	virtual void draw_port(int p_slot_index, Point2i p_pos, bool p_left, const Color &p_color) override;

public:
	AGRerouteNode();
	void set_icon_opacity(float p_opacity);

	void _on_mouse_entered();
	void _on_mouse_exited();
};

class AudioGraphEditorPlugin : public EditorPlugin {
	GDCLASS(AudioGraphEditorPlugin, EditorPlugin);

	AudioGraphEditor *audio_graph_editor = nullptr;
	Button *editor_button = nullptr;

	struct InputPort {
		Button *default_input_button = nullptr;
	};

	struct Link {
		AudioStreamGraphNode *audio_node = nullptr;
		GraphElement *graph_element = nullptr;
		LineEdit *parameter_name = nullptr;
		HashMap<int, InputPort> input_ports;
	};

	HashMap<int, Link> links;

	List<AudioStreamGraph::Connection> connections;

protected:
	static void _bind_methods();

public:
	void clear_links();
	void register_link(int p_id, AudioStreamGraphNode *p_audio_node, GraphElement *p_graph_element);
	void register_default_input_button(int p_node_id, int p_port_id, Button *p_button);
	void register_parameter_name(int p_node_id, LineEdit *p_parameter_name);
	bool has_main_screen() const override { return false; }
	void add_node(int p_id, bool p_just_update);
	void update_node(int p_node_id);
	void update_node_deferred(int p_node_id);
	void remove_node(int p_id, bool p_just_update);
	void connect_nodes(int p_from_node, int p_from_port, int p_to_node, int p_to_port);
	void disconnect_nodes(int p_from_node, int p_from_port, int p_to_node, int p_to_port);
	void set_node_position(int p_id, const Vector2 &p_position);
	void set_connections(const List<AudioStreamGraph::Connection> &p_connections);
	void set_input_port_default_value(int p_node_id, int p_port_id, const Variant &p_value);

	virtual String get_name() const override { return "AudioGraph"; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;

	AudioGraphEditorPlugin();
	~AudioGraphEditorPlugin();
};

#endif // AUDIO_GRAPH_EDITOR_PLUGIN