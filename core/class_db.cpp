/**************************************************************************/
/*  class_db.cpp                                                          */
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

#include "class_db.h"

#include "core/engine.h"
#include "core/os/mutex.h"
#include "core/version.h"

#define OBJTYPE_RLOCK RWLockRead _rw_lockr_(lock);
#define OBJTYPE_WLOCK RWLockWrite _rw_lockw_(lock);

#ifdef DEBUG_METHODS_ENABLED

MethodDefinition D_METHOD(const char *p_name) {
	MethodDefinition md;
	md.name = StaticCString::create(p_name);
	return md;
}

MethodDefinition D_METHOD(const char *p_name, const char *p_arg1) {
	MethodDefinition md;
	md.name = StaticCString::create(p_name);
	md.args.push_back(StaticCString::create(p_arg1));
	return md;
}

MethodDefinition D_METHOD(const char *p_name, const char *p_arg1, const char *p_arg2) {
	MethodDefinition md;
	md.name = StaticCString::create(p_name);
	md.args.resize(2);
	md.args.write[0] = StaticCString::create(p_arg1);
	md.args.write[1] = StaticCString::create(p_arg2);
	return md;
}

MethodDefinition D_METHOD(const char *p_name, const char *p_arg1, const char *p_arg2, const char *p_arg3) {
	MethodDefinition md;
	md.name = StaticCString::create(p_name);
	md.args.resize(3);
	md.args.write[0] = StaticCString::create(p_arg1);
	md.args.write[1] = StaticCString::create(p_arg2);
	md.args.write[2] = StaticCString::create(p_arg3);
	return md;
}

MethodDefinition D_METHOD(const char *p_name, const char *p_arg1, const char *p_arg2, const char *p_arg3, const char *p_arg4) {
	MethodDefinition md;
	md.name = StaticCString::create(p_name);
	md.args.resize(4);
	md.args.write[0] = StaticCString::create(p_arg1);
	md.args.write[1] = StaticCString::create(p_arg2);
	md.args.write[2] = StaticCString::create(p_arg3);
	md.args.write[3] = StaticCString::create(p_arg4);
	return md;
}

MethodDefinition D_METHOD(const char *p_name, const char *p_arg1, const char *p_arg2, const char *p_arg3, const char *p_arg4, const char *p_arg5) {
	MethodDefinition md;
	md.name = StaticCString::create(p_name);
	md.args.resize(5);
	md.args.write[0] = StaticCString::create(p_arg1);
	md.args.write[1] = StaticCString::create(p_arg2);
	md.args.write[2] = StaticCString::create(p_arg3);
	md.args.write[3] = StaticCString::create(p_arg4);
	md.args.write[4] = StaticCString::create(p_arg5);
	return md;
}

MethodDefinition D_METHOD(const char *p_name, const char *p_arg1, const char *p_arg2, const char *p_arg3, const char *p_arg4, const char *p_arg5, const char *p_arg6) {
	MethodDefinition md;
	md.name = StaticCString::create(p_name);
	md.args.resize(6);
	md.args.write[0] = StaticCString::create(p_arg1);
	md.args.write[1] = StaticCString::create(p_arg2);
	md.args.write[2] = StaticCString::create(p_arg3);
	md.args.write[3] = StaticCString::create(p_arg4);
	md.args.write[4] = StaticCString::create(p_arg5);
	md.args.write[5] = StaticCString::create(p_arg6);
	return md;
}

MethodDefinition D_METHOD(const char *p_name, const char *p_arg1, const char *p_arg2, const char *p_arg3, const char *p_arg4, const char *p_arg5, const char *p_arg6, const char *p_arg7) {
	MethodDefinition md;
	md.name = StaticCString::create(p_name);
	md.args.resize(7);
	md.args.write[0] = StaticCString::create(p_arg1);
	md.args.write[1] = StaticCString::create(p_arg2);
	md.args.write[2] = StaticCString::create(p_arg3);
	md.args.write[3] = StaticCString::create(p_arg4);
	md.args.write[4] = StaticCString::create(p_arg5);
	md.args.write[5] = StaticCString::create(p_arg6);
	md.args.write[6] = StaticCString::create(p_arg7);
	return md;
}

MethodDefinition D_METHOD(const char *p_name, const char *p_arg1, const char *p_arg2, const char *p_arg3, const char *p_arg4, const char *p_arg5, const char *p_arg6, const char *p_arg7, const char *p_arg8) {
	MethodDefinition md;
	md.name = StaticCString::create(p_name);
	md.args.resize(8);
	md.args.write[0] = StaticCString::create(p_arg1);
	md.args.write[1] = StaticCString::create(p_arg2);
	md.args.write[2] = StaticCString::create(p_arg3);
	md.args.write[3] = StaticCString::create(p_arg4);
	md.args.write[4] = StaticCString::create(p_arg5);
	md.args.write[5] = StaticCString::create(p_arg6);
	md.args.write[6] = StaticCString::create(p_arg7);
	md.args.write[7] = StaticCString::create(p_arg8);
	return md;
}

MethodDefinition D_METHOD(const char *p_name, const char *p_arg1, const char *p_arg2, const char *p_arg3, const char *p_arg4, const char *p_arg5, const char *p_arg6, const char *p_arg7, const char *p_arg8, const char *p_arg9) {
	MethodDefinition md;
	md.name = StaticCString::create(p_name);
	md.args.resize(9);
	md.args.write[0] = StaticCString::create(p_arg1);
	md.args.write[1] = StaticCString::create(p_arg2);
	md.args.write[2] = StaticCString::create(p_arg3);
	md.args.write[3] = StaticCString::create(p_arg4);
	md.args.write[4] = StaticCString::create(p_arg5);
	md.args.write[5] = StaticCString::create(p_arg6);
	md.args.write[6] = StaticCString::create(p_arg7);
	md.args.write[7] = StaticCString::create(p_arg8);
	md.args.write[8] = StaticCString::create(p_arg9);
	return md;
}

