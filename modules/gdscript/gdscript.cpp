/**************************************************************************/
/*  gdscript.cpp                                                          */
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

#include "gdscript.h"

#include "gdscript_analyzer.h"
#include "gdscript_cache.h"
#include "gdscript_compiler.h"
#include "gdscript_parser.h"
#include "gdscript_rpc_callable.h"
#include "gdscript_tokenizer_buffer.h"
#include "gdscript_warning.h"

#ifdef TOOLS_ENABLED
#include "editor/gdscript_docgen.h"
#endif

#ifdef TESTS_ENABLED
#include "tests/gdscript_test_runner.h"
#endif

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/core_constants.h"
#include "core/io/file_access.h"

#include "scene/resources/packed_scene.h"
#include "scene/scene_string_names.h"

#ifdef TOOLS_ENABLED
#include "core/extension/gdextension_manager.h"
#include "editor/editor_paths.h"
#endif

#include <stdint.h>

///////////////////////////

GDScriptNativeClass::GDScriptNativeClass(const StringName &p_name) {
	name = p_name;
}

bool GDScriptNativeClass::_get(const StringName &p_name, Variant &r_ret) const {
	bool ok;
	int64_t v = ClassDB::get_integer_constant(name, p_name, &ok);

	if (ok) {
		r_ret = v;
		return true;
	}

	MethodBind *method = ClassDB::get_method(name, p_name);
	if (method && method->is_static()) {
		// Native static method.
		r_ret = Callable(this, p_name);
		return true;
	}

	return false;
}

void GDScriptNativeClass::_bind_methods() {
	ClassDB::bind_method(D_METHOD("new"), &GDScriptNativeClass::_new);
}

Variant GDScriptNativeClass::_new() {
	Object *o = instantiate();
	ERR_FAIL_NULL_V_MSG(o, Variant(), "Class type: '" + String(name) + "' is not instantiable.");

	RefCounted *rc = Object::cast_to<RefCounted>(o);
	if (rc) {
		return Ref<RefCounted>(rc);
	} else {
		return o;
	}
}

Object *GDScriptNativeClass::instantiate() {
	return ClassDB::instantiate_no_placeholders(name);
}

Variant GDScriptNativeClass::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (p_method == SNAME("new")) {
		// Constructor.
		return Object::callp(p_method, p_args, p_argcount, r_error);
	}
	MethodBind *method = ClassDB::get_method(name, p_method);
	if (method && method->is_static()) {
		// Native static method.
		return method->call(nullptr, p_args, p_argcount, r_error);
	}

	r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
	return Variant();
}

GDScriptFunction *GDScript::_super_constructor(GDScript *p_script) {
	if (likely(p_script->valid) && p_script->initializer) {
		return p_script->initializer;
	} else {
		GDScript *base_src = p_script->_base;
		if (base_src != nullptr) {
			return _super_constructor(base_src);
		} else {
			return nullptr;
		}
	}
}

void GDScript::_super_implicit_constructor(GDScript *p_script, GDScriptInstance *p_instance, Callable::CallError &r_error) {
	GDScript *base_src = p_script->_base;
	if (base_src != nullptr) {
		_super_implicit_constructor(base_src, p_instance, r_error);
		if (r_error.error != Callable::CallError::CALL_OK) {
			return;
		}
	}
	ERR_FAIL_NULL(p_script->implicit_initializer);
	if (likely(p_script->valid)) {
		p_script->implicit_initializer->call(p_instance, nullptr, 0, r_error);
	} else {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
	}
}

GDScriptInstance *GDScript::_create_instance(const Variant **p_args, int p_argcount, Object *p_owner, bool p_is_ref_counted, Callable::CallError &r_error) {
	/* STEP 1, CREATE */

	GDScriptInstance *instance = memnew(GDScriptInstance);
	instance->base_ref_counted = p_is_ref_counted;
	instance->members.resize(member_indices.size());
	instance->script = Ref<GDScript>(this);
	instance->owner = p_owner;
	instance->owner_id = p_owner->get_instance_id();
#ifdef DEBUG_ENABLED
	//needed for hot reloading
	for (const KeyValue<StringName, MemberInfo> &E : member_indices) {
		instance->member_indices_cache[E.key] = E.value.index;
	}
#endif
	instance->owner->set_script_instance(instance);

	/* STEP 2, INITIALIZE AND CONSTRUCT */
	{
		MutexLock lock(GDScriptLanguage::singleton->mutex);
		instances.insert(instance->owner);
	}

	_super_implicit_constructor(this, instance, r_error);
	if (r_error.error != Callable::CallError::CALL_OK) {
		String error_text = Variant::get_call_error_text(instance->owner, "@implicit_new", nullptr, 0, r_error);
		instance->script = Ref<GDScript>();
		instance->owner->set_script_instance(nullptr);
		{
			MutexLock lock(GDScriptLanguage::singleton->mutex);
			instances.erase(p_owner);
		}
		ERR_FAIL_V_MSG(nullptr, "Error constructing a GDScriptInstance: " + error_text);
	}

	if (p_argcount < 0) {
		return instance;
	}

	initializer = _super_constructor(this);
	if (initializer != nullptr) {
		initializer->call(instance, p_args, p_argcount, r_error);
		if (r_error.error != Callable::CallError::CALL_OK) {
			String error_text = Variant::get_call_error_text(instance->owner, "_init", p_args, p_argcount, r_error);
			instance->script = Ref<GDScript>();
			instance->owner->set_script_instance(nullptr);
			{
				MutexLock lock(GDScriptLanguage::singleton->mutex);
				instances.erase(p_owner);
			}
			ERR_FAIL_V_MSG(nullptr, "Error constructing a GDScriptInstance: " + error_text);
		}
	}
	//@TODO make thread safe
	return instance;
}

Variant GDScript::_new(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	/* STEP 1, CREATE */

	if (!valid) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		return Variant();
	}

	r_error.error = Callable::CallError::CALL_OK;
	Ref<RefCounted> ref;
	Object *owner = nullptr;

	GDScript *_baseptr = this;
	while (_baseptr->_base) {
		_baseptr = _baseptr->_base;
	}

	ERR_FAIL_COND_V(_baseptr->native.is_null(), Variant());
	if (_baseptr->native.ptr()) {
		owner = _baseptr->native->instantiate();
	} else {
		owner = memnew(RefCounted); //by default, no base means use reference
	}
	ERR_FAIL_NULL_V_MSG(owner, Variant(), "Can't inherit from a virtual class.");

	RefCounted *r = Object::cast_to<RefCounted>(owner);
	if (r) {
		ref = Ref<RefCounted>(r);
	}

	GDScriptInstance *instance = _create_instance(p_args, p_argcount, owner, r != nullptr, r_error);
	if (!instance) {
		if (ref.is_null()) {
			memdelete(owner); //no owner, sorry
		}
		return Variant();
	}

	if (ref.is_valid()) {
		return ref;
	} else {
		return owner;
	}
}

bool GDScript::can_instantiate() const {
#ifdef TOOLS_ENABLED
	return valid && (tool || ScriptServer::is_scripting_enabled()) && !Engine::get_singleton()->is_recovery_mode_hint();
#else
	return valid;
#endif
}

Ref<Script> GDScript::get_base_script() const {
	if (_base) {
		return Ref<GDScript>(_base);
	} else {
		return Ref<Script>();
	}
}

StringName GDScript::get_global_name() const {
	return global_name;
}

StringName GDScript::get_instance_base_type() const {
	if (native.is_valid()) {
		return native->get_name();
	}
	if (base.is_valid() && base->is_valid()) {
		return base->get_instance_base_type();
	}
	return StringName();
}

struct _GDScriptMemberSort {
	int index = 0;
	StringName name;
	_FORCE_INLINE_ bool operator<(const _GDScriptMemberSort &p_member) const { return index < p_member.index; }
};

#ifdef TOOLS_ENABLED

void GDScript::_placeholder_erased(PlaceHolderScriptInstance *p_placeholder) {
	placeholders.erase(p_placeholder);
}

#endif

void GDScript::_get_script_method_list(List<MethodInfo> *r_list, bool p_include_base) const {
	const GDScript *current = this;
	while (current) {
		for (const KeyValue<StringName, GDScriptFunction *> &E : current->member_functions) {
			r_list->push_back(E.value->get_method_info());
		}

		if (!p_include_base) {
			return;
		}

		current = current->_base;
	}
}

void GDScript::get_script_method_list(List<MethodInfo> *r_list) const {
	_get_script_method_list(r_list, true);
}

void GDScript::_get_script_property_list(List<PropertyInfo> *r_list, bool p_include_base) const {
	const GDScript *sptr = this;
	List<PropertyInfo> props;

	while (sptr) {
		Vector<_GDScriptMemberSort> msort;
		for (const KeyValue<StringName, MemberInfo> &E : sptr->member_indices) {
			if (!sptr->members.has(E.key)) {
				continue; // Skip base class members.
			}
			_GDScriptMemberSort ms;
			ms.index = E.value.index;
			ms.name = E.key;
			msort.push_back(ms);
		}

		msort.sort();
		msort.reverse();
		for (int i = 0; i < msort.size(); i++) {
			props.push_front(sptr->member_indices[msort[i].name].property_info);
		}

#ifdef TOOLS_ENABLED
		r_list->push_back(sptr->get_class_category());
#endif // TOOLS_ENABLED

		for (const PropertyInfo &E : props) {
			r_list->push_back(E);
		}

		if (!p_include_base) {
			break;
		}

		props.clear();
		sptr = sptr->_base;
	}
}

void GDScript::get_script_property_list(List<PropertyInfo> *r_list) const {
	_get_script_property_list(r_list, true);
}

bool GDScript::has_method(const StringName &p_method) const {
	return member_functions.has(p_method);
}

bool GDScript::has_static_method(const StringName &p_method) const {
	return member_functions.has(p_method) && member_functions[p_method]->is_static();
}

int GDScript::get_script_method_argument_count(const StringName &p_method, bool *r_is_valid) const {
	HashMap<StringName, GDScriptFunction *>::ConstIterator E = member_functions.find(p_method);
	if (!E) {
		if (r_is_valid) {
			*r_is_valid = false;
		}
		return 0;
	}

	if (r_is_valid) {
		*r_is_valid = true;
	}
	return E->value->get_argument_count();
}

MethodInfo GDScript::get_method_info(const StringName &p_method) const {
	HashMap<StringName, GDScriptFunction *>::ConstIterator E = member_functions.find(p_method);
	if (!E) {
		return MethodInfo();
	}

	return E->value->get_method_info();
}

bool GDScript::get_property_default_value(const StringName &p_property, Variant &r_value) const {
#ifdef TOOLS_ENABLED

	HashMap<StringName, Variant>::ConstIterator E = member_default_values_cache.find(p_property);
	if (E) {
		r_value = E->value;
		return true;
	}

	if (base_cache.is_valid()) {
		return base_cache->get_property_default_value(p_property, r_value);
	}
#endif
	return false;
}

ScriptInstance *GDScript::instance_create(Object *p_this) {
	ERR_FAIL_COND_V_MSG(!valid, nullptr, "Script is invalid!");

	GDScript *top = this;
	while (top->_base) {
		top = top->_base;
	}

	if (top->native.is_valid()) {
		if (!ClassDB::is_parent_class(p_this->get_class_name(), top->native->get_name())) {
			if (EngineDebugger::is_active()) {
				GDScriptLanguage::get_singleton()->debug_break_parse(_get_debug_path(), 1, "Script inherits from native type '" + String(top->native->get_name()) + "', so it can't be assigned to an object of type: '" + p_this->get_class() + "'");
			}
			ERR_FAIL_V_MSG(nullptr, "Script inherits from native type '" + String(top->native->get_name()) + "', so it can't be assigned to an object of type '" + p_this->get_class() + "'" + ".");
		}
	}

	Callable::CallError unchecked_error;
	return _create_instance(nullptr, 0, p_this, Object::cast_to<RefCounted>(p_this) != nullptr, unchecked_error);
}

