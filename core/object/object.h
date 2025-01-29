/**************************************************************************/
/*  object.h                                                              */
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

#ifndef OBJECT_H
#define OBJECT_H

#include "core/extension/gdextension_interface.h"
#include "core/object/message_queue.h"
#include "core/object/object_id.h"
#include "core/os/rw_lock.h"
#include "core/os/spin_lock.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/list.h"
#include "core/templates/rb_map.h"
#include "core/templates/safe_refcount.h"
#include "core/variant/callable_bind.h"
#include "core/variant/variant.h"

template <typename T>
class TypedArray;

enum PropertyHint {
	PROPERTY_HINT_NONE, ///< no hint provided.
	PROPERTY_HINT_RANGE, ///< hint_text = "min,max[,step][,or_greater][,or_less][,hide_slider][,radians_as_degrees][,degrees][,exp][,suffix:<keyword>] range.
	PROPERTY_HINT_ENUM, ///< hint_text= "val1,val2,val3,etc"
	PROPERTY_HINT_ENUM_SUGGESTION, ///< hint_text= "val1,val2,val3,etc"
	PROPERTY_HINT_EXP_EASING, /// exponential easing function (Math::ease) use "attenuation" hint string to revert (flip h), "positive_only" to exclude in-out and out-in. (ie: "attenuation,positive_only")
	PROPERTY_HINT_LINK,
	PROPERTY_HINT_FLAGS, ///< hint_text= "flag1,flag2,etc" (as bit flags)
	PROPERTY_HINT_LAYERS_2D_RENDER,
	PROPERTY_HINT_LAYERS_2D_PHYSICS,
	PROPERTY_HINT_LAYERS_2D_NAVIGATION,
	PROPERTY_HINT_LAYERS_3D_RENDER,
	PROPERTY_HINT_LAYERS_3D_PHYSICS,
	PROPERTY_HINT_LAYERS_3D_NAVIGATION,
	PROPERTY_HINT_FILE, ///< a file path must be passed, hint_text (optionally) is a filter "*.png,*.wav,*.doc,"
	PROPERTY_HINT_DIR, ///< a directory path must be passed
	PROPERTY_HINT_GLOBAL_FILE, ///< a file path must be passed, hint_text (optionally) is a filter "*.png,*.wav,*.doc,"
	PROPERTY_HINT_GLOBAL_DIR, ///< a directory path must be passed
	PROPERTY_HINT_RESOURCE_TYPE, ///< a comma-separated resource object type, e.g. "NoiseTexture,GradientTexture2D". Subclasses can be excluded with a "-" prefix if placed *after* the base class, e.g. "Texture2D,-MeshTexture".
	PROPERTY_HINT_MULTILINE_TEXT, ///< used for string properties that can contain multiple lines
	PROPERTY_HINT_EXPRESSION, ///< used for string properties that can contain multiple lines
	PROPERTY_HINT_PLACEHOLDER_TEXT, ///< used to set a placeholder text for string properties
	PROPERTY_HINT_COLOR_NO_ALPHA, ///< used for ignoring alpha component when editing a color
	PROPERTY_HINT_OBJECT_ID,
	PROPERTY_HINT_TYPE_STRING, ///< a type string, the hint is the base type to choose
	PROPERTY_HINT_NODE_PATH_TO_EDITED_NODE, // Deprecated.
	PROPERTY_HINT_OBJECT_TOO_BIG, ///< object is too big to send
	PROPERTY_HINT_NODE_PATH_VALID_TYPES,
	PROPERTY_HINT_SAVE_FILE, ///< a file path must be passed, hint_text (optionally) is a filter "*.png,*.wav,*.doc,". This opens a save dialog
	PROPERTY_HINT_GLOBAL_SAVE_FILE, ///< a file path must be passed, hint_text (optionally) is a filter "*.png,*.wav,*.doc,". This opens a save dialog
	PROPERTY_HINT_INT_IS_OBJECTID, // Deprecated.
	PROPERTY_HINT_INT_IS_POINTER,
	PROPERTY_HINT_ARRAY_TYPE,
	PROPERTY_HINT_LOCALE_ID,
	PROPERTY_HINT_LOCALIZABLE_STRING,
	PROPERTY_HINT_NODE_TYPE, ///< a node object type
	PROPERTY_HINT_HIDE_QUATERNION_EDIT, /// Only Node3D::transform should hide the quaternion editor.
	PROPERTY_HINT_PASSWORD,
	PROPERTY_HINT_LAYERS_AVOIDANCE,
	PROPERTY_HINT_DICTIONARY_TYPE,
	PROPERTY_HINT_TOOL_BUTTON,
	PROPERTY_HINT_ONESHOT, ///< the property will be changed by self after setting, such as AudioStreamPlayer.playing, Particles.emitting.
	PROPERTY_HINT_NO_NODEPATH, /// < this property will not contain a NodePath, regardless of type (Array, Dictionary, List, etc.). Needed for SceneTreeDock.
	PROPERTY_HINT_MAX,
};