MethodDefinition D_METHOD(const char *p_name, const char *p_arg1, const char *p_arg2, const char *p_arg3, const char *p_arg4, const char *p_arg5, const char *p_arg6, const char *p_arg7, const char *p_arg8, const char *p_arg9, const char *p_arg10) {
	MethodDefinition md;
	md.name = StaticCString::create(p_name);
	md.args.resize(10);
	md.args.write[0] = StaticCString::create(p_arg1);
	md.args.write[1] = StaticCString::create(p_arg2);
	md.args.write[2] = StaticCString::create(p_arg3);
	md.args.write[3] = StaticCString::create(p_arg4);
	md.args.write[4] = StaticCString::create(p_arg5);
	md.args.write[5] = StaticCString::create(p_arg6);
	md.args.write[6] = StaticCString::create(p_arg7);
	md.args.write[7] = StaticCString::create(p_arg8);
	md.args.write[8] = StaticCString::create(p_arg9);
	md.args.write[9] = StaticCString::create(p_arg10);
	return md;
}

MethodDefinition D_METHOD(const char *p_name, const char *p_arg1, const char *p_arg2, const char *p_arg3, const char *p_arg4, const char *p_arg5, const char *p_arg6, const char *p_arg7, const char *p_arg8, const char *p_arg9, const char *p_arg10, const char *p_arg11) {
	MethodDefinition md;
	md.name = StaticCString::create(p_name);
	md.args.resize(11);
	md.args.write[0] = StaticCString::create(p_arg1);
	md.args.write[1] = StaticCString::create(p_arg2);
	md.args.write[2] = StaticCString::create(p_arg3);
	md.args.write[3] = StaticCString::create(p_arg4);
	md.args.write[4] = StaticCString::create(p_arg5);
	md.args.write[5] = StaticCString::create(p_arg6);
	md.args.write[6] = StaticCString::create(p_arg7);
	md.args.write[7] = StaticCString::create(p_arg8);
	md.args.write[8] = StaticCString::create(p_arg9);
	md.args.write[9] = StaticCString::create(p_arg10);
	md.args.write[10] = StaticCString::create(p_arg11);
	return md;
}

MethodDefinition D_METHOD(const char *p_name, const char *p_arg1, const char *p_arg2, const char *p_arg3, const char *p_arg4, const char *p_arg5, const char *p_arg6, const char *p_arg7, const char *p_arg8, const char *p_arg9, const char *p_arg10, const char *p_arg11, const char *p_arg12) {
	MethodDefinition md;
	md.name = StaticCString::create(p_name);
	md.args.resize(12);
	md.args.write[0] = StaticCString::create(p_arg1);
	md.args.write[1] = StaticCString::create(p_arg2);
	md.args.write[2] = StaticCString::create(p_arg3);
	md.args.write[3] = StaticCString::create(p_arg4);
	md.args.write[4] = StaticCString::create(p_arg5);
	md.args.write[5] = StaticCString::create(p_arg6);
	md.args.write[6] = StaticCString::create(p_arg7);
	md.args.write[7] = StaticCString::create(p_arg8);
	md.args.write[8] = StaticCString::create(p_arg9);
	md.args.write[9] = StaticCString::create(p_arg10);
	md.args.write[10] = StaticCString::create(p_arg11);
	md.args.write[11] = StaticCString::create(p_arg12);
	return md;
}

MethodDefinition D_METHOD(const char *p_name, const char *p_arg1, const char *p_arg2, const char *p_arg3, const char *p_arg4, const char *p_arg5, const char *p_arg6, const char *p_arg7, const char *p_arg8, const char *p_arg9, const char *p_arg10, const char *p_arg11, const char *p_arg12, const char *p_arg13) {
	MethodDefinition md;
	md.name = StaticCString::create(p_name);
	md.args.resize(13);
	md.args.write[0] = StaticCString::create(p_arg1);
	md.args.write[1] = StaticCString::create(p_arg2);
	md.args.write[2] = StaticCString::create(p_arg3);
	md.args.write[3] = StaticCString::create(p_arg4);
	md.args.write[4] = StaticCString::create(p_arg5);
	md.args.write[5] = StaticCString::create(p_arg6);
	md.args.write[6] = StaticCString::create(p_arg7);
	md.args.write[7] = StaticCString::create(p_arg8);
	md.args.write[8] = StaticCString::create(p_arg9);
	md.args.write[9] = StaticCString::create(p_arg10);
	md.args.write[10] = StaticCString::create(p_arg11);
	md.args.write[11] = StaticCString::create(p_arg12);
	md.args.write[12] = StaticCString::create(p_arg13);
	return md;
}

#endif

ClassDB::APIType ClassDB::current_api = API_CORE;

void ClassDB::set_current_api(APIType p_api) {
	current_api = p_api;
}

ClassDB::APIType ClassDB::get_current_api() {
	return current_api;
}

HashMap<StringName, ClassDB::ClassInfo> ClassDB::classes;
HashMap<StringName, StringName> ClassDB::resource_base_extensions;
HashMap<StringName, StringName> ClassDB::compat_classes;

ClassDB::ClassInfo::ClassInfo() {
	api = API_NONE;
	creation_func = nullptr;
	inherits_ptr = nullptr;
	disabled = false;
	exposed = false;
	class_ptr = nullptr;
}

ClassDB::ClassInfo::~ClassInfo() {
}

bool ClassDB::_is_parent_class(const StringName &p_class, const StringName &p_inherits) {
	StringName inherits = p_class;

	while (inherits.operator String().length()) {
		if (inherits == p_inherits) {
			return true;
		}
		inherits = _get_parent_class(inherits);
	}

	return false;
}

