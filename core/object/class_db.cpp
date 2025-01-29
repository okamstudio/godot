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

#include "core/config/engine.h"
#include "core/io/resource_loader.h"
#include "core/object/script_language.h"
#include "core/version.h"

#define OBJTYPE_RLOCK RWLockRead _rw_lockr_(lock);
#define OBJTYPE_WLOCK RWLockWrite _rw_lockw_(lock);

#ifdef DEBUG_METHODS_ENABLED

MethodDefinition D_METHODP(const char *p_name, const char *const **p_args, uint32_t p_argcount) {
	MethodDefinition md;
	md.name = StaticCString::create(p_name);
	md.args.resize(p_argcount);
	for (uint32_t i = 0; i < p_argcount; i++) {
		md.args.write[i] = StaticCString::create(*p_args[i]);
	}
	return md;
}

#endif

ClassDB::APIType ClassDB::current_api = API_CORE;
HashMap<ClassDB::APIType, uint32_t> ClassDB::api_hashes_cache;

void ClassDB::set_current_api(APIType p_api) {
	DEV_ASSERT(!api_hashes_cache.has(p_api)); // This API type may not be suitable for caching of hash if it can change later.
	current_api = p_api;
}

ClassDB::APIType ClassDB::get_current_api() {
	return current_api;
}

HashMap<StringName, ClassDB::ClassInfo> ClassDB::classes;
HashMap<StringName, StringName> ClassDB::resource_base_extensions;
HashMap<StringName, StringName> ClassDB::compat_classes;

#ifdef TOOLS_ENABLED
HashMap<StringName, ObjectGDExtension> ClassDB::placeholder_extensions;

class PlaceholderExtensionInstance {
	StringName class_name;
	HashMap<StringName, Variant> properties;

	// Checks if a property is from a runtime class, and not a non-runtime base class.
	bool is_runtime_property(const StringName &p_property_name) {
		StringName current_class_name = class_name;

		while (ClassDB::is_class_runtime(current_class_name)) {
			if (ClassDB::has_property(current_class_name, p_property_name, true)) {
				return true;
			}

			current_class_name = ClassDB::get_parent_class(current_class_name);
		}

		return false;
	}

public:
	PlaceholderExtensionInstance(const StringName &p_class_name) {
		class_name = p_class_name;
	}

	~PlaceholderExtensionInstance() {}

	void set(const StringName &p_name, const Variant &p_value, bool &r_valid) {
		r_valid = is_runtime_property(p_name);
		if (r_valid) {
			properties[p_name] = p_value;
		}
	}

	Variant get(const StringName &p_name, bool &r_valid) {
		const Variant *value = properties.getptr(p_name);
		Variant ret;

		if (value) {
			ret = *value;
			r_valid = true;
		} else {
			r_valid = is_runtime_property(p_name);
			if (r_valid) {
				ret = ClassDB::class_get_default_property_value(class_name, p_name);
			}
		}

		return ret;
	}

	static GDExtensionBool placeholder_instance_set(GDExtensionClassInstancePtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionConstVariantPtr p_value) {
		PlaceholderExtensionInstance *self = (PlaceholderExtensionInstance *)p_instance;
		const StringName &name = *(StringName *)p_name;
		const Variant &value = *(const Variant *)p_value;

		bool valid = false;
		self->set(name, value, valid);

		return valid;
	}

	static GDExtensionBool placeholder_instance_get(GDExtensionClassInstancePtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionVariantPtr r_ret) {
		PlaceholderExtensionInstance *self = (PlaceholderExtensionInstance *)p_instance;
		const StringName &name = *(StringName *)p_name;
		Variant *value = (Variant *)r_ret;

		bool valid = false;
		*value = self->get(name, valid);

		return valid;
	}

	static const GDExtensionPropertyInfo *placeholder_instance_get_property_list(GDExtensionClassInstancePtr p_instance, uint32_t *r_count) {
		*r_count = 0;
		return nullptr;
	}

	static void placeholder_instance_free_property_list(GDExtensionClassInstancePtr p_instance, const GDExtensionPropertyInfo *p_list, uint32_t p_count) {
	}

	static GDExtensionBool placeholder_instance_property_can_revert(GDExtensionClassInstancePtr p_instance, GDExtensionConstStringNamePtr p_name) {
		return false;
	}

	static GDExtensionBool placeholder_instance_property_get_revert(GDExtensionClassInstancePtr p_instance, GDExtensionConstStringNamePtr p_name, GDExtensionVariantPtr r_ret) {
		return false;
	}

	static GDExtensionBool placeholder_instance_validate_property(GDExtensionClassInstancePtr p_instance, GDExtensionPropertyInfo *p_property) {
		return false;
	}

	static void placeholder_instance_notification(GDExtensionClassInstancePtr p_instance, int32_t p_what, GDExtensionBool p_reversed) {
	}

	static void placeholder_instance_to_string(GDExtensionClassInstancePtr p_instance, GDExtensionBool *r_is_valid, GDExtensionStringPtr p_out) {
		*r_is_valid = true;
	}

	static void placeholder_instance_reference(GDExtensionClassInstancePtr p_instance) {
	}

	static void placeholder_instance_unreference(GDExtensionClassInstancePtr p_instance) {
	}

	static uint64_t placeholder_instance_get_rid(GDExtensionClassInstancePtr p_instance) {
		return 0;
	}

	static GDExtensionObjectPtr placeholder_class_create_instance(void *p_class_userdata, GDExtensionBool p_notify_postinitialize) {
		ClassDB::ClassInfo *ti = (ClassDB::ClassInfo *)p_class_userdata;

		// Find the closest native parent, that isn't a runtime class.
		ClassDB::ClassInfo *native_parent = ti->inherits_ptr;
		while (native_parent->gdextension || native_parent->is_runtime) {
			native_parent = native_parent->inherits_ptr;
		}
		ERR_FAIL_NULL_V(native_parent->creation_func, nullptr);

		// Construct a placeholder.
		Object *obj = native_parent->creation_func(static_cast<bool>(p_notify_postinitialize));

		// ClassDB::set_object_extension_instance() won't be called for placeholders.
		// We need need to make sure that all the things it would have done (even if
		// done in a different way to support placeholders) will also be done here.

		obj->_extension = ClassDB::get_placeholder_extension(ti->name);
		obj->_extension_instance = memnew(PlaceholderExtensionInstance(ti->name));

#ifdef TOOLS_ENABLED
		if (obj->_extension->track_instance) {
			obj->_extension->track_instance(obj->_extension->tracking_userdata, obj);
		}
#endif

		return obj;
	}

	static GDExtensionObjectPtr placeholder_class_recreate_instance(void *p_class_userdata, GDExtensionObjectPtr p_object) {
		ClassDB::ClassInfo *ti = (ClassDB::ClassInfo *)p_class_userdata;
		return memnew(PlaceholderExtensionInstance(ti->name));
	}

	static void placeholder_class_free_instance(void *p_class_userdata, GDExtensionClassInstancePtr p_instance) {
		PlaceholderExtensionInstance *instance = (PlaceholderExtensionInstance *)p_instance;
		memdelete(instance);
	}

	static GDExtensionClassCallVirtual placeholder_class_get_virtual(void *p_class_userdata, GDExtensionConstStringNamePtr p_name, uint32_t p_hash) {
		return nullptr;
	}
};
#endif

bool ClassDB::_is_parent_class(const StringName &p_class, const StringName &p_inherits) {
	ClassInfo *c = classes.getptr(p_class);
	while (c) {
		if (c->name == p_inherits) {
			return true;
		}
		c = c->inherits_ptr;
	}

	return false;
}

bool ClassDB::is_parent_class(const StringName &p_class, const StringName &p_inherits) {
	OBJTYPE_RLOCK;

	return _is_parent_class(p_class, p_inherits);
}

void ClassDB::get_class_list(List<StringName> *p_classes) {
	OBJTYPE_RLOCK;

	for (const KeyValue<StringName, ClassInfo> &E : classes) {
		p_classes->push_back(E.key);
	}

	p_classes->sort_custom<StringName::AlphCompare>();
}

#ifdef TOOLS_ENABLED
void ClassDB::get_extensions_class_list(List<StringName> *p_classes) {
	OBJTYPE_RLOCK;

	for (const KeyValue<StringName, ClassInfo> &E : classes) {
		if (E.value.api != API_EXTENSION && E.value.api != API_EDITOR_EXTENSION) {
			continue;
		}
		p_classes->push_back(E.key);
	}

	p_classes->sort_custom<StringName::AlphCompare>();
}