enum PropertyUsageFlags {
	PROPERTY_USAGE_NONE = 0,
	PROPERTY_USAGE_STORAGE = 1 << 1,
	PROPERTY_USAGE_EDITOR = 1 << 2,
	PROPERTY_USAGE_INTERNAL = 1 << 3,
	PROPERTY_USAGE_CHECKABLE = 1 << 4, // Used for editing global variables.
	PROPERTY_USAGE_CHECKED = 1 << 5, // Used for editing global variables.
	PROPERTY_USAGE_GROUP = 1 << 6, // Used for grouping props in the editor.
	PROPERTY_USAGE_CATEGORY = 1 << 7,
	PROPERTY_USAGE_SUBGROUP = 1 << 8,
	PROPERTY_USAGE_CLASS_IS_BITFIELD = 1 << 9,
	PROPERTY_USAGE_NO_INSTANCE_STATE = 1 << 10,
	PROPERTY_USAGE_RESTART_IF_CHANGED = 1 << 11,
	PROPERTY_USAGE_SCRIPT_VARIABLE = 1 << 12,
	PROPERTY_USAGE_STORE_IF_NULL = 1 << 13,
	PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED = 1 << 14,
	PROPERTY_USAGE_SCRIPT_DEFAULT_VALUE = 1 << 15, // Deprecated.
	PROPERTY_USAGE_CLASS_IS_ENUM = 1 << 16,
	PROPERTY_USAGE_NIL_IS_VARIANT = 1 << 17,
	PROPERTY_USAGE_ARRAY = 1 << 18, // Used in the inspector to group properties as elements of an array.
	PROPERTY_USAGE_ALWAYS_DUPLICATE = 1 << 19, // When duplicating a resource, always duplicate, even with subresource duplication disabled.
	PROPERTY_USAGE_NEVER_DUPLICATE = 1 << 20, // When duplicating a resource, never duplicate, even with subresource duplication enabled.
	PROPERTY_USAGE_HIGH_END_GFX = 1 << 21,
	PROPERTY_USAGE_NODE_PATH_FROM_SCENE_ROOT = 1 << 22,
	PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT = 1 << 23,
	PROPERTY_USAGE_KEYING_INCREMENTS = 1 << 24, // Used in inspector to increment property when keyed in animation player.
	PROPERTY_USAGE_DEFERRED_SET_RESOURCE = 1 << 25, // Deprecated.
	PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT = 1 << 26, // For Object properties, instantiate them when creating in editor.
	PROPERTY_USAGE_EDITOR_BASIC_SETTING = 1 << 27, //for project or editor settings, show when basic settings are selected.
	PROPERTY_USAGE_READ_ONLY = 1 << 28, // Mark a property as read-only in the inspector.
	PROPERTY_USAGE_SECRET = 1 << 29, // Export preset credentials that should be stored separately from the rest of the export config.

	PROPERTY_USAGE_DEFAULT = PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR,
	PROPERTY_USAGE_NO_EDITOR = PROPERTY_USAGE_STORAGE,
};

#define ADD_SIGNAL(m_signal) ::ClassDB::add_signal(get_class_static(), m_signal)
#define ADD_PROPERTY(m_property, m_setter, m_getter) ::ClassDB::add_property(get_class_static(), m_property, _scs_create(m_setter), _scs_create(m_getter))
#define ADD_PROPERTYI(m_property, m_setter, m_getter, m_index) ::ClassDB::add_property(get_class_static(), m_property, _scs_create(m_setter), _scs_create(m_getter), m_index)
#define ADD_PROPERTY_DEFAULT(m_property, m_default) ::ClassDB::set_property_default_value(get_class_static(), m_property, m_default)
#define ADD_GROUP(m_name, m_prefix) ::ClassDB::add_property_group(get_class_static(), m_name, m_prefix)
#define ADD_GROUP_INDENT(m_name, m_prefix, m_depth) ::ClassDB::add_property_group(get_class_static(), m_name, m_prefix, m_depth)
#define ADD_SUBGROUP(m_name, m_prefix) ::ClassDB::add_property_subgroup(get_class_static(), m_name, m_prefix)
#define ADD_SUBGROUP_INDENT(m_name, m_prefix, m_depth) ::ClassDB::add_property_subgroup(get_class_static(), m_name, m_prefix, m_depth)
#define ADD_LINKED_PROPERTY(m_property, m_linked_property) ::ClassDB::add_linked_property(get_class_static(), m_property, m_linked_property)

#define ADD_ARRAY_COUNT(m_label, m_count_property, m_count_property_setter, m_count_property_getter, m_prefix) ClassDB::add_property_array_count(get_class_static(), m_label, m_count_property, _scs_create(m_count_property_setter), _scs_create(m_count_property_getter), m_prefix)
#define ADD_ARRAY_COUNT_WITH_USAGE_FLAGS(m_label, m_count_property, m_count_property_setter, m_count_property_getter, m_prefix, m_property_usage_flags) ClassDB::add_property_array_count(get_class_static(), m_label, m_count_property, _scs_create(m_count_property_setter), _scs_create(m_count_property_getter), m_prefix, m_property_usage_flags)
#define ADD_ARRAY(m_array_path, m_prefix) ClassDB::add_property_array(get_class_static(), m_array_path, m_prefix)

// Helper macro to use with PROPERTY_HINT_ARRAY_TYPE for arrays of specific resources:
// PropertyInfo(Variant::ARRAY, "fallbacks", PROPERTY_HINT_ARRAY_TYPE, MAKE_RESOURCE_TYPE_HINT("Font")
#define MAKE_RESOURCE_TYPE_HINT(m_type) vformat("%s/%s:%s", Variant::OBJECT, PROPERTY_HINT_RESOURCE_TYPE, m_type)

struct PropertyInfo {
	Variant::Type type = Variant::NIL;
	String name;
	StringName class_name; // For classes
	PropertyHint hint = PROPERTY_HINT_NONE;
	String hint_string;
	uint32_t usage = PROPERTY_USAGE_DEFAULT;

	// If you are thinking about adding another member to this class, ask the maintainer (Juan) first.

	_FORCE_INLINE_ PropertyInfo added_usage(uint32_t p_fl) const {
		PropertyInfo pi = *this;
		pi.usage |= p_fl;
		return pi;
	}

	operator Dictionary() const;

	static PropertyInfo from_dict(const Dictionary &p_dict);

	PropertyInfo() {}

	PropertyInfo(const Variant::Type p_type, const String &p_name, const PropertyHint p_hint = PROPERTY_HINT_NONE, const String &p_hint_string = "", const uint32_t p_usage = PROPERTY_USAGE_DEFAULT, const StringName &p_class_name = StringName()) :
			type(p_type),
			name(p_name),
			hint(p_hint),
			hint_string(p_hint_string),
			usage(p_usage) {
		if (hint == PROPERTY_HINT_RESOURCE_TYPE) {
			class_name = hint_string;
		} else {
			class_name = p_class_name;
		}
	}

	PropertyInfo(const StringName &p_class_name) :
			type(Variant::OBJECT),
			class_name(p_class_name) {}

	explicit PropertyInfo(const GDExtensionPropertyInfo &pinfo) :
			type((Variant::Type)pinfo.type),
			name(*reinterpret_cast<StringName *>(pinfo.name)),
			class_name(*reinterpret_cast<StringName *>(pinfo.class_name)),
			hint((PropertyHint)pinfo.hint),
			hint_string(*reinterpret_cast<String *>(pinfo.hint_string)),
			usage(pinfo.usage) {}

	bool operator==(const PropertyInfo &p_info) const {
		return ((type == p_info.type) &&
				(name == p_info.name) &&
				(class_name == p_info.class_name) &&
				(hint == p_info.hint) &&
				(hint_string == p_info.hint_string) &&
				(usage == p_info.usage));
	}