bool ClassDB::is_parent_class(const StringName &p_class, const StringName &p_inherits) {
	OBJTYPE_RLOCK;

	return _is_parent_class(p_class, p_inherits);
}

void ClassDB::get_class_list(List<StringName> *p_classes) {
	OBJTYPE_RLOCK;

	const StringName *k = nullptr;

	while ((k = classes.next(k))) {
		p_classes->push_back(*k);
	}

	p_classes->sort();
}

void ClassDB::get_inheriters_from_class(const StringName &p_class, List<StringName> *p_classes) {
	OBJTYPE_RLOCK;

	const StringName *k = nullptr;

	while ((k = classes.next(k))) {
		if (*k != p_class && _is_parent_class(*k, p_class)) {
			p_classes->push_back(*k);
		}
	}
}

void ClassDB::get_direct_inheriters_from_class(const StringName &p_class, List<StringName> *p_classes) {
	OBJTYPE_RLOCK;

	const StringName *k = nullptr;

	while ((k = classes.next(k))) {
		if (*k != p_class && _get_parent_class(*k) == p_class) {
			p_classes->push_back(*k);
		}
	}
}

StringName ClassDB::get_parent_class_nocheck(const StringName &p_class) {
	OBJTYPE_RLOCK;

	ClassInfo *ti = classes.getptr(p_class);
	if (!ti) {
		return StringName();
	}
	return ti->inherits;
}

StringName ClassDB::_get_parent_class(const StringName &p_class) {
	ClassInfo *ti = classes.getptr(p_class);
	ERR_FAIL_COND_V_MSG(!ti, StringName(), "Cannot get class '" + String(p_class) + "'.");
	return ti->inherits;
}

StringName ClassDB::get_parent_class(const StringName &p_class) {
	OBJTYPE_RLOCK;

	return _get_parent_class(p_class);
}

ClassDB::APIType ClassDB::get_api_type(const StringName &p_class) {
	OBJTYPE_RLOCK;

	ClassInfo *ti = classes.getptr(p_class);

	ERR_FAIL_COND_V_MSG(!ti, API_NONE, "Cannot get class '" + String(p_class) + "'.");
	return ti->api;
}

uint64_t ClassDB::get_api_hash(APIType p_api) {
	OBJTYPE_RLOCK;
#ifdef DEBUG_METHODS_ENABLED

	uint64_t hash = hash_djb2_one_64(HashMapHasherDefault::hash(VERSION_FULL_CONFIG));

	List<StringName> names;

	const StringName *k = nullptr;

	while ((k = classes.next(k))) {
		names.push_back(*k);
	}
	//must be alphabetically sorted for hash to compute
	names.sort_custom<StringName::AlphCompare>();

	for (List<StringName>::Element *E = names.front(); E; E = E->next()) {
		ClassInfo *t = classes.getptr(E->get());
		ERR_FAIL_COND_V_MSG(!t, 0, "Cannot get class '" + String(E->get()) + "'.");
		if (t->api != p_api || !t->exposed) {
			continue;
		}
		hash = hash_djb2_one_64(t->name.hash(), hash);
		hash = hash_djb2_one_64(t->inherits.hash(), hash);

		{ //methods

			List<StringName> snames;

			k = nullptr;

			while ((k = t->method_map.next(k))) {
				String name = k->operator String();

				ERR_CONTINUE(name.empty());

				if (name[0] == '_') {
					continue; // Ignore non-virtual methods that start with an underscore
				}

				snames.push_back(*k);
			}

			snames.sort_custom<StringName::AlphCompare>();

			for (List<StringName>::Element *F = snames.front(); F; F = F->next()) {
				MethodBind *mb = t->method_map[F->get()];
				hash = hash_djb2_one_64(mb->get_name().hash(), hash);
				hash = hash_djb2_one_64(mb->get_argument_count(), hash);
				hash = hash_djb2_one_64(mb->get_argument_type(-1), hash); //return

				for (int i = 0; i < mb->get_argument_count(); i++) {
					const PropertyInfo info = mb->get_argument_info(i);
					hash = hash_djb2_one_64(info.type, hash);
					hash = hash_djb2_one_64(info.name.hash(), hash);
					hash = hash_djb2_one_64(info.hint, hash);
					hash = hash_djb2_one_64(info.hint_string.hash(), hash);
				}

				hash = hash_djb2_one_64(mb->get_default_argument_count(), hash);

				for (int i = 0; i < mb->get_default_argument_count(); i++) {
					//hash should not change, i hope for tis
					Variant da = mb->get_default_argument(i);
					hash = hash_djb2_one_64(da.hash(), hash);
				}

				hash = hash_djb2_one_64(mb->get_hint_flags(), hash);
			}
		}

		{ //constants

			List<StringName> snames;

			k = nullptr;

			while ((k = t->constant_map.next(k))) {
				snames.push_back(*k);
			}

			snames.sort_custom<StringName::AlphCompare>();

			for (List<StringName>::Element *F = snames.front(); F; F = F->next()) {
				hash = hash_djb2_one_64(F->get().hash(), hash);
				hash = hash_djb2_one_64(t->constant_map[F->get()], hash);
			}
		}

		{ //signals

			List<StringName> snames;

			k = nullptr;

			while ((k = t->signal_map.next(k))) {
				snames.push_back(*k);
			}

			snames.sort_custom<StringName::AlphCompare>();

			for (List<StringName>::Element *F = snames.front(); F; F = F->next()) {
				MethodInfo &mi = t->signal_map[F->get()];
				hash = hash_djb2_one_64(F->get().hash(), hash);
				for (int i = 0; i < mi.arguments.size(); i++) {
					hash = hash_djb2_one_64(mi.arguments[i].type, hash);
				}
			}
		}

		{ //properties

			List<StringName> snames;

			k = nullptr;

			while ((k = t->property_setget.next(k))) {
				snames.push_back(*k);
			}

			snames.sort_custom<StringName::AlphCompare>();

			for (List<StringName>::Element *F = snames.front(); F; F = F->next()) {
				PropertySetGet *psg = t->property_setget.getptr(F->get());
				ERR_FAIL_COND_V(!psg, 0);

				hash = hash_djb2_one_64(F->get().hash(), hash);
				hash = hash_djb2_one_64(psg->setter.hash(), hash);
				hash = hash_djb2_one_64(psg->getter.hash(), hash);
			}
		}

		//property list
		for (List<PropertyInfo>::Element *F = t->property_list.front(); F; F = F->next()) {
			hash = hash_djb2_one_64(F->get().name.hash(), hash);
			hash = hash_djb2_one_64(F->get().type, hash);
			hash = hash_djb2_one_64(F->get().hint, hash);
			hash = hash_djb2_one_64(F->get().hint_string.hash(), hash);
			hash = hash_djb2_one_64(F->get().usage, hash);
		}
	}

	return hash;
#else
	return 0;
#endif
}

