/**************************************************************************/
/*  nativescript.cpp                                                      */
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

#include "nativescript.h"

#include "core/os/mutex.h"
#include "core/os/rw_lock.h"
#include "gdnative/gdnative.h"

#include "core/core_string_names.h"
#include "core/global_constants.h"
#include "core/io/file_access_encrypted.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"

#include "main/main.h"

#include "scene/main/scene_tree.h"
#include "scene/resources/resource_format_text.h"

#include <stdlib.h>

#ifndef NO_THREADS
#include "core/os/thread.h"
#endif

#if defined(TOOLS_ENABLED) && defined(DEBUG_METHODS_ENABLED)
#include "api_generator.h"
#endif

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#endif

void NativeScript::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_class_name", "class_name"), &NativeScript::set_class_name);
	ClassDB::bind_method(D_METHOD("get_class_name"), &NativeScript::get_class_name);

	ClassDB::bind_method(D_METHOD("set_library", "library"), &NativeScript::set_library);
	ClassDB::bind_method(D_METHOD("get_library"), &NativeScript::get_library);

	ClassDB::bind_method(D_METHOD("set_script_class_name", "class_name"), &NativeScript::set_script_class_name);
	ClassDB::bind_method(D_METHOD("get_script_class_name"), &NativeScript::get_script_class_name);
	ClassDB::bind_method(D_METHOD("set_script_class_icon_path", "icon_path"), &NativeScript::set_script_class_icon_path);
	ClassDB::bind_method(D_METHOD("get_script_class_icon_path"), &NativeScript::get_script_class_icon_path);

	ClassDB::bind_method(D_METHOD("get_class_documentation"), &NativeScript::get_class_documentation);
	ClassDB::bind_method(D_METHOD("get_method_documentation", "method"), &NativeScript::get_method_documentation);
	ClassDB::bind_method(D_METHOD("get_signal_documentation", "signal_name"), &NativeScript::get_signal_documentation);
	ClassDB::bind_method(D_METHOD("get_property_documentation", "path"), &NativeScript::get_property_documentation);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "class_name"), "set_class_name", "get_class_name");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "library", PROPERTY_HINT_RESOURCE_TYPE, "GDNativeLibrary"), "set_library", "get_library");
	ADD_GROUP("Script Class", "script_class_");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "script_class_name"), "set_script_class_name", "get_script_class_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "script_class_icon_path", PROPERTY_HINT_FILE), "set_script_class_icon_path", "get_script_class_icon_path");

	ClassDB::bind_vararg_method(METHOD_FLAGS_DEFAULT, "new", &NativeScript::_new, MethodInfo("new"));
}

#define NSL NativeScriptLanguage::get_singleton()

#ifdef TOOLS_ENABLED

void NativeScript::_update_placeholder(PlaceHolderScriptInstance *p_placeholder) {
	NativeScriptDesc *script_data = get_script_desc();

	ERR_FAIL_COND(!script_data);

	List<PropertyInfo> info;
	get_script_property_list(&info);
	Map<StringName, Variant> values;
	for (List<PropertyInfo>::Element *E = info.front(); E; E = E->next()) {
		Variant value;
		get_property_default_value(E->get().name, value);
		values[E->get().name] = value;
	}

	p_placeholder->update(info, values);
}

void NativeScript::_placeholder_erased(PlaceHolderScriptInstance *p_placeholder) {
	placeholders.erase(p_placeholder);
}

#endif

bool NativeScript::inherits_script(const Ref<Script> &p_script) const {
	Ref<NativeScript> ns = p_script;
	if (ns.is_null()) {
		return false;
	}

	const NativeScriptDesc *other_s = ns->get_script_desc();
	if (!other_s) {
		return false;
	}

	const NativeScriptDesc *s = get_script_desc();

	while (s) {
		if (s == other_s) {
			return true;
		}
		s = s->base_data;
	}

	return false;
}

void NativeScript::set_class_name(String p_class_name) {
	class_name = p_class_name;
}

String NativeScript::get_class_name() const {
	return class_name;
}

void NativeScript::set_library(Ref<GDNativeLibrary> p_library) {
	if (!library.is_null()) {
		WARN_PRINT("Library in NativeScript already set. Do nothing.");
		return;
	}
	if (p_library.is_null()) {
		return;
	}
	library = p_library;
	lib_path = library->get_current_library_path();

#ifndef NO_THREADS
	if (Thread::get_caller_id() != Thread::get_main_id()) {
		NSL->defer_init_library(p_library, this);
	} else
#endif
	{
		NSL->init_library(p_library);
		NSL->register_script(this);
	}
}

Ref<GDNativeLibrary> NativeScript::get_library() const {
	return library;
}

void NativeScript::set_script_class_name(String p_type) {
	script_class_name = p_type;
}

String NativeScript::get_script_class_name() const {
	return script_class_name;
}

void NativeScript::set_script_class_icon_path(String p_icon_path) {
	script_class_icon_path = p_icon_path;
}

String NativeScript::get_script_class_icon_path() const {
	return script_class_icon_path;
}

bool NativeScript::can_instance() const {
	NativeScriptDesc *script_data = get_script_desc();

#ifdef TOOLS_ENABLED
	// Only valid if this is either a tool script or a "regular" script.
	// (so an environment where scripting is disabled (and not the editor) would not
	// create objects).
	return script_data && (is_tool() || ScriptServer::is_scripting_enabled());
#else
	return script_data;
#endif
}

Ref<Script> NativeScript::get_base_script() const {
	NativeScriptDesc *script_data = get_script_desc();

	if (!script_data) {
		return Ref<Script>();
	}

	NativeScript *script = (NativeScript *)NSL->create_script();
	Ref<NativeScript> ns = Ref<NativeScript>(script);
	ERR_FAIL_COND_V(!ns.is_valid(), Ref<Script>());

	ns->set_class_name(script_data->base);
	ns->set_library(get_library());
	return ns;
}

StringName NativeScript::get_instance_base_type() const {
	NativeScriptDesc *script_data = get_script_desc();

	if (!script_data) {
		return "";
	}

	return script_data->base_native_type;
}

ScriptInstance *NativeScript::instance_create(Object *p_this) {
	NativeScriptDesc *script_data = get_script_desc();

	if (!script_data) {
		return nullptr;
	}

	NativeScriptInstance *nsi = memnew(NativeScriptInstance);

	nsi->owner = p_this;
	nsi->script = Ref<NativeScript>(this);

#ifndef TOOLS_ENABLED
	if (!ScriptServer::is_scripting_enabled()) {
		nsi->userdata = NULL;
	} else {
		nsi->userdata = script_data->create_func.create_func((godot_object *)p_this, script_data->create_func.method_data);
	}
#else
	nsi->userdata = script_data->create_func.create_func((godot_object *)p_this, script_data->create_func.method_data);
#endif

	owners_lock.lock();
	instance_owners.insert(p_this);
	owners_lock.unlock();

	return nsi;
}

