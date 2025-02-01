/**************************************************************************/
/*  visual_shader.h                                                       */
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

#include "core/string/string_builder.h"
#include "core/templates/safe_refcount.h"
#include "scene/gui/control.h"
#include "scene/resources/shader.h"

class VisualShaderNodeParameter;
class VisualShaderNode;

class VisualShader : public Shader {
	GDCLASS(VisualShader, Shader);

public:
	enum Type {
		TYPE_VERTEX,
		TYPE_FRAGMENT,
		TYPE_LIGHT,
		TYPE_START,
		TYPE_PROCESS,
		TYPE_COLLIDE,
		TYPE_START_CUSTOM,
		TYPE_PROCESS_CUSTOM,
		TYPE_SKY,
		TYPE_FOG,
		TYPE_MAX
	};

	struct Connection {
		int from_node = 0;
		int from_port = 0;
		int to_node = 0;
		int to_port = 0;
	};

	struct DefaultTextureParam {
		StringName name;
		List<Ref<Texture>> params;
	};

	enum VaryingMode {
		VARYING_MODE_VERTEX_TO_FRAG_LIGHT,
		VARYING_MODE_FRAG_TO_LIGHT,
		VARYING_MODE_MAX,
	};

	enum VaryingType {
		VARYING_TYPE_FLOAT,
		VARYING_TYPE_INT,
		VARYING_TYPE_UINT,
		VARYING_TYPE_VECTOR_2D,
		VARYING_TYPE_VECTOR_3D,
		VARYING_TYPE_VECTOR_4D,
		VARYING_TYPE_BOOLEAN,
		VARYING_TYPE_TRANSFORM,
		VARYING_TYPE_MAX,
	};

	struct Varying {
		String name;
		VaryingMode mode = VARYING_MODE_MAX;
		VaryingType type = VARYING_TYPE_MAX;

		Varying() {}

		Varying(String p_name, VaryingMode p_mode, VaryingType p_type) :
				name(p_name), mode(p_mode), type(p_type) {}

		bool from_string(const String &p_str) {
			Vector<String> arr = p_str.split(",");
			if (arr.size() != 2) {
				return false;
			}

			mode = (VaryingMode)arr[0].to_int();
			type = (VaryingType)arr[1].to_int();

			return true;
		}

		String to_string() const {
			return vformat("%s,%s", itos((int)mode), itos((int)type));
		}
	};

private:
	Type current_type;

	struct Node {
		Ref<VisualShaderNode> node;
		Vector2 position;
		LocalVector<int> prev_connected_nodes;
		LocalVector<int> next_connected_nodes;
	};

	struct Graph {
		RBMap<int, Node> nodes;
		List<Connection> connections;
	} graph[TYPE_MAX];

	Shader::Mode shader_mode = Shader::MODE_SPATIAL;
	mutable String previous_code;

	TypedArray<Dictionary> _get_node_connections(Type p_type) const;

	Vector2 graph_offset;

	HashMap<String, int> modes;
	HashSet<StringName> flags;

	HashMap<String, Varying> varyings;
#ifdef TOOLS_ENABLED
	HashMap<String, Variant> preview_params;
#endif
	List<Varying> varyings_list;

	mutable SafeFlag dirty;
	void _queue_update();

	union ConnectionKey {
		struct {
			uint64_t node : 32;
			uint64_t port : 32;
		};
		uint64_t key = 0;
		bool operator<(const ConnectionKey &p_key) const {
			return key < p_key.key;
		}
	};

	Error _write_node(Type p_type, StringBuilder *p_global_code, StringBuilder *p_global_code_per_node, HashMap<Type, StringBuilder> *p_global_code_per_func, StringBuilder &r_code, Vector<DefaultTextureParam> &r_def_tex_params, const VMap<ConnectionKey, const List<Connection>::Element *> &p_input_connections, const VMap<ConnectionKey, const List<Connection>::Element *> &p_output_connections, int p_node, HashSet<int> &r_processed, bool p_for_preview, HashSet<StringName> &r_classes) const;

	void _input_type_changed(Type p_type, int p_id);
	bool has_func_name(RenderingServer::ShaderMode p_mode, const String &p_func_name) const;

