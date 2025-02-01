/**************************************************************************/
/*  visual_shader_editor_plugin.h                                         */
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

#include "editor/editor_properties.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/plugins/editor_resource_conversion_plugin.h"
#include "editor/plugins/shader/shader_editor.h"
#include "scene/gui/graph_edit.h"
#include "scene/resources/syntax_highlighter.h"
#include "scene/resources/visual_shader.h"

class CodeEdit;
class ColorPicker;
class CurveEditor;
class GraphElement;
class GraphFrame;
class MenuButton;
class PopupPanel;
class RichTextLabel;
class Tree;

class VisualShaderEditor;
class MaterialEditor;

class VisualShaderNodePlugin : public RefCounted {
	GDCLASS(VisualShaderNodePlugin, RefCounted);

protected:
	VisualShaderEditor *vseditor = nullptr;

protected:
	static void _bind_methods();

	GDVIRTUAL2RC(Object *, _create_editor, Ref<Resource>, Ref<VisualShaderNode>)

public:
	void set_editor(VisualShaderEditor *p_editor);
	virtual Control *create_editor(const Ref<Resource> &p_parent_resource, const Ref<VisualShaderNode> &p_node);
};

class VSGraphNode : public GraphNode {
	GDCLASS(VSGraphNode, GraphNode);

protected:
	void _draw_port(int p_slot_index, Point2i p_pos, bool p_left, const Color &p_color, const Color &p_rim_color);
	virtual void draw_port(int p_slot_index, Point2i p_pos, bool p_left, const Color &p_color) override;
};

class VSRerouteNode : public VSGraphNode {
	GDCLASS(VSRerouteNode, GraphNode);

	const float FADE_ANIMATION_LENGTH_SEC = 0.3;

	float icon_opacity = 0.0;

protected:
	void _notification(int p_what);

	virtual void draw_port(int p_slot_index, Point2i p_pos, bool p_left, const Color &p_color) override;

public:
	VSRerouteNode();
	void set_icon_opacity(float p_opacity);

	void _on_mouse_entered();
	void _on_mouse_exited();
};

class VisualShaderGraphPlugin : public RefCounted {
	GDCLASS(VisualShaderGraphPlugin, RefCounted);

private:
	VisualShaderEditor *editor = nullptr;

	struct InputPort {
		Button *default_input_button = nullptr;
	};

	struct Port {
		VisualShaderNode::PortType type = VisualShaderNode::PORT_TYPE_SCALAR;
		TextureButton *preview_button = nullptr;
	};

	struct Link {
		VisualShader::Type type = VisualShader::Type::TYPE_MAX;
		VisualShaderNode *visual_node = nullptr;
		GraphElement *graph_element = nullptr;
		bool preview_visible = false;
		int preview_pos = 0;
		HashMap<int, InputPort> input_ports;
		HashMap<int, Port> output_ports;
		VBoxContainer *preview_box = nullptr;
		LineEdit *parameter_name = nullptr;
		CodeEdit *expression_edit = nullptr;
		CurveEditor *curve_editors[3] = { nullptr, nullptr, nullptr };
	};

	Ref<VisualShader> visual_shader;
	HashMap<int, Link> links;
	List<VisualShader::Connection> connections;

	Color vector_expanded_color[4];