PlaceHolderScriptInstance *NativeScript::placeholder_instance_create(Object *p_this) {
#ifdef TOOLS_ENABLED
	PlaceHolderScriptInstance *sins = memnew(PlaceHolderScriptInstance(NSL, Ref<Script>(this), p_this));
	placeholders.insert(sins);

	_update_placeholder(sins);

	return sins;
#else
	return NULL;
#endif
}

bool NativeScript::instance_has(const Object *p_this) const {
	return instance_owners.has((Object *)p_this);
}

bool NativeScript::has_source_code() const {
	return false;
}

String NativeScript::get_source_code() const {
	return "";
}

void NativeScript::set_source_code(const String &p_code) {
}

Error NativeScript::reload(bool p_keep_state) {
	return FAILED;
}

bool NativeScript::has_method(const StringName &p_method) const {
	NativeScriptDesc *script_data = get_script_desc();

	while (script_data) {
		if (script_data->methods.has(p_method)) {
			return true;
		}

		script_data = script_data->base_data;
	}
	return false;
}

MethodInfo NativeScript::get_method_info(const StringName &p_method) const {
	NativeScriptDesc *script_data = get_script_desc();

	if (!script_data) {
		return MethodInfo();
	}

	while (script_data) {
		Map<StringName, NativeScriptDesc::Method>::Element *M = script_data->methods.find(p_method);

		if (M) {
			return M->get().info;
		}

		script_data = script_data->base_data;
	}
	return MethodInfo();
}

bool NativeScript::is_valid() const {
	return true;
}

bool NativeScript::is_tool() const {
	NativeScriptDesc *script_data = get_script_desc();

	if (script_data) {
		return script_data->is_tool;
	}

	return false;
}

ScriptLanguage *NativeScript::get_language() const {
	return NativeScriptLanguage::get_singleton();
}

bool NativeScript::has_script_signal(const StringName &p_signal) const {
	NativeScriptDesc *script_data = get_script_desc();

	while (script_data) {
		if (script_data->signals_.has(p_signal)) {
			return true;
		}
		script_data = script_data->base_data;
	}
	return false;
}

void NativeScript::get_script_signal_list(List<MethodInfo> *r_signals) const {
	NativeScriptDesc *script_data = get_script_desc();

	if (!script_data) {
		return;
	}

	Set<MethodInfo> signals_;

	while (script_data) {
		for (Map<StringName, NativeScriptDesc::Signal>::Element *S = script_data->signals_.front(); S; S = S->next()) {
			signals_.insert(S->get().signal);
		}

		script_data = script_data->base_data;
	}

	for (Set<MethodInfo>::Element *E = signals_.front(); E; E = E->next()) {
		r_signals->push_back(E->get());
	}
}

bool NativeScript::get_property_default_value(const StringName &p_property, Variant &r_value) const {
	NativeScriptDesc *script_data = get_script_desc();

	OrderedHashMap<StringName, NativeScriptDesc::Property>::Element P;
	while (!P && script_data) {
		P = script_data->properties.find(p_property);
		script_data = script_data->base_data;
	}
	if (!P) {
		return false;
	}

	r_value = P.get().default_value;
	return true;
}

void NativeScript::update_exports() {
}

void NativeScript::get_script_method_list(List<MethodInfo> *p_list) const {
	NativeScriptDesc *script_data = get_script_desc();

	if (!script_data) {
		return;
	}

	Set<MethodInfo> methods;

	while (script_data) {
		for (Map<StringName, NativeScriptDesc::Method>::Element *E = script_data->methods.front(); E; E = E->next()) {
			methods.insert(E->get().info);
		}

		script_data = script_data->base_data;
	}

	for (Set<MethodInfo>::Element *E = methods.front(); E; E = E->next()) {
		p_list->push_back(E->get());
	}
}

void NativeScript::get_script_property_list(List<PropertyInfo> *p_list) const {
	NativeScriptDesc *script_data = get_script_desc();

	Set<StringName> existing_properties;
	List<PropertyInfo>::Element *original_back = p_list->back();
	while (script_data) {
		List<PropertyInfo>::Element *insert_position = original_back;

		for (OrderedHashMap<StringName, NativeScriptDesc::Property>::Element E = script_data->properties.front(); E; E = E.next()) {
			if (!existing_properties.has(E.key())) {
				insert_position = p_list->insert_after(insert_position, E.get().info);
				existing_properties.insert(E.key());
			}
		}
		script_data = script_data->base_data;
	}
}

String NativeScript::get_class_documentation() const {
	NativeScriptDesc *script_data = get_script_desc();

	ERR_FAIL_COND_V_MSG(!script_data, "", "Attempt to get class documentation on invalid NativeScript.");

	return script_data->documentation;
}

String NativeScript::get_method_documentation(const StringName &p_method) const {
	NativeScriptDesc *script_data = get_script_desc();

	ERR_FAIL_COND_V_MSG(!script_data, "", "Attempt to get method documentation on invalid NativeScript.");

	while (script_data) {
		Map<StringName, NativeScriptDesc::Method>::Element *method = script_data->methods.find(p_method);

		if (method) {
			return method->get().documentation;
		}

		script_data = script_data->base_data;
	}

	ERR_FAIL_V_MSG("", "Attempt to get method documentation for non-existent method.");
}

String NativeScript::get_signal_documentation(const StringName &p_signal_name) const {
	NativeScriptDesc *script_data = get_script_desc();

	ERR_FAIL_COND_V_MSG(!script_data, "", "Attempt to get signal documentation on invalid NativeScript.");

	while (script_data) {
		Map<StringName, NativeScriptDesc::Signal>::Element *signal = script_data->signals_.find(p_signal_name);

		if (signal) {
			return signal->get().documentation;
		}

		script_data = script_data->base_data;
	}

	ERR_FAIL_V_MSG("", "Attempt to get signal documentation for non-existent signal.");
}

String NativeScript::get_property_documentation(const StringName &p_path) const {
	NativeScriptDesc *script_data = get_script_desc();

	ERR_FAIL_COND_V_MSG(!script_data, "", "Attempt to get property documentation on invalid NativeScript.");

	while (script_data) {
		OrderedHashMap<StringName, NativeScriptDesc::Property>::Element property = script_data->properties.find(p_path);

		if (property) {
			return property.get().documentation;
		}

		script_data = script_data->base_data;
	}

	ERR_FAIL_V_MSG("", "Attempt to get property documentation for non-existent signal.");
}