	bool _check_reroute_subgraph(Type p_type, int p_target_port_type, int p_reroute_node, List<int> *r_visited_reroute_nodes = nullptr) const;

protected:
	virtual void _update_shader() const override;
	static void _bind_methods();

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	virtual void reset_state() override;

public: // internal methods
	void set_shader_type(Type p_type);
	Type get_shader_type() const;

	enum {
		NODE_ID_INVALID = -1,
		NODE_ID_OUTPUT = 0,
	};

	void add_node(Type p_type, const Ref<VisualShaderNode> &p_node, const Vector2 &p_position, int p_id);
	void set_node_position(Type p_type, int p_id, const Vector2 &p_position);

	void add_varying(const String &p_name, VaryingMode p_mode, VaryingType p_type);
	void remove_varying(const String &p_name);
	bool has_varying(const String &p_name) const;
	int get_varyings_count() const;
	const Varying *get_varying_by_index(int p_idx) const;

	void set_varying_mode(const String &p_name, VaryingMode p_mode);
	VaryingMode get_varying_mode(const String &p_name);

	void set_varying_type(const String &p_name, VaryingType p_type);
	VaryingType get_varying_type(const String &p_name);

	void _set_preview_shader_parameter(const String &p_name, const Variant &p_value);
	Variant _get_preview_shader_parameter(const String &p_name) const;
	bool _has_preview_shader_parameter(const String &p_name) const;

	Vector2 get_node_position(Type p_type, int p_id) const;
	Ref<VisualShaderNode> get_node(Type p_type, int p_id) const;

	_FORCE_INLINE_ Ref<VisualShaderNode> get_node_unchecked(Type p_type, int p_id) const {
		return graph[p_type].nodes[p_id].node;
	}
	_FORCE_INLINE_ const LocalVector<int> &get_next_connected_nodes(Type p_type, int p_id) const {
		return graph[p_type].nodes[p_id].next_connected_nodes;
	}
	_FORCE_INLINE_ const LocalVector<int> &get_prev_connected_nodes(Type p_type, int p_id) const {
		return graph[p_type].nodes[p_id].prev_connected_nodes;
	}

	Vector<int> get_node_list(Type p_type) const;
	int get_valid_node_id(Type p_type) const;

	int find_node_id(Type p_type, const Ref<VisualShaderNode> &p_node) const;
	void remove_node(Type p_type, int p_id);
	void replace_node(Type p_type, int p_id, const StringName &p_new_class);

	bool is_node_connection(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port) const;

	bool is_nodes_connected_relatively(const Graph *p_graph, int p_node, int p_target) const;
	bool can_connect_nodes(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port) const;
	Error connect_nodes(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port);
	void disconnect_nodes(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port);
	void connect_nodes_forced(Type p_type, int p_from_node, int p_from_port, int p_to_node, int p_to_port);
	bool is_port_types_compatible(int p_a, int p_b) const;

	void attach_node_to_frame(Type p_type, int p_node, int p_frame);
	void detach_node_from_frame(Type p_type, int p_node);

	String get_reroute_parameter_name(Type p_type, int p_reroute_node) const;

	void rebuild();
	void get_node_connections(Type p_type, List<Connection> *r_connections) const;

	void set_mode(Mode p_mode);
	virtual Mode get_mode() const override;

	virtual bool is_text_shader() const override;

	void set_graph_offset(const Vector2 &p_offset);
	Vector2 get_graph_offset() const;

	String generate_preview_shader(Type p_type, int p_node, int p_port, Vector<DefaultTextureParam> &r_default_tex_params) const;

	String validate_port_name(const String &p_port_name, VisualShaderNode *p_node, int p_port_id, bool p_output) const;
	String validate_parameter_name(const String &p_name, const Ref<VisualShaderNodeParameter> &p_parameter) const;

	VisualShader();
};

VARIANT_ENUM_CAST(VisualShader::Type)
VARIANT_ENUM_CAST(VisualShader::VaryingMode)
VARIANT_ENUM_CAST(VisualShader::VaryingType)
///
///
///

class VisualShaderNode : public Resource {
	GDCLASS(VisualShaderNode, Resource);

public:
	enum PortType {
		PORT_TYPE_SCALAR,
		PORT_TYPE_SCALAR_INT,
		PORT_TYPE_SCALAR_UINT,
		PORT_TYPE_VECTOR_2D,
		PORT_TYPE_VECTOR_3D,
		PORT_TYPE_VECTOR_4D,
		PORT_TYPE_BOOLEAN,
		PORT_TYPE_TRANSFORM,
		PORT_TYPE_SAMPLER,
		PORT_TYPE_MAX,
	};