	// Visual shader specific theme for using MSDF fonts (on GraphNodes) which reduce aliasing at higher zoom levels.
	Ref<Theme> vs_msdf_fonts_theme;

protected:
	static void _bind_methods();

public:
	void set_editor(VisualShaderEditor *p_editor);
	void register_shader(VisualShader *p_visual_shader);
	void set_connections(const List<VisualShader::Connection> &p_connections);
	void register_link(VisualShader::Type p_type, int p_id, VisualShaderNode *p_visual_node, GraphElement *p_graph_element);
	void register_output_port(int p_id, int p_port, VisualShaderNode::PortType p_port_type, TextureButton *p_button);
	void register_parameter_name(int p_id, LineEdit *p_parameter_name);
	void register_default_input_button(int p_node_id, int p_port_id, Button *p_button);
	void register_expression_edit(int p_node_id, CodeEdit *p_expression_edit);
	void register_curve_editor(int p_node_id, int p_index, CurveEditor *p_curve_editor);
	void clear_links();
	void set_shader_type(VisualShader::Type p_type);
	bool is_preview_visible(int p_id) const;
	void update_node(VisualShader::Type p_type, int p_id);
	void update_node_deferred(VisualShader::Type p_type, int p_node_id);
	void add_node(VisualShader::Type p_type, int p_id, bool p_just_update, bool p_update_frames);
	void remove_node(VisualShader::Type p_type, int p_id, bool p_just_update);
	void connect_nodes(VisualShader::Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port);
	void disconnect_nodes(VisualShader::Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port);
	void show_port_preview(VisualShader::Type p_type, int p_node_id, int p_port_id, bool p_is_valid);
	void update_frames(VisualShader::Type p_type, int p_node);
	void set_node_position(VisualShader::Type p_type, int p_id, const Vector2 &p_position);
	void refresh_node_ports(VisualShader::Type p_type, int p_node);
	void set_input_port_default_value(VisualShader::Type p_type, int p_node_id, int p_port_id, const Variant &p_value);
	void update_parameter_refs();
	void set_parameter_name(VisualShader::Type p_type, int p_node_id, const String &p_name);
	void update_curve(int p_node_id);
	void update_curve_xyz(int p_node_id);
	void set_expression(VisualShader::Type p_type, int p_node_id, const String &p_expression);
	void attach_node_to_frame(VisualShader::Type p_type, int p_node_id, int p_frame_id);
	void detach_node_from_frame(VisualShader::Type p_type, int p_node_id);
	void set_frame_color_enabled(VisualShader::Type p_type, int p_node_id, bool p_enable);
	void set_frame_color(VisualShader::Type p_type, int p_node_id, const Color &p_color);
	void set_frame_autoshrink_enabled(VisualShader::Type p_type, int p_node_id, bool p_enable);
	void update_reroute_nodes();
	int get_constant_index(float p_constant) const;
	Ref<Script> get_node_script(int p_node_id) const;
	void update_theme();
	bool is_node_has_parameter_instances_relatively(VisualShader::Type p_type, int p_node) const;
	VisualShader::Type get_shader_type() const;

	VisualShaderGraphPlugin();
	~VisualShaderGraphPlugin();
};

class VisualShaderEditedProperty : public RefCounted {
	GDCLASS(VisualShaderEditedProperty, RefCounted);

private:
	Variant edited_property;

protected:
	static void _bind_methods();

public:
	void set_edited_property(const Variant &p_variant);
	Variant get_edited_property() const;

	VisualShaderEditedProperty() {}
};

class VisualShaderEditor : public ShaderEditor {
	GDCLASS(VisualShaderEditor, ShaderEditor);
	friend class VisualShaderGraphPlugin;

	PopupPanel *property_editor_popup = nullptr;
	EditorProperty *property_editor = nullptr;
	int editing_node = -1;
	int editing_port = -1;
	Ref<VisualShaderEditedProperty> edited_property_holder;

	MaterialEditor *material_editor = nullptr;
	Ref<VisualShader> visual_shader;
	Ref<ShaderMaterial> preview_material;
	Ref<Environment> env;
	String param_filter_name;
	EditorProperty *current_prop = nullptr;
	VBoxContainer *shader_preview_vbox = nullptr;
	GraphEdit *graph = nullptr;
	Button *add_node = nullptr;
	MenuButton *varying_button = nullptr;
	Button *code_preview_button = nullptr;
	Button *shader_preview_button = nullptr;

	int last_to_node = -1;
	int last_to_port = -1;
	Label *info_label = nullptr;

	OptionButton *edit_type = nullptr;
	OptionButton *edit_type_standard = nullptr;
	OptionButton *edit_type_particles = nullptr;
	OptionButton *edit_type_sky = nullptr;
	OptionButton *edit_type_fog = nullptr;
	CheckBox *custom_mode_box = nullptr;
	bool custom_mode_enabled = false;

	bool pending_update_preview = false;
	bool shader_error = false;
	AcceptDialog *code_preview_window = nullptr;
	VBoxContainer *code_preview_vbox = nullptr;
	CodeEdit *preview_text = nullptr;
	Ref<CodeHighlighter> syntax_highlighter = nullptr;
	PanelContainer *error_panel = nullptr;
	Label *error_label = nullptr;

	bool pending_custom_scripts_to_delete = false;
	List<Ref<Script>> custom_scripts_to_delete;

	bool _block_update_options_menu = false;
	bool _block_rebuild_shader = false;

	Point2 saved_node_pos;
	bool saved_node_pos_dirty = false;