Variant NativeScript::_new(const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	if (lib_path.empty() || class_name.empty() || library.is_null()) {
		r_error.error = Variant::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return Variant();
	}

	NativeScriptDesc *script_data = get_script_desc();

	if (!script_data) {
		r_error.error = Variant::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return Variant();
	}

	r_error.error = Variant::CallError::CALL_OK;

	REF ref;
	Object *owner = nullptr;

	if (!(script_data->base_native_type == "")) {
		owner = ClassDB::instance(script_data->base_native_type);
	} else {
		owner = memnew(Reference);
	}

	if (!owner) {
		r_error.error = Variant::CallError::CALL_ERROR_INSTANCE_IS_NULL;
		return Variant();
	}

	Reference *r = Object::cast_to<Reference>(owner);
	if (r) {
		ref = REF(r);
	}

	NativeScriptInstance *instance = (NativeScriptInstance *)instance_create(owner);

	owner->set_script_instance(instance);

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

NativeScript::NativeScript() {
	library = Ref<GDNative>();
	lib_path = "";
	class_name = "";
}

NativeScript::~NativeScript() {
	NSL->unregister_script(this);
}

#define GET_SCRIPT_DESC() script->get_script_desc()

void NativeScriptInstance::_ml_call_reversed(NativeScriptDesc *script_data, const StringName &p_method, const Variant **p_args, int p_argcount) {
	if (script_data->base_data) {
		_ml_call_reversed(script_data->base_data, p_method, p_args, p_argcount);
	}

	Map<StringName, NativeScriptDesc::Method>::Element *E = script_data->methods.find(p_method);
	if (E) {
		godot_variant res = E->get().method.method((godot_object *)owner, E->get().method.method_data, userdata, p_argcount, (godot_variant **)p_args);
		godot_variant_destroy(&res);
	}
}

bool NativeScriptInstance::set(const StringName &p_name, const Variant &p_value) {
	NativeScriptDesc *script_data = GET_SCRIPT_DESC();

	while (script_data) {
		OrderedHashMap<StringName, NativeScriptDesc::Property>::Element P = script_data->properties.find(p_name);
		if (P) {
			P.get().setter.set_func((godot_object *)owner,
					P.get().setter.method_data,
					userdata,
					(godot_variant *)&p_value);
			return true;
		}

		Map<StringName, NativeScriptDesc::Method>::Element *E = script_data->methods.find("_set");
		if (E) {
			Variant name = p_name;
			const Variant *args[2] = { &name, &p_value };

			godot_variant result;
			result = E->get().method.method((godot_object *)owner,
					E->get().method.method_data,
					userdata,
					2,
					(godot_variant **)args);
			bool handled = *(Variant *)&result;
			godot_variant_destroy(&result);
			if (handled) {
				return true;
			}
		}

		script_data = script_data->base_data;
	}
	return false;
}
bool NativeScriptInstance::get(const StringName &p_name, Variant &r_ret) const {
	NativeScriptDesc *script_data = GET_SCRIPT_DESC();

	while (script_data) {
		OrderedHashMap<StringName, NativeScriptDesc::Property>::Element P = script_data->properties.find(p_name);
		if (P) {
			godot_variant value;
			value = P.get().getter.get_func((godot_object *)owner,
					P.get().getter.method_data,
					userdata);
			r_ret = *(Variant *)&value;
			godot_variant_destroy(&value);
			return true;
		}

		Map<StringName, NativeScriptDesc::Method>::Element *E = script_data->methods.find("_get");
		if (E) {
			Variant name = p_name;
			const Variant *args[1] = { &name };

			godot_variant result;
			result = E->get().method.method((godot_object *)owner,
					E->get().method.method_data,
					userdata,
					1,
					(godot_variant **)args);
			r_ret = *(Variant *)&result;
			godot_variant_destroy(&result);
			if (r_ret.get_type() != Variant::NIL) {
				return true;
			}
		}

		script_data = script_data->base_data;
	}
	return false;
}

void NativeScriptInstance::get_property_list(List<PropertyInfo> *p_properties) const {
	script->get_script_property_list(p_properties);

	NativeScriptDesc *script_data = GET_SCRIPT_DESC();

	while (script_data) {
		Map<StringName, NativeScriptDesc::Method>::Element *E = script_data->methods.find("_get_property_list");
		if (E) {
			godot_variant result;
			result = E->get().method.method((godot_object *)owner,
					E->get().method.method_data,
					userdata,
					0,
					nullptr);
			Variant res = *(Variant *)&result;
			godot_variant_destroy(&result);

			ERR_FAIL_COND_MSG(res.get_type() != Variant::ARRAY, "_get_property_list must return an array of dictionaries.");

			Array arr = res;
			for (int i = 0; i < arr.size(); i++) {
				Dictionary d = arr[i];

				ERR_CONTINUE(!d.has("name"));
				ERR_CONTINUE(!d.has("type"));

				PropertyInfo info;

				info.type = Variant::Type(d["type"].operator int64_t());
				ERR_CONTINUE(info.type < 0 || info.type >= Variant::VARIANT_MAX);

				info.name = d["name"];
				ERR_CONTINUE(info.name == "");

				if (d.has("hint")) {
					info.hint = PropertyHint(d["hint"].operator int64_t());
				}

				if (d.has("hint_string")) {
					info.hint_string = d["hint_string"];
				}

				if (d.has("usage")) {
					info.usage = d["usage"];
				}

				p_properties->push_back(info);
			}
		}

		script_data = script_data->base_data;
	}
	return;
}

Variant::Type NativeScriptInstance::get_property_type(const StringName &p_name, bool *r_is_valid) const {
	NativeScriptDesc *script_data = GET_SCRIPT_DESC();

	while (script_data) {
		OrderedHashMap<StringName, NativeScriptDesc::Property>::Element P = script_data->properties.find(p_name);
		if (P) {
			*r_is_valid = true;
			return P.get().info.type;
		}

		script_data = script_data->base_data;
	}
	return Variant::NIL;
}

void NativeScriptInstance::get_method_list(List<MethodInfo> *p_list) const {
	script->get_script_method_list(p_list);
}

bool NativeScriptInstance::has_method(const StringName &p_method) const {
	return script->has_method(p_method);
}

Variant NativeScriptInstance::call(const StringName &p_method, const Variant **p_args, int p_argcount, Variant::CallError &r_error) {
	NativeScriptDesc *script_data = GET_SCRIPT_DESC();

	while (script_data) {
		Map<StringName, NativeScriptDesc::Method>::Element *E = script_data->methods.find(p_method);
		if (E) {
			godot_variant result;

#ifdef DEBUG_ENABLED
			current_method_call = p_method;
#endif

			result = E->get().method.method((godot_object *)owner,
					E->get().method.method_data,
					userdata,
					p_argcount,
					(godot_variant **)p_args);

#ifdef DEBUG_ENABLED
			current_method_call = "";
#endif

			Variant res = *(Variant *)&result;
			godot_variant_destroy(&result);
			r_error.error = Variant::CallError::CALL_OK;
			return res;
		}

		script_data = script_data->base_data;
	}

	r_error.error = Variant::CallError::CALL_ERROR_INVALID_METHOD;
	return Variant();
}

void NativeScriptInstance::notification(int p_notification) {
#ifdef DEBUG_ENABLED
	if (p_notification == MainLoop::NOTIFICATION_CRASH) {
		if (current_method_call != StringName("")) {
			ERR_PRINT("NativeScriptInstance detected crash on method: " + current_method_call);
			current_method_call = "";
		}
	}
#endif

	Variant value = p_notification;
	const Variant *args[1] = { &value };
	call_multilevel("_notification", args, 1);
}

String NativeScriptInstance::to_string(bool *r_valid) {
	if (has_method(CoreStringNames::get_singleton()->_to_string)) {
		Variant::CallError ce;
		Variant ret = call(CoreStringNames::get_singleton()->_to_string, nullptr, 0, ce);
		if (ce.error == Variant::CallError::CALL_OK) {
			if (ret.get_type() != Variant::STRING) {
				if (r_valid) {
					*r_valid = false;
				}
				ERR_FAIL_V_MSG(String(), "Wrong type for " + CoreStringNames::get_singleton()->_to_string + ", must be a String.");
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

void NativeScriptInstance::refcount_incremented() {
	Variant::CallError err;
	call("_refcount_incremented", nullptr, 0, err);
	if (err.error != Variant::CallError::CALL_OK && err.error != Variant::CallError::CALL_ERROR_INVALID_METHOD) {
		ERR_PRINT("Failed to invoke _refcount_incremented - should not happen");
	}
}

bool NativeScriptInstance::refcount_decremented() {
	Variant::CallError err;
	Variant ret = call("_refcount_decremented", nullptr, 0, err);
	if (err.error != Variant::CallError::CALL_OK && err.error != Variant::CallError::CALL_ERROR_INVALID_METHOD) {
		ERR_PRINT("Failed to invoke _refcount_decremented - should not happen");
		return true; // assume we can destroy the object
	}
	if (err.error == Variant::CallError::CALL_ERROR_INVALID_METHOD) {
		// the method does not exist, default is true
		return true;
	}
	return ret;
}

Ref<Script> NativeScriptInstance::get_script() const {
	return script;
}

MultiplayerAPI::RPCMode NativeScriptInstance::get_rpc_mode(const StringName &p_method) const {
	NativeScriptDesc *script_data = GET_SCRIPT_DESC();

	while (script_data) {
		Map<StringName, NativeScriptDesc::Method>::Element *E = script_data->methods.find(p_method);
		if (E) {
			switch (E->get().rpc_mode) {
				case GODOT_METHOD_RPC_MODE_DISABLED:
					return MultiplayerAPI::RPC_MODE_DISABLED;
				case GODOT_METHOD_RPC_MODE_REMOTE:
					return MultiplayerAPI::RPC_MODE_REMOTE;
				case GODOT_METHOD_RPC_MODE_MASTER:
					return MultiplayerAPI::RPC_MODE_MASTER;
				case GODOT_METHOD_RPC_MODE_PUPPET:
					return MultiplayerAPI::RPC_MODE_PUPPET;
				case GODOT_METHOD_RPC_MODE_REMOTESYNC:
					return MultiplayerAPI::RPC_MODE_REMOTESYNC;
				case GODOT_METHOD_RPC_MODE_MASTERSYNC:
					return MultiplayerAPI::RPC_MODE_MASTERSYNC;
				case GODOT_METHOD_RPC_MODE_PUPPETSYNC:
					return MultiplayerAPI::RPC_MODE_PUPPETSYNC;
				default:
					return MultiplayerAPI::RPC_MODE_DISABLED;
			}
		}

		script_data = script_data->base_data;
	}

	return MultiplayerAPI::RPC_MODE_DISABLED;
}

MultiplayerAPI::RPCMode NativeScriptInstance::get_rset_mode(const StringName &p_variable) const {
	NativeScriptDesc *script_data = GET_SCRIPT_DESC();

	while (script_data) {
		OrderedHashMap<StringName, NativeScriptDesc::Property>::Element E = script_data->properties.find(p_variable);
		if (E) {
			switch (E.get().rset_mode) {
				case GODOT_METHOD_RPC_MODE_DISABLED:
					return MultiplayerAPI::RPC_MODE_DISABLED;
				case GODOT_METHOD_RPC_MODE_REMOTE:
					return MultiplayerAPI::RPC_MODE_REMOTE;
				case GODOT_METHOD_RPC_MODE_MASTER:
					return MultiplayerAPI::RPC_MODE_MASTER;
				case GODOT_METHOD_RPC_MODE_PUPPET:
					return MultiplayerAPI::RPC_MODE_PUPPET;
				case GODOT_METHOD_RPC_MODE_REMOTESYNC:
					return MultiplayerAPI::RPC_MODE_REMOTESYNC;
				case GODOT_METHOD_RPC_MODE_MASTERSYNC:
					return MultiplayerAPI::RPC_MODE_MASTERSYNC;
				case GODOT_METHOD_RPC_MODE_PUPPETSYNC:
					return MultiplayerAPI::RPC_MODE_PUPPETSYNC;
				default:
					return MultiplayerAPI::RPC_MODE_DISABLED;
			}
		}

		script_data = script_data->base_data;
	}

	return MultiplayerAPI::RPC_MODE_DISABLED;
}

ScriptLanguage *NativeScriptInstance::get_language() {
	return NativeScriptLanguage::get_singleton();
}

void NativeScriptInstance::call_multilevel(const StringName &p_method, const Variant **p_args, int p_argcount) {
	NativeScriptDesc *script_data = GET_SCRIPT_DESC();

	while (script_data) {
		Map<StringName, NativeScriptDesc::Method>::Element *E = script_data->methods.find(p_method);
		if (E) {
			godot_variant res = E->get().method.method((godot_object *)owner,
					E->get().method.method_data,
					userdata,
					p_argcount,
					(godot_variant **)p_args);
			godot_variant_destroy(&res);
		}
		script_data = script_data->base_data;
	}
}

void NativeScriptInstance::call_multilevel_reversed(const StringName &p_method, const Variant **p_args, int p_argcount) {
	NativeScriptDesc *script_data = GET_SCRIPT_DESC();

	if (script_data) {
		_ml_call_reversed(script_data, p_method, p_args, p_argcount);
	}
}

NativeScriptInstance::~NativeScriptInstance() {
	NativeScriptDesc *script_data = GET_SCRIPT_DESC();

	if (!script_data) {
		return;
	}

	script_data->destroy_func.destroy_func((godot_object *)owner, script_data->destroy_func.method_data, userdata);

	if (owner) {
		script->owners_lock.lock();
		script->instance_owners.erase(owner);
		script->owners_lock.unlock();
	}
}

NativeScriptLanguage *NativeScriptLanguage::singleton;

void NativeScriptLanguage::_unload_stuff(bool p_reload) {
	Map<String, Ref<GDNative>> erase_and_unload;

	for (Map<String, Map<StringName, NativeScriptDesc>>::Element *L = library_classes.front(); L; L = L->next()) {
		String lib_path = L->key();
		Map<StringName, NativeScriptDesc> classes = L->get();

		if (p_reload) {
			Map<String, Ref<GDNative>>::Element *E = library_gdnatives.find(lib_path);
			Ref<GDNative> gdn;

			if (E) {
				gdn = E->get();
			}

			bool should_reload = false;

			if (gdn.is_valid()) {
				Ref<GDNativeLibrary> lib = gdn->get_library();
				if (lib.is_valid()) {
					should_reload = lib->is_reloadable();
				}
			}

			if (!should_reload) {
				continue;
			}
		}

		Map<String, Ref<GDNative>>::Element *E = library_gdnatives.find(lib_path);
		Ref<GDNative> gdn;

		if (E) {
			gdn = E->get();
		}

		for (Map<StringName, NativeScriptDesc>::Element *C = classes.front(); C; C = C->next()) {
			// free property stuff first
			for (OrderedHashMap<StringName, NativeScriptDesc::Property>::Element P = C->get().properties.front(); P; P = P.next()) {
				if (P.get().getter.free_func) {
					P.get().getter.free_func(P.get().getter.method_data);
				}

				if (P.get().setter.free_func) {
					P.get().setter.free_func(P.get().setter.method_data);
				}
			}

			// free method stuff
			for (Map<StringName, NativeScriptDesc::Method>::Element *M = C->get().methods.front(); M; M = M->next()) {
				if (M->get().method.free_func) {
					M->get().method.free_func(M->get().method.method_data);
				}
			}

			// free constructor/destructor
			if (C->get().create_func.free_func) {
				C->get().create_func.free_func(C->get().create_func.method_data);
			}

			if (C->get().destroy_func.free_func) {
				C->get().destroy_func.free_func(C->get().destroy_func.method_data);
			}
		}

		erase_and_unload.insert(lib_path, gdn);
	}

	for (Map<String, Ref<GDNative>>::Element *E = erase_and_unload.front(); E; E = E->next()) {
		String lib_path = E->key();
		Ref<GDNative> gdn = E->get();

		library_classes.erase(lib_path);

		if (gdn.is_valid() && gdn->get_library().is_valid()) {
			Ref<GDNativeLibrary> lib = gdn->get_library();
			void *terminate_fn;
			Error err = gdn->get_symbol(lib->get_symbol_prefix() + _terminate_call_name, terminate_fn, true);

			if (err == OK) {
				void (*terminate)(void *) = (void (*)(void *))terminate_fn;

				terminate((void *)&lib_path);
			}
		}
	}
}

NativeScriptLanguage::NativeScriptLanguage() {
	NativeScriptLanguage::singleton = this;

	_init_call_type = "nativescript_init";
	_init_call_name = "nativescript_init";
	_terminate_call_name = "nativescript_terminate";
	_noarg_call_type = "nativescript_no_arg";
	_frame_call_name = "nativescript_frame";
#ifndef NO_THREADS
	_thread_enter_call_name = "nativescript_thread_enter";
	_thread_exit_call_name = "nativescript_thread_exit";
#endif
}

NativeScriptLanguage::~NativeScriptLanguage() {
	for (Map<String, Ref<GDNative>>::Element *L = NSL->library_gdnatives.front(); L; L = L->next()) {
		Ref<GDNative> lib = L->get();
		// only shut down valid libs, duh!
		if (lib.is_valid()) {
			// If it's a singleton-library then the gdnative module
			// manages the destruction at engine shutdown, not NativeScript.
			if (!lib->get_library()->is_singleton()) {
				lib->terminate();
			}
		}
	}

	NSL->library_classes.clear();
	NSL->library_gdnatives.clear();
	NSL->library_script_users.clear();
}

String NativeScriptLanguage::get_name() const {
	return "NativeScript";
}

void _add_reload_node() {
#ifdef TOOLS_ENABLED
	NativeReloadNode *rn = memnew(NativeReloadNode);
	EditorNode::get_singleton()->add_child(rn);
#endif
}

void NativeScriptLanguage::init() {
#if defined(TOOLS_ENABLED) && defined(DEBUG_METHODS_ENABLED)

	List<String> args = OS::get_singleton()->get_cmdline_args();

	List<String>::Element *E = args.find("--gdnative-generate-json-api");

	if (E && E->next()) {
		if (generate_c_api(E->next()->get()) != OK) {
			ERR_PRINT("Failed to generate C API\n");
		}
		Main::cleanup(true);
		exit(0);
	}
#endif

#ifdef TOOLS_ENABLED
	EditorNode::add_init_callback(&_add_reload_node);
#endif
}
String NativeScriptLanguage::get_type() const {
	return "NativeScript";
}
String NativeScriptLanguage::get_extension() const {
	return "gdns";
}
Error NativeScriptLanguage::execute_file(const String &p_path) {
	return OK; // Qué?
}
void NativeScriptLanguage::finish() {
	_unload_stuff();
}
void NativeScriptLanguage::get_reserved_words(List<String> *p_words) const {
}
bool NativeScriptLanguage::is_control_flow_keyword(String p_keyword) const {
	return false;
}
void NativeScriptLanguage::get_comment_delimiters(List<String> *p_delimiters) const {
}
void NativeScriptLanguage::get_string_delimiters(List<String> *p_delimiters) const {
}

Ref<Script> NativeScriptLanguage::get_template(const String &p_class_name, const String &p_base_class_name) const {
	NativeScript *s = memnew(NativeScript);
	s->set_class_name(p_class_name);
	return Ref<NativeScript>(s);
}
bool NativeScriptLanguage::validate(const String &p_script, int &r_line_error, int &r_col_error, String &r_test_error, const String &p_path, List<String> *r_functions, List<ScriptLanguage::Warning> *r_warnings, Set<int> *r_safe_lines) const {
	return true;
}

Script *NativeScriptLanguage::create_script() const {
	NativeScript *script = memnew(NativeScript);
	return script;
}
bool NativeScriptLanguage::has_named_classes() const {
	return true;
}
bool NativeScriptLanguage::supports_builtin_mode() const {
	return true;
}
int NativeScriptLanguage::find_function(const String &p_function, const String &p_code) const {
	return -1;
}
String NativeScriptLanguage::make_function(const String &p_class, const String &p_name, const PoolStringArray &p_args) const {
	return "";
}
void NativeScriptLanguage::auto_indent_code(String &p_code, int p_from_line, int p_to_line) const {
}
void NativeScriptLanguage::add_global_constant(const StringName &p_variable, const Variant &p_value) {
}

// Debugging stuff here. Not used for now.
String NativeScriptLanguage::debug_get_error() const {
	return "";
}
int NativeScriptLanguage::debug_get_stack_level_count() const {
	return -1;
}
int NativeScriptLanguage::debug_get_stack_level_line(int p_level) const {
	return -1;
}
String NativeScriptLanguage::debug_get_stack_level_function(int p_level) const {
	return "";
}
String NativeScriptLanguage::debug_get_stack_level_source(int p_level) const {
	return "";
}
void NativeScriptLanguage::debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
}
void NativeScriptLanguage::debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
}
void NativeScriptLanguage::debug_get_globals(List<String> *p_locals, List<Variant> *p_values, int p_max_subitems, int p_max_depth) {
}
String NativeScriptLanguage::debug_parse_stack_level_expression(int p_level, const String &p_expression, int p_max_subitems, int p_max_depth) {
	return "";
}
// Debugging stuff end.

void NativeScriptLanguage::reload_all_scripts() {
}

void NativeScriptLanguage::reload_tool_script(const Ref<Script> &p_script, bool p_soft_reload) {
}
void NativeScriptLanguage::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("gdns");
}

void NativeScriptLanguage::get_public_functions(List<MethodInfo> *p_functions) const {
}

void NativeScriptLanguage::get_public_constants(List<Pair<String, Variant>> *p_constants) const {
}

void NativeScriptLanguage::profiling_start() {
#ifdef DEBUG_ENABLED
#ifndef NO_THREADS
	MutexLock lock(mutex);
#endif

	profile_data.clear();
#endif
}

void NativeScriptLanguage::profiling_stop() {
#ifdef DEBUG_ENABLED
#ifndef NO_THREADS
	MutexLock lock(mutex);
#endif
#endif
}

int NativeScriptLanguage::profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) {
#ifdef DEBUG_ENABLED
#ifndef NO_THREADS
	MutexLock lock(mutex);
#endif
	int current = 0;

	for (Map<StringName, ProfileData>::Element *d = profile_data.front(); d; d = d->next()) {
		if (current >= p_info_max) {
			break;
		}

		p_info_arr[current].call_count = d->get().call_count;
		p_info_arr[current].self_time = d->get().self_time;
		p_info_arr[current].total_time = d->get().total_time;
		p_info_arr[current].signature = d->get().signature;
		current++;
	}

	return current;
#else
	return 0;
#endif
}

int NativeScriptLanguage::profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) {
#ifdef DEBUG_ENABLED
#ifndef NO_THREADS
	MutexLock lock(mutex);
#endif
	int current = 0;

	for (Map<StringName, ProfileData>::Element *d = profile_data.front(); d; d = d->next()) {
		if (current >= p_info_max) {
			break;
		}

		if (d->get().last_frame_call_count) {
			p_info_arr[current].call_count = d->get().last_frame_call_count;
			p_info_arr[current].self_time = d->get().last_frame_self_time;
			p_info_arr[current].total_time = d->get().last_frame_total_time;
			p_info_arr[current].signature = d->get().signature;
			current++;
		}
	}

	return current;
#else
	return 0;
#endif
}