	enum Category {
		CATEGORY_NONE,
		CATEGORY_OUTPUT,
		CATEGORY_COLOR,
		CATEGORY_CONDITIONAL,
		CATEGORY_INPUT,
		CATEGORY_SCALAR,
		CATEGORY_TEXTURES,
		CATEGORY_TRANSFORM,
		CATEGORY_UTILITY,
		CATEGORY_VECTOR,
		CATEGORY_SPECIAL,
		CATEGORY_PARTICLE,
		CATEGORY_MAX
	};

private:
	int port_preview = -1;
	int linked_parent_graph_frame = -1;

	HashMap<int, bool> connected_input_ports;
	HashMap<int, int> connected_output_ports;
	HashMap<int, bool> expanded_output_ports;

protected:
	HashMap<int, Variant> default_input_values;
	bool simple_decl = true;
	bool disabled = false;
	bool closable = false;

	static void _bind_methods();

public:
	bool is_simple_decl() const;

	virtual String get_caption() const = 0;

	virtual int get_input_port_count() const = 0;
	virtual PortType get_input_port_type(int p_port) const = 0;
	virtual String get_input_port_name(int p_port) const = 0;
	virtual int get_default_input_port(PortType p_type) const;

	virtual void set_input_port_default_value(int p_port, const Variant &p_value, const Variant &p_prev_value = Variant());
	Variant get_input_port_default_value(int p_port) const; // if NIL (default if node does not set anything) is returned, it means no default value is wanted if disconnected, thus no input var must be supplied (empty string will be supplied)
	Array get_default_input_values() const;
	virtual void set_default_input_values(const Array &p_values);
	virtual void remove_input_port_default_value(int p_port);
	virtual void clear_default_input_values();

	virtual int get_output_port_count() const = 0;
	virtual PortType get_output_port_type(int p_port) const = 0;
	virtual String get_output_port_name(int p_port) const = 0;

	virtual bool is_input_port_default(int p_port, Shader::Mode p_mode) const;

	void set_output_port_for_preview(int p_index);
	int get_output_port_for_preview() const;

	virtual bool is_port_separator(int p_index) const;

	bool is_output_port_connected(int p_port) const;
	void set_output_port_connected(int p_port, bool p_connected);
	bool is_input_port_connected(int p_port) const;
	void set_input_port_connected(int p_port, bool p_connected);
	bool is_any_port_connected() const;
	virtual bool is_generate_input_var(int p_port) const;

	virtual bool has_output_port_preview(int p_port) const;

	virtual bool is_output_port_expandable(int p_port) const;
	void _set_output_ports_expanded(const Array &p_data);
	Array _get_output_ports_expanded() const;
	void _set_output_port_expanded(int p_port, bool p_expanded);
	bool _is_output_port_expanded(int p_port) const;
	int get_expanded_output_port_count() const;

	virtual bool is_code_generated() const;
	virtual bool is_show_prop_names() const;
	virtual bool is_use_prop_slots() const;

	bool is_disabled() const;
	void set_disabled(bool p_disabled = true);

	bool is_deletable() const;
	void set_deletable(bool p_closable = true);

	void set_frame(int p_node);
	int get_frame() const;

	virtual Vector<StringName> get_editable_properties() const;
	virtual HashMap<StringName, String> get_editable_properties_names() const;

	virtual Vector<VisualShader::DefaultTextureParam> get_default_texture_parameters(VisualShader::Type p_type, int p_id) const;
	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const;
	virtual String generate_global_per_node(Shader::Mode p_mode, int p_id) const;
	virtual String generate_global_per_func(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const;
	// If no output is connected, the output var passed will be empty. If no input is connected and input is NIL, the input var passed will be empty.
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const = 0;

	virtual String get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const;

	virtual Category get_category() const;

	VisualShaderNode();
};

VARIANT_ENUM_CAST(VisualShaderNode::PortType)

class VisualShaderNodeCustom : public VisualShaderNode {
	GDCLASS(VisualShaderNodeCustom, VisualShaderNode);

	struct Port {
		String name;
		int type = 0;
	};

