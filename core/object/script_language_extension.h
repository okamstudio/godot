/**************************************************************************/
/*  script_language_extension.h                                           */
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

#include "core/extension/ext_wrappers.gen.inc"
#include "core/object/gdvirtual.gen.inc"
#include "core/object/script_language.h"
#include "core/variant/native_ptr.h"
#include "core/variant/typed_array.h"

class ScriptExtension : public Script {
	GDCLASS(ScriptExtension, Script)

protected:
	EXBIND0R(bool, editor_can_reload_from_file)

	GDVIRTUAL1(_placeholder_erased, GDExtensionPtr<void>)
	virtual void _placeholder_erased(PlaceHolderScriptInstance *p_placeholder) override {
		GDVIRTUAL_CALL(_placeholder_erased, p_placeholder);
	}

	static void _bind_methods();

public:
	EXBIND0RC(bool, can_instantiate)
	EXBIND0RC(Ref<Script>, get_base_script)
	EXBIND0RC(StringName, get_global_name)
	EXBIND1RC(bool, inherits_script, const Ref<Script> &)
	EXBIND0RC(StringName, get_instance_base_type)

	GDVIRTUAL1RC_REQUIRED(GDExtensionPtr<void>, _instance_create, Object *)
	virtual ScriptInstance *instance_create(Object *p_this) override {
		GDExtensionPtr<void> ret = nullptr;
		GDVIRTUAL_CALL(_instance_create, p_this, ret);
		return reinterpret_cast<ScriptInstance *>(ret.operator void *());
	}
	GDVIRTUAL1RC_REQUIRED(GDExtensionPtr<void>, _placeholder_instance_create, Object *)
	PlaceHolderScriptInstance *placeholder_instance_create(Object *p_this) override {
		GDExtensionPtr<void> ret = nullptr;
		GDVIRTUAL_CALL(_placeholder_instance_create, p_this, ret);
		return reinterpret_cast<PlaceHolderScriptInstance *>(ret.operator void *());
	}

	EXBIND1RC(bool, instance_has, const Object *)
	EXBIND0RC(bool, has_source_code)
	EXBIND0RC(String, get_source_code)
	EXBIND1(set_source_code, const String &)
	EXBIND1R(Error, reload, bool)

	GDVIRTUAL0RC_REQUIRED(StringName, _get_doc_class_name)
	GDVIRTUAL0RC_REQUIRED(TypedArray<Dictionary>, _get_documentation)
	GDVIRTUAL0RC(String, _get_class_icon_path)
#ifdef TOOLS_ENABLED
	virtual StringName get_doc_class_name() const override {
		StringName ret;
		GDVIRTUAL_CALL(_get_doc_class_name, ret);
		return ret;
	}

	virtual Vector<DocData::ClassDoc> get_documentation() const override {
		TypedArray<Dictionary> doc;
		GDVIRTUAL_CALL(_get_documentation, doc);

		Vector<DocData::ClassDoc> class_doc;
		for (int i = 0; i < doc.size(); i++) {
			class_doc.append(DocData::ClassDoc::from_dict(doc[i]));
		}

		return class_doc;
	}

	virtual String get_class_icon_path() const override {
		String ret;
		GDVIRTUAL_CALL(_get_class_icon_path, ret);
		return ret;
	}
#endif // TOOLS_ENABLED

	EXBIND1RC(bool, has_method, const StringName &)
	EXBIND1RC(bool, has_static_method, const StringName &)

	GDVIRTUAL1RC(Variant, _get_script_method_argument_count, const StringName &)
	virtual int get_script_method_argument_count(const StringName &p_method, bool *r_is_valid = nullptr) const override {
		Variant ret;
		if (GDVIRTUAL_CALL(_get_script_method_argument_count, p_method, ret) && ret.get_type() == Variant::INT) {
			if (r_is_valid) {
				*r_is_valid = true;
			}
			return ret.operator int();
		}
		// Fallback to default.
		return Script::get_script_method_argument_count(p_method, r_is_valid);
	}

	GDVIRTUAL1RC_REQUIRED(Dictionary, _get_method_info, const StringName &)
	virtual MethodInfo get_method_info(const StringName &p_method) const override {
		Dictionary mi;
		GDVIRTUAL_CALL(_get_method_info, p_method, mi);
		return MethodInfo::from_dict(mi);
	}

	EXBIND0RC(bool, is_tool)
	EXBIND0RC(bool, is_valid)

	virtual bool is_abstract() const override {
		bool abst;
		return GDVIRTUAL_CALL(_is_abstract, abst) && abst;
	}
	GDVIRTUAL0RC(bool, _is_abstract)

	EXBIND0RC(ScriptLanguage *, get_language)
	EXBIND1RC(bool, has_script_signal, const StringName &)

	GDVIRTUAL0RC_REQUIRED(TypedArray<Dictionary>, _get_script_signal_list)