PlaceHolderScriptInstance *GDScript::placeholder_instance_create(Object *p_this) {
#ifdef TOOLS_ENABLED
	PlaceHolderScriptInstance *si = memnew(PlaceHolderScriptInstance(GDScriptLanguage::get_singleton(), Ref<Script>(this), p_this));
	placeholders.insert(si);
	_update_exports(nullptr, false, si);
	return si;
#else
	return nullptr;
#endif
}

bool GDScript::instance_has(const Object *p_this) const {
	MutexLock lock(GDScriptLanguage::singleton->mutex);

	return instances.has((Object *)p_this);
}

bool GDScript::has_source_code() const {
	return !source.is_empty();
}

String GDScript::get_source_code() const {
	return source;
}

void GDScript::set_source_code(const String &p_code) {
	if (source == p_code) {
		return;
	}
	source = p_code;
#ifdef TOOLS_ENABLED
	source_changed_cache = true;
#endif
}

#ifdef TOOLS_ENABLED
void GDScript::_update_exports_values(HashMap<StringName, Variant> &values, List<PropertyInfo> &propnames) {
	for (const KeyValue<StringName, Variant> &E : member_default_values_cache) {
		values[E.key] = E.value;
	}

	for (const PropertyInfo &E : members_cache) {
		propnames.push_back(E);
	}

	if (base_cache.is_valid()) {
		base_cache->_update_exports_values(values, propnames);
	}
}

void GDScript::_add_doc(const DocData::ClassDoc &p_doc) {
	doc_class_name = p_doc.name;
	if (_owner) { // Only the top-level class stores doc info.
		_owner->_add_doc(p_doc);
	} else { // Remove old docs, add new.
		for (int i = 0; i < docs.size(); i++) {
			if (docs[i].name == p_doc.name) {
				docs.remove_at(i);
				break;
			}
		}
		docs.append(p_doc);
	}
}

void GDScript::_clear_doc() {
	doc_class_name = StringName();
	doc = DocData::ClassDoc();
	docs.clear();
}

String GDScript::get_class_icon_path() const {
	return simplified_icon_path;
}
#endif

bool GDScript::_update_exports(bool *r_err, bool p_recursive_call, PlaceHolderScriptInstance *p_instance_to_update, bool p_base_exports_changed) {
#ifdef TOOLS_ENABLED

	static Vector<GDScript *> base_caches;
	if (!p_recursive_call) {
		base_caches.clear();
	}
	base_caches.append(this);

	bool changed = p_base_exports_changed;

	if (source_changed_cache) {
		source_changed_cache = false;
		changed = true;

		String basedir = path;

		if (basedir.is_empty()) {
			basedir = get_path();
		}

		if (!basedir.is_empty()) {
			basedir = basedir.get_base_dir();
		}

		GDScriptParser parser;
		GDScriptAnalyzer analyzer(&parser);
		Error err = parser.parse(source, path, false);

		if (err == OK && analyzer.analyze() == OK) {
			const GDScriptParser::ClassNode *c = parser.get_tree();

			if (base_cache.is_valid()) {
				base_cache->inheriters_cache.erase(get_instance_id());
				base_cache = Ref<GDScript>();
			}

			GDScriptParser::DataType base_type = parser.get_tree()->base_type;
			if (base_type.kind == GDScriptParser::DataType::CLASS) {
				Ref<GDScript> bf = GDScriptCache::get_full_script(base_type.script_path, err, path);
				if (err == OK) {
					bf = Ref<GDScript>(bf->find_class(base_type.class_type->fqcn));
					if (bf.is_valid()) {
						base_cache = bf;
						bf->inheriters_cache.insert(get_instance_id());
					}
				}
			}

			members_cache.clear();
			member_default_values_cache.clear();
			_signals.clear();

			members_cache.push_back(get_class_category());

			for (int i = 0; i < c->members.size(); i++) {
				const GDScriptParser::ClassNode::Member &member = c->members[i];

				switch (member.type) {
					case GDScriptParser::ClassNode::Member::VARIABLE: {
						if (!member.variable->exported) {
							continue;
						}

						members_cache.push_back(member.variable->export_info);
						Variant default_value = analyzer.make_variable_default_value(member.variable);
						member_default_values_cache[member.variable->identifier->name] = default_value;
					} break;
					case GDScriptParser::ClassNode::Member::SIGNAL: {
						_signals[member.signal->identifier->name] = member.signal->method_info;
					} break;
					case GDScriptParser::ClassNode::Member::GROUP: {
						members_cache.push_back(member.annotation->export_info);
					} break;
					default:
						break; // Nothing.
				}
			}
		} else {
			placeholder_fallback_enabled = true;
			return false;
		}
	} else if (placeholder_fallback_enabled) {
		return false;
	}

	placeholder_fallback_enabled = false;

	if (base_cache.is_valid() && base_cache->is_valid()) {
		for (int i = 0; i < base_caches.size(); i++) {
			if (base_caches[i] == base_cache.ptr()) {
				if (r_err) {
					*r_err = true;
				}
				valid = false; // to show error in the editor
				base_cache->valid = false;
				base_cache->inheriters_cache.clear(); // to prevent future stackoverflows
				base_cache.unref();
				base.unref();
				_base = nullptr;
				ERR_FAIL_V_MSG(false, "Cyclic inheritance in script class.");
			}
		}
		if (base_cache->_update_exports(r_err, true)) {
			if (r_err && *r_err) {
				return false;
			}
			changed = true;
		}
	}

	if ((changed || p_instance_to_update) && placeholders.size()) { //hm :(

		// update placeholders if any
		HashMap<StringName, Variant> values;
		List<PropertyInfo> propnames;
		_update_exports_values(values, propnames);

		if (changed) {
			for (PlaceHolderScriptInstance *E : placeholders) {
				E->update(propnames, values);
			}
		} else {
			p_instance_to_update->update(propnames, values);
		}
	}

	return changed;

#else
	return false;
#endif
}

void GDScript::update_exports() {
#ifdef TOOLS_ENABLED
	_update_exports_down(false);
#endif
}

#ifdef TOOLS_ENABLED
void GDScript::_update_exports_down(bool p_base_exports_changed) {
	bool cyclic_error = false;
	bool changed = _update_exports(&cyclic_error, false, nullptr, p_base_exports_changed);

	if (cyclic_error) {
		return;
	}

	HashSet<ObjectID> copy = inheriters_cache; //might get modified

	for (const ObjectID &E : copy) {
		Object *id = ObjectDB::get_instance(E);
		GDScript *s = Object::cast_to<GDScript>(id);

		if (!s) {
			continue;
		}
		s->_update_exports_down(p_base_exports_changed || changed);
	}
}
#endif

String GDScript::_get_debug_path() const {
	if (is_built_in() && !get_name().is_empty()) {
		return vformat("%s(%s)", get_name(), get_script_path());
	} else {
		return get_script_path();
	}
}

Error GDScript::_static_init() {
	if (likely(valid) && static_initializer) {
		Callable::CallError call_err;
		static_initializer->call(nullptr, nullptr, 0, call_err);
		if (call_err.error != Callable::CallError::CALL_OK) {
			return ERR_CANT_CREATE;
		}
	}
	Error err = OK;
	for (KeyValue<StringName, Ref<GDScript>> &inner : subclasses) {
		err = inner.value->_static_init();
		if (err) {
			break;
		}
	}
	return err;
}

void GDScript::_static_default_init() {
	for (const KeyValue<StringName, MemberInfo> &E : static_variables_indices) {
		const GDScriptDataType &type = E.value.data_type;
		// Only initialize builtin types, which are not expected to be `null`.
		if (!type.has_type || type.kind != GDScriptDataType::BUILTIN) {
			continue;
		}
		if (type.builtin_type == Variant::ARRAY && type.has_container_element_type(0)) {
			const GDScriptDataType element_type = type.get_container_element_type(0);
			Array default_value;
			default_value.set_typed(element_type.builtin_type, element_type.native_type, element_type.script_type);
			static_variables.write[E.value.index] = default_value;
		} else if (type.builtin_type == Variant::DICTIONARY && type.has_container_element_types()) {
			const GDScriptDataType key_type = type.get_container_element_type_or_variant(0);
			const GDScriptDataType value_type = type.get_container_element_type_or_variant(1);
			Dictionary default_value;
			default_value.set_typed(key_type.builtin_type, key_type.native_type, key_type.script_type, value_type.builtin_type, value_type.native_type, value_type.script_type);
			static_variables.write[E.value.index] = default_value;
		} else {
			Variant default_value;
			Callable::CallError err;
			Variant::construct(type.builtin_type, default_value, nullptr, 0, err);
			static_variables.write[E.value.index] = default_value;
		}
	}
}

#ifdef TOOLS_ENABLED

void GDScript::_save_old_static_data() {
	old_static_variables_indices = static_variables_indices;
	old_static_variables = static_variables;
	for (KeyValue<StringName, Ref<GDScript>> &inner : subclasses) {
		inner.value->_save_old_static_data();
	}
}

void GDScript::_restore_old_static_data() {
	for (KeyValue<StringName, MemberInfo> &E : old_static_variables_indices) {
		if (static_variables_indices.has(E.key)) {
			static_variables.write[static_variables_indices[E.key].index] = old_static_variables[E.value.index];
		}
	}
	old_static_variables_indices.clear();
	old_static_variables.clear();
	for (KeyValue<StringName, Ref<GDScript>> &inner : subclasses) {
		inner.value->_restore_old_static_data();
	}
}

#endif