	bool is_initialized = false;
	List<Port> input_ports;
	List<Port> output_ports;
	struct Property {
		String name;
	};
	struct DropDownListProperty : public Property {
		Vector<String> options;
	};
	HashMap<int, int> dp_selected_cache;
	HashMap<int, int> dp_default_cache;
	List<DropDownListProperty> dp_props;
	String properties;

	friend class VisualShaderEditor;
	friend class VisualShaderGraphPlugin;

protected:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;
	virtual int get_default_input_port(PortType p_type) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual void set_input_port_default_value(int p_port, const Variant &p_value, const Variant &p_prev_value = Variant()) override;
	virtual void set_default_input_values(const Array &p_values) override;
	virtual void remove_input_port_default_value(int p_port) override;
	virtual void clear_default_input_values() override;

	GDVIRTUAL0RC(String, _get_name)
	GDVIRTUAL0RC(String, _get_description)
	GDVIRTUAL0RC(String, _get_category)
	GDVIRTUAL0RC(PortType, _get_return_icon_type)
	GDVIRTUAL0RC(int, _get_input_port_count)
	GDVIRTUAL1RC(PortType, _get_input_port_type, int)
	GDVIRTUAL1RC(String, _get_input_port_name, int)
	GDVIRTUAL1RC(Variant, _get_input_port_default_value, int)
	GDVIRTUAL1RC(int, _get_default_input_port, PortType)
	GDVIRTUAL0RC(int, _get_output_port_count)
	GDVIRTUAL1RC(PortType, _get_output_port_type, int)
	GDVIRTUAL1RC(String, _get_output_port_name, int)
	GDVIRTUAL0RC(int, _get_property_count)
	GDVIRTUAL1RC(String, _get_property_name, int)
	GDVIRTUAL1RC(int, _get_property_default_index, int)
	GDVIRTUAL1RC(Vector<String>, _get_property_options, int)
	GDVIRTUAL4RC(String, _get_code, TypedArray<String>, TypedArray<String>, Shader::Mode, VisualShader::Type)
	GDVIRTUAL2RC(String, _get_func_code, Shader::Mode, VisualShader::Type)
	GDVIRTUAL1RC(String, _get_global_code, Shader::Mode)
	GDVIRTUAL0RC(bool, _is_highend)
	GDVIRTUAL2RC(bool, _is_available, Shader::Mode, VisualShader::Type)

	bool _is_valid_code(const String &p_code) const;

protected:
	void _set_input_port_default_value(int p_port, const Variant &p_value);

	bool is_available(Shader::Mode p_mode, VisualShader::Type p_type) const;
	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;
	virtual String generate_global_per_node(Shader::Mode p_mode, int p_id) const override;
	virtual String generate_global_per_func(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;

	static void _bind_methods();

public:
	VisualShaderNodeCustom();
	void update_property_default_values();
	void update_input_port_default_values();
	void update_ports();
	void update_properties();

	bool _is_initialized();
	void _set_initialized(bool p_enabled);
	void _set_properties(const String &p_properties);
	String _get_properties() const;

	String _get_name() const;
	String _get_description() const;
	String _get_category() const;
	PortType _get_return_icon_type() const;
	bool _is_highend() const;
	void _set_option_index(int p_op, int p_index);

	int get_option_index(int p_op) const;
};

/////

class VisualShaderNodeInput : public VisualShaderNode {
	GDCLASS(VisualShaderNodeInput, VisualShaderNode);

	friend class VisualShader;
	VisualShader::Type shader_type = VisualShader::TYPE_MAX;
	Shader::Mode shader_mode = Shader::MODE_MAX;

	struct Port {
		Shader::Mode mode = Shader::Mode::MODE_MAX;
		VisualShader::Type shader_type = VisualShader::Type::TYPE_MAX;
		PortType type = PortType::PORT_TYPE_MAX;
		const char *name;
		const char *string;
	};

	static const Port ports[];
	static const Port preview_ports[];

	String input_name = "[None]";

public:
	void set_shader_type(VisualShader::Type p_shader_type);
	void set_shader_mode(Shader::Mode p_shader_mode);

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

public:
	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;
	virtual bool is_output_port_expandable(int p_port) const override;

	virtual String get_caption() const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_input_name(String p_name);
	String get_input_name() const;
	String get_input_real_name() const;