void NativeScriptLanguage::profiling_add_data(StringName p_signature, uint64_t p_time) {
#ifdef DEBUG_ENABLED
#ifndef NO_THREADS
	MutexLock lock(mutex);
#endif

	Map<StringName, ProfileData>::Element *d = profile_data.find(p_signature);
	if (d) {
		d->get().call_count += 1;
		d->get().total_time += p_time;
		d->get().frame_call_count += 1;
		d->get().frame_total_time += p_time;
	} else {
		ProfileData data;

		data.signature = p_signature;
		data.call_count = 1;
		data.self_time = 0;
		data.total_time = p_time;
		data.frame_call_count = 1;
		data.frame_self_time = 0;
		data.frame_total_time = p_time;
		data.last_frame_call_count = 0;
		data.last_frame_self_time = 0;
		data.last_frame_total_time = 0;

		profile_data.insert(p_signature, data);
	}
#endif
}

int NativeScriptLanguage::register_binding_functions(godot_instance_binding_functions p_binding_functions) {
	// find index

	int idx = -1;

	for (int i = 0; i < binding_functions.size(); i++) {
		if (!binding_functions[i].first) {
			// free, we'll take it
			idx = i;
			break;
		}
	}

	if (idx == -1) {
		idx = binding_functions.size();
		binding_functions.resize(idx + 1);
	}

	// set the functions
	binding_functions.write[idx].first = true;
	binding_functions.write[idx].second = p_binding_functions;

	return idx;
}