bool ClassDB::class_exists(const StringName &p_class) {
	OBJTYPE_RLOCK;
	return classes.has(p_class);
}

void ClassDB::add_compatibility_class(const StringName &p_class, const StringName &p_fallback) {
	OBJTYPE_WLOCK;
	compat_classes[p_class] = p_fallback;
}

Object *ClassDB::instance(const StringName &p_class) {
	ClassInfo *ti;
	{
		OBJTYPE_RLOCK;
		ti = classes.getptr(p_class);
		if (!ti || ti->disabled || !ti->creation_func) {
			if (compat_classes.has(p_class)) {
				ti = classes.getptr(compat_classes[p_class]);
			}
		}
		ERR_FAIL_COND_V_MSG(!ti, nullptr, "Cannot get class '" + String(p_class) + "'.");
		ERR_FAIL_COND_V_MSG(ti->disabled, nullptr, "Class '" + String(p_class) + "' is disabled.");
		ERR_FAIL_COND_V_MSG(!ti->creation_func, nullptr, "Class '" + String(p_class) + "' or its base class cannot be instantiated.");
	}
#ifdef TOOLS_ENABLED
	if (ti->api == API_EDITOR && !Engine::get_singleton()->is_editor_hint()) {
		ERR_PRINT("Class '" + String(p_class) + "' can only be instantiated by editor.");
		return nullptr;
	}
#endif
	return ti->creation_func();
}
bool ClassDB::can_instance(const StringName &p_class) {
	OBJTYPE_RLOCK;

	ClassInfo *ti = classes.getptr(p_class);
	ERR_FAIL_COND_V_MSG(!ti, false, "Cannot get class '" + String(p_class) + "'.");
#ifdef TOOLS_ENABLED
	if (ti->api == API_EDITOR && !Engine::get_singleton()->is_editor_hint()) {
		return false;
	}
#endif
	return (!ti->disabled && ti->creation_func != nullptr);
}
bool ClassDB::is_virtual_node(const StringName &p_class) {
	OBJTYPE_RLOCK;

	ClassInfo *ti = classes.getptr(p_class);
	ERR_FAIL_COND_V_MSG(!ti, false, "Cannot get class '" + String(p_class) + "'.");
	return (ti->virtual_node);
}

void ClassDB::_add_class2(const StringName &p_class, const StringName &p_inherits) {
	OBJTYPE_WLOCK;

	const StringName &name = p_class;

	ERR_FAIL_COND_MSG(classes.has(name), "Class '" + String(p_class) + "' already exists.");

	classes[name] = ClassInfo();
	ClassInfo &ti = classes[name];
	ti.name = name;
	ti.inherits = p_inherits;
	ti.api = current_api;

	if (ti.inherits) {
		ERR_FAIL_COND(!classes.has(ti.inherits)); //it MUST be registered.
		ti.inherits_ptr = &classes[ti.inherits];

	} else {
		ti.inherits_ptr = nullptr;
	}
}

static MethodInfo info_from_bind(MethodBind *p_method) {
	MethodInfo minfo;
	minfo.name = p_method->get_name();
	minfo.id = p_method->get_method_id();

	for (int i = 0; i < p_method->get_argument_count(); i++) {
		minfo.arguments.push_back(p_method->get_argument_info(i));
	}

	minfo.return_val = p_method->get_return_info();
	minfo.flags = p_method->get_hint_flags();

	for (int i = 0; i < p_method->get_argument_count(); i++) {
		if (p_method->has_default_argument(i)) {
			minfo.default_arguments.push_back(p_method->get_default_argument(i));
		}
	}

	return minfo;
}

void ClassDB::get_method_list(StringName p_class, List<MethodInfo> *p_methods, bool p_no_inheritance, bool p_exclude_from_properties) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);

	while (type) {
		if (type->disabled) {
			if (p_no_inheritance) {
				break;
			}

			type = type->inherits_ptr;
			continue;
		}

#ifdef DEBUG_METHODS_ENABLED

		for (List<MethodInfo>::Element *E = type->virtual_methods.front(); E; E = E->next()) {
			p_methods->push_back(E->get());
		}

		for (List<StringName>::Element *E = type->method_order.front(); E; E = E->next()) {
			if (p_exclude_from_properties && type->methods_in_properties.has(E->get())) {
				continue;
			}

			MethodBind *method = type->method_map.get(E->get());
			MethodInfo minfo = info_from_bind(method);

			p_methods->push_back(minfo);
		}

#else

		const StringName *K = nullptr;

		while ((K = type->method_map.next(K))) {
			MethodBind *m = type->method_map[*K];
			MethodInfo minfo = info_from_bind(m);
			p_methods->push_back(minfo);
		}

#endif

		if (p_no_inheritance) {
			break;
		}

		type = type->inherits_ptr;
	}
}