	virtual void get_script_signal_list(List<MethodInfo> *r_signals) const override {
		TypedArray<Dictionary> sl;
		GDVIRTUAL_CALL(_get_script_signal_list, sl);
		for (int i = 0; i < sl.size(); i++) {
			r_signals->push_back(MethodInfo::from_dict(sl[i]));
		}
	}

	GDVIRTUAL1RC_REQUIRED(bool, _has_property_default_value, const StringName &)
	GDVIRTUAL1RC_REQUIRED(Variant, _get_property_default_value, const StringName &)

	virtual bool get_property_default_value(const StringName &p_property, Variant &r_value) const override {
		bool has_dv = false;
		if (!GDVIRTUAL_CALL(_has_property_default_value, p_property, has_dv) || !has_dv) {
			return false;
		}
		Variant ret;
		GDVIRTUAL_CALL(_get_property_default_value, p_property, ret);
		r_value = ret;
		return true;
	}

	EXBIND0(update_exports)

	GDVIRTUAL0RC_REQUIRED(TypedArray<Dictionary>, _get_script_method_list)

	virtual void get_script_method_list(List<MethodInfo> *r_methods) const override {
		TypedArray<Dictionary> sl;
		GDVIRTUAL_CALL(_get_script_method_list, sl);
		for (int i = 0; i < sl.size(); i++) {
			r_methods->push_back(MethodInfo::from_dict(sl[i]));
		}
	}

	GDVIRTUAL0RC_REQUIRED(TypedArray<Dictionary>, _get_script_property_list)

	virtual void get_script_property_list(List<PropertyInfo> *r_propertys) const override {
		TypedArray<Dictionary> sl;
		GDVIRTUAL_CALL(_get_script_property_list, sl);
		for (int i = 0; i < sl.size(); i++) {
			r_propertys->push_back(PropertyInfo::from_dict(sl[i]));
		}
	}

	EXBIND1RC(int, get_member_line, const StringName &)

	GDVIRTUAL0RC_REQUIRED(Dictionary, _get_constants)

	virtual void get_constants(HashMap<StringName, Variant> *p_constants) override {
		Dictionary constants;
		GDVIRTUAL_CALL(_get_constants, constants);
		List<Variant> keys;
		constants.get_key_list(&keys);
		for (const Variant &K : keys) {
			p_constants->insert(K, constants[K]);
		}
	}
	GDVIRTUAL0RC_REQUIRED(TypedArray<StringName>, _get_members)
	virtual void get_members(HashSet<StringName> *p_members) override {
		TypedArray<StringName> members;
		GDVIRTUAL_CALL(_get_members, members);
		for (int i = 0; i < members.size(); i++) {
			p_members->insert(members[i]);
		}
	}

	EXBIND0RC(bool, is_placeholder_fallback_enabled)

	GDVIRTUAL0RC_REQUIRED(Variant, _get_rpc_config)

	virtual Variant get_rpc_config() const override {
		Variant ret;
		GDVIRTUAL_CALL(_get_rpc_config, ret);
		return ret;
	}

	ScriptExtension() {}
};

typedef ScriptLanguage::ProfilingInfo ScriptLanguageExtensionProfilingInfo;

GDVIRTUAL_NATIVE_PTR(ScriptLanguageExtensionProfilingInfo)

class ScriptLanguageExtension : public ScriptLanguage {
	GDCLASS(ScriptLanguageExtension, ScriptLanguage)
protected:
	static void _bind_methods();

public:
	EXBIND0RC(String, get_name)

	EXBIND0(init)
	EXBIND0RC(String, get_type)
	EXBIND0RC(String, get_extension)
	EXBIND0(finish)

	/* EDITOR FUNCTIONS */

	GDVIRTUAL0RC_REQUIRED(Vector<String>, _get_reserved_words)

	virtual void get_reserved_words(List<String> *p_words) const override {
		Vector<String> ret;
		GDVIRTUAL_CALL(_get_reserved_words, ret);
		for (int i = 0; i < ret.size(); i++) {
			p_words->push_back(ret[i]);
		}
	}
	EXBIND1RC(bool, is_control_flow_keyword, const String &)

	GDVIRTUAL0RC_REQUIRED(Vector<String>, _get_comment_delimiters)

	virtual void get_comment_delimiters(List<String> *p_words) const override {
		Vector<String> ret;
		GDVIRTUAL_CALL(_get_comment_delimiters, ret);
		for (int i = 0; i < ret.size(); i++) {
			p_words->push_back(ret[i]);
		}
	}

	GDVIRTUAL0RC(Vector<String>, _get_doc_comment_delimiters)

	virtual void get_doc_comment_delimiters(List<String> *p_words) const override {
		Vector<String> ret;
		GDVIRTUAL_CALL(_get_doc_comment_delimiters, ret);
		for (int i = 0; i < ret.size(); i++) {
			p_words->push_back(ret[i]);
		}
	}