void NativeScriptLanguage::unregister_binding_functions(int p_idx) {
	ERR_FAIL_INDEX(p_idx, binding_functions.size());

	// This should only be entered by the main thread after all objectts have been collected, so I assume
	// this doesn't need to be thread safe
	for (Map<void *, Pair<RWLock, Vector<void *>> *>::Element *E = binding_instances.front(); E; E = E->next()) {
		Vector<void *> &binding_data = E->get()->second;

		if (p_idx < binding_data.size() && binding_data[p_idx] && binding_functions[p_idx].second.free_instance_binding_data) {
			binding_functions[p_idx].second.free_instance_binding_data(binding_functions[p_idx].second.data, binding_data[p_idx]);
		}
	}

	binding_functions.write[p_idx].first = false;

	if (binding_functions[p_idx].second.free_func) {
		binding_functions[p_idx].second.free_func(binding_functions[p_idx].second.data);
	}
}

void *NativeScriptLanguage::get_instance_binding_data(int p_idx, Object *p_object) {
	ERR_FAIL_INDEX_V(p_idx, binding_functions.size(), nullptr);

	ERR_FAIL_COND_V_MSG(!binding_functions[p_idx].first, nullptr, "Tried to get binding data for a nativescript binding that does not exist.");

	Pair<RWLock, Vector<void *>> *binding_data = (Pair<RWLock, Vector<void *>> *)p_object->get_script_instance_binding(lang_idx);

	if (!binding_data) {
		return nullptr; // should never happen.
	}

	// We can get here on muliple threads at the same time.
	// Check lucky scenario first with shared access.
	{
		RWLockRead rlock(binding_data->first);
		if (binding_data->second.size() > p_idx && binding_data->second[p_idx]) {
			return binding_data->second[p_idx];
		}
	}

	// Gotta work to do with exclusive access.
	{
		RWLockWrite wlock(binding_data->first);

		// Need to check again, this might have already been allocated
		if (binding_data->second.size() > p_idx && binding_data->second[p_idx]) {
			return binding_data->second[p_idx];
		}

		if (binding_data->second.size() <= p_idx) {
			// okay, add new elements here.
			int old_size = binding_data->second.size();

			binding_data->second.resize(p_idx + 1);

			for (int i = old_size; i <= p_idx; i++) {
				binding_data->second.write[i] = NULL;
			}
		}

		if (!binding_data->second[p_idx]) {
			const void *global_type_tag = get_global_type_tag(p_idx, p_object->get_class_name());

			// no binding data yet, soooooo alloc new one \o/
			binding_data->second.write[p_idx] = binding_functions[p_idx].second.alloc_instance_binding_data(binding_functions[p_idx].second.data, global_type_tag, (godot_object *)p_object);
		}

		return binding_data->second[p_idx];
	}
}