Error GDScript::reload(bool p_keep_state) {
	if (reloading) {
		return OK;
	}
	reloading = true;

	bool has_instances;
	{
		MutexLock lock(GDScriptLanguage::singleton->mutex);

		has_instances = instances.size();
	}

	ERR_FAIL_COND_V(!p_keep_state && has_instances, ERR_ALREADY_IN_USE);

	String basedir = path;

	if (basedir.is_empty()) {
		basedir = get_path();
	}

	if (!basedir.is_empty()) {
		basedir = basedir.get_base_dir();
	}

	// Loading a template, don't parse.
#ifdef TOOLS_ENABLED
	if (EditorPaths::get_singleton() && basedir.begins_with(EditorPaths::get_singleton()->get_project_script_templates_dir())) {
		reloading = false;
		return OK;
	}
#endif

	{
		String source_path = path;
		if (source_path.is_empty()) {
			source_path = get_path();
		}
		if (!source_path.is_empty()) {
			if (GDScriptCache::get_cached_script(source_path).is_null()) {
				MutexLock lock(GDScriptCache::singleton->mutex);
				GDScriptCache::singleton->shallow_gdscript_cache[source_path] = Ref<GDScript>(this);
			}
			if (GDScriptCache::has_parser(source_path)) {
				Error err = OK;
				Ref<GDScriptParserRef> parser_ref = GDScriptCache::get_parser(source_path, GDScriptParserRef::EMPTY, err);
				if (parser_ref.is_valid()) {
					uint32_t source_hash;
					if (!binary_tokens.is_empty()) {
						source_hash = hash_djb2_buffer(binary_tokens.ptr(), binary_tokens.size());
					} else {
						source_hash = source.hash();
					}
					if (parser_ref->get_source_hash() != source_hash) {
						GDScriptCache::remove_parser(source_path);
					}
				}
			}
		}
	}

	bool can_run = ScriptServer::is_scripting_enabled() || is_tool();

#ifdef TOOLS_ENABLED
	if (p_keep_state && can_run && is_valid()) {
		_save_old_static_data();
	}
#endif

	valid = false;
	GDScriptParser parser;
	Error err;
	if (!binary_tokens.is_empty()) {
		err = parser.parse_binary(binary_tokens, path);
	} else {
		err = parser.parse(source, path, false);
	}
	if (err) {
		if (EngineDebugger::is_active()) {
			GDScriptLanguage::get_singleton()->debug_break_parse(_get_debug_path(), parser.get_errors().front()->get().line, "Parser Error: " + parser.get_errors().front()->get().message);
		}
		// TODO: Show all error messages.
		_err_print_error("GDScript::reload", path.is_empty() ? "built-in" : (const char *)path.utf8().get_data(), parser.get_errors().front()->get().line, ("Parse Error: " + parser.get_errors().front()->get().message).utf8().get_data(), false, ERR_HANDLER_SCRIPT);
		reloading = false;
		return ERR_PARSE_ERROR;
	}

	GDScriptAnalyzer analyzer(&parser);
	err = analyzer.analyze();

	if (err) {
		if (EngineDebugger::is_active()) {
			GDScriptLanguage::get_singleton()->debug_break_parse(_get_debug_path(), parser.get_errors().front()->get().line, "Parser Error: " + parser.get_errors().front()->get().message);
		}

		const List<GDScriptParser::ParserError>::Element *e = parser.get_errors().front();
		while (e != nullptr) {
			_err_print_error("GDScript::reload", path.is_empty() ? "built-in" : (const char *)path.utf8().get_data(), e->get().line, ("Parse Error: " + e->get().message).utf8().get_data(), false, ERR_HANDLER_SCRIPT);
			e = e->next();
		}
		reloading = false;
		return ERR_PARSE_ERROR;
	}

	can_run = ScriptServer::is_scripting_enabled() || parser.is_tool();

	GDScriptCompiler compiler;
	err = compiler.compile(&parser, this, p_keep_state);

	if (err) {
		_err_print_error("GDScript::reload", path.is_empty() ? "built-in" : (const char *)path.utf8().get_data(), compiler.get_error_line(), ("Compile Error: " + compiler.get_error()).utf8().get_data(), false, ERR_HANDLER_SCRIPT);
		if (can_run) {
			if (EngineDebugger::is_active()) {
				GDScriptLanguage::get_singleton()->debug_break_parse(_get_debug_path(), compiler.get_error_line(), "Parser Error: " + compiler.get_error());
			}
			reloading = false;
			return ERR_COMPILATION_FAILED;
		} else {
			reloading = false;
			return err;
		}
	}

#ifdef TOOLS_ENABLED
	// Done after compilation because it needs the GDScript object's inner class GDScript objects,
	// which are made by calling make_scripts() within compiler.compile() above.
	GDScriptDocGen::generate_docs(this, parser.get_tree());
#endif

#ifdef DEBUG_ENABLED
	for (const GDScriptWarning &warning : parser.get_warnings()) {
		if (EngineDebugger::is_active()) {
			Vector<ScriptLanguage::StackInfo> si;
			EngineDebugger::get_script_debugger()->send_error("", get_script_path(), warning.start_line, warning.get_name(), warning.get_message(), false, ERR_HANDLER_WARNING, si);
		}
	}
#endif

	if (can_run) {
		err = _static_init();
		if (err) {
			return err;
		}
	}

#ifdef TOOLS_ENABLED
	if (can_run && p_keep_state) {
		_restore_old_static_data();
	}

	if (p_keep_state) {
		// Update the properties in the inspector.
		update_exports();
	}
#endif

	reloading = false;
	return OK;
}

ScriptLanguage *GDScript::get_language() const {
	return GDScriptLanguage::get_singleton();
}

void GDScript::get_constants(HashMap<StringName, Variant> *p_constants) {
	if (p_constants) {
		for (const KeyValue<StringName, Variant> &E : constants) {
			(*p_constants)[E.key] = E.value;
		}
	}
}

void GDScript::get_members(HashSet<StringName> *p_members) {
	if (p_members) {
		for (const StringName &E : members) {
			p_members->insert(E);
		}
	}
}

Variant GDScript::get_rpc_config() const {
	return rpc_config;
}

void GDScript::unload_static() const {
	GDScriptCache::remove_script(fully_qualified_name);
}

Variant GDScript::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	GDScript *top = this;
	while (top) {
		if (likely(top->valid)) {
			HashMap<StringName, GDScriptFunction *>::Iterator E = top->member_functions.find(p_method);
			if (E) {
				ERR_FAIL_COND_V_MSG(!E->value->is_static(), Variant(), "Can't call non-static function '" + String(p_method) + "' in script.");

				return E->value->call(nullptr, p_args, p_argcount, r_error);
			}
		}
		top = top->_base;
	}

	//none found, regular

	return Script::callp(p_method, p_args, p_argcount, r_error);
}

bool GDScript::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == GDScriptLanguage::get_singleton()->strings._script_source) {
		r_ret = get_source_code();
		return true;
	}

	const GDScript *top = this;
	while (top) {
		{
			HashMap<StringName, Variant>::ConstIterator E = top->constants.find(p_name);
			if (E) {
				r_ret = E->value;
				return true;
			}
		}

		{
			HashMap<StringName, MemberInfo>::ConstIterator E = top->static_variables_indices.find(p_name);
			if (E) {
				if (likely(top->valid) && E->value.getter) {
					Callable::CallError ce;
					const Variant ret = const_cast<GDScript *>(this)->callp(E->value.getter, nullptr, 0, ce);
					r_ret = (ce.error == Callable::CallError::CALL_OK) ? ret : Variant();
					return true;
				}
				r_ret = top->static_variables[E->value.index];
				return true;
			}
		}

		if (likely(top->valid)) {
			HashMap<StringName, GDScriptFunction *>::ConstIterator E = top->member_functions.find(p_name);
			if (E && E->value->is_static()) {
				if (top->rpc_config.has(p_name)) {
					r_ret = Callable(memnew(GDScriptRPCCallable(const_cast<GDScript *>(top), E->key)));
				} else {
					r_ret = Callable(const_cast<GDScript *>(top), E->key);
				}
				return true;
			}
		}

		{
			HashMap<StringName, Ref<GDScript>>::ConstIterator E = top->subclasses.find(p_name);
			if (E) {
				r_ret = E->value;
				return true;
			}
		}

		top = top->_base;
	}

	return false;
}

bool GDScript::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == GDScriptLanguage::get_singleton()->strings._script_source) {
		set_source_code(p_value);
		reload(true);
		return true;
	}

	GDScript *top = this;
	while (top) {
		HashMap<StringName, MemberInfo>::ConstIterator E = top->static_variables_indices.find(p_name);
		if (E) {
			const MemberInfo *member = &E->value;
			Variant value = p_value;
			if (member->data_type.has_type && !member->data_type.is_type(value)) {
				const Variant *args = &p_value;
				Callable::CallError err;
				Variant::construct(member->data_type.builtin_type, value, &args, 1, err);
				if (err.error != Callable::CallError::CALL_OK || !member->data_type.is_type(value)) {
					return false;
				}
			}
			if (likely(top->valid) && member->setter) {
				const Variant *args = &value;
				Callable::CallError err;
				callp(member->setter, &args, 1, err);
				return err.error == Callable::CallError::CALL_OK;
			} else {
				top->static_variables.write[member->index] = value;
				return true;
			}
		}

		top = top->_base;
	}

	return false;
}

void GDScript::_get_property_list(List<PropertyInfo> *p_properties) const {
	p_properties->push_back(PropertyInfo(Variant::STRING, "script/source", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_INTERNAL));

	List<const GDScript *> classes;
	const GDScript *top = this;
	while (top) {
		classes.push_back(top);
		top = top->_base;
	}

	for (const List<const GDScript *>::Element *E = classes.back(); E; E = E->prev()) {
		Vector<_GDScriptMemberSort> msort;
		for (const KeyValue<StringName, MemberInfo> &F : E->get()->static_variables_indices) {
			_GDScriptMemberSort ms;
			ms.index = F.value.index;
			ms.name = F.key;
			msort.push_back(ms);
		}
		msort.sort();

		for (int i = 0; i < msort.size(); i++) {
			p_properties->push_back(E->get()->static_variables_indices[msort[i].name].property_info);
		}
	}
}

void GDScript::_bind_methods() {
	ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "new", &GDScript::_new, MethodInfo("new"));
}

void GDScript::set_path_cache(const String &p_path) {
	if (ResourceCache::has(p_path)) {
		set_path(p_path, true);
		return;
	}

	if (is_root_script()) {
		Script::set_path_cache(p_path);
	}

	String old_path = path;
	path = p_path;
	path_valid = true;
	GDScriptCache::move_script(old_path, p_path);

	for (KeyValue<StringName, Ref<GDScript>> &kv : subclasses) {
		kv.value->set_path_cache(p_path);
	}
}

void GDScript::set_path(const String &p_path, bool p_take_over) {
	if (is_root_script()) {
		Script::set_path(p_path, p_take_over);
	}

	String old_path = path;
	path = p_path;
	path_valid = true;
	GDScriptCache::move_script(old_path, p_path);

	for (KeyValue<StringName, Ref<GDScript>> &kv : subclasses) {
		kv.value->set_path(p_path, p_take_over);
	}
}

String GDScript::get_script_path() const {
	if (!path_valid && !get_path().is_empty()) {
		return get_path();
	}
	return path;
}

Error GDScript::load_source_code(const String &p_path) {
	if (p_path.is_empty() || p_path.begins_with("gdscript://") || ResourceLoader::get_resource_type(p_path.get_slice("::", 0)) == "PackedScene") {
		return OK;
	}

	Vector<uint8_t> sourcef;
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (err) {
		const char *err_name;
		if (err < 0 || err >= ERR_MAX) {
			err_name = "(invalid error code)";
		} else {
			err_name = error_names[err];
		}
		ERR_FAIL_COND_V_MSG(err, err, "Attempt to open script '" + p_path + "' resulted in error '" + err_name + "'.");
	}

	uint64_t len = f->get_length();
	sourcef.resize(len + 1);
	uint8_t *w = sourcef.ptrw();
	uint64_t r = f->get_buffer(w, len);
	ERR_FAIL_COND_V(r != len, ERR_CANT_OPEN);
	w[len] = 0;

	String s;
	if (s.parse_utf8((const char *)w, len) != OK) {
		ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Script '" + p_path + "' contains invalid unicode (UTF-8), so it was not loaded. Please ensure that scripts are saved in valid UTF-8 unicode.");
	}

	source = s;
	path = p_path;
	path_valid = true;
#ifdef TOOLS_ENABLED
	source_changed_cache = true;
	set_edited(false);
	set_last_modified_time(FileAccess::get_modified_time(path));
#endif // TOOLS_ENABLED
	return OK;
}

void GDScript::set_binary_tokens_source(const Vector<uint8_t> &p_binary_tokens) {
	binary_tokens = p_binary_tokens;
}

const Vector<uint8_t> &GDScript::get_binary_tokens_source() const {
	return binary_tokens;
}

Vector<uint8_t> GDScript::get_as_binary_tokens() const {
	GDScriptTokenizerBuffer tokenizer;
	return tokenizer.parse_code_string(source, GDScriptTokenizerBuffer::COMPRESS_NONE);
}

const HashMap<StringName, GDScriptFunction *> &GDScript::debug_get_member_functions() const {
	return member_functions;
}

StringName GDScript::debug_get_member_by_index(int p_idx) const {
	for (const KeyValue<StringName, MemberInfo> &E : member_indices) {
		if (E.value.index == p_idx) {
			return E.key;
		}
	}

	return "<error>";
}

StringName GDScript::debug_get_static_var_by_index(int p_idx) const {
	for (const KeyValue<StringName, MemberInfo> &E : static_variables_indices) {
		if (E.value.index == p_idx) {
			return E.key;
		}
	}

	return "<error>";
}

Ref<GDScript> GDScript::get_base() const {
	return base;
}

bool GDScript::inherits_script(const Ref<Script> &p_script) const {
	Ref<GDScript> gd = p_script;
	if (gd.is_null()) {
		return false;
	}

	const GDScript *s = this;

	while (s) {
		if (s == p_script.ptr()) {
			return true;
		}
		s = s->_base;
	}

	return false;
}