void ClassDB::get_extension_class_list(const Ref<GDExtension> &p_extension, List<StringName> *p_classes) {
	OBJTYPE_RLOCK;

	for (const KeyValue<StringName, ClassInfo> &E : classes) {
		if (E.value.api != API_EXTENSION && E.value.api != API_EDITOR_EXTENSION) {
			continue;
		}
		if (!E.value.gdextension || E.value.gdextension->library != p_extension.ptr()) {
			continue;
		}
		p_classes->push_back(E.key);
	}

	p_classes->sort_custom<StringName::AlphCompare>();
}
#endif

void ClassDB::get_inheriters_from_class(const StringName &p_class, List<StringName> *p_classes) {
	OBJTYPE_RLOCK;

	for (const KeyValue<StringName, ClassInfo> &E : classes) {
		if (E.key != p_class && _is_parent_class(E.key, p_class)) {
			p_classes->push_back(E.key);
		}
	}
}

void ClassDB::get_direct_inheriters_from_class(const StringName &p_class, List<StringName> *p_classes) {
	OBJTYPE_RLOCK;

	for (const KeyValue<StringName, ClassInfo> &E : classes) {
		if (E.key != p_class && _get_parent_class(E.key) == p_class) {
			p_classes->push_back(E.key);
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

bool ClassDB::get_inheritance_chain_nocheck(const StringName &p_class, Vector<StringName> &r_result) {
	OBJTYPE_RLOCK;

	ClassInfo *start = classes.getptr(p_class);
	if (!start) {
		return false;
	}

	int classes_to_add = 0;
	for (ClassInfo *ti = start; ti; ti = ti->inherits_ptr) {
		classes_to_add++;
	}

	int64_t old_size = r_result.size();
	r_result.resize(old_size + classes_to_add);
	StringName *w = r_result.ptrw() + old_size;
	for (ClassInfo *ti = start; ti; ti = ti->inherits_ptr) {
		*w++ = ti->name;
	}

	return true;
}

StringName ClassDB::get_compatibility_remapped_class(const StringName &p_class) {
	if (classes.has(p_class)) {
		return p_class;
	}

	if (compat_classes.has(p_class)) {
		return compat_classes[p_class];
	}

	return p_class;
}

StringName ClassDB::_get_parent_class(const StringName &p_class) {
	ClassInfo *ti = classes.getptr(p_class);
	ERR_FAIL_NULL_V_MSG(ti, StringName(), vformat("Cannot get class '%s'.", String(p_class)));
	return ti->inherits;
}

StringName ClassDB::get_parent_class(const StringName &p_class) {
	OBJTYPE_RLOCK;

	return _get_parent_class(p_class);
}

ClassDB::APIType ClassDB::get_api_type(const StringName &p_class) {
	OBJTYPE_RLOCK;

	ClassInfo *ti = classes.getptr(p_class);

	ERR_FAIL_NULL_V_MSG(ti, API_NONE, vformat("Cannot get class '%s'.", String(p_class)));
	return ti->api;
}

uint32_t ClassDB::get_api_hash(APIType p_api) {
#ifdef DEBUG_METHODS_ENABLED
	OBJTYPE_WLOCK;

	if (api_hashes_cache.has(p_api)) {
		return api_hashes_cache[p_api];
	}

	uint64_t hash = hash_murmur3_one_64(HashMapHasherDefault::hash(VERSION_FULL_CONFIG));

	List<StringName> class_list;
	for (const KeyValue<StringName, ClassInfo> &E : classes) {
		class_list.push_back(E.key);
	}
	// Must be alphabetically sorted for hash to compute.
	class_list.sort_custom<StringName::AlphCompare>();

	for (const StringName &E : class_list) {
		ClassInfo *t = classes.getptr(E);
		ERR_FAIL_NULL_V_MSG(t, 0, vformat("Cannot get class '%s'.", String(E)));
		if (t->api != p_api || !t->exposed) {
			continue;
		}
		hash = hash_murmur3_one_64(t->name.hash(), hash);
		hash = hash_murmur3_one_64(t->inherits.hash(), hash);

		{ //methods

			List<StringName> snames;

			for (const KeyValue<StringName, MethodBind *> &F : t->method_map) {
				String name = F.key.operator String();

				ERR_CONTINUE(name.is_empty());

				if (name[0] == '_') {
					continue; // Ignore non-virtual methods that start with an underscore
				}

				snames.push_back(F.key);
			}

			snames.sort_custom<StringName::AlphCompare>();

			for (const StringName &F : snames) {
				MethodBind *mb = t->method_map[F];
				hash = hash_murmur3_one_64(mb->get_name().hash(), hash);
				hash = hash_murmur3_one_64(mb->get_argument_count(), hash);
				hash = hash_murmur3_one_64(mb->get_argument_type(-1), hash); //return

				for (int i = 0; i < mb->get_argument_count(); i++) {
					const PropertyInfo info = mb->get_argument_info(i);
					hash = hash_murmur3_one_64(info.type, hash);
					hash = hash_murmur3_one_64(info.name.hash(), hash);
					hash = hash_murmur3_one_64(info.hint, hash);
					hash = hash_murmur3_one_64(info.hint_string.hash(), hash);
				}

				hash = hash_murmur3_one_64(mb->get_default_argument_count(), hash);

				for (int i = 0; i < mb->get_argument_count(); i++) {
					if (mb->has_default_argument(i)) {
						Variant da = mb->get_default_argument(i);
						hash = hash_murmur3_one_64(da.hash(), hash);
					}
				}

				hash = hash_murmur3_one_64(mb->get_hint_flags(), hash);
			}
		}

		{ //constants

			List<StringName> snames;

			for (const KeyValue<StringName, int64_t> &F : t->constant_map) {
				snames.push_back(F.key);
			}

			snames.sort_custom<StringName::AlphCompare>();

			for (const StringName &F : snames) {
				hash = hash_murmur3_one_64(F.hash(), hash);
				hash = hash_murmur3_one_64(uint64_t(t->constant_map[F]), hash);
			}
		}

		{ //signals

			List<StringName> snames;

			for (const KeyValue<StringName, MethodInfo> &F : t->signal_map) {
				snames.push_back(F.key);
			}

			snames.sort_custom<StringName::AlphCompare>();

			for (const StringName &F : snames) {
				MethodInfo &mi = t->signal_map[F];
				hash = hash_murmur3_one_64(F.hash(), hash);
				for (const PropertyInfo &pi : mi.arguments) {
					hash = hash_murmur3_one_64(pi.type, hash);
				}
			}
		}

		{ //properties

			List<StringName> snames;

			for (const KeyValue<StringName, PropertySetGet> &F : t->property_setget) {
				snames.push_back(F.key);
			}

			snames.sort_custom<StringName::AlphCompare>();

			for (const StringName &F : snames) {
				PropertySetGet *psg = t->property_setget.getptr(F);
				ERR_FAIL_NULL_V(psg, 0);

				hash = hash_murmur3_one_64(F.hash(), hash);
				hash = hash_murmur3_one_64(psg->setter.hash(), hash);
				hash = hash_murmur3_one_64(psg->getter.hash(), hash);
			}
		}

		//property list
		for (const PropertyInfo &F : t->property_list) {
			hash = hash_murmur3_one_64(F.name.hash(), hash);
			hash = hash_murmur3_one_64(F.type, hash);
			hash = hash_murmur3_one_64(F.hint, hash);
			hash = hash_murmur3_one_64(F.hint_string.hash(), hash);
			hash = hash_murmur3_one_64(F.usage, hash);
		}
	}

	hash = hash_fmix32(hash);

	// Extension API changes at runtime; let's just not cache them by now.
	if (p_api != API_EXTENSION && p_api != API_EDITOR_EXTENSION) {
		api_hashes_cache[p_api] = hash;
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

StringName ClassDB::get_compatibility_class(const StringName &p_class) {
	if (compat_classes.has(p_class)) {
		return compat_classes[p_class];
	}
	return StringName();
}

Object *ClassDB::_instantiate_internal(const StringName &p_class, bool p_require_real_class, bool p_notify_postinitialize) {
	ClassInfo *ti;
	{
		OBJTYPE_RLOCK;
		ti = classes.getptr(p_class);
		if (!_can_instantiate(ti)) {
			if (compat_classes.has(p_class)) {
				ti = classes.getptr(compat_classes[p_class]);
			}
		}
		ERR_FAIL_NULL_V_MSG(ti, nullptr, vformat("Cannot get class '%s'.", String(p_class)));
		ERR_FAIL_COND_V_MSG(ti->disabled, nullptr, vformat("Class '%s' is disabled.", String(p_class)));
		ERR_FAIL_NULL_V_MSG(ti->creation_func, nullptr, vformat("Class '%s' or its base class cannot be instantiated.", String(p_class)));
	}

#ifdef TOOLS_ENABLED
	if ((ti->api == API_EDITOR || ti->api == API_EDITOR_EXTENSION) && !Engine::get_singleton()->is_editor_hint()) {
		ERR_PRINT(vformat("Class '%s' can only be instantiated by editor.", String(p_class)));
		return nullptr;
	}
#endif

#ifdef TOOLS_ENABLED
	// Try to create placeholder.
	if (!p_require_real_class && ti->is_runtime && Engine::get_singleton()->is_editor_hint()) {
		bool can_create_placeholder = false;
		if (ti->gdextension) {
			if (ti->gdextension->create_instance2) {
				can_create_placeholder = true;
			}
#ifndef DISABLE_DEPRECATED
			else if (ti->gdextension->create_instance) {
				can_create_placeholder = true;
			}
#endif // DISABLE_DEPRECATED
		} else if (!ti->inherits_ptr || !ti->inherits_ptr->creation_func) {
			ERR_PRINT(vformat("Cannot make a placeholder instance of runtime class %s because its parent cannot be constructed.", ti->name));
		} else {
			can_create_placeholder = true;
		}

		if (can_create_placeholder) {
			ObjectGDExtension *extension = get_placeholder_extension(ti->name);
			return (Object *)extension->create_instance2(extension->class_userdata, p_notify_postinitialize);
		}
	}
#endif // TOOLS_ENABLED

	if (ti->gdextension && ti->gdextension->create_instance2) {
		ObjectGDExtension *extension = ti->gdextension;
		return (Object *)extension->create_instance2(extension->class_userdata, p_notify_postinitialize);
	}
#ifndef DISABLE_DEPRECATED
	else if (ti->gdextension && ti->gdextension->create_instance) {
		ObjectGDExtension *extension = ti->gdextension;
		return (Object *)extension->create_instance(extension->class_userdata);
	}
#endif // DISABLE_DEPRECATED
	else {
		return ti->creation_func(p_notify_postinitialize);
	}
}

bool ClassDB::_can_instantiate(ClassInfo *p_class_info) {
	if (!p_class_info) {
		return false;
	}

	if (p_class_info->disabled || !p_class_info->creation_func) {
		return false;
	}

	if (!p_class_info->gdextension) {
		return true;
	}

	if (p_class_info->gdextension->create_instance2) {
		return true;
	}

#ifndef DISABLE_DEPRECATED
	if (p_class_info->gdextension->create_instance) {
		return true;
	}
#endif //  DISABLE_DEPRECATED
	return false;
}

Object *ClassDB::instantiate(const StringName &p_class) {
	return _instantiate_internal(p_class);
}

Object *ClassDB::instantiate_no_placeholders(const StringName &p_class) {
	return _instantiate_internal(p_class, true);
}

Object *ClassDB::instantiate_without_postinitialization(const StringName &p_class) {
	return _instantiate_internal(p_class, true, false);
}

#ifdef TOOLS_ENABLED
ObjectGDExtension *ClassDB::get_placeholder_extension(const StringName &p_class) {
	ObjectGDExtension *placeholder_extension = placeholder_extensions.getptr(p_class);
	if (placeholder_extension) {
		return placeholder_extension;
	}

	ClassInfo *ti;
	{
		OBJTYPE_RLOCK;
		ti = classes.getptr(p_class);
		if (!_can_instantiate(ti)) {
			if (compat_classes.has(p_class)) {
				ti = classes.getptr(compat_classes[p_class]);
			}
		}
		ERR_FAIL_NULL_V_MSG(ti, nullptr, vformat("Cannot get class '%s'.", String(p_class)));
		ERR_FAIL_COND_V_MSG(ti->disabled, nullptr, vformat("Class '%s' is disabled.", String(p_class)));
	}

	// Make a "fake" extension to act as a placeholder.
	placeholder_extensions[p_class] = ObjectGDExtension();
	placeholder_extension = placeholder_extensions.getptr(p_class);

	placeholder_extension->is_runtime = true;
	placeholder_extension->is_placeholder = true;

	if (ti->gdextension) {
		placeholder_extension->library = ti->gdextension->library;
		placeholder_extension->parent = ti->gdextension->parent;
		placeholder_extension->children = ti->gdextension->children;
		placeholder_extension->parent_class_name = ti->gdextension->parent_class_name;
		placeholder_extension->class_name = ti->gdextension->class_name;
		placeholder_extension->editor_class = ti->gdextension->editor_class;
		placeholder_extension->reloadable = ti->gdextension->reloadable;
		placeholder_extension->is_virtual = ti->gdextension->is_virtual;
		placeholder_extension->is_abstract = ti->gdextension->is_abstract;
		placeholder_extension->is_exposed = ti->gdextension->is_exposed;

		placeholder_extension->tracking_userdata = ti->gdextension->tracking_userdata;
		placeholder_extension->track_instance = ti->gdextension->track_instance;
		placeholder_extension->untrack_instance = ti->gdextension->untrack_instance;
	} else {
		placeholder_extension->library = nullptr;
		placeholder_extension->parent = nullptr;
		placeholder_extension->parent_class_name = ti->inherits;
		placeholder_extension->class_name = ti->name;
		placeholder_extension->editor_class = ti->api == API_EDITOR;
		placeholder_extension->reloadable = false;
		placeholder_extension->is_virtual = ti->is_virtual;
		placeholder_extension->is_abstract = false;
		placeholder_extension->is_exposed = ti->exposed;
	}

	placeholder_extension->set = &PlaceholderExtensionInstance::placeholder_instance_set;
	placeholder_extension->get = &PlaceholderExtensionInstance::placeholder_instance_get;
	placeholder_extension->get_property_list = &PlaceholderExtensionInstance::placeholder_instance_get_property_list;
	placeholder_extension->free_property_list2 = &PlaceholderExtensionInstance::placeholder_instance_free_property_list;
	placeholder_extension->property_can_revert = &PlaceholderExtensionInstance::placeholder_instance_property_can_revert;
	placeholder_extension->property_get_revert = &PlaceholderExtensionInstance::placeholder_instance_property_get_revert;
	placeholder_extension->validate_property = &PlaceholderExtensionInstance::placeholder_instance_validate_property;
#ifndef DISABLE_DEPRECATED
	placeholder_extension->notification = nullptr;
	placeholder_extension->free_property_list = nullptr;
#endif // DISABLE_DEPRECATED
	placeholder_extension->notification2 = &PlaceholderExtensionInstance::placeholder_instance_notification;
	placeholder_extension->to_string = &PlaceholderExtensionInstance::placeholder_instance_to_string;
	placeholder_extension->reference = &PlaceholderExtensionInstance::placeholder_instance_reference;
	placeholder_extension->unreference = &PlaceholderExtensionInstance::placeholder_instance_unreference;
	placeholder_extension->get_rid = &PlaceholderExtensionInstance::placeholder_instance_get_rid;

	placeholder_extension->class_userdata = ti;
#ifndef DISABLE_DEPRECATED
	placeholder_extension->create_instance = nullptr;
#endif // DISABLE_DEPRECATED
	placeholder_extension->create_instance2 = &PlaceholderExtensionInstance::placeholder_class_create_instance;
	placeholder_extension->free_instance = &PlaceholderExtensionInstance::placeholder_class_free_instance;
#ifndef DISABLE_DEPRECATED
	placeholder_extension->get_virtual = nullptr;
	placeholder_extension->get_virtual_call_data = nullptr;
#endif // DISABLE_DEPRECATED
	placeholder_extension->get_virtual2 = &PlaceholderExtensionInstance::placeholder_class_get_virtual;
	placeholder_extension->get_virtual_call_data2 = nullptr;
	placeholder_extension->call_virtual_with_data = nullptr;
	placeholder_extension->recreate_instance = &PlaceholderExtensionInstance::placeholder_class_recreate_instance;

	return placeholder_extension;
}
#endif

void ClassDB::set_object_extension_instance(Object *p_object, const StringName &p_class, GDExtensionClassInstancePtr p_instance) {
	ERR_FAIL_NULL(p_object);
	ClassInfo *ti;
	{
		OBJTYPE_RLOCK;
		ti = classes.getptr(p_class);
		if (!_can_instantiate(ti)) {
			if (compat_classes.has(p_class)) {
				ti = classes.getptr(compat_classes[p_class]);
			}
		}
		ERR_FAIL_NULL_MSG(ti, vformat("Cannot get class '%s'.", String(p_class)));
		ERR_FAIL_COND_MSG(ti->disabled, vformat("Class '%s' is disabled.", String(p_class)));
		ERR_FAIL_NULL_MSG(ti->gdextension, vformat("Class '%s' has no native extension.", String(p_class)));
	}

	p_object->_extension = ti->gdextension;
	p_object->_extension_instance = p_instance;

#ifdef TOOLS_ENABLED
	if (p_object->_extension->track_instance) {
		p_object->_extension->track_instance(p_object->_extension->tracking_userdata, p_object);
	}
#endif
}

bool ClassDB::can_instantiate(const StringName &p_class) {
	String script_path;
	{
		OBJTYPE_RLOCK;

		ClassInfo *ti = classes.getptr(p_class);
		if (!ti) {
			if (!ScriptServer::is_global_class(p_class)) {
				ERR_FAIL_V_MSG(false, vformat("Cannot get class '%s'.", String(p_class)));
			}
			script_path = ScriptServer::get_global_class_path(p_class);
			goto use_script; // Open the lock for resource loading.
		}
#ifdef TOOLS_ENABLED
		if ((ti->api == API_EDITOR || ti->api == API_EDITOR_EXTENSION) && !Engine::get_singleton()->is_editor_hint()) {
			return false;
		}
#endif
		return _can_instantiate(ti);
	}

use_script:
	Ref<Script> scr = ResourceLoader::load(script_path);
	return scr.is_valid() && scr->is_valid() && !scr->is_abstract();
}

bool ClassDB::is_abstract(const StringName &p_class) {
	String script_path;
	{
		OBJTYPE_RLOCK;

		ClassInfo *ti = classes.getptr(p_class);
		if (!ti) {
			if (!ScriptServer::is_global_class(p_class)) {
				ERR_FAIL_V_MSG(false, vformat("Cannot get class '%s'.", String(p_class)));
			}
			script_path = ScriptServer::get_global_class_path(p_class);
			goto use_script; // Open the lock for resource loading.
		}

		if (ti->creation_func != nullptr) {
			return false;
		}
		if (!ti->gdextension) {
			return true;
		}
#ifndef DISABLE_DEPRECATED
		return ti->gdextension->create_instance2 == nullptr && ti->gdextension->create_instance == nullptr;
#else
		return ti->gdextension->create_instance2 == nullptr;
#endif //  DISABLE_DEPRECATED
	}

use_script:
	Ref<Script> scr = ResourceLoader::load(script_path);
	return scr.is_valid() && scr->is_valid() && scr->is_abstract();
}

bool ClassDB::is_virtual(const StringName &p_class) {
	String script_path;
	{
		OBJTYPE_RLOCK;

		ClassInfo *ti = classes.getptr(p_class);
		if (!ti) {
			if (!ScriptServer::is_global_class(p_class)) {
				ERR_FAIL_V_MSG(false, vformat("Cannot get class '%s'.", String(p_class)));
			}
			script_path = ScriptServer::get_global_class_path(p_class);
			goto use_script; // Open the lock for resource loading.
		}
#ifdef TOOLS_ENABLED
		if ((ti->api == API_EDITOR || ti->api == API_EDITOR_EXTENSION) && !Engine::get_singleton()->is_editor_hint()) {
			return false;
		}
#endif
		return (_can_instantiate(ti) && ti->is_virtual);
	}

use_script:
	Ref<Script> scr = ResourceLoader::load(script_path);
	return scr.is_valid() && scr->is_valid() && scr->is_abstract();
}

void ClassDB::_add_class2(const StringName &p_class, const StringName &p_inherits) {
	OBJTYPE_WLOCK;

	const StringName &name = p_class;

	ERR_FAIL_COND_MSG(classes.has(name), vformat("Class '%s' already exists.", String(p_class)));

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

void ClassDB::get_method_list(const StringName &p_class, List<MethodInfo> *p_methods, bool p_no_inheritance, bool p_exclude_from_properties) {
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
		for (const MethodInfo &E : type->virtual_methods) {
			p_methods->push_back(E);
		}

		for (const StringName &E : type->method_order) {
			if (p_exclude_from_properties && type->methods_in_properties.has(E)) {
				continue;
			}

			MethodBind *method = type->method_map.get(E);
			MethodInfo minfo = info_from_bind(method);

			p_methods->push_back(minfo);
		}
#else
		for (KeyValue<StringName, MethodBind *> &E : type->method_map) {
			MethodBind *m = E.value;
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

void ClassDB::get_method_list_with_compatibility(const StringName &p_class, List<Pair<MethodInfo, uint32_t>> *p_methods, bool p_no_inheritance, bool p_exclude_from_properties) {
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
		for (const MethodInfo &E : type->virtual_methods) {
			Pair<MethodInfo, uint32_t> pair(E, E.get_compatibility_hash());
			p_methods->push_back(pair);
		}

		for (const StringName &E : type->method_order) {
			if (p_exclude_from_properties && type->methods_in_properties.has(E)) {
				continue;
			}

			MethodBind *method = type->method_map.get(E);
			MethodInfo minfo = info_from_bind(method);

			Pair<MethodInfo, uint32_t> pair(minfo, method->get_hash());
			p_methods->push_back(pair);
		}
#else
		for (KeyValue<StringName, MethodBind *> &E : type->method_map) {
			MethodBind *method = E.value;
			MethodInfo minfo = info_from_bind(method);

			Pair<MethodInfo, uint32_t> pair(minfo, method->get_hash());
			p_methods->push_back(pair);
		}
#endif

		for (const KeyValue<StringName, LocalVector<MethodBind *, unsigned int, false, false>> &E : type->method_map_compatibility) {
			LocalVector<MethodBind *> compat = E.value;
			for (MethodBind *method : compat) {
				MethodInfo minfo = info_from_bind(method);

				Pair<MethodInfo, uint32_t> pair(minfo, method->get_hash());
				p_methods->push_back(pair);
			}
		}

		if (p_no_inheritance) {
			break;
		}

		type = type->inherits_ptr;
	}
}

bool ClassDB::get_method_info(const StringName &p_class, const StringName &p_method, MethodInfo *r_info, bool p_no_inheritance, bool p_exclude_from_properties) {
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
		MethodBind **method = type->method_map.getptr(p_method);
		if (method && *method) {
			if (r_info != nullptr) {
				MethodInfo minfo = info_from_bind(*method);
				*r_info = minfo;
			}
			return true;
		} else if (type->virtual_methods_map.has(p_method)) {
			if (r_info) {
				*r_info = type->virtual_methods_map[p_method];
			}
			return true;
		}
#else
		if (type->method_map.has(p_method)) {
			if (r_info) {
				MethodBind *m = type->method_map[p_method];
				MethodInfo minfo = info_from_bind(m);
				*r_info = minfo;
			}
			return true;
		}
#endif

		if (p_no_inheritance) {
			break;
		}

		type = type->inherits_ptr;
	}

	return false;
}

MethodBind *ClassDB::get_method(const StringName &p_class, const StringName &p_name) {
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

Vector<uint32_t> ClassDB::get_method_compatibility_hashes(const StringName &p_class, const StringName &p_name) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);

	while (type) {
		if (type->method_map_compatibility.has(p_name)) {
			LocalVector<MethodBind *> *c = type->method_map_compatibility.getptr(p_name);
			Vector<uint32_t> ret;
			for (uint32_t i = 0; i < c->size(); i++) {
				ret.push_back((*c)[i]->get_hash());
			}
			return ret;
		}
		type = type->inherits_ptr;
	}
	return Vector<uint32_t>();
}

MethodBind *ClassDB::get_method_with_compatibility(const StringName &p_class, const StringName &p_name, uint64_t p_hash, bool *r_method_exists, bool *r_is_deprecated) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);

	while (type) {
		MethodBind **method = type->method_map.getptr(p_name);
		if (method && *method) {
			if (r_method_exists) {
				*r_method_exists = true;
			}
			if ((*method)->get_hash() == p_hash) {
				return *method;
			}
		}

		LocalVector<MethodBind *> *compat = type->method_map_compatibility.getptr(p_name);
		if (compat) {
			if (r_method_exists) {
				*r_method_exists = true;
			}
			for (uint32_t i = 0; i < compat->size(); i++) {
				if ((*compat)[i]->get_hash() == p_hash) {
					if (r_is_deprecated) {
						*r_is_deprecated = true;
					}
					return (*compat)[i];
				}
			}
		}
		type = type->inherits_ptr;
	}
	return nullptr;
}

void ClassDB::bind_integer_constant(const StringName &p_class, const StringName &p_enum, const StringName &p_name, int64_t p_constant, bool p_is_bitfield) {
	OBJTYPE_WLOCK;

	ClassInfo *type = classes.getptr(p_class);

	ERR_FAIL_NULL(type);

	if (type->constant_map.has(p_name)) {
		ERR_FAIL();
	}

	type->constant_map[p_name] = p_constant;

	String enum_name = p_enum;
	if (!enum_name.is_empty()) {
		if (enum_name.contains_char('.')) {
			enum_name = enum_name.get_slicec('.', 1);
		}

		ClassInfo::EnumInfo *constants_list = type->enum_map.getptr(enum_name);

		if (constants_list) {
			constants_list->constants.push_back(p_name);
			constants_list->is_bitfield = p_is_bitfield;
		} else {
			ClassInfo::EnumInfo new_list;
			new_list.is_bitfield = p_is_bitfield;
			new_list.constants.push_back(p_name);
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
		for (const StringName &E : type->constant_order) {
			p_constants->push_back(E);
		}
#else

		for (const KeyValue<StringName, int64_t> &E : type->constant_map) {
			p_constants->push_back(E.key);
		}

#endif
		if (p_no_inheritance) {
			break;
		}

		type = type->inherits_ptr;
	}
}

int64_t ClassDB::get_integer_constant(const StringName &p_class, const StringName &p_name, bool *p_success) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);

	while (type) {
		int64_t *constant = type->constant_map.getptr(p_name);
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

bool ClassDB::has_integer_constant(const StringName &p_class, const StringName &p_name, bool p_no_inheritance) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);

	while (type) {
		if (type->constant_map.has(p_name)) {
			return true;
		}
		if (p_no_inheritance) {
			return false;
		}

		type = type->inherits_ptr;
	}

	return false;
}

StringName ClassDB::get_integer_constant_enum(const StringName &p_class, const StringName &p_name, bool p_no_inheritance) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);

	while (type) {
		for (KeyValue<StringName, ClassInfo::EnumInfo> &E : type->enum_map) {
			List<StringName> &constants_list = E.value.constants;
			const List<StringName>::Element *found = constants_list.find(p_name);
			if (found) {
				return E.key;
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
		for (KeyValue<StringName, ClassInfo::EnumInfo> &E : type->enum_map) {
			p_enums->push_back(E.key);
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
		const ClassInfo::EnumInfo *constants = type->enum_map.getptr(p_enum);

		if (constants) {
			for (const List<StringName>::Element *E = constants->constants.front(); E; E = E->next()) {
				p_constants->push_back(E->get());
			}
		}

		if (p_no_inheritance) {
			break;
		}

		type = type->inherits_ptr;
	}
}

void ClassDB::set_method_error_return_values(const StringName &p_class, const StringName &p_method, const Vector<Error> &p_values) {
#ifdef DEBUG_METHODS_ENABLED
	OBJTYPE_WLOCK;
	ClassInfo *type = classes.getptr(p_class);

	ERR_FAIL_NULL(type);

	type->method_error_values[p_method] = p_values;
#endif
}

Vector<Error> ClassDB::get_method_error_return_values(const StringName &p_class, const StringName &p_method) {
#ifdef DEBUG_METHODS_ENABLED
	OBJTYPE_RLOCK;
	ClassInfo *type = classes.getptr(p_class);

	ERR_FAIL_NULL_V(type, Vector<Error>());

	if (!type->method_error_values.has(p_method)) {
		return Vector<Error>();
	}
	return type->method_error_values[p_method];
#else
	return Vector<Error>();
#endif
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

bool ClassDB::is_enum_bitfield(const StringName &p_class, const StringName &p_name, bool p_no_inheritance) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);

	while (type) {
		if (type->enum_map.has(p_name) && type->enum_map[p_name].is_bitfield) {
			return true;
		}
		if (p_no_inheritance) {
			return false;
		}

		type = type->inherits_ptr;
	}

	return false;
}

void ClassDB::add_signal(const StringName &p_class, const MethodInfo &p_signal) {
	OBJTYPE_WLOCK;

	ClassInfo *type = classes.getptr(p_class);
	ERR_FAIL_NULL(type);

	StringName sname = p_signal.name;

#ifdef DEBUG_METHODS_ENABLED
	ClassInfo *check = type;
	while (check) {
		ERR_FAIL_COND_MSG(check->signal_map.has(sname), vformat("Class '%s' already has signal '%s'.", String(p_class), String(sname)));
		check = check->inherits_ptr;
	}
#endif

	type->signal_map[sname] = p_signal;
}

void ClassDB::get_signal_list(const StringName &p_class, List<MethodInfo> *p_signals, bool p_no_inheritance) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);
	ERR_FAIL_NULL(type);

	ClassInfo *check = type;

	while (check) {
		for (KeyValue<StringName, MethodInfo> &E : check->signal_map) {
			p_signals->push_back(E.value);
		}

		if (p_no_inheritance) {
			return;
		}

		check = check->inherits_ptr;
	}
}