void *NativeScriptLanguage::alloc_instance_binding_data(Object *p_object) {
	{
		// Look at the binding instances with shared access.
		// If data is there return it
		RWLockRead rlock(binding_instances_lock);

		if (binding_instances.has(p_object)) {
			return binding_instances[p_object];
		}
	}

	{
		// The data was not there, lock with exclusive access.
		RWLockWrite wlock(binding_instances_lock);

		// We need to check again as that might have changed from the shared access check.
		if (binding_instances.has(p_object)) {
			return binding_instances[p_object];
		}

		// Data is not there, create it.
		Pair<RWLock, Vector<void *>> *binding_data = new Pair<RWLock, Vector<void *>>;

		binding_data->second.resize(binding_functions.size());

		for (int i = 0; i < binding_functions.size(); i++) {
			binding_data->second.write[i] = NULL;
		}

		binding_instances.insert(p_object, binding_data);

		return (void *)binding_data;
	}
}

void NativeScriptLanguage::free_instance_binding_data(Object *p_object, void *p_data) {
	if (!p_data) {
		return;
	}

	Pair<RWLock, Vector<void *>> &binding_data = (*(Pair<RWLock, Vector<void *>> *)p_data);

	for (int i = 0; i < binding_data.second.size(); i++) {
		if (!binding_data.second[i]) {
			continue;
		}

		if (binding_functions[i].first && binding_functions[i].second.free_instance_binding_data) {
			binding_functions[i].second.free_instance_binding_data(binding_functions[i].second.data, binding_data.second[i]);
		}
	}

	{
		RWLockWrite wlock(binding_instances_lock);
		binding_instances.erase(p_object);
	}

	delete &binding_data;
}