	GDVIRTUAL0RC_REQUIRED(Vector<String>, _get_string_delimiters)

	virtual void get_string_delimiters(List<String> *p_words) const override {
		Vector<String> ret;
		GDVIRTUAL_CALL(_get_string_delimiters, ret);
		for (int i = 0; i < ret.size(); i++) {
			p_words->push_back(ret[i]);
		}
	}

	EXBIND3RC(Ref<Script>, make_template, const String &, const String &, const String &)

	GDVIRTUAL1RC_REQUIRED(TypedArray<Dictionary>, _get_built_in_templates, StringName)

	virtual Vector<ScriptTemplate> get_built_in_templates(const StringName &p_object) override {
		TypedArray<Dictionary> ret;
		GDVIRTUAL_CALL(_get_built_in_templates, p_object, ret);
		Vector<ScriptTemplate> stret;
		for (int i = 0; i < ret.size(); i++) {
			Dictionary d = ret[i];
			ScriptTemplate st;
			ERR_CONTINUE(!d.has("inherit"));
			st.inherit = d["inherit"];
			ERR_CONTINUE(!d.has("name"));
			st.name = d["name"];
			ERR_CONTINUE(!d.has("description"));
			st.description = d["description"];
			ERR_CONTINUE(!d.has("content"));
			st.content = d["content"];
			ERR_CONTINUE(!d.has("id"));
			st.id = d["id"];
			ERR_CONTINUE(!d.has("origin"));
			st.origin = TemplateLocation(int(d["origin"]));
			stret.push_back(st);
		}
		return stret;
	}

	EXBIND0R(bool, is_using_templates)

	GDVIRTUAL6RC_REQUIRED(Dictionary, _validate, const String &, const String &, bool, bool, bool, bool)
	virtual bool validate(const String &p_script, const String &p_path = "", List<String> *r_functions = nullptr, List<ScriptError> *r_errors = nullptr, List<Warning> *r_warnings = nullptr, HashSet<int> *r_safe_lines = nullptr) const override {
		Dictionary ret;
		GDVIRTUAL_CALL(_validate, p_script, p_path, r_functions != nullptr, r_errors != nullptr, r_warnings != nullptr, r_safe_lines != nullptr, ret);
		if (!ret.has("valid")) {
			return false;
		}
		if (r_functions != nullptr && ret.has("functions")) {
			Vector<String> functions = ret["functions"];
			for (int i = 0; i < functions.size(); i++) {
				r_functions->push_back(functions[i]);
			}
		}
		if (r_errors != nullptr && ret.has("errors")) {
			Array errors = ret["errors"];
			for (const Variant &error : errors) {
				Dictionary err = error;
				ERR_CONTINUE(!err.has("line"));
				ERR_CONTINUE(!err.has("column"));
				ERR_CONTINUE(!err.has("message"));

				ScriptError serr;
				if (err.has("path")) {
					serr.path = err["path"];
				}
				serr.line = err["line"];
				serr.column = err["column"];
				serr.message = err["message"];

				r_errors->push_back(serr);
			}
		}
		if (r_warnings != nullptr && ret.has("warnings")) {
			ERR_FAIL_COND_V(!ret.has("warnings"), false);
			Array warnings = ret["warnings"];
			for (const Variant &warning : warnings) {
				Dictionary warn = warning;
				ERR_CONTINUE(!warn.has("start_line"));
				ERR_CONTINUE(!warn.has("end_line"));
				ERR_CONTINUE(!warn.has("leftmost_column"));
				ERR_CONTINUE(!warn.has("rightmost_column"));
				ERR_CONTINUE(!warn.has("code"));
				ERR_CONTINUE(!warn.has("string_code"));
				ERR_CONTINUE(!warn.has("message"));

				Warning swarn;
				swarn.start_line = warn["start_line"];
				swarn.end_line = warn["end_line"];
				swarn.leftmost_column = warn["leftmost_column"];
				swarn.rightmost_column = warn["rightmost_column"];
				swarn.code = warn["code"];
				swarn.string_code = warn["string_code"];
				swarn.message = warn["message"];

				r_warnings->push_back(swarn);
			}
		}
		if (r_safe_lines != nullptr && ret.has("safe_lines")) {
			PackedInt32Array safe_lines = ret["safe_lines"];
			for (int i = 0; i < safe_lines.size(); i++) {
				r_safe_lines->insert(safe_lines[i]);
			}
		}
		return ret["valid"];
	}

	EXBIND1RC(String, validate_path, const String &)
	GDVIRTUAL0RC_REQUIRED(Object *, _create_script)
	Script *create_script() const override {
		Object *ret = nullptr;
		GDVIRTUAL_CALL(_create_script, ret);
		return Object::cast_to<Script>(ret);
	}
#ifndef DISABLE_DEPRECATED
	EXBIND0RC(bool, has_named_classes)
#endif
	EXBIND0RC(bool, supports_builtin_mode)
	EXBIND0RC(bool, supports_documentation)
	EXBIND0RC(bool, can_inherit_from_file)