GDScript *GDScript::find_class(const String &p_qualified_name) {
	String first = p_qualified_name.get_slice("::", 0);

	Vector<String> class_names;
	GDScript *result = nullptr;
	// Empty initial name means start here.
	if (first.is_empty() || first == global_name) {
		class_names = p_qualified_name.split("::");
		result = this;
	} else if (p_qualified_name.begins_with(get_root_script()->path)) {
		// Script path could have a class path separator("::") in it.
		class_names = p_qualified_name.trim_prefix(get_root_script()->path).split("::");
		result = get_root_script();
	} else if (HashMap<StringName, Ref<GDScript>>::Iterator E = subclasses.find(first)) {
		class_names = p_qualified_name.split("::");
		result = E->value.ptr();
	} else if (_owner != nullptr) {
		// Check parent scope.
		return _owner->find_class(p_qualified_name);
	}

	// Starts at index 1 because index 0 was handled above.
	for (int i = 1; result != nullptr && i < class_names.size(); i++) {
		if (HashMap<StringName, Ref<GDScript>>::Iterator E = result->subclasses.find(class_names[i])) {
			result = E->value.ptr();
		} else {
			// Couldn't find inner class.
			return nullptr;
		}
	}

	return result;
}

bool GDScript::has_class(const GDScript *p_script) {
	String fqn = p_script->fully_qualified_name;
	if (fully_qualified_name.is_empty() && fqn.get_slice("::", 0).is_empty()) {
		return p_script == this;
	} else if (fqn.begins_with(fully_qualified_name)) {
		return p_script == find_class(fqn.trim_prefix(fully_qualified_name));
	}
	return false;
}

GDScript *GDScript::get_root_script() {
	GDScript *result = this;
	while (result->_owner) {
		result = result->_owner;
	}
	return result;
}

RBSet<GDScript *> GDScript::get_dependencies() {
	RBSet<GDScript *> dependencies;

	_collect_dependencies(dependencies, this);
	dependencies.erase(this);

	return dependencies;
}

HashMap<GDScript *, RBSet<GDScript *>> GDScript::get_all_dependencies() {
	HashMap<GDScript *, RBSet<GDScript *>> all_dependencies;

	List<GDScript *> scripts;
	{
		MutexLock lock(GDScriptLanguage::singleton->mutex);

		SelfList<GDScript> *elem = GDScriptLanguage::singleton->script_list.first();
		while (elem) {
			scripts.push_back(elem->self());
			elem = elem->next();
		}
	}

	for (GDScript *scr : scripts) {
		if (scr == nullptr || scr->destructing) {
			continue;
		}
		all_dependencies.insert(scr, scr->get_dependencies());
	}

	return all_dependencies;
}

RBSet<GDScript *> GDScript::get_must_clear_dependencies() {
	RBSet<GDScript *> dependencies = get_dependencies();
	RBSet<GDScript *> must_clear_dependencies;
	HashMap<GDScript *, RBSet<GDScript *>> all_dependencies = get_all_dependencies();

	RBSet<GDScript *> cant_clear;
	for (KeyValue<GDScript *, RBSet<GDScript *>> &E : all_dependencies) {
		if (dependencies.has(E.key)) {
			continue;
		}
		for (GDScript *F : E.value) {
			if (dependencies.has(F)) {
				cant_clear.insert(F);
			}
		}
	}

	for (GDScript *E : dependencies) {
		if (cant_clear.has(E) || ScriptServer::is_global_class(E->get_fully_qualified_name())) {
			continue;
		}
		must_clear_dependencies.insert(E);
	}

	cant_clear.clear();
	dependencies.clear();
	all_dependencies.clear();
	return must_clear_dependencies;
}

bool GDScript::has_script_signal(const StringName &p_signal) const {
	if (_signals.has(p_signal)) {
		return true;
	}
	if (base.is_valid()) {
		return base->has_script_signal(p_signal);
	}
#ifdef TOOLS_ENABLED
	else if (base_cache.is_valid()) {
		return base_cache->has_script_signal(p_signal);
	}
#endif
	return false;
}

void GDScript::_get_script_signal_list(List<MethodInfo> *r_list, bool p_include_base) const {
	for (const KeyValue<StringName, MethodInfo> &E : _signals) {
		r_list->push_back(E.value);
	}

	if (!p_include_base) {
		return;
	}

	if (base.is_valid()) {
		base->get_script_signal_list(r_list);
	}
#ifdef TOOLS_ENABLED
	else if (base_cache.is_valid()) {
		base_cache->get_script_signal_list(r_list);
	}
#endif
}

void GDScript::get_script_signal_list(List<MethodInfo> *r_signals) const {
	_get_script_signal_list(r_signals, true);
}

GDScript *GDScript::_get_gdscript_from_variant(const Variant &p_variant) {
	Object *obj = p_variant;
	if (obj == nullptr || obj->get_instance_id().is_null()) {
		return nullptr;
	}
	return Object::cast_to<GDScript>(obj);
}

void GDScript::_collect_function_dependencies(GDScriptFunction *p_func, RBSet<GDScript *> &p_dependencies, const GDScript *p_except) {
	if (p_func == nullptr) {
		return;
	}
	for (GDScriptFunction *lambda : p_func->lambdas) {
		_collect_function_dependencies(lambda, p_dependencies, p_except);
	}
	for (const Variant &V : p_func->constants) {
		GDScript *scr = _get_gdscript_from_variant(V);
		if (scr != nullptr && scr != p_except) {
			scr->_collect_dependencies(p_dependencies, p_except);
		}
	}
}

void GDScript::_collect_dependencies(RBSet<GDScript *> &p_dependencies, const GDScript *p_except) {
	if (p_dependencies.has(this)) {
		return;
	}
	if (this != p_except) {
		p_dependencies.insert(this);
	}

	for (const KeyValue<StringName, GDScriptFunction *> &E : member_functions) {
		_collect_function_dependencies(E.value, p_dependencies, p_except);
	}

	if (implicit_initializer) {
		_collect_function_dependencies(implicit_initializer, p_dependencies, p_except);
	}

	if (implicit_ready) {
		_collect_function_dependencies(implicit_ready, p_dependencies, p_except);
	}

	if (static_initializer) {
		_collect_function_dependencies(static_initializer, p_dependencies, p_except);
	}

	for (KeyValue<StringName, Ref<GDScript>> &E : subclasses) {
		if (E.value != p_except) {
			E.value->_collect_dependencies(p_dependencies, p_except);
		}
	}

	for (const KeyValue<StringName, Variant> &E : constants) {
		GDScript *scr = _get_gdscript_from_variant(E.value);
		if (scr != nullptr && scr != p_except) {
			scr->_collect_dependencies(p_dependencies, p_except);
		}
	}
}

GDScript::GDScript() :
		script_list(this) {
	{
		MutexLock lock(GDScriptLanguage::get_singleton()->mutex);

		GDScriptLanguage::get_singleton()->script_list.add(&script_list);
	}

	path = vformat("gdscript://%d.gd", get_instance_id());
}

void GDScript::_save_orphaned_subclasses(ClearData *p_clear_data) {
	struct ClassRefWithName {
		ObjectID id;
		String fully_qualified_name;
	};
	Vector<ClassRefWithName> weak_subclasses;
	// collect subclasses ObjectID and name
	for (KeyValue<StringName, Ref<GDScript>> &E : subclasses) {
		E.value->_owner = nullptr; //bye, you are no longer owned cause I died
		ClassRefWithName subclass;
		subclass.id = E.value->get_instance_id();
		subclass.fully_qualified_name = E.value->fully_qualified_name;
		weak_subclasses.push_back(subclass);
	}

	// clear subclasses to allow unused subclasses to be deleted
	for (KeyValue<StringName, Ref<GDScript>> &E : subclasses) {
		p_clear_data->scripts.insert(E.value);
	}
	subclasses.clear();
	// subclasses are also held by constants, clear those as well
	for (KeyValue<StringName, Variant> &E : constants) {
		GDScript *gdscr = _get_gdscript_from_variant(E.value);
		if (gdscr != nullptr) {
			p_clear_data->scripts.insert(gdscr);
		}
	}
	constants.clear();

	// keep orphan subclass only for subclasses that are still in use
	for (int i = 0; i < weak_subclasses.size(); i++) {
		ClassRefWithName subclass = weak_subclasses[i];
		Object *obj = ObjectDB::get_instance(subclass.id);
		if (!obj) {
			continue;
		}
		// subclass is not released
		GDScriptLanguage::get_singleton()->add_orphan_subclass(subclass.fully_qualified_name, subclass.id);
	}
}

#ifdef DEBUG_ENABLED
String GDScript::debug_get_script_name(const Ref<Script> &p_script) {
	if (p_script.is_valid()) {
		Ref<GDScript> gdscript = p_script;
		if (gdscript.is_valid()) {
			if (gdscript->get_local_name() != StringName()) {
				return gdscript->get_local_name();
			}
			return gdscript->get_fully_qualified_name().get_file();
		}

		if (p_script->get_global_name() != StringName()) {
			return p_script->get_global_name();
		} else if (!p_script->get_path().is_empty()) {
			return p_script->get_path().get_file();
		} else if (!p_script->get_name().is_empty()) {
			return p_script->get_name(); // Resource name.
		}
	}

	return "<unknown script>";
}
#endif

String GDScript::canonicalize_path(const String &p_path) {
	if (p_path.get_extension() == "gdc") {
		return p_path.get_basename() + ".gd";
	}
	return p_path;
}

GDScript::UpdatableFuncPtr::UpdatableFuncPtr(GDScriptFunction *p_function) {
	if (p_function == nullptr) {
		return;
	}

	ptr = p_function;
	script = ptr->get_script();
	ERR_FAIL_NULL(script);

	MutexLock script_lock(script->func_ptrs_to_update_mutex);
	list_element = script->func_ptrs_to_update.push_back(this);
}

GDScript::UpdatableFuncPtr::~UpdatableFuncPtr() {
	ERR_FAIL_NULL(script);

	if (list_element) {
		MutexLock script_lock(script->func_ptrs_to_update_mutex);
		list_element->erase();
		list_element = nullptr;
	}
}

void GDScript::_recurse_replace_function_ptrs(const HashMap<GDScriptFunction *, GDScriptFunction *> &p_replacements) const {
	MutexLock lock(func_ptrs_to_update_mutex);
	for (UpdatableFuncPtr *updatable : func_ptrs_to_update) {
		HashMap<GDScriptFunction *, GDScriptFunction *>::ConstIterator replacement = p_replacements.find(updatable->ptr);
		if (replacement) {
			updatable->ptr = replacement->value;
		} else {
			// Probably a lambda from another reload, ignore.
			updatable->ptr = nullptr;
		}
	}

	for (HashMap<StringName, Ref<GDScript>>::ConstIterator subscript = subclasses.begin(); subscript; ++subscript) {
		subscript->value->_recurse_replace_function_ptrs(p_replacements);
	}
}

void GDScript::clear(ClearData *p_clear_data) {
	if (clearing) {
		return;
	}
	clearing = true;

	ClearData data;
	ClearData *clear_data = p_clear_data;
	bool is_root = false;

	// If `clear_data` is `nullptr`, it means that it's the root.
	// The root is in charge to clear functions and scripts of itself and its dependencies
	if (clear_data == nullptr) {
		clear_data = &data;
		is_root = true;
	}

	{
		MutexLock lock(func_ptrs_to_update_mutex);
		for (UpdatableFuncPtr *updatable : func_ptrs_to_update) {
			updatable->ptr = nullptr;
		}
	}

	// If we're in the process of shutting things down then every single script will be cleared
	// anyway, so we can safely skip this very costly operation.
	if (!GDScriptLanguage::singleton->finishing) {
		RBSet<GDScript *> must_clear_dependencies = get_must_clear_dependencies();
		for (GDScript *E : must_clear_dependencies) {
			clear_data->scripts.insert(E);
			E->clear(clear_data);
		}
	}

	for (const KeyValue<StringName, GDScriptFunction *> &E : member_functions) {
		clear_data->functions.insert(E.value);
	}
	member_functions.clear();

	for (KeyValue<StringName, MemberInfo> &E : member_indices) {
		clear_data->scripts.insert(E.value.data_type.script_type_ref);
		E.value.data_type.script_type_ref = Ref<Script>();
	}

	for (KeyValue<StringName, MemberInfo> &E : static_variables_indices) {
		clear_data->scripts.insert(E.value.data_type.script_type_ref);
		E.value.data_type.script_type_ref = Ref<Script>();
	}
	static_variables.clear();
	static_variables_indices.clear();

	if (implicit_initializer) {
		clear_data->functions.insert(implicit_initializer);
		implicit_initializer = nullptr;
	}

	if (implicit_ready) {
		clear_data->functions.insert(implicit_ready);
		implicit_ready = nullptr;
	}

	if (static_initializer) {
		clear_data->functions.insert(static_initializer);
		static_initializer = nullptr;
	}

	_save_orphaned_subclasses(clear_data);

#ifdef TOOLS_ENABLED
	// Clearing inner class doc, script doc only cleared when the script source deleted.
	if (_owner) {
		_clear_doc();
	}
#endif

	// If it's not the root, skip clearing the data
	if (is_root) {
		// All dependencies have been accounted for
		for (GDScriptFunction *E : clear_data->functions) {
			memdelete(E);
		}
		for (Ref<Script> &E : clear_data->scripts) {
			Ref<GDScript> gdscr = E;
			if (gdscr.is_valid()) {
				GDScriptCache::remove_script(gdscr->get_path());
			}
		}
		clear_data->clear();
	}
}