MethodBind *ClassDB::get_method(StringName p_class, StringName p_name) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);

	while (type) {
		MethodBind **method = type->method_map.getptr(p_name);
		if (method && *method) {
			return *method;
		}
		type = type->inherits_ptr;
	}
	return nullptr;
}

void ClassDB::bind_integer_constant(const StringName &p_class, const StringName &p_enum, const StringName &p_name, int p_constant) {
	OBJTYPE_WLOCK;

	ClassInfo *type = classes.getptr(p_class);

	ERR_FAIL_COND(!type);

	if (type->constant_map.has(p_name)) {
		ERR_FAIL();
	}

	type->constant_map[p_name] = p_constant;

	String enum_name = p_enum;
	if (enum_name != String()) {
		if (enum_name.find(".") != -1) {
			enum_name = enum_name.get_slicec('.', 1);
		}

		List<StringName> *constants_list = type->enum_map.getptr(enum_name);

		if (constants_list) {
			constants_list->push_back(p_name);
		} else {
			List<StringName> new_list;
			new_list.push_back(p_name);
			type->enum_map[enum_name] = new_list;
		}
	}

#ifdef DEBUG_METHODS_ENABLED
	type->constant_order.push_back(p_name);
#endif
}

void ClassDB::get_integer_constant_list(const StringName &p_class, List<String> *p_constants, bool p_no_inheritance) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);

	while (type) {
#ifdef DEBUG_METHODS_ENABLED
		for (List<StringName>::Element *E = type->constant_order.front(); E; E = E->next()) {
			p_constants->push_back(E->get());
		}
#else
		const StringName *K = NULL;

		while ((K = type->constant_map.next(K))) {
			p_constants->push_back(*K);
		}

#endif
		if (p_no_inheritance) {
			break;
		}

		type = type->inherits_ptr;
	}
}

int ClassDB::get_integer_constant(const StringName &p_class, const StringName &p_name, bool *p_success) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);

	while (type) {
		int *constant = type->constant_map.getptr(p_name);
		if (constant) {
			if (p_success) {
				*p_success = true;
			}
			return *constant;
		}

		type = type->inherits_ptr;
	}

	if (p_success) {
		*p_success = false;
	}

	return 0;
}

StringName ClassDB::get_integer_constant_enum(const StringName &p_class, const StringName &p_name, bool p_no_inheritance) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);

	while (type) {
		const StringName *k = nullptr;
		while ((k = type->enum_map.next(k))) {
			List<StringName> &constants_list = type->enum_map.get(*k);
			const List<StringName>::Element *found = constants_list.find(p_name);
			if (found) {
				return *k;
			}
		}

		if (p_no_inheritance) {
			break;
		}

		type = type->inherits_ptr;
	}

	return StringName();
}

void ClassDB::get_enum_list(const StringName &p_class, List<StringName> *p_enums, bool p_no_inheritance) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);

	while (type) {
		const StringName *k = nullptr;
		while ((k = type->enum_map.next(k))) {
			p_enums->push_back(*k);
		}

		if (p_no_inheritance) {
			break;
		}

		type = type->inherits_ptr;
	}
}

void ClassDB::get_enum_constants(const StringName &p_class, const StringName &p_enum, List<StringName> *p_constants, bool p_no_inheritance) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);

	while (type) {
		const List<StringName> *constants = type->enum_map.getptr(p_enum);

		if (constants) {
			for (const List<StringName>::Element *E = constants->front(); E; E = E->next()) {
				p_constants->push_back(E->get());
			}
		}

		if (p_no_inheritance) {
			break;
		}

		type = type->inherits_ptr;
	}
}

bool ClassDB::has_enum(const StringName &p_class, const StringName &p_name, bool p_no_inheritance) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);

	while (type) {
		if (type->enum_map.has(p_name)) {
			return true;
		}
		if (p_no_inheritance) {
			return false;
		}
		type = type->inherits_ptr;
	}

	return false;
}

void ClassDB::add_signal(StringName p_class, const MethodInfo &p_signal) {
	OBJTYPE_WLOCK;

	ClassInfo *type = classes.getptr(p_class);
	ERR_FAIL_COND(!type);

	StringName sname = p_signal.name;

#ifdef DEBUG_METHODS_ENABLED
	ClassInfo *check = type;
	while (check) {
		ERR_FAIL_COND_MSG(check->signal_map.has(sname), "Class '" + String(p_class) + "' already has signal '" + String(sname) + "'.");
		check = check->inherits_ptr;
	}
#endif

	type->signal_map[sname] = p_signal;
}

void ClassDB::get_signal_list(StringName p_class, List<MethodInfo> *p_signals, bool p_no_inheritance) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);
	ERR_FAIL_COND(!type);

	ClassInfo *check = type;

	while (check) {
		const StringName *S = nullptr;
		while ((S = check->signal_map.next(S))) {
			p_signals->push_back(check->signal_map[*S]);
		}

		if (p_no_inheritance) {
			return;
		}

		check = check->inherits_ptr;
	}
}

bool ClassDB::has_signal(StringName p_class, StringName p_signal) {
	OBJTYPE_RLOCK;
	ClassInfo *type = classes.getptr(p_class);
	ClassInfo *check = type;
	while (check) {
		if (check->signal_map.has(p_signal)) {
			return true;
		}
		check = check->inherits_ptr;
	}

	return false;
}