	bool operator<(const PropertyInfo &p_info) const {
		return name < p_info.name;
	}
};

TypedArray<Dictionary> convert_property_list(const List<PropertyInfo> *p_list);

enum MethodFlags {
	METHOD_FLAG_NORMAL = 1,
	METHOD_FLAG_EDITOR = 2,
	METHOD_FLAG_CONST = 4,
	METHOD_FLAG_VIRTUAL = 8,
	METHOD_FLAG_VARARG = 16,
	METHOD_FLAG_STATIC = 32,
	METHOD_FLAG_OBJECT_CORE = 64,
	METHOD_FLAG_VIRTUAL_REQUIRED = 128,
	METHOD_FLAGS_DEFAULT = METHOD_FLAG_NORMAL,
};

struct MethodInfo {
	String name;
	PropertyInfo return_val;
	uint32_t flags = METHOD_FLAGS_DEFAULT;
	int id = 0;
	List<PropertyInfo> arguments;
	Vector<Variant> default_arguments;
	int return_val_metadata = 0;
	Vector<int> arguments_metadata;

	int get_argument_meta(int p_arg) const {
		ERR_FAIL_COND_V(p_arg < -1 || p_arg > arguments.size(), 0);
		if (p_arg == -1) {
			return return_val_metadata;
		}
		return arguments_metadata.size() > p_arg ? arguments_metadata[p_arg] : 0;
	}

	inline bool operator==(const MethodInfo &p_method) const { return id == p_method.id && name == p_method.name; }
	inline bool operator<(const MethodInfo &p_method) const { return id == p_method.id ? (name < p_method.name) : (id < p_method.id); }

	operator Dictionary() const;

	static MethodInfo from_dict(const Dictionary &p_dict);

	uint32_t get_compatibility_hash() const;

	MethodInfo() {}

	explicit MethodInfo(const GDExtensionMethodInfo &pinfo) :
			name(*reinterpret_cast<StringName *>(pinfo.name)),
			return_val(PropertyInfo(pinfo.return_value)),
			flags(pinfo.flags),
			id(pinfo.id) {
		for (uint32_t j = 0; j < pinfo.argument_count; j++) {
			arguments.push_back(PropertyInfo(pinfo.arguments[j]));
		}
		const Variant *def_values = (const Variant *)pinfo.default_arguments;
		for (uint32_t j = 0; j < pinfo.default_argument_count; j++) {
			default_arguments.push_back(def_values[j]);
		}
	}

	void _push_params(const PropertyInfo &p_param) {
		arguments.push_back(p_param);
	}

	template <typename... VarArgs>
	void _push_params(const PropertyInfo &p_param, VarArgs... p_params) {
		arguments.push_back(p_param);
		_push_params(p_params...);
	}

	MethodInfo(const String &p_name) { name = p_name; }

	template <typename... VarArgs>
	MethodInfo(const String &p_name, VarArgs... p_params) {
		name = p_name;
		_push_params(p_params...);
	}

	MethodInfo(Variant::Type ret) { return_val.type = ret; }
	MethodInfo(Variant::Type ret, const String &p_name) {
		return_val.type = ret;
		name = p_name;
	}

	template <typename... VarArgs>
	MethodInfo(Variant::Type ret, const String &p_name, VarArgs... p_params) {
		name = p_name;
		return_val.type = ret;
		_push_params(p_params...);
	}

	MethodInfo(const PropertyInfo &p_ret, const String &p_name) {
		return_val = p_ret;
		name = p_name;
	}

	template <typename... VarArgs>
	MethodInfo(const PropertyInfo &p_ret, const String &p_name, VarArgs... p_params) {
		return_val = p_ret;
		name = p_name;
		_push_params(p_params...);
	}
};

// API used to extend in GDExtension and other C compatible compiled languages.
class MethodBind;
class GDExtension;

struct ObjectGDExtension {
	GDExtension *library = nullptr;
	ObjectGDExtension *parent = nullptr;
	List<ObjectGDExtension *> children;
	StringName parent_class_name;
	StringName class_name;
	bool editor_class = false;
	bool reloadable = false;
	bool is_virtual = false;
	bool is_abstract = false;
	bool is_exposed = true;
#ifdef TOOLS_ENABLED
	bool is_runtime = false;
	bool is_placeholder = false;
#endif
	GDExtensionClassSet set;
	GDExtensionClassGet get;
	GDExtensionClassGetPropertyList get_property_list;
	GDExtensionClassFreePropertyList2 free_property_list2;
	GDExtensionClassPropertyCanRevert property_can_revert;
	GDExtensionClassPropertyGetRevert property_get_revert;
	GDExtensionClassValidateProperty validate_property;
#ifndef DISABLE_DEPRECATED
	GDExtensionClassNotification notification;
	GDExtensionClassFreePropertyList free_property_list;
#endif // DISABLE_DEPRECATED
	GDExtensionClassNotification2 notification2;
	GDExtensionClassToString to_string;
	GDExtensionClassReference reference;
	GDExtensionClassReference unreference;
	GDExtensionClassGetRID get_rid;

	_FORCE_INLINE_ bool is_class(const String &p_class) const {
		const ObjectGDExtension *e = this;
		while (e) {
			if (p_class == e->class_name.operator String()) {
				return true;
			}
			e = e->parent;
		}
		return false;
	}
	void *class_userdata = nullptr;

#ifndef DISABLE_DEPRECATED
	GDExtensionClassCreateInstance create_instance;
#endif // DISABLE_DEPRECATED
	GDExtensionClassCreateInstance2 create_instance2;
	GDExtensionClassFreeInstance free_instance;
#ifndef DISABLE_DEPRECATED
	GDExtensionClassGetVirtual get_virtual;
	GDExtensionClassGetVirtualCallData get_virtual_call_data;
#endif // DISABLE_DEPRECATED
	GDExtensionClassGetVirtual2 get_virtual2;
	GDExtensionClassGetVirtualCallData2 get_virtual_call_data2;
	GDExtensionClassCallVirtualWithData call_virtual_with_data;
	GDExtensionClassRecreateInstance recreate_instance;

#ifdef TOOLS_ENABLED
	void *tracking_userdata = nullptr;
	void (*track_instance)(void *p_userdata, void *p_instance) = nullptr;
	void (*untrack_instance)(void *p_userdata, void *p_instance) = nullptr;
#endif
};