bool ClassDB::has_signal(const StringName &p_class, const StringName &p_signal, bool p_no_inheritance) {
	OBJTYPE_RLOCK;
	ClassInfo *type = classes.getptr(p_class);
	ClassInfo *check = type;
	while (check) {
		if (check->signal_map.has(p_signal)) {
			return true;
		}
		if (p_no_inheritance) {
			return false;
		}
		check = check->inherits_ptr;
	}

	return false;
}

bool ClassDB::get_signal(const StringName &p_class, const StringName &p_signal, MethodInfo *r_signal) {
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

void ClassDB::add_property_group(const StringName &p_class, const String &p_name, const String &p_prefix, int p_indent_depth) {
	OBJTYPE_WLOCK;
	ClassInfo *type = classes.getptr(p_class);
	ERR_FAIL_NULL(type);

	String prefix = p_prefix;
	if (p_indent_depth > 0) {
		prefix = vformat("%s,%d", p_prefix, p_indent_depth);
	}

	type->property_list.push_back(PropertyInfo(Variant::NIL, p_name, PROPERTY_HINT_NONE, prefix, PROPERTY_USAGE_GROUP));
}

void ClassDB::add_property_subgroup(const StringName &p_class, const String &p_name, const String &p_prefix, int p_indent_depth) {
	OBJTYPE_WLOCK;
	ClassInfo *type = classes.getptr(p_class);
	ERR_FAIL_NULL(type);

	String prefix = p_prefix;
	if (p_indent_depth > 0) {
		prefix = vformat("%s,%d", p_prefix, p_indent_depth);
	}

	type->property_list.push_back(PropertyInfo(Variant::NIL, p_name, PROPERTY_HINT_NONE, prefix, PROPERTY_USAGE_SUBGROUP));
}

void ClassDB::add_property_array_count(const StringName &p_class, const String &p_label, const StringName &p_count_property, const StringName &p_count_setter, const StringName &p_count_getter, const String &p_array_element_prefix, uint32_t p_count_usage) {
	add_property(p_class, PropertyInfo(Variant::INT, p_count_property, PROPERTY_HINT_NONE, "", p_count_usage | PROPERTY_USAGE_ARRAY, vformat("%s,%s", p_label, p_array_element_prefix)), p_count_setter, p_count_getter);
}

void ClassDB::add_property_array(const StringName &p_class, const StringName &p_path, const String &p_array_element_prefix) {
	OBJTYPE_WLOCK;
	ClassInfo *type = classes.getptr(p_class);
	ERR_FAIL_NULL(type);

	type->property_list.push_back(PropertyInfo(Variant::NIL, p_path, PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_ARRAY, p_array_element_prefix));
}

// NOTE: For implementation simplicity reasons, this method doesn't allow setters to have optional arguments at the end.
void ClassDB::add_property(const StringName &p_class, const PropertyInfo &p_pinfo, const StringName &p_setter, const StringName &p_getter, int p_index) {
	lock.read_lock();
	ClassInfo *type = classes.getptr(p_class);
	lock.read_unlock();

	ERR_FAIL_NULL(type);

	MethodBind *mb_set = nullptr;
	if (p_setter) {
		mb_set = get_method(p_class, p_setter);
#ifdef DEBUG_METHODS_ENABLED

		ERR_FAIL_NULL_MSG(mb_set, vformat("Invalid setter '%s::%s' for property '%s'.", p_class, p_setter, p_pinfo.name));

		int exp_args = 1 + (p_index >= 0 ? 1 : 0);
		ERR_FAIL_COND_MSG(mb_set->get_argument_count() != exp_args, vformat("Invalid function for setter '%s::%s' for property '%s'.", p_class, p_setter, p_pinfo.name));
#endif
	}

	MethodBind *mb_get = nullptr;
	if (p_getter) {
		mb_get = get_method(p_class, p_getter);
#ifdef DEBUG_METHODS_ENABLED

		ERR_FAIL_NULL_MSG(mb_get, vformat("Invalid getter '%s::%s' for property '%s'.", p_class, p_getter, p_pinfo.name));

		int exp_args = 0 + (p_index >= 0 ? 1 : 0);
		ERR_FAIL_COND_MSG(mb_get->get_argument_count() != exp_args, vformat("Invalid function for getter '%s::%s' for property '%s'.", p_class, p_getter, p_pinfo.name));
#endif
	}

#ifdef DEBUG_METHODS_ENABLED
	ERR_FAIL_COND_MSG(type->property_setget.has(p_pinfo.name), vformat("Object '%s' already has property '%s'.", p_class, p_pinfo.name));
#endif

	OBJTYPE_WLOCK

	type->property_list.push_back(p_pinfo);
	type->property_map[p_pinfo.name] = p_pinfo;
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

void ClassDB::set_property_default_value(const StringName &p_class, const StringName &p_name, const Variant &p_default) {
	if (!default_values.has(p_class)) {
		default_values[p_class] = HashMap<StringName, Variant>();
	}
	default_values[p_class][p_name] = p_default;
}

void ClassDB::add_linked_property(const StringName &p_class, const String &p_property, const String &p_linked_property) {
#ifdef TOOLS_ENABLED
	OBJTYPE_WLOCK;
	ClassInfo *type = classes.getptr(p_class);
	ERR_FAIL_NULL(type);

	ERR_FAIL_COND(!type->property_map.has(p_property));
	ERR_FAIL_COND(!type->property_map.has(p_linked_property));

	if (!type->linked_properties.has(p_property)) {
		type->linked_properties.insert(p_property, List<StringName>());
	}
	type->linked_properties[p_property].push_back(p_linked_property);

#endif
}

void ClassDB::get_property_list(const StringName &p_class, List<PropertyInfo> *p_list, bool p_no_inheritance, const Object *p_validator) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);
	ClassInfo *check = type;
	while (check) {
		for (const PropertyInfo &pi : check->property_list) {
			if (p_validator) {
				// Making a copy as we may modify it.
				PropertyInfo pi_mut = pi;
				p_validator->validate_property(pi_mut);
				p_list->push_back(pi_mut);
			} else {
				p_list->push_back(pi);
			}
		}

		if (p_no_inheritance) {
			return;
		}
		check = check->inherits_ptr;
	}
}