	ConfirmationDialog *members_dialog = nullptr;
	VisualShaderNode::PortType members_input_port_type = VisualShaderNode::PORT_TYPE_MAX;
	VisualShaderNode::PortType members_output_port_type = VisualShaderNode::PORT_TYPE_MAX;
	PopupMenu *popup_menu = nullptr;
	PopupMenu *connection_popup_menu = nullptr;
	PopupMenu *constants_submenu = nullptr;
	MenuButton *tools = nullptr;

	ConfirmationDialog *add_varying_dialog = nullptr;
	OptionButton *varying_type = nullptr;
	LineEdit *varying_name = nullptr;
	OptionButton *varying_mode = nullptr;
	Label *varying_error_label = nullptr;

	ConfirmationDialog *remove_varying_dialog = nullptr;
	Tree *varyings = nullptr;

	PopupPanel *frame_title_change_popup = nullptr;
	LineEdit *frame_title_change_edit = nullptr;

	PopupPanel *frame_tint_color_pick_popup = nullptr;
	ColorPicker *frame_tint_color_picker = nullptr;

	bool code_preview_first = true;
	bool code_preview_showed = false;

	bool shader_preview_showed = true;

	LineEdit *param_filter = nullptr;
	MenuButton *preview_tools = nullptr;
	String selected_param_id;
	Tree *parameters = nullptr;
	HashMap<String, PropertyInfo> parameter_props;
	VBoxContainer *param_vbox = nullptr;
	VBoxContainer *param_vbox2 = nullptr;

	enum ShaderModeFlags {
		MODE_FLAGS_SPATIAL_CANVASITEM = 1,
		MODE_FLAGS_SKY = 2,
		MODE_FLAGS_PARTICLES = 4,
		MODE_FLAGS_FOG = 8,
	};

	int mode = MODE_FLAGS_SPATIAL_CANVASITEM;

	enum TypeFlags {
		TYPE_FLAGS_VERTEX = 1,
		TYPE_FLAGS_FRAGMENT = 2,
		TYPE_FLAGS_LIGHT = 4,
	};

	enum ParticlesTypeFlags {
		TYPE_FLAGS_EMIT = 1,
		TYPE_FLAGS_PROCESS = 2,
		TYPE_FLAGS_COLLIDE = 4,
		TYPE_FLAGS_EMIT_CUSTOM = 8,
		TYPE_FLAGS_PROCESS_CUSTOM = 16,
	};

	enum SkyTypeFlags {
		TYPE_FLAGS_SKY = 1,
	};

	enum FogTypeFlags {
		TYPE_FLAGS_FOG = 1,
	};

	enum ToolsMenuOptions {
		EXPAND_ALL,
		COLLAPSE_ALL
	};

	enum PreviewToolsMenuOptions {
		COPY_PARAMS_FROM_MATERIAL,
		PASTE_PARAMS_TO_MATERIAL,
	};

#ifdef MINGW_ENABLED
#undef DELETE
#endif

	enum NodeMenuOptions {
		ADD,
		SEPARATOR, // ignore
		CUT,
		COPY,
		PASTE,
		DELETE,
		DUPLICATE,
		CLEAR_COPY_BUFFER,
		SEPARATOR2, // ignore
		FLOAT_CONSTANTS,
		CONVERT_CONSTANTS_TO_PARAMETERS,
		CONVERT_PARAMETERS_TO_CONSTANTS,
		UNLINK_FROM_PARENT_FRAME,
		SEPARATOR3, // ignore
		SET_FRAME_TITLE,
		ENABLE_FRAME_COLOR,
		SET_FRAME_COLOR,
		ENABLE_FRAME_AUTOSHRINK,
	};

	enum ConnectionMenuOptions {
		INSERT_NEW_NODE,
		INSERT_NEW_REROUTE,
		DISCONNECT,
	};

	enum class VaryingMenuOptions {
		ADD,
		REMOVE,
	};

	Tree *members = nullptr;
	AcceptDialog *alert = nullptr;
	LineEdit *node_filter = nullptr;
	RichTextLabel *node_desc = nullptr;
	Label *highend_label = nullptr;

	void _tools_menu_option(int p_idx);
	void _show_members_dialog(bool at_mouse_pos, VisualShaderNode::PortType p_input_port_type = VisualShaderNode::PORT_TYPE_MAX, VisualShaderNode::PortType p_output_port_type = VisualShaderNode::PORT_TYPE_MAX);

	void _varying_menu_id_pressed(int p_idx);
	void _show_add_varying_dialog();
	void _show_remove_varying_dialog();

	void _preview_tools_menu_option(int p_idx);
	void _clear_preview_param();
	void _update_preview_parameter_list();
	bool _update_preview_parameter_tree();