	int get_input_index_count() const;
	PortType get_input_index_type(int p_index) const;
	String get_input_index_name(int p_index) const;

	PortType get_input_type_by_name(String p_name) const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual Category get_category() const override { return CATEGORY_INPUT; }

	VisualShaderNodeInput();
};

///

class VisualShaderNodeOutput : public VisualShaderNode {
	GDCLASS(VisualShaderNodeOutput, VisualShaderNode);

public:
	friend class VisualShader;
	VisualShader::Type shader_type = VisualShader::Type::TYPE_MAX;
	Shader::Mode shader_mode = Shader::Mode::MODE_MAX;

	struct Port {
		Shader::Mode mode = Shader::Mode::MODE_MAX;
		VisualShader::Type shader_type = VisualShader::Type::TYPE_MAX;
		PortType type = PortType::PORT_TYPE_MAX;
		const char *name;
		const char *string;
	};

	static const Port ports[];

public:
	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;
	Variant get_input_port_default_value(int p_port) const;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual bool is_port_separator(int p_index) const override;

	virtual String get_caption() const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_OUTPUT; }

	VisualShaderNodeOutput();
};

class VisualShaderNodeParameter : public VisualShaderNode {
	GDCLASS(VisualShaderNodeParameter, VisualShaderNode);

public:
	enum Qualifier {
		QUAL_NONE,
		QUAL_GLOBAL,
		QUAL_INSTANCE,
		QUAL_MAX,
	};

private:
	String parameter_name = "";
	Qualifier qualifier = QUAL_NONE;
	bool global_code_generated = false;

protected:
	static void _bind_methods();
	String _get_qual_str() const;

#ifndef DISABLE_DEPRECATED
	bool _set(const StringName &p_name, const Variant &p_value);
#endif

public:
	void set_parameter_name(const String &p_name);
	String get_parameter_name() const;

	void set_qualifier(Qualifier p_qual);
	Qualifier get_qualifier() const;

	void set_global_code_generated(bool p_enabled);
	bool is_global_code_generated() const;

	virtual bool is_qualifier_supported(Qualifier p_qual) const = 0;
	virtual bool is_convertible_to_constant() const = 0;

	virtual Vector<StringName> get_editable_properties() const override;
	virtual String get_warning(Shader::Mode p_mode, VisualShader::Type p_type) const override;

	virtual Category get_category() const override { return CATEGORY_INPUT; }

	VisualShaderNodeParameter();
};

VARIANT_ENUM_CAST(VisualShaderNodeParameter::Qualifier)

class VisualShaderNodeParameterRef : public VisualShaderNode {
	GDCLASS(VisualShaderNodeParameterRef, VisualShaderNode);

public:
	enum ParameterType {
		PARAMETER_TYPE_FLOAT,
		PARAMETER_TYPE_INT,
		PARAMETER_TYPE_UINT,
		PARAMETER_TYPE_BOOLEAN,
		PARAMETER_TYPE_VECTOR2,
		PARAMETER_TYPE_VECTOR3,
		PARAMETER_TYPE_VECTOR4,
		PARAMETER_TYPE_TRANSFORM,
		PARAMETER_TYPE_COLOR,
		UNIFORM_TYPE_SAMPLER,
	};

	struct Parameter {
		String name;
		ParameterType type;
	};

private:
	RID shader_rid;
	String parameter_name = "[None]";
	ParameterType param_type = ParameterType::PARAMETER_TYPE_FLOAT;

protected:
	static void _bind_methods();

public:
	static void add_parameter(RID p_shader_rid, const String &p_name, ParameterType p_type);
	static void clear_parameters(RID p_shader_rid);
	static bool has_parameter(RID p_shader_rid, const String &p_name);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	bool is_shader_valid() const;
	void set_shader_rid(const RID &p_shader);

	void set_parameter_name(const String &p_name);
	String get_parameter_name() const;

	void update_parameter_type();

	void _set_parameter_type(int p_parameter_type);
	int _get_parameter_type() const;

	int get_parameters_count() const;
	String get_parameter_name_by_index(int p_idx) const;
	ParameterType get_parameter_type_by_name(const String &p_name) const;
	ParameterType get_parameter_type_by_index(int p_idx) const;
	PortType get_port_type_by_index(int p_idx) const;