GDScript::~GDScript() {
	if (destructing) {
		return;
	}
	destructing = true;

	if (is_print_verbose_enabled()) {
		MutexLock lock(func_ptrs_to_update_mutex);
		if (!func_ptrs_to_update.is_empty()) {
			print_line(vformat("GDScript: %d orphaned lambdas becoming invalid at destruction of script '%s'.", func_ptrs_to_update.size(), fully_qualified_name));
		}
	}

	clear();

	{
		MutexLock lock(GDScriptLanguage::get_singleton()->mutex);

		while (SelfList<GDScriptFunctionState> *E = pending_func_states.first()) {
			// Order matters since clearing the stack may already cause
			// the GDScriptFunctionState to be destroyed and thus removed from the list.
			pending_func_states.remove(E);
			GDScriptFunctionState *state = E->self();
			ObjectID state_id = state->get_instance_id();
			state->_clear_connections();
			if (ObjectDB::get_instance(state_id)) {
				state->_clear_stack();
			}
		}
	}

	{
		MutexLock lock(GDScriptLanguage::get_singleton()->mutex);

		script_list.remove_from_list();
	}
}

//////////////////////////////
//         INSTANCE         //
//////////////////////////////

bool GDScriptInstance::set(const StringName &p_name, const Variant &p_value) {
	{
		HashMap<StringName, GDScript::MemberInfo>::Iterator E = script->member_indices.find(p_name);
		if (E) {
			const GDScript::MemberInfo *member = &E->value;
			Variant value = p_value;
			if (member->data_type.has_type && !member->data_type.is_type(value)) {
				const Variant *args = &p_value;
				Callable::CallError err;
				Variant::construct(member->data_type.builtin_type, value, &args, 1, err);
				if (err.error != Callable::CallError::CALL_OK || !member->data_type.is_type(value)) {
					return false;
				}
			}
			if (likely(script->valid) && member->setter) {
				const Variant *args = &value;
				Callable::CallError err;
				callp(member->setter, &args, 1, err);
				return err.error == Callable::CallError::CALL_OK;
			} else {
				members.write[member->index] = value;
				return true;
			}
		}
	}

	GDScript *sptr = script.ptr();
	while (sptr) {
		{
			HashMap<StringName, GDScript::MemberInfo>::ConstIterator E = sptr->static_variables_indices.find(p_name);
			if (E) {
				const GDScript::MemberInfo *member = &E->value;
				Variant value = p_value;
				if (member->data_type.has_type && !member->data_type.is_type(value)) {
					const Variant *args = &p_value;
					Callable::CallError err;
					Variant::construct(member->data_type.builtin_type, value, &args, 1, err);
					if (err.error != Callable::CallError::CALL_OK || !member->data_type.is_type(value)) {
						return false;
					}
				}
				if (likely(sptr->valid) && member->setter) {
					const Variant *args = &value;
					Callable::CallError err;
					callp(member->setter, &args, 1, err);
					return err.error == Callable::CallError::CALL_OK;
				} else {
					sptr->static_variables.write[member->index] = value;
					return true;
				}
			}
		}

		if (likely(sptr->valid)) {
			HashMap<StringName, GDScriptFunction *>::Iterator E = sptr->member_functions.find(GDScriptLanguage::get_singleton()->strings._set);
			if (E) {
				Variant name = p_name;
				const Variant *args[2] = { &name, &p_value };

				Callable::CallError err;
				Variant ret = E->value->call(this, (const Variant **)args, 2, err);
				if (err.error == Callable::CallError::CALL_OK && ret.get_type() == Variant::BOOL && ret.operator bool()) {
					return true;
				}
			}
		}

		sptr = sptr->_base;
	}

	return false;
}

bool GDScriptInstance::get(const StringName &p_name, Variant &r_ret) const {
	{
		HashMap<StringName, GDScript::MemberInfo>::ConstIterator E = script->member_indices.find(p_name);
		if (E) {
			if (likely(script->valid) && E->value.getter) {
				Callable::CallError err;
				const Variant ret = const_cast<GDScriptInstance *>(this)->callp(E->value.getter, nullptr, 0, err);
				r_ret = (err.error == Callable::CallError::CALL_OK) ? ret : Variant();
				return true;
			}
			r_ret = members[E->value.index];
			return true;
		}
	}

	const GDScript *sptr = script.ptr();
	while (sptr) {
		{
			HashMap<StringName, Variant>::ConstIterator E = sptr->constants.find(p_name);
			if (E) {
				r_ret = E->value;
				return true;
			}
		}

		{
			HashMap<StringName, GDScript::MemberInfo>::ConstIterator E = sptr->static_variables_indices.find(p_name);
			if (E) {
				if (likely(sptr->valid) && E->value.getter) {
					Callable::CallError ce;
					const Variant ret = const_cast<GDScript *>(sptr)->callp(E->value.getter, nullptr, 0, ce);
					r_ret = (ce.error == Callable::CallError::CALL_OK) ? ret : Variant();
					return true;
				}
				r_ret = sptr->static_variables[E->value.index];
				return true;
			}
		}

		{
			HashMap<StringName, MethodInfo>::ConstIterator E = sptr->_signals.find(p_name);
			if (E) {
				r_ret = Signal(owner, E->key);
				return true;
			}
		}

		if (likely(sptr->valid)) {
			HashMap<StringName, GDScriptFunction *>::ConstIterator E = sptr->member_functions.find(p_name);
			if (E) {
				if (sptr->rpc_config.has(p_name)) {
					r_ret = Callable(memnew(GDScriptRPCCallable(owner, E->key)));
				} else {
					r_ret = Callable(owner, E->key);
				}
				return true;
			}
		}

		{
			HashMap<StringName, Ref<GDScript>>::ConstIterator E = sptr->subclasses.find(p_name);
			if (E) {
				r_ret = E->value;
				return true;
			}
		}

		if (likely(sptr->valid)) {
			HashMap<StringName, GDScriptFunction *>::ConstIterator E = sptr->member_functions.find(GDScriptLanguage::get_singleton()->strings._get);
			if (E) {
				Variant name = p_name;
				const Variant *args[1] = { &name };

				Callable::CallError err;
				Variant ret = E->value->call(const_cast<GDScriptInstance *>(this), (const Variant **)args, 1, err);
				if (err.error == Callable::CallError::CALL_OK && ret.get_type() != Variant::NIL) {
					r_ret = ret;
					return true;
				}
			}
		}
		sptr = sptr->_base;
	}

	return false;
}

Variant::Type GDScriptInstance::get_property_type(const StringName &p_name, bool *r_is_valid) const {
	if (script->member_indices.has(p_name)) {
		if (r_is_valid) {
			*r_is_valid = true;
		}
		return script->member_indices[p_name].property_info.type;
	}

	if (r_is_valid) {
		*r_is_valid = false;
	}
	return Variant::NIL;
}

void GDScriptInstance::validate_property(PropertyInfo &p_property) const {
	const GDScript *sptr = script.ptr();
	while (sptr) {
		if (likely(sptr->valid)) {
			HashMap<StringName, GDScriptFunction *>::ConstIterator E = sptr->member_functions.find(GDScriptLanguage::get_singleton()->strings._validate_property);
			if (E) {
				Variant property = (Dictionary)p_property;
				const Variant *args[1] = { &property };

				Callable::CallError err;
				Variant ret = E->value->call(const_cast<GDScriptInstance *>(this), args, 1, err);
				if (err.error == Callable::CallError::CALL_OK) {
					p_property = PropertyInfo::from_dict(property);
					return;
				}
			}
		}
		sptr = sptr->_base;
	}
}

void GDScriptInstance::get_property_list(List<PropertyInfo> *p_properties) const {
	// exported members, not done yet!

	const GDScript *sptr = script.ptr();
	List<PropertyInfo> props;

	while (sptr) {
		if (likely(sptr->valid)) {
			HashMap<StringName, GDScriptFunction *>::ConstIterator E = sptr->member_functions.find(GDScriptLanguage::get_singleton()->strings._get_property_list);
			if (E) {
				Callable::CallError err;
				Variant ret = E->value->call(const_cast<GDScriptInstance *>(this), nullptr, 0, err);
				if (err.error == Callable::CallError::CALL_OK) {
					ERR_FAIL_COND_MSG(ret.get_type() != Variant::ARRAY, "Wrong type for _get_property_list, must be an array of dictionaries.");

					Array arr = ret;
					for (int i = 0; i < arr.size(); i++) {
						Dictionary d = arr[i];
						ERR_CONTINUE(!d.has("name"));
						ERR_CONTINUE(!d.has("type"));

						PropertyInfo pinfo;
						pinfo.name = d["name"];
						pinfo.type = Variant::Type(d["type"].operator int());
						if (d.has("hint")) {
							pinfo.hint = PropertyHint(d["hint"].operator int());
						}
						if (d.has("hint_string")) {
							pinfo.hint_string = d["hint_string"];
						}
						if (d.has("usage")) {
							pinfo.usage = d["usage"];
						}
						if (d.has("class_name")) {
							pinfo.class_name = d["class_name"];
						}

						ERR_CONTINUE(pinfo.name.is_empty() && (pinfo.usage & PROPERTY_USAGE_STORAGE));
						ERR_CONTINUE(pinfo.type < 0 || pinfo.type >= Variant::VARIANT_MAX);

						props.push_back(pinfo);
					}
				}
			}
		}

		//instance a fake script for editing the values

		Vector<_GDScriptMemberSort> msort;
		for (const KeyValue<StringName, GDScript::MemberInfo> &F : sptr->member_indices) {
			if (!sptr->members.has(F.key)) {
				continue; // Skip base class members.
			}
			_GDScriptMemberSort ms;
			ms.index = F.value.index;
			ms.name = F.key;
			msort.push_back(ms);
		}

		msort.sort();
		msort.reverse();
		for (int i = 0; i < msort.size(); i++) {
			props.push_front(sptr->member_indices[msort[i].name].property_info);
		}

#ifdef TOOLS_ENABLED
		p_properties->push_back(sptr->get_class_category());
#endif // TOOLS_ENABLED

		for (PropertyInfo &prop : props) {
			validate_property(prop);
			p_properties->push_back(prop);
		}

		props.clear();

		sptr = sptr->_base;
	}
}

bool GDScriptInstance::property_can_revert(const StringName &p_name) const {
	Variant name = p_name;
	const Variant *args[1] = { &name };

	const GDScript *sptr = script.ptr();
	while (sptr) {
		if (likely(sptr->valid)) {
			HashMap<StringName, GDScriptFunction *>::ConstIterator E = sptr->member_functions.find(GDScriptLanguage::get_singleton()->strings._property_can_revert);
			if (E) {
				Callable::CallError err;
				Variant ret = E->value->call(const_cast<GDScriptInstance *>(this), args, 1, err);
				if (err.error == Callable::CallError::CALL_OK && ret.get_type() == Variant::BOOL && ret.operator bool()) {
					return true;
				}
			}
		}
		sptr = sptr->_base;
	}

	return false;
}