	void _update_nodes();
	void _update_graph();

	struct AddOption {
		String name;
		String category;
		String type;
		String description;
		Vector<Variant> ops;
		Ref<Script> script;
		int mode = 0;
		int return_type = 0;
		int func = 0;
		bool highend = false;
		bool is_custom = false;
		bool is_native = false;
		mutable int temp_idx = 0;

		AddOption(const String &p_name = String(), const String &p_category = String(), const String &p_type = String(), const String &p_description = String(), const Vector<Variant> &p_ops = Vector<Variant>(), int p_return_type = -1, int p_mode = -1, int p_func = -1, bool p_highend = false) {
			name = p_name;
			type = p_type;
			category = p_category;
			description = p_description;
			ops = p_ops;
			return_type = p_return_type;
			mode = p_mode;
			func = p_func;
			highend = p_highend;
		}
	};
	struct _OptionComparator {
		_FORCE_INLINE_ bool operator()(const AddOption &a, const AddOption &b) const {
			return a.category.count("/") > b.category.count("/") || (a.category + "/" + a.name).naturalnocasecmp_to(b.category + "/" + b.name) < 0;
		}
	};

	Vector<AddOption> add_options;
	int cubemap_node_option_idx;
	int texture2d_node_option_idx;
	int texture2d_array_node_option_idx;
	int texture3d_node_option_idx;
	int custom_node_option_idx;
	int curve_node_option_idx;
	int curve_xyz_node_option_idx;
	int mesh_emitter_option_idx;
	List<String> keyword_list;

	List<VisualShaderNodeParameterRef> uniform_refs;

	void _draw_color_over_button(Object *p_obj, Color p_color);

	void _setup_node(VisualShaderNode *p_node, const Vector<Variant> &p_ops);
	void _add_node(int p_idx, const Vector<Variant> &p_ops, const String &p_resource_path = "", int p_node_idx = -1);
	void _add_varying(const String &p_name, VisualShader::VaryingMode p_mode, VisualShader::VaryingType p_type);
	void _remove_varying(const String &p_name);
	void _update_options_menu();
	void _set_mode(int p_which);

	void _show_preview_text();
	void _preview_close_requested();
	void _preview_size_changed();
	void _update_preview();
	void _update_next_previews(int p_node_id);
	void _get_next_nodes_recursively(VisualShader::Type p_type, int p_node_id, LocalVector<int> &r_nodes) const;
	String _get_description(int p_idx);

	void _show_shader_preview();

	Vector<int> nodes_link_to_frame_buffer; // Contains the nodes that are requested to be linked to a frame. This is used to perform one Undo/Redo operation for dragging nodes.
	int frame_node_id_to_link_to = -1;

	struct DragOp {
		VisualShader::Type type = VisualShader::Type::TYPE_MAX;
		int node = 0;
		Vector2 from;
		Vector2 to;
	};
	List<DragOp> drag_buffer;

	bool drag_dirty = false;
	void _node_dragged(const Vector2 &p_from, const Vector2 &p_to, int p_node);
	void _nodes_dragged();
	bool updating = false;

	void _connection_request(const String &p_from, int p_from_index, const String &p_to, int p_to_index);
	void _disconnection_request(const String &p_from, int p_from_index, const String &p_to, int p_to_index);

	void _scroll_changed(const Vector2 &p_scroll);
	void _node_selected(Object *p_node);

	void _delete_nodes(int p_type, const List<int> &p_nodes);
	void _delete_node_request(int p_type, int p_node);
	void _delete_nodes_request(const TypedArray<StringName> &p_nodes);

	void _node_changed(int p_id);

	void _nodes_linked_to_frame_request(const TypedArray<StringName> &p_nodes, const StringName &p_frame);
	void _frame_rect_changed(const GraphFrame *p_frame, const Rect2 &p_new_rect);

	void _edit_port_default_input(Object *p_button, int p_node, int p_port);
	void _port_edited(const StringName &p_property, const Variant &p_value, const String &p_field, bool p_changing);

	int to_node = -1;
	int to_slot = -1;
	int from_node = -1;
	int from_slot = -1;

	Ref<GraphEdit::Connection> clicked_connection;
	bool connection_node_insert_requested = false;

	HashSet<int> selected_constants;
	HashSet<int> selected_parameters;
	int selected_frame = -1;
	int selected_float_constant = -1;