	virtual Vector<StringName> get_editable_properties() const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_INPUT; }

	VisualShaderNodeParameterRef();
};

class VisualShaderNodeResizableBase : public VisualShaderNode {
	GDCLASS(VisualShaderNodeResizableBase, VisualShaderNode);

protected:
	Size2 size = Size2(0, 0);
	bool allow_v_resize = true;

protected:
	static void _bind_methods();

public:
	void set_size(const Size2 &p_size);
	Size2 get_size() const;

	bool is_allow_v_resize() const;
	void set_allow_v_resize(bool p_enabled);

	VisualShaderNodeResizableBase();
};

class VisualShaderNodeFrame : public VisualShaderNodeResizableBase {
	GDCLASS(VisualShaderNodeFrame, VisualShaderNodeResizableBase);

protected:
	String title = "Title";
	bool tint_color_enabled = false;
	Color tint_color = Color(0.3, 0.3, 0.3, 0.75);
	bool autoshrink = true;
	HashSet<int> attached_nodes;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	void set_title(const String &p_title);
	String get_title() const;

	void set_tint_color_enabled(bool p_enable);
	bool is_tint_color_enabled() const;

	void set_tint_color(const Color &p_color);
	Color get_tint_color() const;

	void set_autoshrink_enabled(bool p_enable);
	bool is_autoshrink_enabled() const;

	void add_attached_node(int p_node);
	void remove_attached_node(int p_node);
	void set_attached_nodes(const PackedInt32Array &p_nodes);
	PackedInt32Array get_attached_nodes() const;

	virtual Category get_category() const override { return CATEGORY_NONE; }

	VisualShaderNodeFrame();
};

#ifndef DISABLE_DEPRECATED
// Deprecated, for compatibility only.
class VisualShaderNodeComment : public VisualShaderNodeFrame {
	GDCLASS(VisualShaderNodeComment, VisualShaderNodeFrame);

	String description;

protected:
	static void _bind_methods();

public:
	virtual String get_caption() const override { return "Comment(Deprecated)"; }

	virtual Category get_category() const override { return CATEGORY_NONE; }

	void set_description(const String &p_description);
	String get_description() const;

	VisualShaderNodeComment() {}
};
#endif

class VisualShaderNodeGroupBase : public VisualShaderNodeResizableBase {
	GDCLASS(VisualShaderNodeGroupBase, VisualShaderNodeResizableBase);

private:
	void _apply_port_changes();

protected:
	String inputs = "";
	String outputs = "";
	bool editable = false;

	struct Port {
		PortType type = PortType::PORT_TYPE_MAX;
		String name;
	};

	HashMap<int, Port> input_ports;
	HashMap<int, Port> output_ports;
	HashMap<int, Control *> controls;

protected:
	static void _bind_methods();

public:
	void set_inputs(const String &p_inputs);
	String get_inputs() const;

	void set_outputs(const String &p_outputs);
	String get_outputs() const;

	bool is_valid_port_name(const String &p_name) const;

	void add_input_port(int p_id, int p_type, const String &p_name);
	void remove_input_port(int p_id);
	virtual int get_input_port_count() const override;
	bool has_input_port(int p_id) const;
	void clear_input_ports();

	void add_output_port(int p_id, int p_type, const String &p_name);
	void remove_output_port(int p_id);
	virtual int get_output_port_count() const override;
	bool has_output_port(int p_id) const;
	void clear_output_ports();

	void set_input_port_type(int p_id, int p_type);
	virtual PortType get_input_port_type(int p_id) const override;
	void set_input_port_name(int p_id, const String &p_name);
	virtual String get_input_port_name(int p_id) const override;

	void set_output_port_type(int p_id, int p_type);
	virtual PortType get_output_port_type(int p_id) const override;
	void set_output_port_name(int p_id, const String &p_name);
	virtual String get_output_port_name(int p_id) const override;

	int get_free_input_port_id() const;
	int get_free_output_port_id() const;

	void set_ctrl_pressed(Control *p_control, int p_index);
	Control *is_ctrl_pressed(int p_index);

	void set_editable(bool p_enabled);
	bool is_editable() const;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_SPECIAL; }

	VisualShaderNodeGroupBase();
};