bool ClassDB::get_signal(StringName p_class, StringName p_signal, MethodInfo *r_signal) {
	OBJTYPE_RLOCK;
	ClassInfo *type = classes.getptr(p_class);
	ClassInfo *check = type;
	while (check) {
		if (check->signal_map.has(p_signal)) {
			if (r_signal) {
				*r_signal = check->signal_map[p_signal];
			}
			return true;
		}
		check = check->inherits_ptr;
	}

	return false;
}

void ClassDB::add_property_group(StringName p_class, const String &p_name, const String &p_prefix) {
	OBJTYPE_WLOCK;
	ClassInfo *type = classes.getptr(p_class);
	ERR_FAIL_COND(!type);

	type->property_list.push_back(PropertyInfo(Variant::NIL, p_name, PROPERTY_HINT_NONE, p_prefix, PROPERTY_USAGE_GROUP));
}

void ClassDB::add_property(StringName p_class, const PropertyInfo &p_pinfo, const StringName &p_setter, const StringName &p_getter, int p_index) {
	lock.read_lock();
	ClassInfo *type = classes.getptr(p_class);
	lock.read_unlock();

	ERR_FAIL_COND(!type);

	MethodBind *mb_set = nullptr;
	if (p_setter) {
		mb_set = get_method(p_class, p_setter);
#ifdef DEBUG_METHODS_ENABLED

		ERR_FAIL_COND_MSG(!mb_set, "Invalid setter '" + p_class + "::" + p_setter + "' for property '" + p_pinfo.name + "'.");

		int exp_args = 1 + (p_index >= 0 ? 1 : 0);
		ERR_FAIL_COND_MSG(mb_set->get_argument_count() != exp_args, "Invalid function for setter '" + p_class + "::" + p_setter + " for property '" + p_pinfo.name + "'.");
#endif
	}

	MethodBind *mb_get = nullptr;
	if (p_getter) {
		mb_get = get_method(p_class, p_getter);
#ifdef DEBUG_METHODS_ENABLED

		ERR_FAIL_COND_MSG(!mb_get, "Invalid getter '" + p_class + "::" + p_getter + "' for property '" + p_pinfo.name + "'.");

		int exp_args = 0 + (p_index >= 0 ? 1 : 0);
		ERR_FAIL_COND_MSG(mb_get->get_argument_count() != exp_args, "Invalid function for getter '" + p_class + "::" + p_getter + "' for property: '" + p_pinfo.name + "'.");
#endif
	}

#ifdef DEBUG_METHODS_ENABLED
	ERR_FAIL_COND_MSG(type->property_setget.has(p_pinfo.name), "Object '" + p_class + "' already has property '" + p_pinfo.name + "'.");
#endif

	OBJTYPE_WLOCK

	type->property_list.push_back(p_pinfo);
#ifdef DEBUG_METHODS_ENABLED
	if (mb_get) {
		type->methods_in_properties.insert(p_getter);
	}
	if (mb_set) {
		type->methods_in_properties.insert(p_setter);
	}
#endif
	PropertySetGet psg;
	psg.setter = p_setter;
	psg.getter = p_getter;
	psg._setptr = mb_set;
	psg._getptr = mb_get;
	psg.index = p_index;
	psg.type = p_pinfo.type;

	type->property_setget[p_pinfo.name] = psg;
}

void ClassDB::set_property_default_value(StringName p_class, const StringName &p_name, const Variant &p_default) {
	if (!default_values.has(p_class)) {
		default_values[p_class] = HashMap<StringName, Variant>();
	}
	default_values[p_class][p_name] = p_default;
}

void ClassDB::get_property_list(StringName p_class, List<PropertyInfo> *p_list, bool p_no_inheritance, const Object *p_validator) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);
	ClassInfo *check = type;
	while (check) {
		for (List<PropertyInfo>::Element *E = check->property_list.front(); E; E = E->next()) {
			if (p_validator) {
				PropertyInfo pi = E->get();
				p_validator->_validate_property(pi);
				p_list->push_back(pi);
			} else {
				p_list->push_back(E->get());
			}
		}

		if (p_no_inheritance) {
			return;
		}
		check = check->inherits_ptr;
	}
}
bool ClassDB::set_property(Object *p_object, const StringName &p_property, const Variant &p_value, bool *r_valid) {
	ERR_FAIL_NULL_V(p_object, false);

	ClassInfo *type = classes.getptr(p_object->get_class_name());
	ClassInfo *check = type;
	while (check) {
		const PropertySetGet *psg = check->property_setget.getptr(p_property);
		if (psg) {
			if (!psg->setter) {
				if (r_valid) {
					*r_valid = false;
				}
				return true; //return true but do nothing
			}

			Variant::CallError ce;

			if (psg->index >= 0) {
				Variant index = psg->index;
				const Variant *arg[2] = { &index, &p_value };
				//p_object->call(psg->setter,arg,2,ce);
				if (psg->_setptr) {
					psg->_setptr->call(p_object, arg, 2, ce);
				} else {
					p_object->call(psg->setter, arg, 2, ce);
				}

			} else {
				const Variant *arg[1] = { &p_value };
				if (psg->_setptr) {
					psg->_setptr->call(p_object, arg, 1, ce);
				} else {
					p_object->call(psg->setter, arg, 1, ce);
				}
			}

			if (r_valid) {
				*r_valid = ce.error == Variant::CallError::CALL_OK;
			}

			return true;
		}

		check = check->inherits_ptr;
	}

	return false;
}
bool ClassDB::get_property(Object *p_object, const StringName &p_property, Variant &r_value) {
	ERR_FAIL_NULL_V(p_object, false);

	ClassInfo *type = classes.getptr(p_object->get_class_name());
	ClassInfo *check = type;
	while (check) {
		const PropertySetGet *psg = check->property_setget.getptr(p_property);
		if (psg) {
			if (!psg->getter) {
				return true; //return true but do nothing
			}

			if (psg->index >= 0) {
				Variant index = psg->index;
				const Variant *arg[1] = { &index };
				Variant::CallError ce;
				r_value = p_object->call(psg->getter, arg, 1, ce);

			} else {
				Variant::CallError ce;
				if (psg->_getptr) {
					r_value = psg->_getptr->call(p_object, nullptr, 0, ce);
				} else {
					r_value = p_object->call(psg->getter, nullptr, 0, ce);
				}
			}
			return true;
		}

		const int *c = check->constant_map.getptr(p_property);
		if (c) {
			r_value = *c;
			return true;
		}

		check = check->inherits_ptr;
	}

	return false;
}