	void _convert_constants_to_parameters(bool p_vice_versa);
	void _detach_nodes_from_frame_request();
	void _detach_nodes_from_frame(int p_type, const List<int> &p_nodes);
	void _replace_node(VisualShader::Type p_type_id, int p_node_id, const StringName &p_from, const StringName &p_to);
	void _update_constant(VisualShader::Type p_type_id, int p_node_id, const Variant &p_var, int p_preview_port);
	void _update_parameter(VisualShader::Type p_type_id, int p_node_id, const Variant &p_var, int p_preview_port);

	void _unlink_node_from_parent_frame(int p_node_id);

	void _connection_drag_ended();
	void _connection_to_empty(const String &p_from, int p_from_slot, const Vector2 &p_release_position);
	void _connection_from_empty(const String &p_to, int p_to_slot, const Vector2 &p_release_position);
	bool _check_node_drop_on_connection(const Vector2 &p_position, Ref<GraphEdit::Connection> *r_closest_connection, int *r_node_id = nullptr, int *r_to_port = nullptr);
	void _handle_node_drop_on_connection();

	void _frame_title_popup_show(const Point2 &p_position, int p_node_id);
	void _frame_title_popup_hide();
	void _frame_title_popup_focus_out();
	void _frame_title_text_changed(const String &p_new_text);
	void _frame_title_text_submitted(const String &p_new_text);

	void _frame_color_enabled_changed(int p_node_id);
	void _frame_color_popup_show(const Point2 &p_position, int p_node_id);
	void _frame_color_popup_hide();
	void _frame_color_confirm();
	void _frame_color_changed(const Color &p_color);

	void _frame_autoshrink_enabled_changed(int p_node_id);

	void _parameter_line_edit_changed(const String &p_text, int p_node_id);
	void _parameter_line_edit_focus_out(Object *p_line_edit, int p_node_id);

	void _port_name_focus_out(Object *p_line_edit, int p_node_id, int p_port_id, bool p_output);

	struct CopyItem {
		int id;
		Ref<VisualShaderNode> node;
		Vector2 position;
		Vector2 size;
		String group_inputs;
		String group_outputs;
		String expression;
		bool disabled = false;
	};

	void _dup_copy_nodes(int p_type, List<CopyItem> &r_nodes, List<VisualShader::Connection> &r_connections);
	void _dup_paste_nodes(int p_type, List<CopyItem> &r_items, const List<VisualShader::Connection> &p_connections, const Vector2 &p_offset, bool p_duplicate);

	void _duplicate_nodes();

	static Vector2 selection_center;
	static List<CopyItem> copy_items_buffer;
	static List<VisualShader::Connection> copy_connections_buffer;

	void _clear_copy_buffer();
	void _copy_nodes(bool p_cut);
	void _paste_nodes(bool p_use_custom_position = false, const Vector2 &p_custom_position = Vector2());

	Vector<Ref<VisualShaderNodePlugin>> plugins;
	Ref<VisualShaderGraphPlugin> graph_plugin;

	void _mode_selected(int p_id);
	void _custom_mode_toggled(bool p_enabled);

	void _input_select_item(Ref<VisualShaderNodeInput> p_input, const String &p_name);
	void _parameter_ref_select_item(Ref<VisualShaderNodeParameterRef> p_parameter_ref, const String &p_name);
	void _varying_select_item(Ref<VisualShaderNodeVarying> p_varying, const String &p_name);

	void _float_constant_selected(int p_which);

	VisualShader::Type get_current_shader_type() const;

	void _add_input_port(int p_node, int p_port, int p_port_type, const String &p_name);
	void _remove_input_port(int p_node, int p_port);
	void _change_input_port_type(int p_type, int p_node, int p_port);
	void _change_input_port_name(const String &p_text, Object *p_line_edit, int p_node, int p_port);

	void _add_output_port(int p_node, int p_port, int p_port_type, const String &p_name);
	void _remove_output_port(int p_node, int p_port);
	void _change_output_port_type(int p_type, int p_node, int p_port);
	void _change_output_port_name(const String &p_text, Object *p_line_edit, int p_node, int p_port);
	void _expand_output_port(int p_node, int p_port, bool p_expand);

	void _expression_focus_out(Object *p_code_edit, int p_node);

	void _set_node_size(int p_type, int p_node, const Size2 &p_size);
	void _node_resized(const Vector2 &p_new_size, int p_type, int p_node);

	void _preview_select_port(int p_node, int p_port);
	void _graph_gui_input(const Ref<InputEvent> &p_event);

	void _member_filter_changed(const String &p_text);
	void _sbox_input(const Ref<InputEvent> &p_event);
	void _member_selected();
	void _member_create();
	void _member_cancel();