void NativeScriptLanguage::refcount_incremented_instance_binding(Object *p_object) {
	void *data = p_object->get_script_instance_binding(lang_idx);

	if (!data) {
		return;
	}

	Vector<void *> &binding_data = (*(Pair<RWLock, Vector<void *>> *)data).second;

	for (int i = 0; i < binding_data.size(); i++) {
		if (!binding_data[i]) {
			continue;
		}

		if (!binding_functions[i].first) {
			continue;
		}

		if (binding_functions[i].second.refcount_incremented_instance_binding) {
			binding_functions[i].second.refcount_incremented_instance_binding(binding_data[i], p_object);
		}
	}
}

bool NativeScriptLanguage::refcount_decremented_instance_binding(Object *p_object) {
	void *data = p_object->get_script_instance_binding(lang_idx);

	if (!data) {
		return true;
	}

	Vector<void *> &binding_data = (*(Pair<RWLock, Vector<void *>> *)data).second;

	bool can_die = true;

	for (int i = 0; i < binding_data.size(); i++) {
		if (!binding_data[i]) {
			continue;
		}

		if (!binding_functions[i].first) {
			continue;
		}

		if (binding_functions[i].second.refcount_decremented_instance_binding) {
			can_die = can_die && binding_functions[i].second.refcount_decremented_instance_binding(binding_data[i], p_object);
		}
	}

	return can_die;
}

void NativeScriptLanguage::set_global_type_tag(int p_idx, StringName p_class_name, const void *p_type_tag) {
	if (!global_type_tags.has(p_idx)) {
		global_type_tags.insert(p_idx, HashMap<StringName, const void *>());
	}

	HashMap<StringName, const void *> &tags = global_type_tags[p_idx];

	tags.set(p_class_name, p_type_tag);
}

const void *NativeScriptLanguage::get_global_type_tag(int p_idx, StringName p_class_name) const {
	if (!global_type_tags.has(p_idx)) {
		return nullptr;
	}

	const HashMap<StringName, const void *> &tags = global_type_tags[p_idx];

	if (!tags.has(p_class_name)) {
		return nullptr;
	}

	const void *tag = tags.get(p_class_name);

	return tag;
}

#ifndef NO_THREADS
void NativeScriptLanguage::defer_init_library(Ref<GDNativeLibrary> lib, NativeScript *script) {
	MutexLock lock(mutex);
	libs_to_init.insert(lib);
	scripts_to_register.insert(script);
	has_objects_to_register.set();
}
#endif

void NativeScriptLanguage::init_library(const Ref<GDNativeLibrary> &lib) {
#ifndef NO_THREADS
	MutexLock lock(mutex);
#endif
	// See if this library was "registered" already.
	const String &lib_path = lib->get_current_library_path();
	ERR_FAIL_COND_MSG(lib_path.length() == 0, lib->get_name() + " does not have a library for the current platform.");
	Map<String, Ref<GDNative>>::Element *E = library_gdnatives.find(lib_path);

	if (!E) {
		Ref<GDNative> gdn;
		gdn.instance();
		gdn->set_library(lib);

		// TODO check the return value?
		gdn->initialize();

		library_gdnatives.insert(lib_path, gdn);

		library_classes.insert(lib_path, Map<StringName, NativeScriptDesc>());

		if (!library_script_users.has(lib_path)) {
			library_script_users.insert(lib_path, Set<NativeScript *>());
		}

		void *proc_ptr;

		Error err = gdn->get_symbol(lib->get_symbol_prefix() + _init_call_name, proc_ptr);

		if (err != OK) {
			ERR_PRINT(String("No " + _init_call_name + " in \"" + lib_path + "\" found").utf8().get_data());
		} else {
			((void (*)(godot_string *))proc_ptr)((godot_string *)&lib_path);
		}
	} else {
		// already initialized. Nice.
	}
}

void NativeScriptLanguage::register_script(NativeScript *script) {
#ifndef NO_THREADS
	MutexLock lock(mutex);
#endif
	library_script_users[script->lib_path].insert(script);
}

void NativeScriptLanguage::unregister_script(NativeScript *script) {
#ifndef NO_THREADS
	MutexLock lock(mutex);
#endif
	Map<String, Set<NativeScript *>>::Element *S = library_script_users.find(script->lib_path);
	if (S) {
		S->get().erase(script);
		if (S->get().size() == 0) {
			library_script_users.erase(S);

			Map<String, Ref<GDNative>>::Element *G = library_gdnatives.find(script->lib_path);
			if (G && G->get()->get_library()->is_reloadable()) {
				// ONLY if the library is marked as reloadable, and no more instances of its scripts exist do we unload the library

				// First remove meta data related to the library
				Map<String, Map<StringName, NativeScriptDesc>>::Element *L = library_classes.find(script->lib_path);
				if (L) {
					Map<StringName, NativeScriptDesc> classes = L->get();

					for (Map<StringName, NativeScriptDesc>::Element *C = classes.front(); C; C = C->next()) {
						// free property stuff first
						for (OrderedHashMap<StringName, NativeScriptDesc::Property>::Element P = C->get().properties.front(); P; P = P.next()) {
							if (P.get().getter.free_func) {
								P.get().getter.free_func(P.get().getter.method_data);
							}

							if (P.get().setter.free_func) {
								P.get().setter.free_func(P.get().setter.method_data);
							}
						}

						// free method stuff
						for (Map<StringName, NativeScriptDesc::Method>::Element *M = C->get().methods.front(); M; M = M->next()) {
							if (M->get().method.free_func) {
								M->get().method.free_func(M->get().method.method_data);
							}
						}

						// free constructor/destructor
						if (C->get().create_func.free_func) {
							C->get().create_func.free_func(C->get().create_func.method_data);
						}

						if (C->get().destroy_func.free_func) {
							C->get().destroy_func.free_func(C->get().destroy_func.method_data);
						}
					}

					library_classes.erase(script->lib_path);
				}

				// now unload the library
				G->get()->terminate();
				library_gdnatives.erase(G);
			}
		}
	}
#ifndef NO_THREADS
	scripts_to_register.erase(script);
#endif
}

void NativeScriptLanguage::call_libraries_cb(const StringName &name) {
	// library_gdnatives is modified only from the main thread, so it's safe not to use mutex here
	for (Map<String, Ref<GDNative>>::Element *L = library_gdnatives.front(); L; L = L->next()) {
		if (L->get().is_null()) {
			continue;
		}

		if (L->get()->is_initialized()) {
			void *proc_ptr;
			Error err = L->get()->get_symbol(L->get()->get_library()->get_symbol_prefix() + name, proc_ptr);

			if (!err) {
				((void (*)())proc_ptr)();
			}
		}
	}
}