int ClassDB::get_property_index(const StringName &p_class, const StringName &p_property, bool *r_is_valid) {
	ClassInfo *type = classes.getptr(p_class);
	ClassInfo *check = type;
	while (check) {
		const PropertySetGet *psg = check->property_setget.getptr(p_property);
		if (psg) {
			if (r_is_valid) {
				*r_is_valid = true;
			}

			return psg->index;
		}

		check = check->inherits_ptr;
	}
	if (r_is_valid) {
		*r_is_valid = false;
	}

	return -1;
}

Variant::Type ClassDB::get_property_type(const StringName &p_class, const StringName &p_property, bool *r_is_valid) {
	ClassInfo *type = classes.getptr(p_class);
	ClassInfo *check = type;
	while (check) {
		const PropertySetGet *psg = check->property_setget.getptr(p_property);
		if (psg) {
			if (r_is_valid) {
				*r_is_valid = true;
			}

			return psg->type;
		}

		check = check->inherits_ptr;
	}
	if (r_is_valid) {
		*r_is_valid = false;
	}

	return Variant::NIL;
}

StringName ClassDB::get_property_setter(StringName p_class, const StringName &p_property) {
	ClassInfo *type = classes.getptr(p_class);
	ClassInfo *check = type;
	while (check) {
		const PropertySetGet *psg = check->property_setget.getptr(p_property);
		if (psg) {
			return psg->setter;
		}

		check = check->inherits_ptr;
	}

	return StringName();
}

StringName ClassDB::get_property_getter(StringName p_class, const StringName &p_property) {
	ClassInfo *type = classes.getptr(p_class);
	ClassInfo *check = type;
	while (check) {
		const PropertySetGet *psg = check->property_setget.getptr(p_property);
		if (psg) {
			return psg->getter;
		}

		check = check->inherits_ptr;
	}

	return StringName();
}

bool ClassDB::has_property(const StringName &p_class, const StringName &p_property, bool p_no_inheritance) {
	ClassInfo *type = classes.getptr(p_class);
	ClassInfo *check = type;
	while (check) {
		if (check->property_setget.has(p_property)) {
			return true;
		}

		if (p_no_inheritance) {
			break;
		}
		check = check->inherits_ptr;
	}

	return false;
}

void ClassDB::set_method_flags(StringName p_class, StringName p_method, int p_flags) {
	OBJTYPE_WLOCK;
	ClassInfo *type = classes.getptr(p_class);
	ClassInfo *check = type;
	ERR_FAIL_COND(!check);
	ERR_FAIL_COND(!check->method_map.has(p_method));
	check->method_map[p_method]->set_hint_flags(p_flags);
}

bool ClassDB::has_method(StringName p_class, StringName p_method, bool p_no_inheritance) {
	ClassInfo *type = classes.getptr(p_class);
	ClassInfo *check = type;
	while (check) {
		if (check->method_map.has(p_method)) {
			return true;
		}
		if (p_no_inheritance) {
			return false;
		}
		check = check->inherits_ptr;
	}

	return false;
}