bool GDScriptInstance::property_get_revert(const StringName &p_name, Variant &r_ret) const {
	Variant name = p_name;
	const Variant *args[1] = { &name };

	const GDScript *sptr = script.ptr();
	while (sptr) {
		if (likely(sptr->valid)) {
			HashMap<StringName, GDScriptFunction *>::ConstIterator E = sptr->member_functions.find(GDScriptLanguage::get_singleton()->strings._property_get_revert);
			if (E) {
				Callable::CallError err;
				Variant ret = E->value->call(const_cast<GDScriptInstance *>(this), args, 1, err);
				if (err.error == Callable::CallError::CALL_OK && ret.get_type() != Variant::NIL) {
					r_ret = ret;
					return true;
				}
			}
		}
		sptr = sptr->_base;
	}

	return false;
}

void GDScriptInstance::get_method_list(List<MethodInfo> *p_list) const {
	const GDScript *sptr = script.ptr();
	while (sptr) {
		for (const KeyValue<StringName, GDScriptFunction *> &E : sptr->member_functions) {
			p_list->push_back(E.value->get_method_info());
		}
		sptr = sptr->_base;
	}
}

bool GDScriptInstance::has_method(const StringName &p_method) const {
	const GDScript *sptr = script.ptr();
	while (sptr) {
		HashMap<StringName, GDScriptFunction *>::ConstIterator E = sptr->member_functions.find(p_method);
		if (E) {
			return true;
		}
		sptr = sptr->_base;
	}

	return false;
}

int GDScriptInstance::get_method_argument_count(const StringName &p_method, bool *r_is_valid) const {
	const GDScript *sptr = script.ptr();
	while (sptr) {
		HashMap<StringName, GDScriptFunction *>::ConstIterator E = sptr->member_functions.find(p_method);
		if (E) {
			if (r_is_valid) {
				*r_is_valid = true;
			}
			return E->value->get_argument_count();
		}
		sptr = sptr->_base;
	}

	if (r_is_valid) {
		*r_is_valid = false;
	}
	return 0;
}

void GDScriptInstance::_call_implicit_ready_recursively(GDScript *p_script) {
	// Call base class first.
	if (p_script->_base) {
		_call_implicit_ready_recursively(p_script->_base);
	}
	if (likely(p_script->valid) && p_script->implicit_ready) {
		Callable::CallError err;
		p_script->implicit_ready->call(this, nullptr, 0, err);
	}
}

Variant GDScriptInstance::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	GDScript *sptr = script.ptr();
	if (unlikely(p_method == SceneStringName(_ready))) {
		// Call implicit ready first, including for the super classes recursively.
		_call_implicit_ready_recursively(sptr);
	}
	while (sptr) {
		if (likely(sptr->valid)) {
			HashMap<StringName, GDScriptFunction *>::Iterator E = sptr->member_functions.find(p_method);
			if (E) {
				return E->value->call(this, p_args, p_argcount, r_error);
			}
		}
		sptr = sptr->_base;
	}

	r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
	return Variant();
}

void GDScriptInstance::notification(int p_notification, bool p_reversed) {
	if (unlikely(!script->valid)) {
		return;
	}

	//notification is not virtual, it gets called at ALL levels just like in C.
	Variant value = p_notification;
	const Variant *args[1] = { &value };

	List<GDScript *> pl;
	GDScript *sptr = script.ptr();
	while (sptr) {
		if (p_reversed) {
			pl.push_back(sptr);
		} else {
			pl.push_front(sptr);
		}
		sptr = sptr->_base;
	}
	for (GDScript *sc : pl) {
		if (likely(sc->valid)) {
			HashMap<StringName, GDScriptFunction *>::Iterator E = sc->member_functions.find(GDScriptLanguage::get_singleton()->strings._notification);
			if (E) {
				Callable::CallError err;
				E->value->call(this, args, 1, err);
				if (err.error != Callable::CallError::CALL_OK) {
					//print error about notification call
				}
			}
		}
	}
}

String GDScriptInstance::to_string(bool *r_valid) {
	if (has_method(CoreStringName(_to_string))) {
		Callable::CallError ce;
		Variant ret = callp(CoreStringName(_to_string), nullptr, 0, ce);
		if (ce.error == Callable::CallError::CALL_OK) {
			if (ret.get_type() != Variant::STRING) {
				if (r_valid) {
					*r_valid = false;
				}
				ERR_FAIL_V_MSG(String(), "Wrong type for " + CoreStringName(_to_string) + ", must be a String.");
			}
			if (r_valid) {
				*r_valid = true;
			}
			return ret.operator String();
		}
	}
	if (r_valid) {
		*r_valid = false;
	}
	return String();
}

Ref<Script> GDScriptInstance::get_script() const {
	return script;
}

ScriptLanguage *GDScriptInstance::get_language() {
	return GDScriptLanguage::get_singleton();
}

const Variant GDScriptInstance::get_rpc_config() const {
	return script->get_rpc_config();
}

void GDScriptInstance::reload_members() {
#ifdef DEBUG_ENABLED

	Vector<Variant> new_members;
	new_members.resize(script->member_indices.size());

	//pass the values to the new indices
	for (KeyValue<StringName, GDScript::MemberInfo> &E : script->member_indices) {
		if (member_indices_cache.has(E.key)) {
			Variant value = members[member_indices_cache[E.key]];
			new_members.write[E.value.index] = value;
		}
	}

	members.resize(new_members.size()); //resize

	//apply
	members = new_members;

	//pass the values to the new indices
	member_indices_cache.clear();
	for (const KeyValue<StringName, GDScript::MemberInfo> &E : script->member_indices) {
		member_indices_cache[E.key] = E.value.index;
	}

#endif
}

GDScriptInstance::GDScriptInstance() {
	owner = nullptr;
	base_ref_counted = false;
}

GDScriptInstance::~GDScriptInstance() {
	MutexLock lock(GDScriptLanguage::get_singleton()->mutex);

	while (SelfList<GDScriptFunctionState> *E = pending_func_states.first()) {
		// Order matters since clearing the stack may already cause
		// the GDSCriptFunctionState to be destroyed and thus removed from the list.
		pending_func_states.remove(E);
		GDScriptFunctionState *state = E->self();
		ObjectID state_id = state->get_instance_id();
		state->_clear_connections();
		if (ObjectDB::get_instance(state_id)) {
			state->_clear_stack();
		}
	}

	if (script.is_valid() && owner) {
		script->instances.erase(owner);
	}
}

/************* SCRIPT LANGUAGE **************/

GDScriptLanguage *GDScriptLanguage::singleton = nullptr;

String GDScriptLanguage::get_name() const {
	return "GDScript";
}

/* LANGUAGE FUNCTIONS */

void GDScriptLanguage::_add_global(const StringName &p_name, const Variant &p_value) {
	if (globals.has(p_name)) {
		//overwrite existing
		global_array.write[globals[p_name]] = p_value;
		return;
	}

	if (global_array_empty_indexes.size()) {
		int index = global_array_empty_indexes[global_array_empty_indexes.size() - 1];
		globals[p_name] = index;
		global_array.write[index] = p_value;
		global_array_empty_indexes.resize(global_array_empty_indexes.size() - 1);
	} else {
		globals[p_name] = global_array.size();
		global_array.push_back(p_value);
		_global_array = global_array.ptrw();
	}
}

void GDScriptLanguage::_remove_global(const StringName &p_name) {
	if (!globals.has(p_name)) {
		return;
	}
	global_array_empty_indexes.push_back(globals[p_name]);
	global_array.write[globals[p_name]] = Variant::NIL;
	globals.erase(p_name);
}

void GDScriptLanguage::add_global_constant(const StringName &p_variable, const Variant &p_value) {
	_add_global(p_variable, p_value);
}

void GDScriptLanguage::add_named_global_constant(const StringName &p_name, const Variant &p_value) {
	named_globals[p_name] = p_value;
}

Variant GDScriptLanguage::get_any_global_constant(const StringName &p_name) {
	if (named_globals.has(p_name)) {
		return named_globals[p_name];
	}
	if (globals.has(p_name)) {
		return _global_array[globals[p_name]];
	}
	ERR_FAIL_V_MSG(Variant(), vformat("Could not find any global constant with name: %s.", p_name));
}

void GDScriptLanguage::remove_named_global_constant(const StringName &p_name) {
	ERR_FAIL_COND(!named_globals.has(p_name));
	named_globals.erase(p_name);
}

void GDScriptLanguage::init() {
	//populate global constants
	int gcc = CoreConstants::get_global_constant_count();
	for (int i = 0; i < gcc; i++) {
		_add_global(StaticCString::create(CoreConstants::get_global_constant_name(i)), CoreConstants::get_global_constant_value(i));
	}

	_add_global(StaticCString::create("PI"), Math_PI);
	_add_global(StaticCString::create("TAU"), Math_TAU);
	_add_global(StaticCString::create("INF"), INFINITY);
	_add_global(StaticCString::create("NAN"), NAN);

	//populate native classes

	List<StringName> class_list;
	ClassDB::get_class_list(&class_list);
	for (const StringName &n : class_list) {
		if (globals.has(n)) {
			continue;
		}
		Ref<GDScriptNativeClass> nc = memnew(GDScriptNativeClass(n));
		_add_global(n, nc);
	}

	//populate singletons

	List<Engine::Singleton> singletons;
	Engine::get_singleton()->get_singletons(&singletons);
	for (const Engine::Singleton &E : singletons) {
		_add_global(E.name, E.ptr);
	}

#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_editor_hint()) {
		GDExtensionManager::get_singleton()->connect("extension_loaded", callable_mp(this, &GDScriptLanguage::_extension_loaded));
		GDExtensionManager::get_singleton()->connect("extension_unloading", callable_mp(this, &GDScriptLanguage::_extension_unloading));
	}
#endif

#ifdef TESTS_ENABLED
	GDScriptTests::GDScriptTestRunner::handle_cmdline();
#endif
}

#ifdef TOOLS_ENABLED
void GDScriptLanguage::_extension_loaded(const Ref<GDExtension> &p_extension) {
	List<StringName> class_list;
	ClassDB::get_extension_class_list(p_extension, &class_list);
	for (const StringName &n : class_list) {
		if (globals.has(n)) {
			continue;
		}
		Ref<GDScriptNativeClass> nc = memnew(GDScriptNativeClass(n));
		_add_global(n, nc);
	}
}

void GDScriptLanguage::_extension_unloading(const Ref<GDExtension> &p_extension) {
	List<StringName> class_list;
	ClassDB::get_extension_class_list(p_extension, &class_list);
	for (const StringName &n : class_list) {
		_remove_global(n);
	}
}
#endif

String GDScriptLanguage::get_type() const {
	return "GDScript";
}

String GDScriptLanguage::get_extension() const {
	return "gd";
}

void GDScriptLanguage::finish() {
	if (finishing) {
		return;
	}
	finishing = true;

	_call_stack.free();

	// Clear the cache before parsing the script_list
	GDScriptCache::clear();

	// Clear dependencies between scripts, to ensure cyclic references are broken
	// (to avoid leaks at exit).
	SelfList<GDScript> *s = script_list.first();
	while (s) {
		// This ensures the current script is not released before we can check
		// what's the next one in the list (we can't get the next upfront because we
		// don't know if the reference breaking will cause it -or any other after
		// it, for that matter- to be released so the next one is not the same as
		// before).
		Ref<GDScript> scr = s->self();
		if (scr.is_valid()) {
			for (KeyValue<StringName, GDScriptFunction *> &E : scr->member_functions) {
				GDScriptFunction *func = E.value;
				for (int i = 0; i < func->argument_types.size(); i++) {
					func->argument_types.write[i].script_type_ref = Ref<Script>();
				}
				func->return_type.script_type_ref = Ref<Script>();
			}
			for (KeyValue<StringName, GDScript::MemberInfo> &E : scr->member_indices) {
				E.value.data_type.script_type_ref = Ref<Script>();
			}

			// Clear backup for scripts that could slip out of the cyclic reference
			// check
			scr->clear();
		}
		s = s->next();
	}
	script_list.clear();
	function_list.clear();

	finishing = false;
}