	EXBIND2RC(int, find_function, const String &, const String &)
	EXBIND3RC(String, make_function, const String &, const String &, const PackedStringArray &)
	EXBIND0RC(bool, can_make_function)
	EXBIND3R(Error, open_in_external_editor, const Ref<Script> &, int, int)
	EXBIND0R(bool, overrides_external_editor)

	GDVIRTUAL0RC(ScriptNameCasing, _preferred_file_name_casing);

	virtual ScriptNameCasing preferred_file_name_casing() const override {
		ScriptNameCasing ret;
		if (GDVIRTUAL_CALL(_preferred_file_name_casing, ret)) {
			return ret;
		}
		return ScriptNameCasing::SCRIPT_NAME_CASING_SNAKE_CASE;
	}

	GDVIRTUAL3RC_REQUIRED(Dictionary, _complete_code, const String &, const String &, Object *)

	virtual Error complete_code(const String &p_code, const String &p_path, Object *p_owner, List<CodeCompletionOption> *r_options, bool &r_force, String &r_call_hint) override {
		Dictionary ret;
		GDVIRTUAL_CALL(_complete_code, p_code, p_path, p_owner, ret);
		if (!ret.has("result")) {
			return ERR_UNAVAILABLE;
		}

		if (r_options != nullptr && ret.has("options")) {
			Array options = ret["options"];
			for (const Variant &var : options) {
				Dictionary op = var;
				CodeCompletionOption option;
				ERR_CONTINUE(!op.has("kind"));
				option.kind = CodeCompletionKind(int(op["kind"]));
				ERR_CONTINUE(!op.has("display"));
				option.display = op["display"];
				ERR_CONTINUE(!op.has("insert_text"));
				option.insert_text = op["insert_text"];
				ERR_CONTINUE(!op.has("font_color"));
				option.font_color = op["font_color"];
				ERR_CONTINUE(!op.has("icon"));
				option.icon = op["icon"];
				ERR_CONTINUE(!op.has("default_value"));
				option.default_value = op["default_value"];
				ERR_CONTINUE(!op.has("location"));
				option.location = op["location"];
				if (op.has("matches")) {
					PackedInt32Array matches = op["matches"];
					ERR_CONTINUE(matches.size() & 1);
					for (int j = 0; j < matches.size(); j += 2) {
						option.matches.push_back(Pair<int, int>(matches[j], matches[j + 1]));
					}
				}
				r_options->push_back(option);
			}
		}

		ERR_FAIL_COND_V(!ret.has("force"), ERR_UNAVAILABLE);
		r_force = ret["force"];
		ERR_FAIL_COND_V(!ret.has("call_hint"), ERR_UNAVAILABLE);
		r_call_hint = ret["call_hint"];
		ERR_FAIL_COND_V(!ret.has("result"), ERR_UNAVAILABLE);
		Error result = Error(int(ret["result"]));

		return result;
	}

	GDVIRTUAL4RC_REQUIRED(Dictionary, _lookup_code, const String &, const String &, const String &, Object *)

	virtual Error lookup_code(const String &p_code, const String &p_symbol, const String &p_path, Object *p_owner, LookupResult &r_result) override {
		Dictionary ret;
		GDVIRTUAL_CALL(_lookup_code, p_code, p_symbol, p_path, p_owner, ret);

		ERR_FAIL_COND_V(!ret.has("result"), ERR_UNAVAILABLE);
		const Error result = Error(int(ret["result"]));

		ERR_FAIL_COND_V(!ret.has("type"), ERR_UNAVAILABLE);
		r_result.type = LookupResultType(int(ret["type"]));

		r_result.class_name = ret.get("class_name", "");
		r_result.class_member = ret.get("class_member", "");

		r_result.description = ret.get("description", "");
		r_result.is_deprecated = ret.get("is_deprecated", false);
		r_result.deprecated_message = ret.get("deprecated_message", "");
		r_result.is_deprecated = ret.get("is_deprecated", false);
		r_result.experimental_message = ret.get("experimental_message", "");

		r_result.doc_type = ret.get("doc_type", "");
		r_result.enumeration = ret.get("enumeration", "");
		r_result.is_bitfield = ret.get("is_bitfield", false);

		r_result.value = ret.get("value", "");

		r_result.script = ret.get("script", Ref<Script>());
		r_result.script_path = ret.get("script_path", "");
		r_result.location = ret.get("location", -1);

		return result;
	}