class VisualShaderNodeExpression : public VisualShaderNodeGroupBase {
	GDCLASS(VisualShaderNodeExpression, VisualShaderNodeGroupBase);

private:
	bool _is_valid_identifier_char(char32_t p_c) const;
	String _replace_port_names(const Vector<Pair<String, String>> &p_pairs, const String &p_expression) const;

protected:
	String expression = "";

	static void _bind_methods();

public:
	virtual String get_caption() const override;

	void set_expression(const String &p_expression);
	String get_expression() const;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;
	virtual bool is_output_port_expandable(int p_port) const override;

	VisualShaderNodeExpression();
};

class VisualShaderNodeGlobalExpression : public VisualShaderNodeExpression {
	GDCLASS(VisualShaderNodeGlobalExpression, VisualShaderNodeExpression);

public:
	virtual String get_caption() const override;

	virtual String generate_global(Shader::Mode p_mode, VisualShader::Type p_type, int p_id) const override;

	VisualShaderNodeGlobalExpression();
};

class VisualShaderNodeVarying : public VisualShaderNode {
	GDCLASS(VisualShaderNodeVarying, VisualShaderNode);

public:
	struct Varying {
		String name;
		VisualShader::VaryingMode mode;
		VisualShader::VaryingType type;
		bool assigned = false;
	};

protected:
	RID shader_rid;
	VisualShader::VaryingType varying_type = VisualShader::VARYING_TYPE_FLOAT;
	String varying_name = "[None]";

public: // internal
	static void add_varying(RID p_shader_rid, const String &p_name, VisualShader::VaryingMode p_mode, VisualShader::VaryingType p_type);
	static void clear_varyings(RID p_shader_rid);
	static bool has_varying(RID p_shader_rid, const String &p_name);

	void set_shader_rid(const RID &p_shader);

	int get_varyings_count() const;
	String get_varying_name_by_index(int p_idx) const;
	VisualShader::VaryingMode get_varying_mode_by_name(const String &p_name) const;
	VisualShader::VaryingMode get_varying_mode_by_index(int p_idx) const;
	VisualShader::VaryingType get_varying_type_by_name(const String &p_name) const;
	VisualShader::VaryingType get_varying_type_by_index(int p_idx) const;
	PortType get_port_type_by_index(int p_idx) const;

protected:
	static void _bind_methods();

protected:
	String get_type_str() const;
	PortType get_port_type(VisualShader::VaryingType p_type, int p_port) const;

public:
	virtual String get_caption() const override = 0;

	virtual int get_input_port_count() const override = 0;
	virtual PortType get_input_port_type(int p_port) const override = 0;
	virtual String get_input_port_name(int p_port) const override = 0;

	virtual int get_output_port_count() const override = 0;
	virtual PortType get_output_port_type(int p_port) const override = 0;
	virtual String get_output_port_name(int p_port) const override = 0;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override = 0;

	void set_varying_name(String p_varying_name);
	String get_varying_name() const;

	void set_varying_type(VisualShader::VaryingType p_varying_type);
	VisualShader::VaryingType get_varying_type() const;

	VisualShaderNodeVarying();
};

class VisualShaderNodeVaryingSetter : public VisualShaderNodeVarying {
	GDCLASS(VisualShaderNodeVaryingSetter, VisualShaderNodeVarying);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_OUTPUT; }

	VisualShaderNodeVaryingSetter();
};

class VisualShaderNodeVaryingGetter : public VisualShaderNodeVarying {
	GDCLASS(VisualShaderNodeVaryingGetter, VisualShaderNodeVarying);

public:
	virtual String get_caption() const override;

	virtual int get_input_port_count() const override;
	virtual PortType get_input_port_type(int p_port) const override;
	virtual String get_input_port_name(int p_port) const override;

	virtual int get_output_port_count() const override;
	virtual PortType get_output_port_type(int p_port) const override;
	virtual String get_output_port_name(int p_port) const override;
	virtual bool has_output_port_preview(int p_port) const override;

	virtual String generate_code(Shader::Mode p_mode, VisualShader::Type p_type, int p_id, const String *p_input_vars, const String *p_output_vars, bool p_for_preview = false) const override;

	virtual Category get_category() const override { return CATEGORY_INPUT; }

	VisualShaderNodeVaryingGetter();
};

extern String make_unique_id(VisualShader::Type p_type, int p_id, const String &p_name);