	void _varying_create();
	void _varying_validate();
	void _varying_type_changed(int p_index);
	void _varying_mode_changed(int p_index);
	void _varying_name_changed(const String &p_name);
	void _varying_deleted();
	void _varying_selected();
	void _varying_unselected();
	void _update_varying_tree();

	void _set_custom_node_option(int p_index, int p_node, int p_op);

	Vector2 menu_point;
	void _node_menu_id_pressed(int p_idx);
	void _connection_menu_id_pressed(int p_idx);

	Variant get_drag_data_fw(const Point2 &p_point, Control *p_from);
	bool can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const;
	void drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from);

	bool _is_available(int p_mode);
	void _update_parameters(bool p_update_refs);
	void _update_parameter_refs(HashSet<String> &p_names);
	void _update_varyings();

	void _update_options_menu_deferred();
	void _rebuild_shader_deferred();

	void _visibility_changed();

	void _get_current_mode_limits(int &r_begin_type, int &r_end_type) const;
	void _update_custom_script(const Ref<Script> &p_script);
	void _script_created(const Ref<Script> &p_script);
	void _resource_saved(const Ref<Resource> &p_resource);
	void _resource_removed(const Ref<Resource> &p_resource);
	void _resources_removed();

	void _param_property_changed(const String &p_property, const Variant &p_value, const String &p_field = "", bool p_changing = false);
	void _update_current_param();
	void _param_filter_changed(const String &p_text);
	void _param_selected();
	void _param_unselected();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	virtual void edit_shader(const Ref<Shader> &p_shader) override;
	virtual void apply_shaders() override;
	virtual bool is_unsaved() const override;
	virtual void save_external_data(const String &p_str = "") override;
	virtual void validate_script() override;

	void add_plugin(const Ref<VisualShaderNodePlugin> &p_plugin);
	void remove_plugin(const Ref<VisualShaderNodePlugin> &p_plugin);

	VisualShaderGraphPlugin *get_graph_plugin() { return graph_plugin.ptr(); }

	void clear_custom_types();
	void add_custom_type(const String &p_name, const String &p_type, const Ref<Script> &p_script, const String &p_description, int p_return_icon_type, const String &p_category, bool p_highend);

	Dictionary get_custom_node_data(Ref<VisualShaderNodeCustom> &p_custom_node);
	void update_custom_type(const Ref<Resource> &p_resource);

	virtual Size2 get_minimum_size() const override;

	Ref<VisualShader> get_visual_shader() const { return visual_shader; }

	VisualShaderEditor();
};

class VisualShaderNodePluginDefault : public VisualShaderNodePlugin {
	GDCLASS(VisualShaderNodePluginDefault, VisualShaderNodePlugin);

public:
	virtual Control *create_editor(const Ref<Resource> &p_parent_resource, const Ref<VisualShaderNode> &p_node) override;
};

class EditorPropertyVisualShaderMode : public EditorProperty {
	GDCLASS(EditorPropertyVisualShaderMode, EditorProperty);
	OptionButton *options = nullptr;

	void _option_selected(int p_which);

public:
	void setup(const Vector<String> &p_options);
	virtual void update_property() override;
	void set_option_button_clip(bool p_enable);
	EditorPropertyVisualShaderMode();
};

class EditorInspectorVisualShaderModePlugin : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorVisualShaderModePlugin, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
};

class VisualShaderNodePortPreview : public Control {
	GDCLASS(VisualShaderNodePortPreview, Control);
	TextureRect *checkerboard = nullptr;
	Ref<VisualShader> shader;
	Ref<ShaderMaterial> preview_mat;
	VisualShader::Type type = VisualShader::Type::TYPE_MAX;
	int node = 0;
	int port = 0;
	bool is_valid = false;
	void _shader_changed(); //must regen
protected:
	void _notification(int p_what);

public:
	virtual Size2 get_minimum_size() const override;
	void setup(const Ref<VisualShader> &p_shader, Ref<ShaderMaterial> &p_preview_material, VisualShader::Type p_type, bool p_has_transparency, int p_node, int p_port, bool p_is_valid);
};

class VisualShaderConversionPlugin : public EditorResourceConversionPlugin {
	GDCLASS(VisualShaderConversionPlugin, EditorResourceConversionPlugin);

public:
	virtual String converts_to() const override;
	virtual bool handles(const Ref<Resource> &p_resource) const override;
	virtual Ref<Resource> convert(const Ref<Resource> &p_resource) const override;
};