void NativeScriptLanguage::frame() {
#ifndef NO_THREADS
	if (has_objects_to_register.is_set()) {
		MutexLock lock(mutex);
		for (Set<Ref<GDNativeLibrary>>::Element *L = libs_to_init.front(); L; L = L->next()) {
			init_library(L->get());
		}
		libs_to_init.clear();
		for (Set<NativeScript *>::Element *S = scripts_to_register.front(); S; S = S->next()) {
			register_script(S->get());
		}
		scripts_to_register.clear();
		has_objects_to_register.clear();
	}
#endif

#ifdef DEBUG_ENABLED
	{
#ifndef NO_THREADS
		MutexLock lock(mutex);
#endif

		for (Map<StringName, ProfileData>::Element *d = profile_data.front(); d; d = d->next()) {
			d->get().last_frame_call_count = d->get().frame_call_count;
			d->get().last_frame_self_time = d->get().frame_self_time;
			d->get().last_frame_total_time = d->get().frame_total_time;
			d->get().frame_call_count = 0;
			d->get().frame_self_time = 0;
			d->get().frame_total_time = 0;
		}
	}
#endif

	call_libraries_cb(_frame_call_name);
}

#ifndef NO_THREADS

void NativeScriptLanguage::thread_enter() {
	call_libraries_cb(_thread_enter_call_name);
}

void NativeScriptLanguage::thread_exit() {
	call_libraries_cb(_thread_exit_call_name);
}

#endif // NO_THREADS

bool NativeScriptLanguage::handles_global_class_type(const String &p_type) const {
	return p_type == "NativeScript";
}

String NativeScriptLanguage::get_global_class_name(const String &p_path, String *r_base_type, String *r_icon_path) const {
	if (!p_path.empty()) {
		Ref<NativeScript> script = ResourceLoader::load(p_path, "NativeScript");
		if (script.is_valid()) {
			if (r_base_type) {
				*r_base_type = script->get_instance_base_type();
			}
			if (r_icon_path) {
				*r_icon_path = script->get_script_class_icon_path();
			}
			return script->get_script_class_name();
		}
		if (r_base_type) {
			*r_base_type = String();
		}
		if (r_icon_path) {
			*r_icon_path = String();
		}
	}
	return String();
}

void NativeReloadNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_notification"), &NativeReloadNode::_notification);
}

void NativeReloadNode::_notification(int p_what) {
#ifdef TOOLS_ENABLED

	switch (p_what) {
		case MainLoop::NOTIFICATION_WM_FOCUS_OUT: {
			if (unloaded) {
				break;
			}
#ifndef NO_THREADS
			MutexLock lock(NSL->mutex);
#endif
			NSL->_unload_stuff(true);

			for (Map<String, Ref<GDNative>>::Element *L = NSL->library_gdnatives.front(); L; L = L->next()) {
				Ref<GDNative> gdn = L->get();

				if (gdn.is_null()) {
					continue;
				}

				// Don't unload what should not be reloaded!
				if (!gdn->get_library()->is_reloadable()) {
					continue;
				}

				// singleton libraries might have alive pointers living inside the
				// editor. Also reloading a singleton library would mean that
				// the singleton entry will not be called again, as this only
				// happens at engine startup.
				if (gdn->get_library()->is_singleton()) {
					continue;
				}

				gdn->terminate();
			}

			unloaded = true;

		} break;

		case MainLoop::NOTIFICATION_WM_FOCUS_IN: {
			if (!unloaded) {
				break;
			}
#ifndef NO_THREADS
			MutexLock lock(NSL->mutex);
#endif
			Set<StringName> libs_to_remove;
			for (Map<String, Ref<GDNative>>::Element *L = NSL->library_gdnatives.front(); L; L = L->next()) {
				Ref<GDNative> gdn = L->get();

				if (gdn.is_null()) {
					continue;
				}

				if (!gdn->get_library()->is_reloadable()) {
					continue;
				}

				// since singleton libraries are not unloaded there is no point
				// in loading them again.
				if (gdn->get_library()->is_singleton()) {
					continue;
				}

				if (!gdn->initialize()) {
					libs_to_remove.insert(L->key());
					continue;
				}

				NSL->library_classes.insert(L->key(), Map<StringName, NativeScriptDesc>());

				// here the library registers all the classes and stuff.

				void *proc_ptr;
				Error err = gdn->get_symbol(gdn->get_library()->get_symbol_prefix() + "nativescript_init", proc_ptr);
				if (err != OK) {
					ERR_PRINT(String("No godot_nativescript_init in \"" + L->key() + "\" found").utf8().get_data());
				} else {
					((void (*)(void *))proc_ptr)((void *)&L->key());
				}

				for (Map<String, Set<NativeScript *>>::Element *U = NSL->library_script_users.front(); U; U = U->next()) {
					// Multiple GDNative libraries may be reloaded. The library and script
					// path should match to prevent failing `NSL->library_classes` lookup
					// from `get_script_desc()` in `script->_update_placeholder` below.
					// This check also prevents "!script_data is true" error from occurring
					// when e. g. re-focusing editor window.
					if (L->key() != U->key()) {
						continue;
					}

					for (Set<NativeScript *>::Element *S = U->get().front(); S; S = S->next()) {
						NativeScript *script = S->get();

						if (script->placeholders.size() == 0) {
							continue;
						}

						for (Set<PlaceHolderScriptInstance *>::Element *P = script->placeholders.front(); P; P = P->next()) {
							script->_update_placeholder(P->get());
						}
					}
				}
			}

			unloaded = false;

			for (Set<StringName>::Element *R = libs_to_remove.front(); R; R = R->next()) {
				NSL->library_gdnatives.erase(R->get());
			}

		} break;
		default: {
		};
	}
#endif
}

RES ResourceFormatLoaderNativeScript::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_no_subresource_cache) {
	return ResourceFormatLoaderText::singleton->load(p_path, p_original_path, r_error, p_no_subresource_cache);
}

void ResourceFormatLoaderNativeScript::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("gdns");
}

bool ResourceFormatLoaderNativeScript::handles_type(const String &p_type) const {
	return (p_type == "Script" || p_type == "NativeScript");
}

String ResourceFormatLoaderNativeScript::get_resource_type(const String &p_path) const {
	String el = p_path.get_extension().to_lower();
	if (el == "gdns") {
		return "NativeScript";
	}
	return "";
}

Error ResourceFormatSaverNativeScript::save(const String &p_path, const RES &p_resource, uint32_t p_flags) {
	ResourceFormatSaverText rfst;
	return rfst.save(p_path, p_resource, p_flags);
}

bool ResourceFormatSaverNativeScript::recognize(const RES &p_resource) const {
	return Object::cast_to<NativeScript>(*p_resource) != nullptr;
}

void ResourceFormatSaverNativeScript::get_recognized_extensions(const RES &p_resource, List<String> *p_extensions) const {
	if (Object::cast_to<NativeScript>(*p_resource)) {
		p_extensions->push_back("gdns");
	}
}