#define GDVIRTUAL_CALL(m_name, ...) _gdvirtual_##m_name##_call(__VA_ARGS__)
#define GDVIRTUAL_CALL_PTR(m_obj, m_name, ...) m_obj->_gdvirtual_##m_name##_call(__VA_ARGS__)

#ifdef DEBUG_METHODS_ENABLED
#define GDVIRTUAL_BIND(m_name, ...) ::ClassDB::add_virtual_method(get_class_static(), _gdvirtual_##m_name##_get_method_info(), true, sarray(__VA_ARGS__));
#else
#define GDVIRTUAL_BIND(m_name, ...)
#endif
#define GDVIRTUAL_BIND_COMPAT(m_alias, ...) ::ClassDB::add_virtual_compatibility_method(get_class_static(), _gdvirtual_##m_alias##_get_method_info(), true, sarray(__VA_ARGS__));
#define GDVIRTUAL_IS_OVERRIDDEN(m_name) _gdvirtual_##m_name##_overridden()
#define GDVIRTUAL_IS_OVERRIDDEN_PTR(m_obj, m_name) m_obj->_gdvirtual_##m_name##_overridden()

/*
 * The following is an incomprehensible blob of hacks and workarounds to
 * compensate for many of the fallacies in C++. As a plus, this macro pretty
 * much alone defines the object model.
 */

#define GDCLASS(m_class, m_inherits)                                                                                                        \
private:                                                                                                                                    \
	void operator=(const m_class &p_rval) {}                                                                                                \
	friend class ::ClassDB;                                                                                                                 \
                                                                                                                                            \