	GDVIRTUAL3RC_REQUIRED(String, _auto_indent_code, const String &, int, int)
	virtual void auto_indent_code(String &p_code, int p_from_line, int p_to_line) const override {
		String ret;
		GDVIRTUAL_CALL(_auto_indent_code, p_code, p_from_line, p_to_line, ret);
		p_code = ret;
	}
	EXBIND2(add_global_constant, const StringName &, const Variant &)
	EXBIND2(add_named_global_constant, const StringName &, const Variant &)
	EXBIND1(remove_named_global_constant, const StringName &)

	/* MULTITHREAD FUNCTIONS */

	//some VMs need to be notified of thread creation/exiting to allocate a stack
	EXBIND0(thread_enter)
	EXBIND0(thread_exit)

	EXBIND0RC(String, debug_get_error)
	EXBIND0RC(int, debug_get_stack_level_count)
	EXBIND1RC(int, debug_get_stack_level_line, int)
	EXBIND1RC(String, debug_get_stack_level_function, int)
	EXBIND1RC(String, debug_get_stack_level_source, int)

	GDVIRTUAL3R_REQUIRED(Dictionary, _debug_get_stack_level_locals, int, int, int)
	virtual void debug_get_stack_level_locals(int p_level, List<String> *p_locals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override {
		Dictionary ret;
		GDVIRTUAL_CALL(_debug_get_stack_level_locals, p_level, p_max_subitems, p_max_depth, ret);
		if (ret.size() == 0) {
			return;
		}
		if (p_locals != nullptr && ret.has("locals")) {
			PackedStringArray strings = ret["locals"];
			for (int i = 0; i < strings.size(); i++) {
				p_locals->push_back(strings[i]);
			}
		}
		if (p_values != nullptr && ret.has("values")) {
			Array values = ret["values"];
			for (const Variant &value : values) {
				p_values->push_back(value);
			}
		}
	}
	GDVIRTUAL3R_REQUIRED(Dictionary, _debug_get_stack_level_members, int, int, int)
	virtual void debug_get_stack_level_members(int p_level, List<String> *p_members, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override {
		Dictionary ret;
		GDVIRTUAL_CALL(_debug_get_stack_level_members, p_level, p_max_subitems, p_max_depth, ret);
		if (ret.size() == 0) {
			return;
		}
		if (p_members != nullptr && ret.has("members")) {
			PackedStringArray strings = ret["members"];
			for (int i = 0; i < strings.size(); i++) {
				p_members->push_back(strings[i]);
			}
		}
		if (p_values != nullptr && ret.has("values")) {
			Array values = ret["values"];
			for (const Variant &value : values) {
				p_values->push_back(value);
			}
		}
	}
	GDVIRTUAL1R_REQUIRED(GDExtensionPtr<void>, _debug_get_stack_level_instance, int)

	virtual ScriptInstance *debug_get_stack_level_instance(int p_level) override {
		GDExtensionPtr<void> ret = nullptr;
		GDVIRTUAL_CALL(_debug_get_stack_level_instance, p_level, ret);
		return reinterpret_cast<ScriptInstance *>(ret.operator void *());
	}
	GDVIRTUAL2R_REQUIRED(Dictionary, _debug_get_globals, int, int)
	virtual void debug_get_globals(List<String> *p_globals, List<Variant> *p_values, int p_max_subitems = -1, int p_max_depth = -1) override {
		Dictionary ret;
		GDVIRTUAL_CALL(_debug_get_globals, p_max_subitems, p_max_depth, ret);
		if (ret.size() == 0) {
			return;
		}
		if (p_globals != nullptr && ret.has("globals")) {
			PackedStringArray strings = ret["globals"];
			for (int i = 0; i < strings.size(); i++) {
				p_globals->push_back(strings[i]);
			}
		}
		if (p_values != nullptr && ret.has("values")) {
			Array values = ret["values"];
			for (const Variant &value : values) {
				p_values->push_back(value);
			}
		}
	}

	EXBIND4R(String, debug_parse_stack_level_expression, int, const String &, int, int)

	GDVIRTUAL0R_REQUIRED(TypedArray<Dictionary>, _debug_get_current_stack_info)
	virtual Vector<StackInfo> debug_get_current_stack_info() override {
		TypedArray<Dictionary> ret;
		GDVIRTUAL_CALL(_debug_get_current_stack_info, ret);
		Vector<StackInfo> sret;
		for (const Variant &var : ret) {
			StackInfo si;
			Dictionary d = var;
			ERR_CONTINUE(!d.has("file"));
			ERR_CONTINUE(!d.has("func"));
			ERR_CONTINUE(!d.has("line"));
			si.file = d["file"];
			si.func = d["func"];
			si.line = d["line"];
			sret.push_back(si);
		}
		return sret;
	}

	EXBIND0(reload_all_scripts)
	EXBIND2(reload_scripts, const Array &, bool)
	EXBIND2(reload_tool_script, const Ref<Script> &, bool)
	/* LOADER FUNCTIONS */

	GDVIRTUAL0RC_REQUIRED(PackedStringArray, _get_recognized_extensions)

	virtual void get_recognized_extensions(List<String> *p_extensions) const override {
		PackedStringArray ret;
		GDVIRTUAL_CALL(_get_recognized_extensions, ret);
		for (int i = 0; i < ret.size(); i++) {
			p_extensions->push_back(ret[i]);
		}
	}

	GDVIRTUAL0RC_REQUIRED(TypedArray<Dictionary>, _get_public_functions)
	virtual void get_public_functions(List<MethodInfo> *p_functions) const override {
		TypedArray<Dictionary> ret;
		GDVIRTUAL_CALL(_get_public_functions, ret);
		for (const Variant &var : ret) {
			MethodInfo mi = MethodInfo::from_dict(var);
			p_functions->push_back(mi);
		}
	}
	GDVIRTUAL0RC_REQUIRED(Dictionary, _get_public_constants)
	virtual void get_public_constants(List<Pair<String, Variant>> *p_constants) const override {
		Dictionary ret;
		GDVIRTUAL_CALL(_get_public_constants, ret);
		for (int i = 0; i < ret.size(); i++) {
			Dictionary d = ret[i];
			ERR_CONTINUE(!d.has("name"));
			ERR_CONTINUE(!d.has("value"));
			p_constants->push_back(Pair<String, Variant>(d["name"], d["value"]));
		}
	}
	GDVIRTUAL0RC_REQUIRED(TypedArray<Dictionary>, _get_public_annotations)
	virtual void get_public_annotations(List<MethodInfo> *p_annotations) const override {
		TypedArray<Dictionary> ret;
		GDVIRTUAL_CALL(_get_public_annotations, ret);
		for (const Variant &var : ret) {
			MethodInfo mi = MethodInfo::from_dict(var);
			p_annotations->push_back(mi);
		}
	}

	EXBIND0(profiling_start)
	EXBIND0(profiling_stop)
	EXBIND1(profiling_set_save_native_calls, bool)

	GDVIRTUAL2R_REQUIRED(int, _profiling_get_accumulated_data, GDExtensionPtr<ScriptLanguageExtensionProfilingInfo>, int)

	virtual int profiling_get_accumulated_data(ProfilingInfo *p_info_arr, int p_info_max) override {
		int ret = 0;
		GDVIRTUAL_CALL(_profiling_get_accumulated_data, p_info_arr, p_info_max, ret);
		return ret;
	}

	GDVIRTUAL2R_REQUIRED(int, _profiling_get_frame_data, GDExtensionPtr<ScriptLanguageExtensionProfilingInfo>, int)

	virtual int profiling_get_frame_data(ProfilingInfo *p_info_arr, int p_info_max) override {
		int ret = 0;
		GDVIRTUAL_CALL(_profiling_get_frame_data, p_info_arr, p_info_max, ret);
		return ret;
	}

	EXBIND0(frame)

	EXBIND1RC(bool, handles_global_class_type, const String &)

	GDVIRTUAL1RC_REQUIRED(Dictionary, _get_global_class_name, const String &)

	virtual String get_global_class_name(const String &p_path, String *r_base_type = nullptr, String *r_icon_path = nullptr, bool *r_is_abstract = nullptr, bool *r_is_tool = nullptr) const override {
		Dictionary ret;
		GDVIRTUAL_CALL(_get_global_class_name, p_path, ret);
		if (!ret.has("name")) {
			return String();
		}
		if (r_base_type != nullptr && ret.has("base_type")) {
			*r_base_type = ret["base_type"];
		}
		if (r_icon_path != nullptr && ret.has("icon_path")) {
			*r_icon_path = ret["icon_path"];
		}
		if (r_is_abstract != nullptr && ret.has("is_abstract")) {
			*r_is_abstract = ret["is_abstract"];
		}
		if (r_is_tool != nullptr && ret.has("is_tool")) {
			*r_is_tool = ret["is_tool"];
		}
		return ret["name"];
	}
};

VARIANT_ENUM_CAST(ScriptLanguageExtension::LookupResultType)
VARIANT_ENUM_CAST(ScriptLanguageExtension::CodeCompletionKind)
VARIANT_ENUM_CAST(ScriptLanguageExtension::CodeCompletionLocation)

class ScriptInstanceExtension : public ScriptInstance {
public:
	const GDExtensionScriptInstanceInfo3 *native_info;

#ifndef DISABLE_DEPRECATED
	bool free_native_info = false;
	struct DeprecatedNativeInfo {
		GDExtensionScriptInstanceNotification notification_func = nullptr;
		GDExtensionScriptInstanceFreePropertyList free_property_list_func = nullptr;
		GDExtensionScriptInstanceFreeMethodList free_method_list_func = nullptr;
	};
	DeprecatedNativeInfo *deprecated_native_info = nullptr;
#endif // DISABLE_DEPRECATED

	GDExtensionScriptInstanceDataPtr instance = nullptr;

// There should not be warnings on explicit casts.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#endif

	virtual bool set(const StringName &p_name, const Variant &p_value) override {
		if (native_info->set_func) {
			return native_info->set_func(instance, (GDExtensionConstStringNamePtr)&p_name, (GDExtensionConstVariantPtr)&p_value);
		}
		return false;
	}
	virtual bool get(const StringName &p_name, Variant &r_ret) const override {
		if (native_info->get_func) {
			return native_info->get_func(instance, (GDExtensionConstStringNamePtr)&p_name, (GDExtensionVariantPtr)&r_ret);
		}
		return false;
	}
	virtual void get_property_list(List<PropertyInfo> *p_list) const override {
		if (native_info->get_property_list_func) {
			uint32_t pcount;
			const GDExtensionPropertyInfo *pinfo = native_info->get_property_list_func(instance, &pcount);

#ifdef TOOLS_ENABLED
			if (pcount > 0) {
				if (native_info->get_class_category_func) {
					GDExtensionPropertyInfo gdext_class_category;
					if (native_info->get_class_category_func(instance, &gdext_class_category)) {
						p_list->push_back(PropertyInfo(gdext_class_category));
					}
				} else {
					Ref<Script> script = get_script();
					if (script.is_valid()) {
						p_list->push_back(script->get_class_category());
					}
				}
			}
#endif // TOOLS_ENABLED

			for (uint32_t i = 0; i < pcount; i++) {
				p_list->push_back(PropertyInfo(pinfo[i]));
			}
			if (native_info->free_property_list_func) {
				native_info->free_property_list_func(instance, pinfo, pcount);
#ifndef DISABLE_DEPRECATED
			} else if (deprecated_native_info && deprecated_native_info->free_property_list_func) {
				deprecated_native_info->free_property_list_func(instance, pinfo);
#endif // DISABLE_DEPRECATED
			}
		}
	}
	virtual Variant::Type get_property_type(const StringName &p_name, bool *r_is_valid = nullptr) const override {
		if (native_info->get_property_type_func) {
			GDExtensionBool is_valid = 0;
			GDExtensionVariantType type = native_info->get_property_type_func(instance, (GDExtensionConstStringNamePtr)&p_name, &is_valid);
			if (r_is_valid) {
				*r_is_valid = is_valid != 0;
			}
			return Variant::Type(type);
		}
		return Variant::NIL;
	}
	virtual void validate_property(PropertyInfo &p_property) const override {
		if (native_info->validate_property_func) {
			// GDExtension uses a StringName rather than a String for property name.
			StringName prop_name = p_property.name;
			GDExtensionPropertyInfo gdext_prop = {
				(GDExtensionVariantType)p_property.type,
				&prop_name,
				&p_property.class_name,
				(uint32_t)p_property.hint,
				&p_property.hint_string,
				p_property.usage,
			};
			if (native_info->validate_property_func(instance, &gdext_prop)) {
				p_property.type = (Variant::Type)gdext_prop.type;
				p_property.name = *reinterpret_cast<StringName *>(gdext_prop.name);
				p_property.class_name = *reinterpret_cast<StringName *>(gdext_prop.class_name);
				p_property.hint = (PropertyHint)gdext_prop.hint;
				p_property.hint_string = *reinterpret_cast<String *>(gdext_prop.hint_string);
				p_property.usage = gdext_prop.usage;
			}
		}
	}

	virtual bool property_can_revert(const StringName &p_name) const override {
		if (native_info->property_can_revert_func) {
			return native_info->property_can_revert_func(instance, (GDExtensionConstStringNamePtr)&p_name);
		}
		return false;
	}
	virtual bool property_get_revert(const StringName &p_name, Variant &r_ret) const override {
		if (native_info->property_get_revert_func) {
			return native_info->property_get_revert_func(instance, (GDExtensionConstStringNamePtr)&p_name, (GDExtensionVariantPtr)&r_ret);
		}
		return false;
	}

	virtual Object *get_owner() override {
		if (native_info->get_owner_func) {
			return (Object *)native_info->get_owner_func(instance);
		}
		return nullptr;
	}
	static void _add_property_with_state(GDExtensionConstStringNamePtr p_name, GDExtensionConstVariantPtr p_value, void *p_userdata) {
		List<Pair<StringName, Variant>> *state = (List<Pair<StringName, Variant>> *)p_userdata;
		state->push_back(Pair<StringName, Variant>(*(const StringName *)p_name, *(const Variant *)p_value));
	}
	virtual void get_property_state(List<Pair<StringName, Variant>> &state) override {
		if (native_info->get_property_state_func) {
			native_info->get_property_state_func(instance, _add_property_with_state, &state);
		}
	}

	virtual void get_method_list(List<MethodInfo> *p_list) const override {
		if (native_info->get_method_list_func) {
			uint32_t mcount;
			const GDExtensionMethodInfo *minfo = native_info->get_method_list_func(instance, &mcount);
			for (uint32_t i = 0; i < mcount; i++) {
				p_list->push_back(MethodInfo(minfo[i]));
			}
			if (native_info->free_method_list_func) {
				native_info->free_method_list_func(instance, minfo, mcount);
#ifndef DISABLE_DEPRECATED
			} else if (deprecated_native_info && deprecated_native_info->free_method_list_func) {
				deprecated_native_info->free_method_list_func(instance, minfo);
#endif // DISABLE_DEPRECATED
			}
		}
	}
	virtual bool has_method(const StringName &p_method) const override {
		if (native_info->has_method_func) {
			return native_info->has_method_func(instance, (GDExtensionStringNamePtr)&p_method);
		}
		return false;
	}

	virtual int get_method_argument_count(const StringName &p_method, bool *r_is_valid = nullptr) const override {
		if (native_info->get_method_argument_count_func) {
			GDExtensionBool is_valid = 0;
			GDExtensionInt ret = native_info->get_method_argument_count_func(instance, (GDExtensionStringNamePtr)&p_method, &is_valid);
			if (r_is_valid) {
				*r_is_valid = is_valid != 0;
			}
			return ret;
		}
		// Fallback to default.
		return ScriptInstance::get_method_argument_count(p_method, r_is_valid);
	}

	virtual Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override {
		Variant ret;
		if (native_info->call_func) {
			GDExtensionCallError ce;
			native_info->call_func(instance, (GDExtensionConstStringNamePtr)&p_method, (GDExtensionConstVariantPtr *)p_args, p_argcount, (GDExtensionVariantPtr)&ret, &ce);
			r_error.error = Callable::CallError::Error(ce.error);
			r_error.argument = ce.argument;
			r_error.expected = ce.expected;
		}
		return ret;
	}

	virtual void notification(int p_notification, bool p_reversed = false) override {
		if (native_info->notification_func) {
			native_info->notification_func(instance, p_notification, p_reversed);
#ifndef DISABLE_DEPRECATED
		} else if (deprecated_native_info && deprecated_native_info->notification_func) {
			deprecated_native_info->notification_func(instance, p_notification);
#endif // DISABLE_DEPRECATED
		}
	}

	virtual String to_string(bool *r_valid) override {
		if (native_info->to_string_func) {
			GDExtensionBool valid;
			String ret;
			native_info->to_string_func(instance, &valid, reinterpret_cast<GDExtensionStringPtr>(&ret));
			if (r_valid) {
				*r_valid = valid != 0;
			}
			return ret;
		}
		return String();
	}

	virtual void refcount_incremented() override {
		if (native_info->refcount_incremented_func) {
			native_info->refcount_incremented_func(instance);
		}
	}
	virtual bool refcount_decremented() override {
		if (native_info->refcount_decremented_func) {
			return native_info->refcount_decremented_func(instance);
		}
		return false;
	}

	virtual Ref<Script> get_script() const override {
		if (native_info->get_script_func) {
			GDExtensionObjectPtr script = native_info->get_script_func(instance);
			return Ref<Script>(reinterpret_cast<Script *>(script));
		}
		return Ref<Script>();
	}

	virtual bool is_placeholder() const override {
		if (native_info->is_placeholder_func) {
			return native_info->is_placeholder_func(instance);
		}
		return false;
	}

	virtual void property_set_fallback(const StringName &p_name, const Variant &p_value, bool *r_valid) override {
		if (native_info->set_fallback_func) {
			bool ret = native_info->set_fallback_func(instance, (GDExtensionConstStringNamePtr)&p_name, (GDExtensionConstVariantPtr)&p_value);
			if (r_valid) {
				*r_valid = ret;
			}
		}
	}
	virtual Variant property_get_fallback(const StringName &p_name, bool *r_valid) override {
		Variant ret;
		if (native_info->get_fallback_func) {
			bool valid = native_info->get_fallback_func(instance, (GDExtensionConstStringNamePtr)&p_name, (GDExtensionVariantPtr)&ret);
			if (r_valid) {
				*r_valid = valid;
			}
		}
		return ret;
	}

	virtual ScriptLanguage *get_language() override {
		if (native_info->get_language_func) {
			GDExtensionScriptLanguagePtr lang = native_info->get_language_func(instance);
			return reinterpret_cast<ScriptLanguage *>(lang);
		}
		return nullptr;
	}
	virtual ~ScriptInstanceExtension() {
		if (native_info->free_func) {
			native_info->free_func(instance);
		}
#ifndef DISABLE_DEPRECATED
		if (free_native_info) {
			memfree(const_cast<GDExtensionScriptInstanceInfo3 *>(native_info));
		}
		if (deprecated_native_info) {
			memfree(deprecated_native_info);
		}
#endif // DISABLE_DEPRECATED
	}

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
};