void ClassDB::get_linked_properties_info(const StringName &p_class, const StringName &p_property, List<StringName> *r_properties, bool p_no_inheritance) {
#ifdef TOOLS_ENABLED
	ClassInfo *check = classes.getptr(p_class);
	while (check) {
		if (!check->linked_properties.has(p_property)) {
			return;
		}
		for (const StringName &E : check->linked_properties[p_property]) {
			r_properties->push_back(E);
		}

		if (p_no_inheritance) {
			break;
		}
		check = check->inherits_ptr;
	}
#endif
}

bool ClassDB::get_property_info(const StringName &p_class, const StringName &p_property, PropertyInfo *r_info, bool p_no_inheritance, const Object *p_validator) {
	OBJTYPE_RLOCK;

	ClassInfo *check = classes.getptr(p_class);
	while (check) {
		if (check->property_map.has(p_property)) {
			PropertyInfo pinfo = check->property_map[p_property];
			if (p_validator) {
				p_validator->validate_property(pinfo);
			}
			if (r_info) {
				*r_info = pinfo;
			}
			return true;
		}
		if (p_no_inheritance) {
			break;
		}
		check = check->inherits_ptr;
	}

	return false;
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

			Callable::CallError ce;

			if (psg->index >= 0) {
				Variant index = psg->index;
				const Variant *arg[2] = { &index, &p_value };
				//p_object->call(psg->setter,arg,2,ce);
				if (psg->_setptr) {
					psg->_setptr->call(p_object, arg, 2, ce);
				} else {
					p_object->callp(psg->setter, arg, 2, ce);
				}

			} else {
				const Variant *arg[1] = { &p_value };
				if (psg->_setptr) {
					psg->_setptr->call(p_object, arg, 1, ce);
				} else {
					p_object->callp(psg->setter, arg, 1, ce);
				}
			}

			if (r_valid) {
				*r_valid = ce.error == Callable::CallError::CALL_OK;
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
				Callable::CallError ce;
				const Variant value = p_object->callp(psg->getter, arg, 1, ce);
				r_value = (ce.error == Callable::CallError::CALL_OK) ? value : Variant();

			} else {
				Callable::CallError ce;
				if (psg->_getptr) {
					r_value = psg->_getptr->call(p_object, nullptr, 0, ce);
				} else {
					const Variant value = p_object->callp(psg->getter, nullptr, 0, ce);
					r_value = (ce.error == Callable::CallError::CALL_OK) ? value : Variant();
				}
			}
			return true;
		}

		const int64_t *c = check->constant_map.getptr(p_property); //constants count
		if (c) {
			r_value = *c;
			return true;
		}

		if (check->method_map.has(p_property)) { //methods count
			r_value = Callable(p_object, p_property);
			return true;
		}

		if (check->signal_map.has(p_property)) { //signals count
			r_value = Signal(p_object, p_property);
			return true;
		}

		check = check->inherits_ptr;
	}

	// The "free()" method is special, so we assume it exists and return a Callable.
	if (p_property == CoreStringName(free_)) {
		r_value = Callable(p_object, p_property);
		return true;
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

StringName ClassDB::get_property_setter(const StringName &p_class, const StringName &p_property) {
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

StringName ClassDB::get_property_getter(const StringName &p_class, const StringName &p_property) {
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

void ClassDB::set_method_flags(const StringName &p_class, const StringName &p_method, int p_flags) {
	OBJTYPE_WLOCK;
	ClassInfo *type = classes.getptr(p_class);
	ClassInfo *check = type;
	ERR_FAIL_NULL(check);
	ERR_FAIL_COND(!check->method_map.has(p_method));
	check->method_map[p_method]->set_hint_flags(p_flags);
}

bool ClassDB::has_method(const StringName &p_class, const StringName &p_method, bool p_no_inheritance) {
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

int ClassDB::get_method_argument_count(const StringName &p_class, const StringName &p_method, bool *r_is_valid, bool p_no_inheritance) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);

	while (type) {
		MethodBind **method = type->method_map.getptr(p_method);
		if (method && *method) {
			if (r_is_valid) {
				*r_is_valid = true;
			}
			return (*method)->get_argument_count();
		}
		if (p_no_inheritance) {
			break;
		}
		type = type->inherits_ptr;
	}

	if (r_is_valid) {
		*r_is_valid = false;
	}
	return 0;
}

void ClassDB::bind_method_custom(const StringName &p_class, MethodBind *p_method) {
	_bind_method_custom(p_class, p_method, false);
}
void ClassDB::bind_compatibility_method_custom(const StringName &p_class, MethodBind *p_method) {
	_bind_method_custom(p_class, p_method, true);
}

void ClassDB::_bind_compatibility(ClassInfo *type, MethodBind *p_method) {
	if (!type->method_map_compatibility.has(p_method->get_name())) {
		type->method_map_compatibility.insert(p_method->get_name(), LocalVector<MethodBind *>());
	}
	type->method_map_compatibility[p_method->get_name()].push_back(p_method);
}

void ClassDB::_bind_method_custom(const StringName &p_class, MethodBind *p_method, bool p_compatibility) {
	OBJTYPE_WLOCK;

	ClassInfo *type = classes.getptr(p_class);
	if (!type) {
		ERR_FAIL_MSG(vformat("Couldn't bind custom method '%s' for instance '%s'.", p_method->get_name(), p_class));
	}

	if (p_compatibility) {
		_bind_compatibility(type, p_method);
		return;
	}

	if (type->method_map.has(p_method->get_name())) {
		// overloading not supported
		ERR_FAIL_MSG(vformat("Method already bound '%s::%s'.", p_class, p_method->get_name()));
	}

#ifdef DEBUG_METHODS_ENABLED
	type->method_order.push_back(p_method->get_name());
#endif

	type->method_map[p_method->get_name()] = p_method;
}

MethodBind *ClassDB::_bind_vararg_method(MethodBind *p_bind, const StringName &p_name, const Vector<Variant> &p_default_args, bool p_compatibility) {
	MethodBind *bind = p_bind;
	bind->set_name(p_name);
	bind->set_default_arguments(p_default_args);

	String instance_type = bind->get_instance_class();

	ClassInfo *type = classes.getptr(instance_type);
	if (!type) {
		memdelete(bind);
		ERR_FAIL_NULL_V(type, nullptr);
	}

	if (p_compatibility) {
		_bind_compatibility(type, bind);
		return bind;
	}

	if (type->method_map.has(p_name)) {
		memdelete(bind);
		// Overloading not supported
		ERR_FAIL_V_MSG(nullptr, vformat("Method already bound: '%s::%s'.", instance_type, p_name));
	}
	type->method_map[p_name] = bind;
#ifdef DEBUG_METHODS_ENABLED
	// FIXME: <reduz> set_return_type is no longer in MethodBind, so I guess it should be moved to vararg method bind
	//bind->set_return_type("Variant");
	type->method_order.push_back(p_name);
#endif

	return bind;
}

#ifdef DEBUG_METHODS_ENABLED
MethodBind *ClassDB::bind_methodfi(uint32_t p_flags, MethodBind *p_bind, bool p_compatibility, const MethodDefinition &method_name, const Variant **p_defs, int p_defcount) {
	StringName mdname = method_name.name;
#else
MethodBind *ClassDB::bind_methodfi(uint32_t p_flags, MethodBind *p_bind, bool p_compatibility, const char *method_name, const Variant **p_defs, int p_defcount) {
	StringName mdname = StaticCString::create(method_name);
#endif

	OBJTYPE_WLOCK;
	ERR_FAIL_NULL_V(p_bind, nullptr);
	p_bind->set_name(mdname);

	String instance_type = p_bind->get_instance_class();

#ifdef DEBUG_ENABLED

	ERR_FAIL_COND_V_MSG(!p_compatibility && has_method(instance_type, mdname), nullptr, vformat("Class '%s' already has a method '%s'.", String(instance_type), String(mdname)));
#endif

	ClassInfo *type = classes.getptr(instance_type);
	if (!type) {
		memdelete(p_bind);
		ERR_FAIL_V_MSG(nullptr, vformat("Couldn't bind method '%s' for instance '%s'.", mdname, instance_type));
	}

	if (!p_compatibility && type->method_map.has(mdname)) {
		memdelete(p_bind);
		// overloading not supported
		ERR_FAIL_V_MSG(nullptr, vformat("Method already bound '%s::%s'.", instance_type, mdname));
	}

#ifdef DEBUG_METHODS_ENABLED

	if (method_name.args.size() > p_bind->get_argument_count()) {
		memdelete(p_bind);
		ERR_FAIL_V_MSG(nullptr, vformat("Method definition provides more arguments than the method actually has '%s::%s'.", instance_type, mdname));
	}

	if (p_defcount > p_bind->get_argument_count()) {
		memdelete(p_bind);
		ERR_FAIL_V_MSG(nullptr, vformat("Method definition for '%s::%s' provides more default arguments than the method has arguments.", instance_type, mdname));
	}

	p_bind->set_argument_names(method_name.args);

	if (!p_compatibility) {
		type->method_order.push_back(mdname);
	}
#endif

	if (p_compatibility) {
		_bind_compatibility(type, p_bind);
	} else {
		type->method_map[mdname] = p_bind;
	}

	Vector<Variant> defvals;

	defvals.resize(p_defcount);
	for (int i = 0; i < p_defcount; i++) {
		defvals.write[i] = *p_defs[i];
	}

	p_bind->set_default_arguments(defvals);
	p_bind->set_hint_flags(p_flags);
	return p_bind;
}

void ClassDB::add_virtual_method(const StringName &p_class, const MethodInfo &p_method, bool p_virtual, const Vector<String> &p_arg_names, bool p_object_core) {
	ERR_FAIL_COND_MSG(!classes.has(p_class), vformat("Request for nonexistent class '%s'.", p_class));

	OBJTYPE_WLOCK;

#ifdef DEBUG_METHODS_ENABLED
	MethodInfo mi = p_method;
	if (p_virtual) {
		mi.flags |= METHOD_FLAG_VIRTUAL;
	}
	if (p_object_core) {
		mi.flags |= METHOD_FLAG_OBJECT_CORE;
	}

	if (!p_object_core) {
		if (p_arg_names.size() != mi.arguments.size()) {
			WARN_PRINT(vformat("Mismatch argument name count for virtual method: '%s::%s'.", String(p_class), p_method.name));
		} else {
			List<PropertyInfo>::Iterator itr = mi.arguments.begin();
			for (int i = 0; i < p_arg_names.size(); ++itr, ++i) {
				itr->name = p_arg_names[i];
			}
		}
	}

	if (classes[p_class].virtual_methods_map.has(p_method.name)) {
		// overloading not supported
		ERR_FAIL_MSG(vformat("Virtual method already bound '%s::%s'.", String(p_class), p_method.name));
	}
	classes[p_class].virtual_methods.push_back(mi);
	classes[p_class].virtual_methods_map[p_method.name] = mi;

#endif
}

void ClassDB::add_virtual_compatibility_method(const StringName &p_class, const MethodInfo &p_method, bool p_virtual, const Vector<String> &p_arg_names, bool p_object_core) {
	ERR_FAIL_COND_MSG(!classes.has(p_class), vformat("Request for nonexistent class '%s'.", p_class));

	OBJTYPE_WLOCK;

	HashMap<StringName, Vector<uint32_t>> &virtual_methods_compat = classes[p_class].virtual_methods_compat;

	Vector<uint32_t> *compat_hashes = virtual_methods_compat.getptr(p_method.name);
	if (!compat_hashes) {
		virtual_methods_compat[p_method.name] = Vector<uint32_t>();
		compat_hashes = &virtual_methods_compat[p_method.name];
	}

	compat_hashes->push_back(p_method.get_compatibility_hash());
}

void ClassDB::get_virtual_methods(const StringName &p_class, List<MethodInfo> *p_methods, bool p_no_inheritance) {
	ERR_FAIL_COND_MSG(!classes.has(p_class), vformat("Request for nonexistent class '%s'.", p_class));

#ifdef DEBUG_METHODS_ENABLED

	ClassInfo *type = classes.getptr(p_class);
	ClassInfo *check = type;
	while (check) {
		for (const MethodInfo &E : check->virtual_methods) {
			p_methods->push_back(E);
		}

		if (p_no_inheritance) {
			return;
		}
		check = check->inherits_ptr;
	}

#endif
}

Vector<uint32_t> ClassDB::get_virtual_method_compatibility_hashes(const StringName &p_class, const StringName &p_name) {
	OBJTYPE_RLOCK;

	ClassInfo *type = classes.getptr(p_class);

	while (type) {
		if (type->virtual_methods_compat.has(p_name)) {
			Vector<uint32_t> *compat_hashes = type->virtual_methods_compat.getptr(p_name);
			if (compat_hashes) {
				return *compat_hashes;
			}
			break;
		}
		type = type->inherits_ptr;
	}

	return Vector<uint32_t>();
}

void ClassDB::add_extension_class_virtual_method(const StringName &p_class, const GDExtensionClassVirtualMethodInfo *p_method_info) {
	ERR_FAIL_COND_MSG(!classes.has(p_class), vformat("Request for nonexistent class '%s'.", p_class));

#ifdef DEBUG_METHODS_ENABLED
	PackedStringArray arg_names;

	MethodInfo mi;
	mi.name = *reinterpret_cast<StringName *>(p_method_info->name);
	mi.return_val = PropertyInfo(p_method_info->return_value);
	mi.return_val_metadata = p_method_info->return_value_metadata;
	mi.flags = p_method_info->method_flags;
	for (int i = 0; i < (int)p_method_info->argument_count; i++) {
		PropertyInfo arg(p_method_info->arguments[i]);
		mi.arguments.push_back(arg);
		mi.arguments_metadata.push_back(p_method_info->arguments_metadata[i]);
		arg_names.push_back(arg.name);
	}

	add_virtual_method(p_class, mi, true, arg_names);
#endif
}

void ClassDB::set_class_enabled(const StringName &p_class, bool p_enable) {
	OBJTYPE_WLOCK;

	ERR_FAIL_COND_MSG(!classes.has(p_class), vformat("Request for nonexistent class '%s'.", p_class));
	classes[p_class].disabled = !p_enable;
}

bool ClassDB::is_class_enabled(const StringName &p_class) {
	OBJTYPE_RLOCK;

	ClassInfo *ti = classes.getptr(p_class);
	if (!ti || !ti->creation_func) {
		if (compat_classes.has(p_class)) {
			ti = classes.getptr(compat_classes[p_class]);
		}
	}

	ERR_FAIL_NULL_V_MSG(ti, false, vformat("Cannot get class '%s'.", String(p_class)));
	return !ti->disabled;
}

bool ClassDB::is_class_exposed(const StringName &p_class) {
	OBJTYPE_RLOCK;

	ClassInfo *ti = classes.getptr(p_class);
	ERR_FAIL_NULL_V_MSG(ti, false, vformat("Cannot get class '%s'.", String(p_class)));
	return ti->exposed;
}

bool ClassDB::is_class_reloadable(const StringName &p_class) {
	OBJTYPE_RLOCK;

	ClassInfo *ti = classes.getptr(p_class);
	ERR_FAIL_NULL_V_MSG(ti, false, vformat("Cannot get class '%s'.", String(p_class)));
	return ti->reloadable;
}

bool ClassDB::is_class_runtime(const StringName &p_class) {
	OBJTYPE_RLOCK;

	ClassInfo *ti = classes.getptr(p_class);
	ERR_FAIL_NULL_V_MSG(ti, false, vformat("Cannot get class '%s'.", String(p_class)));
	return ti->is_runtime;
}

void ClassDB::add_resource_base_extension(const StringName &p_extension, const StringName &p_class) {
	if (resource_base_extensions.has(p_extension)) {
		return;
	}

	resource_base_extensions[p_extension] = p_class;
}

void ClassDB::get_resource_base_extensions(List<String> *p_extensions) {
	for (const KeyValue<StringName, StringName> &E : resource_base_extensions) {
		p_extensions->push_back(E.key);
	}
}

bool ClassDB::is_resource_extension(const StringName &p_extension) {
	return resource_base_extensions.has(p_extension);
}

void ClassDB::get_extensions_for_type(const StringName &p_class, List<String> *p_extensions) {
	for (const KeyValue<StringName, StringName> &E : resource_base_extensions) {
		if (is_parent_class(p_class, E.value) || is_parent_class(E.value, p_class)) {
			p_extensions->push_back(E.key);
		}
	}
}

HashMap<StringName, HashMap<StringName, Variant>> ClassDB::default_values;
HashSet<StringName> ClassDB::default_values_cached;

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
		} else if (ClassDB::can_instantiate(p_class) && !ClassDB::is_virtual(p_class)) { // Keep this condition in sync with doc_tools.cpp get_documentation_default_value.
			c = ClassDB::instantiate_no_placeholders(p_class);
			cleanup_c = true;
		}

		if (c) {
			List<PropertyInfo> plist;
			c->get_property_list(&plist);
			for (const PropertyInfo &E : plist) {
				if (E.usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR)) {
					if (!default_values[p_class].has(E.name)) {
						Variant v = c->get(E.name);
						default_values[p_class][E.name] = v;
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

	Variant var = default_values[p_class][p_property];

#ifdef DEBUG_ENABLED
	// Some properties may have an instantiated Object as default value,
	// (like Path2D's `curve` used to have), but that's not a good practice.
	// Instead, those properties should use PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT
	// to be auto-instantiated when created in the editor with the following method:
	// EditorNode::get_editor_data().instantiate_object_properties(obj);
	if (var.get_type() == Variant::OBJECT) {
		Object *obj = var.get_validated_object();
		if (obj) {
			WARN_PRINT(vformat("Instantiated %s used as default value for %s's \"%s\" property.", obj->get_class(), p_class, p_property));
		}
	}
#endif

	return var;
}

void ClassDB::register_extension_class(ObjectGDExtension *p_extension) {
	GLOBAL_LOCK_FUNCTION;

	ERR_FAIL_COND_MSG(classes.has(p_extension->class_name), vformat("Class already registered: '%s'.", String(p_extension->class_name)));
	ERR_FAIL_COND_MSG(!classes.has(p_extension->parent_class_name), vformat("Parent class name for extension class not found: '%s'.", String(p_extension->parent_class_name)));

	ClassInfo *parent = classes.getptr(p_extension->parent_class_name);

#ifdef TOOLS_ENABLED
	// @todo This is a limitation of the current implementation, but it should be possible to remove.
	ERR_FAIL_COND_MSG(p_extension->is_runtime && parent->gdextension && !parent->is_runtime, vformat("Extension runtime class '%s' cannot descend from '%s' which isn't also a runtime class.", String(p_extension->class_name), parent->name));
#endif

	ClassInfo c;
	c.api = p_extension->editor_class ? API_EDITOR_EXTENSION : API_EXTENSION;
	c.gdextension = p_extension;
	c.name = p_extension->class_name;
	c.is_virtual = p_extension->is_virtual;
	if (!p_extension->is_abstract) {
		// Find the closest ancestor which is either non-abstract or native (or both).
		ClassInfo *concrete_ancestor = parent;
		while (concrete_ancestor->creation_func == nullptr &&
				concrete_ancestor->inherits_ptr != nullptr &&
				concrete_ancestor->gdextension != nullptr) {
			concrete_ancestor = concrete_ancestor->inherits_ptr;
		}
		ERR_FAIL_NULL_MSG(concrete_ancestor->creation_func, vformat("Extension class '%s' cannot extend native abstract class '%s'.", String(p_extension->class_name), String(concrete_ancestor->name)));
		c.creation_func = concrete_ancestor->creation_func;
	}
	c.inherits = parent->name;
	c.class_ptr = parent->class_ptr;
	c.inherits_ptr = parent;
	c.exposed = p_extension->is_exposed;
	if (c.exposed) {
		// The parent classes should be exposed if it has an exposed child class.
		while (parent && !parent->exposed) {
			parent->exposed = true;
			parent = classes.getptr(parent->name);
		}
	}
	c.reloadable = p_extension->reloadable;
#ifdef TOOLS_ENABLED
	c.is_runtime = p_extension->is_runtime;
#endif

	classes[p_extension->class_name] = c;
}

void ClassDB::unregister_extension_class(const StringName &p_class, bool p_free_method_binds) {
	ClassInfo *c = classes.getptr(p_class);
	ERR_FAIL_NULL_MSG(c, vformat("Class '%s' does not exist.", String(p_class)));
	if (p_free_method_binds) {
		for (KeyValue<StringName, MethodBind *> &F : c->method_map) {
			memdelete(F.value);
		}
	}
	classes.erase(p_class);
	default_values_cached.erase(p_class);
	default_values.erase(p_class);
#ifdef TOOLS_ENABLED
	placeholder_extensions.erase(p_class);
#endif
}

HashMap<StringName, ClassDB::NativeStruct> ClassDB::native_structs;
void ClassDB::register_native_struct(const StringName &p_name, const String &p_code, uint64_t p_current_size) {
	NativeStruct ns;
	ns.ccode = p_code;
	ns.struct_size = p_current_size;
	native_structs[p_name] = ns;
}

void ClassDB::get_native_struct_list(List<StringName> *r_names) {
	for (const KeyValue<StringName, NativeStruct> &E : native_structs) {
		r_names->push_back(E.key);
	}
}

String ClassDB::get_native_struct_code(const StringName &p_name) {
	ERR_FAIL_COND_V(!native_structs.has(p_name), String());
	return native_structs[p_name].ccode;
}

uint64_t ClassDB::get_native_struct_size(const StringName &p_name) {
	ERR_FAIL_COND_V(!native_structs.has(p_name), 0);
	return native_structs[p_name].struct_size;
}

RWLock ClassDB::lock;

void ClassDB::cleanup_defaults() {
	default_values.clear();
	default_values_cached.clear();
}

void ClassDB::cleanup() {
	//OBJTYPE_LOCK; hah not here

	for (KeyValue<StringName, ClassInfo> &E : classes) {
		ClassInfo &ti = E.value;

		for (KeyValue<StringName, MethodBind *> &F : ti.method_map) {
			memdelete(F.value);
		}
		for (KeyValue<StringName, LocalVector<MethodBind *>> &F : ti.method_map_compatibility) {
			for (uint32_t i = 0; i < F.value.size(); i++) {
				memdelete(F.value[i]);
			}
		}
	}

	classes.clear();
	resource_base_extensions.clear();
	compat_classes.clear();
	native_structs.clear();
}

// Array to use in optional parameters on methods and the DEFVAL_ARRAY macro.
Array ClassDB::default_array_arg = Array::create_read_only();

bool ClassDB::is_default_array_arg(const Array &p_array) {
	return p_array.is_same_instance(default_array_arg);
}

//