#ifdef DEBUG_METHODS_ENABLED
MethodBind *ClassDB::bind_methodfi(uint32_t p_flags, MethodBind *p_bind, const MethodDefinition &method_name, const Variant **p_defs, int p_defcount) {
	StringName mdname = method_name.name;
#else
MethodBind *ClassDB::bind_methodfi(uint32_t p_flags, MethodBind *p_bind, const char *method_name, const Variant **p_defs, int p_defcount) {
	StringName mdname = StaticCString::create(method_name);
#endif

	OBJTYPE_WLOCK;
	ERR_FAIL_COND_V(!p_bind, nullptr);
	p_bind->set_name(mdname);

	String instance_type = p_bind->get_instance_class();

#ifdef DEBUG_ENABLED

	ERR_FAIL_COND_V_MSG(has_method(instance_type, mdname), nullptr, "Class " + String(instance_type) + " already has a method " + String(mdname) + ".");
#endif

	ClassInfo *type = classes.getptr(instance_type);
	if (!type) {
		memdelete(p_bind);
		ERR_FAIL_V_MSG(nullptr, "Couldn't bind method '" + mdname + "' for instance '" + instance_type + "'.");
	}

	if (type->method_map.has(mdname)) {
		memdelete(p_bind);
		// overloading not supported
		ERR_FAIL_V_MSG(nullptr, "Method already bound '" + instance_type + "::" + mdname + "'.");
	}

#ifdef DEBUG_METHODS_ENABLED

	if (method_name.args.size() > p_bind->get_argument_count()) {
		memdelete(p_bind);
		ERR_FAIL_V_MSG(nullptr, "Method definition provides more arguments than the method actually has '" + instance_type + "::" + mdname + "'.");
	}

	p_bind->set_argument_names(method_name.args);

	type->method_order.push_back(mdname);
#endif

	type->method_map[mdname] = p_bind;

	Vector<Variant> defvals;

	defvals.resize(p_defcount);
	for (int i = 0; i < p_defcount; i++) {
		defvals.write[i] = *p_defs[p_defcount - i - 1];
	}

	p_bind->set_default_arguments(defvals);
	p_bind->set_hint_flags(p_flags);
	return p_bind;
}

void ClassDB::add_virtual_method(const StringName &p_class, const MethodInfo &p_method, bool p_virtual) {
	ERR_FAIL_COND_MSG(!classes.has(p_class), "Request for nonexistent class '" + p_class + "'.");

	OBJTYPE_WLOCK;

#ifdef DEBUG_METHODS_ENABLED
	MethodInfo mi = p_method;
	if (p_virtual) {
		mi.flags |= METHOD_FLAG_VIRTUAL;
	}
	classes[p_class].virtual_methods.push_back(mi);

#endif
}

void ClassDB::get_virtual_methods(const StringName &p_class, List<MethodInfo> *p_methods, bool p_no_inheritance) {
	ERR_FAIL_COND_MSG(!classes.has(p_class), "Request for nonexistent class '" + p_class + "'.");

#ifdef DEBUG_METHODS_ENABLED

	ClassInfo *type = classes.getptr(p_class);
	ClassInfo *check = type;
	while (check) {
		for (List<MethodInfo>::Element *E = check->virtual_methods.front(); E; E = E->next()) {
			p_methods->push_back(E->get());
		}

		if (p_no_inheritance) {
			return;
		}
		check = check->inherits_ptr;
	}

#endif
}

void ClassDB::set_class_enabled(StringName p_class, bool p_enable) {
	OBJTYPE_WLOCK;

	ERR_FAIL_COND_MSG(!classes.has(p_class), "Request for nonexistent class '" + p_class + "'.");
	classes[p_class].disabled = !p_enable;
}

bool ClassDB::is_class_enabled(StringName p_class) {
	OBJTYPE_RLOCK;

	ClassInfo *ti = classes.getptr(p_class);
	if (!ti || !ti->creation_func) {
		if (compat_classes.has(p_class)) {
			ti = classes.getptr(compat_classes[p_class]);
		}
	}

	ERR_FAIL_COND_V_MSG(!ti, false, "Cannot get class '" + String(p_class) + "'.");
	return !ti->disabled;
}

bool ClassDB::is_class_exposed(StringName p_class) {
	OBJTYPE_RLOCK;

	ClassInfo *ti = classes.getptr(p_class);
	ERR_FAIL_COND_V_MSG(!ti, false, "Cannot get class '" + String(p_class) + "'.");
	return ti->exposed;
}

StringName ClassDB::get_category(const StringName &p_node) {
	ERR_FAIL_COND_V(!classes.has(p_node), StringName());
#ifdef DEBUG_ENABLED
	return classes[p_node].category;
#else
	return StringName();
#endif
}

void ClassDB::add_resource_base_extension(const StringName &p_extension, const StringName &p_class) {
	if (resource_base_extensions.has(p_extension)) {
		return;
	}

	resource_base_extensions[p_extension] = p_class;
}

void ClassDB::get_resource_base_extensions(List<String> *p_extensions) {
	const StringName *K = nullptr;

	while ((K = resource_base_extensions.next(K))) {
		p_extensions->push_back(*K);
	}
}

void ClassDB::get_extensions_for_type(const StringName &p_class, List<String> *p_extensions) {
	const StringName *K = nullptr;

	while ((K = resource_base_extensions.next(K))) {
		StringName cmp = resource_base_extensions[*K];
		if (is_parent_class(p_class, cmp) || is_parent_class(cmp, p_class)) {
			p_extensions->push_back(*K);
		}
	}
}

HashMap<StringName, HashMap<StringName, Variant>> ClassDB::default_values;
Set<StringName> ClassDB::default_values_cached;

Variant ClassDB::class_get_default_property_value(const StringName &p_class, const StringName &p_property, bool *r_valid) {
	if (!default_values_cached.has(p_class)) {
		if (!default_values.has(p_class)) {
			default_values[p_class] = HashMap<StringName, Variant>();
		}

		Object *c = nullptr;
		bool cleanup_c = false;

		if (Engine::get_singleton()->has_singleton(p_class)) {
			c = Engine::get_singleton()->get_singleton_object(p_class);
			cleanup_c = false;
		} else if (ClassDB::can_instance(p_class)) {
			c = ClassDB::instance(p_class);
			cleanup_c = true;
		}

		if (c) {
			List<PropertyInfo> plist;
			c->get_property_list(&plist);
			for (List<PropertyInfo>::Element *E = plist.front(); E; E = E->next()) {
				if (E->get().usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR)) {
					if (!default_values[p_class].has(E->get().name)) {
						Variant v = c->get(E->get().name);
						default_values[p_class][E->get().name] = v;
					}
				}
			}

			if (cleanup_c) {
				memdelete(c);
			}
		}

		default_values_cached.insert(p_class);
	}

	if (!default_values.has(p_class)) {
		if (r_valid != nullptr) {
			*r_valid = false;
		}
		return Variant();
	}

	if (!default_values[p_class].has(p_property)) {
		if (r_valid != nullptr) {
			*r_valid = false;
		}
		return Variant();
	}

	if (r_valid != nullptr) {
		*r_valid = true;
	}
	return default_values[p_class][p_property];
}

RWLock ClassDB::lock;

void ClassDB::cleanup_defaults() {
	default_values.clear();
	default_values_cached.clear();
}

void ClassDB::cleanup() {
	//OBJTYPE_LOCK; hah not here

	const StringName *k = nullptr;

	while ((k = classes.next(k))) {
		ClassInfo &ti = classes[*k];

		const StringName *m = nullptr;
		while ((m = ti.method_map.next(m))) {
			memdelete(ti.method_map[*m]);
		}
	}
	classes.clear();
	resource_base_extensions.clear();
	compat_classes.clear();
}

//