void GDScriptLanguage::profiling_start() {
#ifdef DEBUG_ENABLED
	MutexLock lock(mutex);

	SelfList<GDScriptFunction> *elem = function_list.first();
	while (elem) {
		elem->self()->profile.call_count.set(0);
		elem->self()->profile.self_time.set(0);
		elem->self()->profile.total_time.set(0);
		elem->self()->profile.frame_call_count.set(0);
		elem->self()->profile.frame_self_time.set(0);
		elem->self()->profile.frame_total_time.set(0);
		elem->self()->profile.last_frame_call_count = 0;
		elem->self()->profile.last_frame_self_time = 0;
		elem->self()->profile.last_frame_total_time = 0;
		elem->self()->profile.native_calls.clear();
		elem->self()->profile.last_native_calls.clear();
		elem = elem->next();
	}

	profiling = true;
#endif
}

void GDScriptLanguage::profiling_set_save_native_calls(bool p_enable) {
#ifdef DEBUG_ENABLED
	MutexLock lock(mutex);
	profile_native_calls = p_enable;
#endif
}

void GDScriptLanguage::profiling_stop() {
#ifdef DEBUG_ENABLED
	MutexLock lock(mutex);

	profiling = false;
#endif
}

int GDScriptLanguage::profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) {
	int current = 0;
#ifdef DEBUG_ENABLED

	MutexLock lock(mutex);

	profiling_collate_native_call_data(true);
	SelfList<GDScriptFunction> *elem = function_list.first();
	while (elem) {
		if (current >= p_info_max) {
			break;
		}
		int last_non_internal = current;
		p_info_arr[current].call_count = elem->self()->profile.call_count.get();
		p_info_arr[current].self_time = elem->self()->profile.self_time.get();
		p_info_arr[current].total_time = elem->self()->profile.total_time.get();
		p_info_arr[current].signature = elem->self()->profile.signature;
		current++;

		int nat_time = 0;
		HashMap<String, GDScriptFunction::Profile::NativeProfile>::ConstIterator nat_calls = elem->self()->profile.native_calls.begin();
		while (nat_calls) {
			p_info_arr[current].call_count = nat_calls->value.call_count;
			p_info_arr[current].total_time = nat_calls->value.total_time;
			p_info_arr[current].self_time = nat_calls->value.total_time;
			p_info_arr[current].signature = nat_calls->value.signature;
			nat_time += nat_calls->value.total_time;
			current++;
			++nat_calls;
		}
		p_info_arr[last_non_internal].internal_time = nat_time;
		elem = elem->next();
	}
#endif

	return current;
}

int GDScriptLanguage::profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) {
	int current = 0;

#ifdef DEBUG_ENABLED
	MutexLock lock(mutex);

	profiling_collate_native_call_data(false);
	SelfList<GDScriptFunction> *elem = function_list.first();
	while (elem) {
		if (current >= p_info_max) {
			break;
		}
		if (elem->self()->profile.last_frame_call_count > 0) {
			int last_non_internal = current;
			p_info_arr[current].call_count = elem->self()->profile.last_frame_call_count;
			p_info_arr[current].self_time = elem->self()->profile.last_frame_self_time;
			p_info_arr[current].total_time = elem->self()->profile.last_frame_total_time;
			p_info_arr[current].signature = elem->self()->profile.signature;
			current++;

			int nat_time = 0;
			HashMap<String, GDScriptFunction::Profile::NativeProfile>::ConstIterator nat_calls = elem->self()->profile.last_native_calls.begin();
			while (nat_calls) {
				p_info_arr[current].call_count = nat_calls->value.call_count;
				p_info_arr[current].total_time = nat_calls->value.total_time;
				p_info_arr[current].self_time = nat_calls->value.total_time;
				p_info_arr[current].internal_time = nat_calls->value.total_time;
				p_info_arr[current].signature = nat_calls->value.signature;
				nat_time += nat_calls->value.total_time;
				current++;
				++nat_calls;
			}
			p_info_arr[last_non_internal].internal_time = nat_time;
		}
		elem = elem->next();
	}
#endif

	return current;
}

void GDScriptLanguage::profiling_collate_native_call_data(bool p_accumulated) {
#ifdef DEBUG_ENABLED
	// The same native call can be called from multiple functions, so join them together here.
	// Only use the name of the function (ie signature.split[2]).
	HashMap<String, GDScriptFunction::Profile::NativeProfile *> seen_nat_calls;
	SelfList<GDScriptFunction> *elem = function_list.first();
	while (elem) {
		HashMap<String, GDScriptFunction::Profile::NativeProfile> *nat_calls = p_accumulated ? &elem->self()->profile.native_calls : &elem->self()->profile.last_native_calls;
		HashMap<String, GDScriptFunction::Profile::NativeProfile>::Iterator it = nat_calls->begin();

		while (it != nat_calls->end()) {
			Vector<String> sig = it->value.signature.split("::");
			HashMap<String, GDScriptFunction::Profile::NativeProfile *>::ConstIterator already_found = seen_nat_calls.find(sig[2]);
			if (already_found) {
				already_found->value->total_time += it->value.total_time;
				already_found->value->call_count += it->value.call_count;
				elem->self()->profile.last_native_calls.remove(it);
			} else {
				seen_nat_calls.insert(sig[2], &it->value);
			}
			++it;
		}
		elem = elem->next();
	}
#endif
}

struct GDScriptDepSort {
	//must support sorting so inheritance works properly (parent must be reloaded first)
	bool operator()(const Ref<GDScript> &A, const Ref<GDScript> &B) const {
		if (A == B) {
			return false; //shouldn't happen but..
		}
		const GDScript *I = B->get_base().ptr();
		while (I) {
			if (I == A.ptr()) {
				// A is a base of B
				return true;
			}

			I = I->get_base().ptr();
		}

		return false; //not a base
	}
};

void GDScriptLanguage::reload_all_scripts() {
#ifdef DEBUG_ENABLED
	print_verbose("GDScript: Reloading all scripts");
	Array scripts;
	{
		MutexLock lock(mutex);

		SelfList<GDScript> *elem = script_list.first();
		while (elem) {
			if (elem->self()->get_path().is_resource_file()) {
				print_verbose("GDScript: Found: " + elem->self()->get_path());
				scripts.push_back(Ref<GDScript>(elem->self())); //cast to gdscript to avoid being erased by accident
			}
			elem = elem->next();
		}

#ifdef TOOLS_ENABLED
		if (Engine::get_singleton()->is_editor_hint()) {
			// Reload all pointers to existing singletons so that tool scripts can work with the reloaded extensions.
			List<Engine::Singleton> singletons;
			Engine::get_singleton()->get_singletons(&singletons);
			for (const Engine::Singleton &E : singletons) {
				if (globals.has(E.name)) {
					_add_global(E.name, E.ptr);
				}
			}
		}
#endif // TOOLS_ENABLED
	}

	reload_scripts(scripts, true);
#endif // DEBUG_ENABLED
}

void GDScriptLanguage::reload_scripts(const Array &p_scripts, bool p_soft_reload) {
#ifdef DEBUG_ENABLED

	List<Ref<GDScript>> scripts;
	{
		MutexLock lock(mutex);

		SelfList<GDScript> *elem = script_list.first();
		while (elem) {
			// Scripts will reload all subclasses, so only reload root scripts.
			if (elem->self()->is_root_script() && !elem->self()->get_path().is_empty()) {
				scripts.push_back(Ref<GDScript>(elem->self())); //cast to gdscript to avoid being erased by accident
			}
			elem = elem->next();
		}
	}

	//when someone asks you why dynamically typed languages are easier to write....

	HashMap<Ref<GDScript>, HashMap<ObjectID, List<Pair<StringName, Variant>>>> to_reload;

	//as scripts are going to be reloaded, must proceed without locking here

	scripts.sort_custom<GDScriptDepSort>(); //update in inheritance dependency order

	for (Ref<GDScript> &scr : scripts) {
		bool reload = p_scripts.has(scr) || to_reload.has(scr->get_base());

		if (!reload) {
			continue;
		}

		to_reload.insert(scr, HashMap<ObjectID, List<Pair<StringName, Variant>>>());

		if (!p_soft_reload) {
			//save state and remove script from instances
			HashMap<ObjectID, List<Pair<StringName, Variant>>> &map = to_reload[scr];

			while (scr->instances.front()) {
				Object *obj = scr->instances.front()->get();
				//save instance info
				List<Pair<StringName, Variant>> state;
				if (obj->get_script_instance()) {
					obj->get_script_instance()->get_property_state(state);
					map[obj->get_instance_id()] = state;
					obj->set_script(Variant());
				}
			}

			//same thing for placeholders
#ifdef TOOLS_ENABLED

			while (scr->placeholders.size()) {
				Object *obj = (*scr->placeholders.begin())->get_owner();

				//save instance info
				if (obj->get_script_instance()) {
					map.insert(obj->get_instance_id(), List<Pair<StringName, Variant>>());
					List<Pair<StringName, Variant>> &state = map[obj->get_instance_id()];
					obj->get_script_instance()->get_property_state(state);
					obj->set_script(Variant());
				} else {
					// no instance found. Let's remove it so we don't loop forever
					scr->placeholders.erase(*scr->placeholders.begin());
				}
			}

#endif // TOOLS_ENABLED

			for (const KeyValue<ObjectID, List<Pair<StringName, Variant>>> &F : scr->pending_reload_state) {
				map[F.key] = F.value; //pending to reload, use this one instead
			}
		}
	}

	for (KeyValue<Ref<GDScript>, HashMap<ObjectID, List<Pair<StringName, Variant>>>> &E : to_reload) {
		Ref<GDScript> scr = E.key;
		print_verbose("GDScript: Reloading: " + scr->get_path());
		if (scr->is_built_in()) {
			// TODO: It would be nice to do it more efficiently than loading the whole scene again.
			Ref<PackedScene> scene = ResourceLoader::load(scr->get_path().get_slice("::", 0), "", ResourceFormatLoader::CACHE_MODE_IGNORE_DEEP);
			ERR_CONTINUE(scene.is_null());

			Ref<SceneState> state = scene->get_state();
			Ref<GDScript> fresh = state->get_sub_resource(scr->get_path());
			ERR_CONTINUE(fresh.is_null());

			scr->set_source_code(fresh->get_source_code());
		} else {
			scr->load_source_code(scr->get_path());
		}
		scr->reload(p_soft_reload);

		//restore state if saved
		for (KeyValue<ObjectID, List<Pair<StringName, Variant>>> &F : E.value) {
			List<Pair<StringName, Variant>> &saved_state = F.value;

			Object *obj = ObjectDB::get_instance(F.key);
			if (!obj) {
				continue;
			}

			if (!p_soft_reload) {
				//clear it just in case (may be a pending reload state)
				obj->set_script(Variant());
			}
			obj->set_script(scr);

			ScriptInstance *script_inst = obj->get_script_instance();

			if (!script_inst) {
				//failed, save reload state for next time if not saved
				if (!scr->pending_reload_state.has(obj->get_instance_id())) {
					scr->pending_reload_state[obj->get_instance_id()] = saved_state;
				}
				continue;
			}

			if (script_inst->is_placeholder() && scr->is_placeholder_fallback_enabled()) {
				PlaceHolderScriptInstance *placeholder = static_cast<PlaceHolderScriptInstance *>(script_inst);
				for (List<Pair<StringName, Variant>>::Element *G = saved_state.front(); G; G = G->next()) {
					placeholder->property_set_fallback(G->get().first, G->get().second);
				}
			} else {
				for (List<Pair<StringName, Variant>>::Element *G = saved_state.front(); G; G = G->next()) {
					script_inst->set(G->get().first, G->get().second);
				}
			}

			scr->pending_reload_state.erase(obj->get_instance_id()); //as it reloaded, remove pending state
		}

		//if instance states were saved, set them!
	}

#endif // DEBUG_ENABLED
}

void GDScriptLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {
	Array scripts;
	scripts.push_back(p_script);
	reload_scripts(scripts, p_soft_reload);
}

void GDScriptLanguage::frame() {
#ifdef DEBUG_ENABLED
	if (profiling) {
		MutexLock lock(mutex);

		SelfList<GDScriptFunction> *elem = function_list.first();
		while (elem) {
			elem->self()->profile.last_frame_call_count = elem->self()->profile.frame_call_count.get();
			elem->self()->profile.last_frame_self_time = elem->self()->profile.frame_self_time.get();
			elem->self()->profile.last_frame_total_time = elem->self()->profile.frame_total_time.get();
			elem->self()->profile.last_native_calls = elem->self()->profile.native_calls;
			elem->self()->profile.frame_call_count.set(0);
			elem->self()->profile.frame_self_time.set(0);
			elem->self()->profile.frame_total_time.set(0);
			elem->self()->profile.native_calls.clear();
			elem = elem->next();
		}
	}

#endif
}

/* EDITOR FUNCTIONS */
void GDScriptLanguage::get_reserved_words(List<String> *p_words) const {
	// Please keep alphabetical order within categories.
	static const char *_reserved_words[] = {
		// Control flow.
		"break",
		"continue",
		"elif",
		"else",
		"for",
		"if",
		"match",
		"pass",
		"return",
		"when",
		"while",
		// Declarations.
		"class",
		"class_name",
		"const",
		"enum",
		"extends",
		"func",
		"namespace", // Reserved for potential future use.
		"signal",
		"static",
		"trait", // Reserved for potential future use.
		"var",
		// Other keywords.
		"await",
		"breakpoint",
		"self",
		"super",
		"yield", // Reserved for potential future use.
		// Operators.
		"and",
		"as",
		"in",
		"is",
		"not",
		"or",
		// Special values (tokenizer treats them as literals, not as tokens).
		"false",
		"null",
		"true",
		// Constants.
		"INF",
		"NAN",
		"PI",
		"TAU",
		// Functions (highlighter uses global function color instead).
		"assert",
		"preload",
		// Types (highlighter uses type color instead).
		"void",
		nullptr,
	};

	const char **w = _reserved_words;

	while (*w) {
		p_words->push_back(*w);
		w++;
	}
}

bool GDScriptLanguage::is_control_flow_keyword(const String &p_keyword) const {
	// Please keep alphabetical order.
	return p_keyword == "break" ||
			p_keyword == "continue" ||
			p_keyword == "elif" ||
			p_keyword == "else" ||
			p_keyword == "for" ||
			p_keyword == "if" ||
			p_keyword == "match" ||
			p_keyword == "pass" ||
			p_keyword == "return" ||
			p_keyword == "when" ||
			p_keyword == "while";
}

bool GDScriptLanguage::handles_global_class_type(const String &p_type) const {
	return p_type == "GDScript";
}

String GDScriptLanguage::get_global_class_name(const String &p_path, String *r_base_type, String *r_icon_path) const {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (err) {
		return String();
	}

	String source = f->get_as_utf8_string();

	GDScriptParser parser;
	err = parser.parse(source, p_path, false, false);

	const GDScriptParser::ClassNode *c = parser.get_tree();
	if (!c) {
		return String(); // No class parsed.
	}

	/* **WARNING**
	 *
	 * This function is written with the goal to be *extremely* error tolerant, as such
	 * it should meet the following requirements:
	 *
	 * - It must not rely on the analyzer (in fact, the analyzer must not be used here),
	 *   because at the time global classes are parsed, the dependencies may not be present
	 *   yet, hence the function will fail (which is unintended).
	 * - It must not fail even if the parsing fails, because even if the file is broken,
	 *   it should attempt its best to retrieve the inheritance metadata.
	 *
	 * Before changing this function, please ask the current maintainer of EditorFileSystem.
	 */

	if (r_base_type) {
		const GDScriptParser::ClassNode *subclass = c;
		String path = p_path;
		GDScriptParser subparser;
		while (subclass) {
			if (subclass->extends_used) {
				if (!subclass->extends_path.is_empty()) {
					if (subclass->extends.size() == 0) {
						get_global_class_name(subclass->extends_path, r_base_type);
						subclass = nullptr;
						break;
					} else {
						Vector<GDScriptParser::IdentifierNode *> extend_classes = subclass->extends;

						Ref<FileAccess> subfile = FileAccess::open(subclass->extends_path, FileAccess::READ);
						if (subfile.is_null()) {
							break;
						}
						String subsource = subfile->get_as_utf8_string();

						if (subsource.is_empty()) {
							break;
						}
						String subpath = subclass->extends_path;
						if (subpath.is_relative_path()) {
							subpath = path.get_base_dir().path_join(subpath).simplify_path();
						}

						if (OK != subparser.parse(subsource, subpath, false)) {
							break;
						}
						path = subpath;
						subclass = subparser.get_tree();

						while (extend_classes.size() > 0) {
							bool found = false;
							for (int i = 0; i < subclass->members.size(); i++) {
								if (subclass->members[i].type != GDScriptParser::ClassNode::Member::CLASS) {
									continue;
								}

								const GDScriptParser::ClassNode *inner_class = subclass->members[i].m_class;
								if (inner_class->identifier->name == extend_classes[0]->name) {
									extend_classes.remove_at(0);
									found = true;
									subclass = inner_class;
									break;
								}
							}
							if (!found) {
								subclass = nullptr;
								break;
							}
						}
					}
				} else if (subclass->extends.size() == 1) {
					*r_base_type = subclass->extends[0]->name;
					subclass = nullptr;
				} else {
					break;
				}
			} else {
				*r_base_type = "RefCounted";
				subclass = nullptr;
			}
		}
	}
	if (r_icon_path) {
		*r_icon_path = c->simplified_icon_path;
	}
	return c->identifier != nullptr ? String(c->identifier->name) : String();
}

thread_local GDScriptLanguage::CallStack GDScriptLanguage::_call_stack;

GDScriptLanguage::GDScriptLanguage() {
	ERR_FAIL_COND(singleton);
	singleton = this;
	strings._init = StaticCString::create("_init");
	strings._static_init = StaticCString::create("_static_init");
	strings._notification = StaticCString::create("_notification");
	strings._set = StaticCString::create("_set");
	strings._get = StaticCString::create("_get");
	strings._get_property_list = StaticCString::create("_get_property_list");
	strings._validate_property = StaticCString::create("_validate_property");
	strings._property_can_revert = StaticCString::create("_property_can_revert");
	strings._property_get_revert = StaticCString::create("_property_get_revert");
	strings._script_source = StaticCString::create("script/source");
	_debug_parse_err_line = -1;
	_debug_parse_err_file = "";

#ifdef DEBUG_ENABLED
	profiling = false;
	profile_native_calls = false;
	script_frame_time = 0;
#endif

	int dmcs = GLOBAL_DEF(PropertyInfo(Variant::INT, "debug/settings/gdscript/max_call_stack", PROPERTY_HINT_RANGE, "512," + itos(GDScriptFunction::MAX_CALL_DEPTH - 1) + ",1"), 1024);

	if (EngineDebugger::is_active()) {
		//debugging enabled!

		_debug_max_call_stack = dmcs;
	} else {
		_debug_max_call_stack = 0;
	}

#ifdef DEBUG_ENABLED
	GLOBAL_DEF("debug/gdscript/warnings/enable", true);
	GLOBAL_DEF("debug/gdscript/warnings/exclude_addons", true);
	GLOBAL_DEF("debug/gdscript/warnings/renamed_in_godot_4_hint", true);
	for (int i = 0; i < (int)GDScriptWarning::WARNING_MAX; i++) {
		GDScriptWarning::Code code = (GDScriptWarning::Code)i;
		Variant default_enabled = GDScriptWarning::get_default_value(code);
		String path = GDScriptWarning::get_settings_path_from_code(code);
		GLOBAL_DEF(GDScriptWarning::get_property_info(code), default_enabled);
	}
#endif // DEBUG_ENABLED
}

GDScriptLanguage::~GDScriptLanguage() {
	singleton = nullptr;
}

void GDScriptLanguage::add_orphan_subclass(const String &p_qualified_name, const ObjectID &p_subclass) {
	orphan_subclasses[p_qualified_name] = p_subclass;
}

Ref<GDScript> GDScriptLanguage::get_orphan_subclass(const String &p_qualified_name) {
	HashMap<String, ObjectID>::Iterator orphan_subclass_element = orphan_subclasses.find(p_qualified_name);
	if (!orphan_subclass_element) {
		return Ref<GDScript>();
	}
	ObjectID orphan_subclass = orphan_subclass_element->value;
	Object *obj = ObjectDB::get_instance(orphan_subclass);
	orphan_subclasses.remove(orphan_subclass_element);
	if (!obj) {
		return Ref<GDScript>();
	}
	return Ref<GDScript>(Object::cast_to<GDScript>(obj));
}

Ref<GDScript> GDScriptLanguage::get_script_by_fully_qualified_name(const String &p_name) {
	{
		MutexLock lock(mutex);

		SelfList<GDScript> *elem = script_list.first();
		while (elem) {
			GDScript *scr = elem->self();
			if (scr->fully_qualified_name == p_name) {
				return scr;
			}
			elem = elem->next();
		}
	}

	Ref<GDScript> scr;
	scr.instantiate();
	scr->fully_qualified_name = p_name;
	return scr;
}

/*************** RESOURCE ***************/

Ref<Resource> ResourceFormatLoaderGDScript::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	Error err;
	bool ignoring = p_cache_mode == CACHE_MODE_IGNORE || p_cache_mode == CACHE_MODE_IGNORE_DEEP;
	Ref<GDScript> scr = GDScriptCache::get_full_script(p_original_path, err, "", ignoring);

	if (err && scr.is_valid()) {
		// If !scr.is_valid(), the error was likely from scr->load_source_code(), which already generates an error.
		ERR_PRINT_ED(vformat(R"(Failed to load script "%s" with error "%s".)", p_original_path, error_names[err]));
	}

	if (r_error) {
		// Don't fail loading because of parsing error.
		*r_error = scr.is_valid() ? OK : err;
	}

	return scr;
}

void ResourceFormatLoaderGDScript::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("gd");
	p_extensions->push_back("gdc");
}

bool ResourceFormatLoaderGDScript::handles_type(const String &p_type) const {
	return (p_type == "Script" || p_type == "GDScript");
}

String ResourceFormatLoaderGDScript::get_resource_type(const String &p_path) const {
	String el = p_path.get_extension().to_lower();
	if (el == "gd" || el == "gdc") {
		return "GDScript";
	}
	return "";
}

void ResourceFormatLoaderGDScript::get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_MSG(file.is_null(), "Cannot open file '" + p_path + "'.");

	String source = file->get_as_utf8_string();
	if (source.is_empty()) {
		return;
	}

	GDScriptParser parser;
	if (OK != parser.parse(source, p_path, false)) {
		return;
	}

	for (const String &E : parser.get_dependencies()) {
		p_dependencies->push_back(E);
	}
}

Error ResourceFormatSaverGDScript::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<GDScript> sqscr = p_resource;
	ERR_FAIL_COND_V(sqscr.is_null(), ERR_INVALID_PARAMETER);

	String source = sqscr->get_source_code();

	{
		Error err;
		Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);

		ERR_FAIL_COND_V_MSG(err, err, "Cannot save GDScript file '" + p_path + "'.");

		file->store_string(source);
		if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
			return ERR_CANT_CREATE;
		}
	}

	if (ScriptServer::is_reload_scripts_on_save_enabled()) {
		GDScriptLanguage::get_singleton()->reload_tool_script(p_resource, true);
	}

	return OK;
}

void ResourceFormatSaverGDScript::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (Object::cast_to<GDScript>(*p_resource)) {
		p_extensions->push_back("gd");
	}
}

bool ResourceFormatSaverGDScript::recognize(const Ref<Resource> &p_resource) const {
	return Object::cast_to<GDScript>(*p_resource) != nullptr;
}