public:                                                                                                                                     \
	typedef m_class self_type;                                                                                                              \
	static constexpr bool _class_is_enabled = !bool(GD_IS_DEFINED(ClassDB_Disable_##m_class)) && m_inherits::_class_is_enabled;             \
	virtual String get_class() const override {                                                                                             \
		if (_get_extension()) {                                                                                                             \
			return _get_extension()->class_name.operator String();                                                                          \
		}                                                                                                                                   \
		return String(#m_class);                                                                                                            \
	}                                                                                                                                       \
	virtual const StringName *_get_class_namev() const override {                                                                           \
		static StringName _class_name_static;                                                                                               \
		if (unlikely(!_class_name_static)) {                                                                                                \
			StringName::assign_static_unique_class_name(&_class_name_static, #m_class);                                                     \
		}                                                                                                                                   \
		return &_class_name_static;                                                                                                         \
	}                                                                                                                                       \
	static _FORCE_INLINE_ void *get_class_ptr_static() {                                                                                    \
		static int ptr;                                                                                                                     \
		return &ptr;                                                                                                                        \
	}                                                                                                                                       \
	static _FORCE_INLINE_ String get_class_static() {                                                                                       \
		return String(#m_class);                                                                                                            \
	}                                                                                                                                       \
	static _FORCE_INLINE_ String get_parent_class_static() {                                                                                \
		return m_inherits::get_class_static();                                                                                              \
	}                                                                                                                                       \
	static void get_inheritance_list_static(List<String> *p_inheritance_list) {                                                             \
		m_inherits::get_inheritance_list_static(p_inheritance_list);                                                                        \
		p_inheritance_list->push_back(String(#m_class));                                                                                    \
	}                                                                                                                                       \
	virtual bool is_class(const String &p_class) const override {                                                                           \
		if (_get_extension() && _get_extension()->is_class(p_class)) {                                                                      \
			return true;                                                                                                                    \
		}                                                                                                                                   \
		return (p_class == (#m_class)) ? true : m_inherits::is_class(p_class);                                                              \
	}                                                                                                                                       \
	virtual bool is_class_ptr(void *p_ptr) const override {                                                                                 \
		return (p_ptr == get_class_ptr_static()) ? true : m_inherits::is_class_ptr(p_ptr);                                                  \
	}                                                                                                                                       \
                                                                                                                                            \
	static void get_valid_parents_static(List<String> *p_parents) {                                                                         \
		if (m_class::_get_valid_parents_static != m_inherits::_get_valid_parents_static) {                                                  \
			m_class::_get_valid_parents_static(p_parents);                                                                                  \
		}                                                                                                                                   \
                                                                                                                                            \
		m_inherits::get_valid_parents_static(p_parents);                                                                                    \
	}                                                                                                                                       \
                                                                                                                                            \
protected:                                                                                                                                  \
	_FORCE_INLINE_ static void (*_get_bind_methods())() {                                                                                   \
		return &m_class::_bind_methods;                                                                                                     \
	}                                                                                                                                       \
	_FORCE_INLINE_ static void (*_get_bind_compatibility_methods())() {                                                                     \
		return &m_class::_bind_compatibility_methods;                                                                                       \
	}                                                                                                                                       \
                                                                                                                                            \
public:                                                                                                                                     \
	static void initialize_class() {                                                                                                        \
		static bool initialized = false;                                                                                                    \
		if (initialized) {                                                                                                                  \
			return;                                                                                                                         \
		}                                                                                                                                   \
		m_inherits::initialize_class();                                                                                                     \
		::ClassDB::_add_class<m_class>();                                                                                                   \
		if (m_class::_get_bind_methods() != m_inherits::_get_bind_methods()) {                                                              \
			_bind_methods();                                                                                                                \
		}                                                                                                                                   \
		if (m_class::_get_bind_compatibility_methods() != m_inherits::_get_bind_compatibility_methods()) {                                  \
			_bind_compatibility_methods();                                                                                                  \
		}                                                                                                                                   \
		initialized = true;                                                                                                                 \
	}                                                                                                                                       \
                                                                                                                                            \
protected:                                                                                                                                  \
	virtual void _initialize_classv() override {                                                                                            \
		initialize_class();                                                                                                                 \
	}                                                                                                                                       \
	_FORCE_INLINE_ bool (Object::*_get_get() const)(const StringName &p_name, Variant &) const {                                            \
		return (bool(Object::*)(const StringName &, Variant &) const) & m_class::_get;                                                      \
	}                                                                                                                                       \
	virtual bool _getv(const StringName &p_name, Variant &r_ret) const override {                                                           \
		if (m_class::_get_get() != m_inherits::_get_get()) {                                                                                \
			if (_get(p_name, r_ret)) {                                                                                                      \
				return true;                                                                                                                \
			}                                                                                                                               \
		}                                                                                                                                   \
		return m_inherits::_getv(p_name, r_ret);                                                                                            \
	}                                                                                                                                       \
	_FORCE_INLINE_ bool (Object::*_get_set() const)(const StringName &p_name, const Variant &p_property) {                                  \
		return (bool(Object::*)(const StringName &, const Variant &)) & m_class::_set;                                                      \
	}                                                                                                                                       \
	virtual bool _setv(const StringName &p_name, const Variant &p_property) override {                                                      \
		if (m_inherits::_setv(p_name, p_property)) {                                                                                        \
			return true;                                                                                                                    \
		}                                                                                                                                   \
		if (m_class::_get_set() != m_inherits::_get_set()) {                                                                                \
			return _set(p_name, p_property);                                                                                                \
		}                                                                                                                                   \
		return false;                                                                                                                       \
	}                                                                                                                                       \
	_FORCE_INLINE_ void (Object::*_get_get_property_list() const)(List<PropertyInfo> * p_list) const {                                      \
		return (void(Object::*)(List<PropertyInfo> *) const) & m_class::_get_property_list;                                                 \
	}                                                                                                                                       \
	virtual void _get_property_listv(List<PropertyInfo> *p_list, bool p_reversed) const override {                                          \
		if (!p_reversed) {                                                                                                                  \
			m_inherits::_get_property_listv(p_list, p_reversed);                                                                            \
		}                                                                                                                                   \
		p_list->push_back(PropertyInfo(Variant::NIL, get_class_static(), PROPERTY_HINT_NONE, get_class_static(), PROPERTY_USAGE_CATEGORY)); \
		::ClassDB::get_property_list(#m_class, p_list, true, this);                                                                         \
		if (m_class::_get_get_property_list() != m_inherits::_get_get_property_list()) {                                                    \
			_get_property_list(p_list);                                                                                                     \
		}                                                                                                                                   \
		if (p_reversed) {                                                                                                                   \
			m_inherits::_get_property_listv(p_list, p_reversed);                                                                            \
		}                                                                                                                                   \
	}                                                                                                                                       \
	_FORCE_INLINE_ void (Object::*_get_validate_property() const)(PropertyInfo & p_property) const {                                        \
		return (void(Object::*)(PropertyInfo &) const) & m_class::_validate_property;                                                       \
	}                                                                                                                                       \
	virtual void _validate_propertyv(PropertyInfo &p_property) const override {                                                             \
		m_inherits::_validate_propertyv(p_property);                                                                                        \
		if (m_class::_get_validate_property() != m_inherits::_get_validate_property()) {                                                    \
			_validate_property(p_property);                                                                                                 \
		}                                                                                                                                   \
	}                                                                                                                                       \
	_FORCE_INLINE_ bool (Object::*_get_property_can_revert() const)(const StringName &p_name) const {                                       \
		return (bool(Object::*)(const StringName &) const) & m_class::_property_can_revert;                                                 \
	}                                                                                                                                       \
	virtual bool _property_can_revertv(const StringName &p_name) const override {                                                           \
		if (m_class::_get_property_can_revert() != m_inherits::_get_property_can_revert()) {                                                \
			if (_property_can_revert(p_name)) {                                                                                             \
				return true;                                                                                                                \
			}                                                                                                                               \
		}                                                                                                                                   \
		return m_inherits::_property_can_revertv(p_name);                                                                                   \
	}                                                                                                                                       \
	_FORCE_INLINE_ bool (Object::*_get_property_get_revert() const)(const StringName &p_name, Variant &) const {                            \
		return (bool(Object::*)(const StringName &, Variant &) const) & m_class::_property_get_revert;                                      \
	}                                                                                                                                       \
	virtual bool _property_get_revertv(const StringName &p_name, Variant &r_ret) const override {                                           \
		if (m_class::_get_property_get_revert() != m_inherits::_get_property_get_revert()) {                                                \
			if (_property_get_revert(p_name, r_ret)) {                                                                                      \
				return true;                                                                                                                \
			}                                                                                                                               \
		}                                                                                                                                   \
		return m_inherits::_property_get_revertv(p_name, r_ret);                                                                            \
	}                                                                                                                                       \
	_FORCE_INLINE_ void (Object::*_get_notification() const)(int) {                                                                         \
		return (void(Object::*)(int)) & m_class::_notification;                                                                             \
	}                                                                                                                                       \
	virtual void _notificationv(int p_notification, bool p_reversed) override {                                                             \
		if (!p_reversed) {                                                                                                                  \
			m_inherits::_notificationv(p_notification, p_reversed);                                                                         \
		}                                                                                                                                   \
		if (m_class::_get_notification() != m_inherits::_get_notification()) {                                                              \
			_notification(p_notification);                                                                                                  \
		}                                                                                                                                   \
		if (p_reversed) {                                                                                                                   \
			m_inherits::_notificationv(p_notification, p_reversed);                                                                         \
		}                                                                                                                                   \
	}                                                                                                                                       \
                                                                                                                                            \
private:

#define OBJ_SAVE_TYPE(m_class)                       \
public:                                              \
	virtual String get_save_class() const override { \
		return #m_class;                             \
	}                                                \
                                                     \
private:

class ScriptInstance;

class Object {
public:
	typedef Object self_type;

	enum ConnectFlags {
		CONNECT_DEFERRED = 1,
		CONNECT_PERSIST = 2, // hint for scene to save this connection
		CONNECT_ONE_SHOT = 4,
		CONNECT_REFERENCE_COUNTED = 8,
		CONNECT_INHERITED = 16, // Used in editor builds.
	};

	struct Connection {
		::Signal signal;
		Callable callable;

		uint32_t flags = 0;
		bool operator<(const Connection &p_conn) const;

		operator Variant() const;

		Connection() {}
		Connection(const Variant &p_variant);
	};

private:
#ifdef DEBUG_ENABLED
	friend struct _ObjectDebugLock;
#endif
	friend bool predelete_handler(Object *);
	friend void postinitialize_handler(Object *);

	ObjectGDExtension *_extension = nullptr;
	GDExtensionClassInstancePtr _extension_instance = nullptr;

	struct SignalData {
		struct Slot {
			int reference_count = 0;
			Connection conn;
			List<Connection>::Element *cE = nullptr;
		};

		MethodInfo user;
		HashMap<Callable, Slot, HashableHasher<Callable>> slot_map;
		bool removable = false;
	};

	HashMap<StringName, SignalData> signal_map;
	List<Connection> connections;
#ifdef DEBUG_ENABLED
	SafeRefCount _lock_index;
#endif
	bool _block_signals = false;
	int _predelete_ok = 0;
	ObjectID _instance_id;
	bool _predelete();
	void _initialize();
	void _postinitialize();
	bool _can_translate = true;
	bool _emitting = false;
#ifdef TOOLS_ENABLED
	bool _edited = false;
	uint32_t _edited_version = 0;
	HashSet<String> editor_section_folding;
#endif
	ScriptInstance *script_instance = nullptr;
	Variant script; // Reference does not exist yet, store it in a Variant.
	HashMap<StringName, Variant> metadata;
	HashMap<StringName, Variant *> metadata_properties;
	mutable const StringName *_class_name_ptr = nullptr;

	void _add_user_signal(const String &p_name, const Array &p_args = Array());
	bool _has_user_signal(const StringName &p_name) const;
	void _remove_user_signal(const StringName &p_name);
	Error _emit_signal(const Variant **p_args, int p_argcount, Callable::CallError &r_error);
	TypedArray<Dictionary> _get_signal_list() const;
	TypedArray<Dictionary> _get_signal_connection_list(const StringName &p_signal) const;
	TypedArray<Dictionary> _get_incoming_connections() const;
	void _set_bind(const StringName &p_set, const Variant &p_value);
	Variant _get_bind(const StringName &p_name) const;
	void _set_indexed_bind(const NodePath &p_name, const Variant &p_value);
	Variant _get_indexed_bind(const NodePath &p_name) const;
	int _get_method_argument_count_bind(const StringName &p_name) const;

	_FORCE_INLINE_ void _construct_object(bool p_reference);

	friend class RefCounted;
	bool type_is_reference = false;

	BinaryMutex _instance_binding_mutex;
	struct InstanceBinding {
		void *binding = nullptr;
		void *token = nullptr;
		GDExtensionInstanceBindingFreeCallback free_callback = nullptr;
		GDExtensionInstanceBindingReferenceCallback reference_callback = nullptr;
	};
	InstanceBinding *_instance_bindings = nullptr;
	uint32_t _instance_binding_count = 0;

	Object(bool p_reference);

protected:
	StringName _translation_domain;

	_FORCE_INLINE_ bool _instance_binding_reference(bool p_reference) {
		bool can_die = true;
		if (_instance_bindings) {
			MutexLock instance_binding_lock(_instance_binding_mutex);
			for (uint32_t i = 0; i < _instance_binding_count; i++) {
				if (_instance_bindings[i].reference_callback) {
					if (!_instance_bindings[i].reference_callback(_instance_bindings[i].token, _instance_bindings[i].binding, p_reference)) {
						can_die = false;
					}
				}
			}
		}
		return can_die;
	}

	friend class GDExtensionMethodBind;
	_ALWAYS_INLINE_ const ObjectGDExtension *_get_extension() const { return _extension; }
	_ALWAYS_INLINE_ GDExtensionClassInstancePtr _get_extension_instance() const { return _extension_instance; }
	virtual void _initialize_classv() { initialize_class(); }
	virtual bool _setv(const StringName &p_name, const Variant &p_property) { return false; }
	virtual bool _getv(const StringName &p_name, Variant &r_property) const { return false; }
	virtual void _get_property_listv(List<PropertyInfo> *p_list, bool p_reversed) const {}
	virtual void _validate_propertyv(PropertyInfo &p_property) const {}
	virtual bool _property_can_revertv(const StringName &p_name) const { return false; }
	virtual bool _property_get_revertv(const StringName &p_name, Variant &r_property) const { return false; }
	virtual void _notificationv(int p_notification, bool p_reversed) {}

	static void _bind_methods();
	static void _bind_compatibility_methods() {}
	bool _set(const StringName &p_name, const Variant &p_property) { return false; }
	bool _get(const StringName &p_name, Variant &r_property) const { return false; }
	void _get_property_list(List<PropertyInfo> *p_list) const {}
	void _validate_property(PropertyInfo &p_property) const {}
	bool _property_can_revert(const StringName &p_name) const { return false; }
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const { return false; }
	void _notification(int p_notification) {}

	_FORCE_INLINE_ static void (*_get_bind_methods())() {
		return &Object::_bind_methods;
	}
	_FORCE_INLINE_ static void (*_get_bind_compatibility_methods())() {
		return &Object::_bind_compatibility_methods;
	}
	_FORCE_INLINE_ bool (Object::*_get_get() const)(const StringName &p_name, Variant &r_ret) const {
		return &Object::_get;
	}
	_FORCE_INLINE_ bool (Object::*_get_set() const)(const StringName &p_name, const Variant &p_property) {
		return &Object::_set;
	}
	_FORCE_INLINE_ void (Object::*_get_get_property_list() const)(List<PropertyInfo> *p_list) const {
		return &Object::_get_property_list;
	}
	_FORCE_INLINE_ void (Object::*_get_validate_property() const)(PropertyInfo &p_property) const {
		return &Object::_validate_property;
	}
	_FORCE_INLINE_ bool (Object::*_get_property_can_revert() const)(const StringName &p_name) const {
		return &Object::_property_can_revert;
	}
	_FORCE_INLINE_ bool (Object::*_get_property_get_revert() const)(const StringName &p_name, Variant &) const {
		return &Object::_property_get_revert;
	}
	_FORCE_INLINE_ void (Object::*_get_notification() const)(int) {
		return &Object::_notification;
	}
	static void get_valid_parents_static(List<String> *p_parents);
	static void _get_valid_parents_static(List<String> *p_parents);

	Variant _call_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error);
	Variant _call_deferred_bind(const Variant **p_args, int p_argcount, Callable::CallError &r_error);

	virtual const StringName *_get_class_namev() const {
		static StringName _class_name_static;
		if (unlikely(!_class_name_static)) {
			StringName::assign_static_unique_class_name(&_class_name_static, "Object");
		}
		return &_class_name_static;
	}

	TypedArray<StringName> _get_meta_list_bind() const;
	TypedArray<Dictionary> _get_property_list_bind() const;
	TypedArray<Dictionary> _get_method_list_bind() const;

	void _clear_internal_resource_paths(const Variant &p_var);

	friend class ClassDB;
	friend class PlaceholderExtensionInstance;

	bool _disconnect(const StringName &p_signal, const Callable &p_callable, bool p_force = false);

#ifdef TOOLS_ENABLED
	struct VirtualMethodTracker {
		void **method;
		bool *initialized;
		VirtualMethodTracker *next;
	};

	mutable VirtualMethodTracker *virtual_method_list = nullptr;
#endif

public: // Should be protected, but bug in clang++.
	static void initialize_class();
	_FORCE_INLINE_ static void register_custom_data_to_otdb() {}

public:
	static constexpr bool _class_is_enabled = true;

	void notify_property_list_changed();

	static void *get_class_ptr_static() {
		static int ptr;
		return &ptr;
	}

	void detach_from_objectdb();
	_FORCE_INLINE_ ObjectID get_instance_id() const { return _instance_id; }

	template <typename T>
	static T *cast_to(Object *p_object) {
		return p_object ? dynamic_cast<T *>(p_object) : nullptr;
	}

	template <typename T>
	static const T *cast_to(const Object *p_object) {
		return p_object ? dynamic_cast<const T *>(p_object) : nullptr;
	}

	enum {
		NOTIFICATION_POSTINITIALIZE = 0,
		NOTIFICATION_PREDELETE = 1,
		NOTIFICATION_EXTENSION_RELOADED = 2,
		// Internal notification to send after NOTIFICATION_PREDELETE, not bound to scripting.
		NOTIFICATION_PREDELETE_CLEANUP = 3,
	};

	/* TYPE API */
	static void get_inheritance_list_static(List<String> *p_inheritance_list) { p_inheritance_list->push_back("Object"); }

	static String get_class_static() { return "Object"; }
	static String get_parent_class_static() { return String(); }

	virtual String get_class() const {
		if (_extension) {
			return _extension->class_name.operator String();
		}
		return "Object";
	}
	virtual String get_save_class() const { return get_class(); } //class stored when saving

	virtual bool is_class(const String &p_class) const {
		if (_extension && _extension->is_class(p_class)) {
			return true;
		}
		return (p_class == "Object");
	}
	virtual bool is_class_ptr(void *p_ptr) const { return get_class_ptr_static() == p_ptr; }

	_FORCE_INLINE_ const StringName &get_class_name() const {
		if (_extension) {
			// Can't put inside the unlikely as constructor can run it
			return _extension->class_name;
		}

		if (unlikely(!_class_name_ptr)) {
			// While class is initializing / deinitializing, constructors and destructurs
			// need access to the proper class at the proper stage.
			return *_get_class_namev();
		}
		return *_class_name_ptr;
	}

	StringName get_class_name_for_extension(const GDExtension *p_library) const;

	/* IAPI */

	void set(const StringName &p_name, const Variant &p_value, bool *r_valid = nullptr);
	Variant get(const StringName &p_name, bool *r_valid = nullptr) const;
	void set_indexed(const Vector<StringName> &p_names, const Variant &p_value, bool *r_valid = nullptr);
	Variant get_indexed(const Vector<StringName> &p_names, bool *r_valid = nullptr) const;

	void get_property_list(List<PropertyInfo> *p_list, bool p_reversed = false) const;
	void validate_property(PropertyInfo &p_property) const;
	bool property_can_revert(const StringName &p_name) const;
	Variant property_get_revert(const StringName &p_name) const;

	bool has_method(const StringName &p_method) const;
	int get_method_argument_count(const StringName &p_method, bool *r_is_valid = nullptr) const;
	void get_method_list(List<MethodInfo> *p_list) const;
	Variant callv(const StringName &p_method, const Array &p_args);
	virtual Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error);
	virtual Variant call_const(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error);

	template <typename... VarArgs>
	Variant call(const StringName &p_method, VarArgs... p_args) {
		Variant args[sizeof...(p_args) + 1] = { p_args..., Variant() }; // +1 makes sure zero sized arrays are also supported.
		const Variant *argptrs[sizeof...(p_args) + 1];
		for (uint32_t i = 0; i < sizeof...(p_args); i++) {
			argptrs[i] = &args[i];
		}
		Callable::CallError cerr;
		const Variant ret = callp(p_method, sizeof...(p_args) == 0 ? nullptr : (const Variant **)argptrs, sizeof...(p_args), cerr);
		return (cerr.error == Callable::CallError::CALL_OK) ? ret : Variant();
	}

	void notification(int p_notification, bool p_reversed = false);
	virtual String to_string();

	// Used mainly by script, get and set all INCLUDING string.
	virtual Variant getvar(const Variant &p_key, bool *r_valid = nullptr) const;
	virtual void setvar(const Variant &p_key, const Variant &p_value, bool *r_valid = nullptr);

	/* SCRIPT */

// When in debug, some non-virtual functions can be overridden for multithreaded guards.
#ifdef DEBUG_ENABLED
#define MTVIRTUAL virtual
#else
#define MTVIRTUAL
#endif

	MTVIRTUAL void set_script(const Variant &p_script);
	MTVIRTUAL Variant get_script() const;

	MTVIRTUAL bool has_meta(const StringName &p_name) const;
	MTVIRTUAL void set_meta(const StringName &p_name, const Variant &p_value);
	MTVIRTUAL void remove_meta(const StringName &p_name);
	MTVIRTUAL Variant get_meta(const StringName &p_name, const Variant &p_default = Variant()) const;
	MTVIRTUAL void get_meta_list(List<StringName> *p_list) const;
	MTVIRTUAL void merge_meta_from(const Object *p_src);

#ifdef TOOLS_ENABLED
	void set_edited(bool p_edited);
	bool is_edited() const;
	// This function is used to check when something changed beyond a point, it's used mainly for generating previews.
	uint32_t get_edited_version() const;
#endif

	void set_script_instance(ScriptInstance *p_instance);
	_FORCE_INLINE_ ScriptInstance *get_script_instance() const { return script_instance; }

	// Some script languages can't control instance creation, so this function eases the process.
	void set_script_and_instance(const Variant &p_script, ScriptInstance *p_instance);

	void add_user_signal(const MethodInfo &p_signal);

	template <typename... VarArgs>
	Error emit_signal(const StringName &p_name, VarArgs... p_args) {
		Variant args[sizeof...(p_args) + 1] = { p_args..., Variant() }; // +1 makes sure zero sized arrays are also supported.
		const Variant *argptrs[sizeof...(p_args) + 1];
		for (uint32_t i = 0; i < sizeof...(p_args); i++) {
			argptrs[i] = &args[i];
		}
		return emit_signalp(p_name, sizeof...(p_args) == 0 ? nullptr : (const Variant **)argptrs, sizeof...(p_args));
	}

	MTVIRTUAL Error emit_signalp(const StringName &p_name, const Variant **p_args, int p_argcount);
	MTVIRTUAL bool has_signal(const StringName &p_name) const;
	MTVIRTUAL void get_signal_list(List<MethodInfo> *p_signals) const;
	MTVIRTUAL void get_signal_connection_list(const StringName &p_signal, List<Connection> *p_connections) const;
	MTVIRTUAL void get_all_signal_connections(List<Connection> *p_connections) const;
	MTVIRTUAL int get_persistent_signal_connection_count() const;
	MTVIRTUAL void get_signals_connected_to_this(List<Connection> *p_connections) const;

	MTVIRTUAL Error connect(const StringName &p_signal, const Callable &p_callable, uint32_t p_flags = 0);
	MTVIRTUAL void disconnect(const StringName &p_signal, const Callable &p_callable);
	MTVIRTUAL bool is_connected(const StringName &p_signal, const Callable &p_callable) const;
	MTVIRTUAL bool has_connections(const StringName &p_signal) const;

	template <typename... VarArgs>
	void call_deferred(const StringName &p_name, VarArgs... p_args) {
		MessageQueue::get_singleton()->push_call(this, p_name, p_args...);
	}

	void set_deferred(const StringName &p_property, const Variant &p_value);

	void set_block_signals(bool p_block);
	bool is_blocking_signals() const;

	Variant::Type get_static_property_type(const StringName &p_property, bool *r_valid = nullptr) const;
	Variant::Type get_static_property_type_indexed(const Vector<StringName> &p_path, bool *r_valid = nullptr) const;

	// Translate message (internationalization).
	String tr(const StringName &p_message, const StringName &p_context = "") const;
	String tr_n(const StringName &p_message, const StringName &p_message_plural, int p_n, const StringName &p_context = "") const;

	bool _is_queued_for_deletion = false; // Set to true by SceneTree::queue_delete().
	bool is_queued_for_deletion() const;

	_FORCE_INLINE_ void set_message_translation(bool p_enable) { _can_translate = p_enable; }
	_FORCE_INLINE_ bool can_translate_messages() const { return _can_translate; }

	virtual StringName get_translation_domain() const;
	virtual void set_translation_domain(const StringName &p_domain);

#ifdef TOOLS_ENABLED
	virtual void get_argument_options(const StringName &p_function, int p_idx, List<String> *r_options) const;
	void editor_set_section_unfold(const String &p_section, bool p_unfolded);
	bool editor_is_section_unfolded(const String &p_section);
	const HashSet<String> &editor_get_section_folding() const { return editor_section_folding; }
	void editor_clear_section_folding() { editor_section_folding.clear(); }

#endif

	// Used by script languages to store binding data.
	void *get_instance_binding(void *p_token, const GDExtensionInstanceBindingCallbacks *p_callbacks);
	// Used on creation by binding only.
	void set_instance_binding(void *p_token, void *p_binding, const GDExtensionInstanceBindingCallbacks *p_callbacks);
	bool has_instance_binding(void *p_token);
	void free_instance_binding(void *p_token);

#ifdef TOOLS_ENABLED
	void clear_internal_extension();
	void reset_internal_extension(ObjectGDExtension *p_extension);
	bool is_extension_placeholder() const { return _extension && _extension->is_placeholder; }
#endif

	void clear_internal_resource_paths();

	_ALWAYS_INLINE_ bool is_ref_counted() const { return type_is_reference; }

	void cancel_free();

	Object();
	virtual ~Object();
};

bool predelete_handler(Object *p_object);
void postinitialize_handler(Object *p_object);

class ObjectDB {
// This needs to add up to 63, 1 bit is for reference.
#define OBJECTDB_VALIDATOR_BITS 39
#define OBJECTDB_VALIDATOR_MASK ((uint64_t(1) << OBJECTDB_VALIDATOR_BITS) - 1)
#define OBJECTDB_SLOT_MAX_COUNT_BITS 24
#define OBJECTDB_SLOT_MAX_COUNT_MASK ((uint64_t(1) << OBJECTDB_SLOT_MAX_COUNT_BITS) - 1)
#define OBJECTDB_REFERENCE_BIT (uint64_t(1) << (OBJECTDB_SLOT_MAX_COUNT_BITS + OBJECTDB_VALIDATOR_BITS))

	struct ObjectSlot { // 128 bits per slot.
		uint64_t validator : OBJECTDB_VALIDATOR_BITS;
		uint64_t next_free : OBJECTDB_SLOT_MAX_COUNT_BITS;
		uint64_t is_ref_counted : 1;
		Object *object = nullptr;
	};

	static SpinLock spin_lock;
	static uint32_t slot_count;
	static uint32_t slot_max;
	static ObjectSlot *object_slots;
	static uint64_t validator_counter;

	friend class Object;
	friend void unregister_core_types();
	static void cleanup();

	static ObjectID add_instance(Object *p_object);
	static void remove_instance(Object *p_object);

	friend void register_core_types();
	static void setup();

public:
	typedef void (*DebugFunc)(Object *p_obj);

	_ALWAYS_INLINE_ static Object *get_instance(ObjectID p_instance_id) {
		uint64_t id = p_instance_id;
		uint32_t slot = id & OBJECTDB_SLOT_MAX_COUNT_MASK;

		ERR_FAIL_COND_V(slot >= slot_max, nullptr); // This should never happen unless RID is corrupted.

		spin_lock.lock();

		uint64_t validator = (id >> OBJECTDB_SLOT_MAX_COUNT_BITS) & OBJECTDB_VALIDATOR_MASK;

		if (unlikely(object_slots[slot].validator != validator)) {
			spin_lock.unlock();
			return nullptr;
		}

		Object *object = object_slots[slot].object;

		spin_lock.unlock();

		return object;
	}
	static void debug_objects(DebugFunc p_func);
	static int get_object_count();
};

#endif // OBJECT_H
