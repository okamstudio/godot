/**************************************************************************/
/*  runtime_interop.cpp                                                   */
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

#include "runtime_interop.h"

#include "../csharp_script.h"
#include "../interop_types.h"
#include "../managed_callable.h"
#include "../mono_gd/gd_mono_cache.h"
#include "../signal_awaiter_utils.h"
#include "../utils/path_utils.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/debugger/engine_debugger.h"
#include "core/debugger/script_debugger.h"
#include "core/io/marshalls.h"
#include "core/object/class_db.h"
#include "core/object/method_bind.h"
#include "core/os/os.h"
#include "core/string/string_name.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_file_system.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// For ArrayPrivate and DictionaryPrivate
static_assert(sizeof(SafeRefCount) == sizeof(uint32_t));

typedef Object *(*godotsharp_class_creation_func)(bool);

bool godotsharp_dotnet_module_is_initialized() {
	return GDMono::get_singleton()->is_initialized();
}

MethodBind *godotsharp_method_bind_get_method(const StringName *p_classname, const StringName *p_methodname) {
	return ClassDB::get_method(*p_classname, *p_methodname);
}

MethodBind *godotsharp_method_bind_get_method_with_compatibility(const StringName *p_classname, const StringName *p_methodname, uint64_t p_hash) {
	return ClassDB::get_method_with_compatibility(*p_classname, *p_methodname, p_hash);
}

godotsharp_class_creation_func godotsharp_get_class_constructor(const StringName *p_classname) {
	ClassDB::ClassInfo *class_info = ClassDB::classes.getptr(*p_classname);
	if (class_info) {
		return class_info->creation_func;
	}
	return nullptr;
}

Object *godotsharp_engine_get_singleton(const String *p_name) {
	return Engine::get_singleton()->get_singleton_object(*p_name);
}

int32_t godotsharp_stack_info_vector_resize(
		Vector<ScriptLanguage::StackInfo> *p_stack_info_vector, int p_size) {
	return (int32_t)p_stack_info_vector->resize(p_size);
}

void godotsharp_stack_info_vector_destroy(
		Vector<ScriptLanguage::StackInfo> *p_stack_info_vector) {
	p_stack_info_vector->~Vector();
}

void godotsharp_internal_script_debugger_send_error(const String *p_func,
		const String *p_file, int32_t p_line, const String *p_err, const String *p_descr,
		ErrorHandlerType p_type, const Vector<ScriptLanguage::StackInfo> *p_stack_info_vector) {
	const String file = ProjectSettings::get_singleton()->localize_path(p_file->simplify_path());
	EngineDebugger::get_script_debugger()->send_error(*p_func, file, p_line, *p_err, *p_descr,
			true, p_type, *p_stack_info_vector);
}

bool godotsharp_internal_script_debugger_is_active() {
	return EngineDebugger::is_active();
}

GCHandleIntPtr godotsharp_internal_object_get_associated_gchandle(Object *p_ptr) {
#ifdef DEBUG_ENABLED
	CRASH_COND(p_ptr == nullptr);
#endif

	if (p_ptr->get_script_instance()) {
		CSharpInstance *cs_instance = CAST_CSHARP_INSTANCE(p_ptr->get_script_instance());
		if (cs_instance) {
			if (!cs_instance->is_destructing_script_instance()) {
				return cs_instance->get_gchandle_intptr();
			}
			return { nullptr };
		}
	}

	void *data = CSharpLanguage::get_existing_instance_binding(p_ptr);

	if (data) {
		CSharpScriptBinding &script_binding = ((RBMap<Object *, CSharpScriptBinding>::Element *)data)->get();
		if (script_binding.inited) {
			MonoGCHandleData &gchandle = script_binding.gchandle;
			return !gchandle.is_released() ? gchandle.get_intptr() : GCHandleIntPtr{ nullptr };
		}
	}

	return { nullptr };
}

void godotsharp_internal_object_disposed(Object *p_ptr, GCHandleIntPtr p_gchandle_to_free) {
#ifdef DEBUG_ENABLED
	CRASH_COND(p_ptr == nullptr);
#endif

	if (p_ptr->get_script_instance()) {
		CSharpInstance *cs_instance = CAST_CSHARP_INSTANCE(p_ptr->get_script_instance());
		if (cs_instance) {
			if (!cs_instance->is_destructing_script_instance()) {
				cs_instance->mono_object_disposed(p_gchandle_to_free);
				p_ptr->set_script_instance(nullptr);
			}
			return;
		}
	}

	void *data = CSharpLanguage::get_existing_instance_binding(p_ptr);

	if (data) {
		CSharpScriptBinding &script_binding = ((RBMap<Object *, CSharpScriptBinding>::Element *)data)->get();
		if (script_binding.inited) {
			if (!script_binding.gchandle.is_released()) {
				CSharpLanguage::release_binding_gchandle_thread_safe(p_gchandle_to_free, script_binding);
			}
		}
	}
}

void godotsharp_internal_refcounted_disposed(Object *p_ptr, GCHandleIntPtr p_gchandle_to_free, bool p_is_finalizer) {
#ifdef DEBUG_ENABLED
	CRASH_COND(p_ptr == nullptr);
	// This is only called with RefCounted derived classes
	CRASH_COND(!Object::cast_to<RefCounted>(p_ptr));
#endif

	RefCounted *rc = static_cast<RefCounted *>(p_ptr);

	if (rc->get_script_instance()) {
		CSharpInstance *cs_instance = CAST_CSHARP_INSTANCE(rc->get_script_instance());
		if (cs_instance) {
			if (!cs_instance->is_destructing_script_instance()) {
				bool delete_owner;
				bool remove_script_instance;

				cs_instance->mono_object_disposed_baseref(p_gchandle_to_free, p_is_finalizer,
						delete_owner, remove_script_instance);

				if (delete_owner) {
					memdelete(rc);
				} else if (remove_script_instance) {
					rc->set_script_instance(nullptr);
				}
			}
			return;
		}
	}

	// Unsafe refcount decrement. The managed instance also counts as a reference.
	// See: CSharpLanguage::alloc_instance_binding_data(Object *p_object)
	CSharpLanguage::get_singleton()->pre_unsafe_unreference(rc);
	if (rc->unreference()) {
		memdelete(rc);
	} else {
		void *data = CSharpLanguage::get_existing_instance_binding(rc);

		if (data) {
			CSharpScriptBinding &script_binding = ((RBMap<Object *, CSharpScriptBinding>::Element *)data)->get();
			if (script_binding.inited) {
				if (!script_binding.gchandle.is_released()) {
					CSharpLanguage::release_binding_gchandle_thread_safe(p_gchandle_to_free, script_binding);
				}
			}
		}
	}
}

int32_t godotsharp_internal_signal_awaiter_connect(Object *p_source, StringName *p_signal, Object *p_target, GCHandleIntPtr p_awaiter_handle_ptr) {
	StringName signal = p_signal ? *p_signal : StringName();
	return (int32_t)gd_mono_connect_signal_awaiter(p_source, signal, p_target, p_awaiter_handle_ptr);
}

GCHandleIntPtr godotsharp_internal_unmanaged_get_script_instance_managed(Object *p_unmanaged, bool *r_has_cs_script_instance) {
#ifdef DEBUG_ENABLED
	CRASH_COND(!p_unmanaged);
	CRASH_COND(!r_has_cs_script_instance);
#endif

	if (p_unmanaged->get_script_instance()) {
		CSharpInstance *cs_instance = CAST_CSHARP_INSTANCE(p_unmanaged->get_script_instance());

		if (cs_instance) {
			*r_has_cs_script_instance = true;
			return cs_instance->get_gchandle_intptr();
		}
	}

	*r_has_cs_script_instance = false;
	return { nullptr };
}

GCHandleIntPtr godotsharp_internal_unmanaged_get_instance_binding_managed(Object *p_unmanaged) {
#ifdef DEBUG_ENABLED
	CRASH_COND(!p_unmanaged);
#endif

	void *data = CSharpLanguage::get_instance_binding_with_setup(p_unmanaged);
	ERR_FAIL_NULL_V(data, { nullptr });
	CSharpScriptBinding &script_binding = ((RBMap<Object *, CSharpScriptBinding>::Element *)data)->value();
	ERR_FAIL_COND_V(!script_binding.inited, { nullptr });

	return script_binding.gchandle.get_intptr();
}

GCHandleIntPtr godotsharp_internal_unmanaged_instance_binding_create_managed(Object *p_unmanaged, GCHandleIntPtr p_old_gchandle) {
#ifdef DEBUG_ENABLED
	CRASH_COND(!p_unmanaged);
#endif

	void *data = CSharpLanguage::get_instance_binding_with_setup(p_unmanaged);
	ERR_FAIL_NULL_V(data, { nullptr });
	CSharpScriptBinding &script_binding = ((RBMap<Object *, CSharpScriptBinding>::Element *)data)->value();
	ERR_FAIL_COND_V(!script_binding.inited, { nullptr });

	MonoGCHandleData &gchandle = script_binding.gchandle;

	// TODO: Possible data race?
	CRASH_COND(gchandle.get_intptr().value != p_old_gchandle.value);

	CSharpLanguage::get_singleton()->release_script_gchandle(gchandle);
	script_binding.inited = false;

	// Create a new one

#ifdef DEBUG_ENABLED
	CRASH_COND(script_binding.type_name == StringName());
#endif

	bool parent_is_object_class = ClassDB::is_parent_class(p_unmanaged->get_class_name(), script_binding.type_name);
	ERR_FAIL_COND_V_MSG(!parent_is_object_class, { nullptr },
			"Type inherits from native type '" + script_binding.type_name + "', so it can't be instantiated in object of type: '" + p_unmanaged->get_class() + "'.");

	GCHandleIntPtr strong_gchandle =
			GDMonoCache::managed_callbacks.ScriptManagerBridge_CreateManagedForGodotObjectBinding(
					&script_binding.type_name, p_unmanaged);

	ERR_FAIL_NULL_V(strong_gchandle.value, { nullptr });

	gchandle = MonoGCHandleData(strong_gchandle, gdmono::GCHandleType::STRONG_HANDLE);
	script_binding.inited = true;

	// Tie managed to unmanaged
	RefCounted *rc = Object::cast_to<RefCounted>(p_unmanaged);

	if (rc) {
		// Unsafe refcount increment. The managed instance also counts as a reference.
		// This way if the unmanaged world has no references to our owner
		// but the managed instance is alive, the refcount will be 1 instead of 0.
		// See: godot_icall_RefCounted_Dtor(MonoObject *p_obj, Object *p_ptr)
		rc->reference();
		CSharpLanguage::get_singleton()->post_unsafe_reference(rc);
	}

	return gchandle.get_intptr();
}

void godotsharp_internal_tie_native_managed_to_unmanaged(GCHandleIntPtr p_gchandle_intptr, Object *p_unmanaged, const StringName *p_native_name, bool p_ref_counted) {
	CSharpLanguage::tie_native_managed_to_unmanaged(p_gchandle_intptr, p_unmanaged, p_native_name, p_ref_counted);
}

void godotsharp_internal_tie_user_managed_to_unmanaged(GCHandleIntPtr p_gchandle_intptr, Object *p_unmanaged, Ref<CSharpScript> *p_script, bool p_ref_counted) {
	CSharpLanguage::tie_user_managed_to_unmanaged(p_gchandle_intptr, p_unmanaged, p_script, p_ref_counted);
}

void godotsharp_internal_tie_managed_to_unmanaged_with_pre_setup(GCHandleIntPtr p_gchandle_intptr, Object *p_unmanaged) {
	CSharpLanguage::tie_managed_to_unmanaged_with_pre_setup(p_gchandle_intptr, p_unmanaged);
}

void godotsharp_internal_new_csharp_script(Ref<CSharpScript> *r_dest) {
	memnew_placement(r_dest, Ref<CSharpScript>(memnew(CSharpScript)));
}

void godotsharp_internal_editor_file_system_update_files(const PackedStringArray &p_script_paths) {
#ifdef TOOLS_ENABLED
	// If the EditorFileSystem singleton is available, update the file;
	// otherwise, the file will be updated when the singleton becomes available.
	EditorFileSystem *efs = EditorFileSystem::get_singleton();
	if (efs) {
		efs->update_files(p_script_paths);
	}
#else
	// EditorFileSystem is only available when running in the Godot editor.
	DEV_ASSERT(false);
#endif
}

bool godotsharp_internal_script_load(const String *p_path, Ref<CSharpScript> *r_dest) {
	Ref<Resource> res = ResourceLoader::load(*p_path);
	if (res.is_valid()) {
		memnew_placement(r_dest, Ref<CSharpScript>(res));
		return true;
	} else {
		memnew_placement(r_dest, Ref<CSharpScript>());
		return false;
	}
}

void godotsharp_internal_reload_registered_script(CSharpScript *p_script) {
	CRASH_COND(!p_script);
	CSharpScript::reload_registered_script(Ref<CSharpScript>(p_script));
}

void godotsharp_array_filter_godot_objects_by_native(StringName *p_native_name, const Array *p_input, Array *r_output) {
	memnew_placement(r_output, Array);

	for (int i = 0; i < p_input->size(); ++i) {
		if (ClassDB::is_parent_class(((Object *)(*p_input)[i])->get_class(), *p_native_name)) {
			r_output->push_back(p_input[i]);
		}
	}
}

void godotsharp_array_filter_godot_objects_by_non_native(const Array *p_input, Array *r_output) {
	memnew_placement(r_output, Array);

	for (int i = 0; i < p_input->size(); ++i) {
		CSharpInstance *si = CAST_CSHARP_INSTANCE(((Object *)(*p_input)[i])->get_script_instance());

		if (si != nullptr) {
			r_output->push_back(p_input[i]);
		}
	}
}

void godotsharp_ref_new_from_ref_counted_ptr(Ref<RefCounted> *r_dest, RefCounted *p_ref_counted_ptr) {
	memnew_placement(r_dest, Ref<RefCounted>(p_ref_counted_ptr));
}

void godotsharp_ref_destroy(Ref<RefCounted> *p_instance) {
	p_instance->~Ref();
}

void godotsharp_string_name_new_from_string(StringName *r_dest, const String *p_name) {
	memnew_placement(r_dest, StringName(*p_name));
}

void godotsharp_node_path_new_from_string(NodePath *r_dest, const String *p_name) {
	memnew_placement(r_dest, NodePath(*p_name));
}

void godotsharp_string_name_as_string(String *r_dest, const StringName *p_name) {
	memnew_placement(r_dest, String(p_name->operator String()));
}

void godotsharp_node_path_as_string(String *r_dest, const NodePath *p_np) {
	memnew_placement(r_dest, String(p_np->operator String()));
}

godot_packed_array godotsharp_packed_byte_array_new_mem_copy(const uint8_t *p_src, int32_t p_length) {
	godot_packed_array ret;
	memnew_placement(&ret, PackedByteArray);
	PackedByteArray *array = reinterpret_cast<PackedByteArray *>(&ret);
	array->resize(p_length);
	uint8_t *dst = array->ptrw();
	memcpy(dst, p_src, p_length * sizeof(uint8_t));
	return ret;
}

godot_packed_array godotsharp_packed_int32_array_new_mem_copy(const int32_t *p_src, int32_t p_length) {
	godot_packed_array ret;
	memnew_placement(&ret, PackedInt32Array);
	PackedInt32Array *array = reinterpret_cast<PackedInt32Array *>(&ret);
	array->resize(p_length);
	int32_t *dst = array->ptrw();
	memcpy(dst, p_src, p_length * sizeof(int32_t));
	return ret;
}

godot_packed_array godotsharp_packed_int64_array_new_mem_copy(const int64_t *p_src, int32_t p_length) {
	godot_packed_array ret;
	memnew_placement(&ret, PackedInt64Array);
	PackedInt64Array *array = reinterpret_cast<PackedInt64Array *>(&ret);
	array->resize(p_length);
	int64_t *dst = array->ptrw();
	memcpy(dst, p_src, p_length * sizeof(int64_t));
	return ret;
}

godot_packed_array godotsharp_packed_float32_array_new_mem_copy(const float *p_src, int32_t p_length) {
	godot_packed_array ret;
	memnew_placement(&ret, PackedFloat32Array);
	PackedFloat32Array *array = reinterpret_cast<PackedFloat32Array *>(&ret);
	array->resize(p_length);
	float *dst = array->ptrw();
	memcpy(dst, p_src, p_length * sizeof(float));
	return ret;
}

godot_packed_array godotsharp_packed_float64_array_new_mem_copy(const double *p_src, int32_t p_length) {
	godot_packed_array ret;
	memnew_placement(&ret, PackedFloat64Array);
	PackedFloat64Array *array = reinterpret_cast<PackedFloat64Array *>(&ret);
	array->resize(p_length);
	double *dst = array->ptrw();
	memcpy(dst, p_src, p_length * sizeof(double));
	return ret;
}

godot_packed_array godotsharp_packed_vector2_array_new_mem_copy(const Vector2 *p_src, int32_t p_length) {
	godot_packed_array ret;
	memnew_placement(&ret, PackedVector2Array);
	PackedVector2Array *array = reinterpret_cast<PackedVector2Array *>(&ret);
	array->resize(p_length);
	Vector2 *dst = array->ptrw();
	memcpy(dst, p_src, p_length * sizeof(Vector2));
	return ret;
}

godot_packed_array godotsharp_packed_vector3_array_new_mem_copy(const Vector3 *p_src, int32_t p_length) {
	godot_packed_array ret;
	memnew_placement(&ret, PackedVector3Array);
	PackedVector3Array *array = reinterpret_cast<PackedVector3Array *>(&ret);
	array->resize(p_length);
	Vector3 *dst = array->ptrw();
	memcpy(dst, p_src, p_length * sizeof(Vector3));
	return ret;
}

godot_packed_array godotsharp_packed_vector4_array_new_mem_copy(const Vector4 *p_src, int32_t p_length) {
	godot_packed_array ret;
	memnew_placement(&ret, PackedVector4Array);
	PackedVector4Array *array = reinterpret_cast<PackedVector4Array *>(&ret);
	array->resize(p_length);
	Vector4 *dst = array->ptrw();
	memcpy(dst, p_src, p_length * sizeof(Vector4));
	return ret;
}

godot_packed_array godotsharp_packed_color_array_new_mem_copy(const Color *p_src, int32_t p_length) {
	godot_packed_array ret;
	memnew_placement(&ret, PackedColorArray);
	PackedColorArray *array = reinterpret_cast<PackedColorArray *>(&ret);
	array->resize(p_length);
	Color *dst = array->ptrw();
	memcpy(dst, p_src, p_length * sizeof(Color));
	return ret;
}

void godotsharp_packed_string_array_add(PackedStringArray *r_dest, const String *p_element) {
	r_dest->append(*p_element);
}

void godotsharp_callable_new_with_delegate(GCHandleIntPtr p_delegate_handle, void *p_trampoline,
		const Object *p_object, Callable *r_callable) {
	// TODO: Use pooling for ManagedCallable instances.
	ObjectID objid = p_object ? p_object->get_instance_id() : ObjectID();
	CallableCustom *managed_callable = memnew(ManagedCallable(p_delegate_handle, p_trampoline, objid));
	memnew_placement(r_callable, Callable(managed_callable));
}

bool godotsharp_callable_get_data_for_marshalling(const Callable *p_callable,
		GCHandleIntPtr *r_delegate_handle, void **r_trampoline, Object **r_object, StringName *r_name) {
	if (p_callable->is_custom()) {
		CallableCustom *custom = p_callable->get_custom();
		CallableCustom::CompareEqualFunc compare_equal_func = custom->get_compare_equal_func();

		if (compare_equal_func == ManagedCallable::compare_equal_func_ptr) {
			ManagedCallable *managed_callable = static_cast<ManagedCallable *>(custom);
			*r_delegate_handle = managed_callable->get_delegate();
			*r_trampoline = managed_callable->get_trampoline();
			*r_object = nullptr;
			memnew_placement(r_name, StringName());
			return true;
		} else if (compare_equal_func == SignalAwaiterCallable::compare_equal_func_ptr) {
			SignalAwaiterCallable *signal_awaiter_callable = static_cast<SignalAwaiterCallable *>(custom);
			*r_delegate_handle = { nullptr };
			*r_trampoline = nullptr;
			*r_object = ObjectDB::get_instance(signal_awaiter_callable->get_object());
			memnew_placement(r_name, StringName(signal_awaiter_callable->get_signal()));
			return true;
		} else if (compare_equal_func == EventSignalCallable::compare_equal_func_ptr) {
			EventSignalCallable *event_signal_callable = static_cast<EventSignalCallable *>(custom);
			*r_delegate_handle = { nullptr };
			*r_trampoline = nullptr;
			*r_object = ObjectDB::get_instance(event_signal_callable->get_object());
			memnew_placement(r_name, StringName(event_signal_callable->get_signal()));
			return true;
		}

		// Some other CallableCustom. We only support ManagedCallable.
		*r_delegate_handle = { nullptr };
		*r_trampoline = nullptr;
		*r_object = nullptr;
		memnew_placement(r_name, StringName());
		return false;
	} else {
		*r_delegate_handle = { nullptr };
		*r_trampoline = nullptr;
		*r_object = ObjectDB::get_instance(p_callable->get_object_id());
		memnew_placement(r_name, StringName(p_callable->get_method()));
		return true;
	}
}

godot_variant godotsharp_callable_call(Callable *p_callable, const Variant **p_args, const int32_t p_arg_count, Callable::CallError *p_call_error) {
	godot_variant ret;
	memnew_placement(&ret, Variant);

	Variant *ret_val = (Variant *)&ret;

	p_callable->callp(p_args, p_arg_count, *ret_val, *p_call_error);

	return ret;
}

void godotsharp_callable_call_deferred(Callable *p_callable, const Variant **p_args, const int32_t p_arg_count) {
	p_callable->call_deferredp(p_args, p_arg_count);
}

godot_color godotsharp_color_from_ok_hsl(float p_h, float p_s, float p_l, float p_alpha) {
	godot_color ret;
	Color *dest = (Color *)&ret;
	memnew_placement(dest, Color(Color::from_ok_hsl(p_h, p_s, p_l, p_alpha)));
	return ret;
}

// GDNative functions

// gdnative.h

void godotsharp_method_bind_ptrcall(MethodBind *p_method_bind, Object *p_instance, const void **p_args, void *p_ret) {
	p_method_bind->ptrcall(p_instance, p_args, p_ret);
}

godot_variant godotsharp_method_bind_call(MethodBind *p_method_bind, Object *p_instance, const godot_variant **p_args, const int32_t p_arg_count, Callable::CallError *p_call_error) {
	godot_variant ret;
	memnew_placement(&ret, Variant());

	Variant *ret_val = (Variant *)&ret;

	*ret_val = p_method_bind->call(p_instance, (const Variant **)p_args, p_arg_count, *p_call_error);

	return ret;
}

// variant.h

void godotsharp_variant_new_copy(godot_variant *r_dest, const Variant *p_src) {
	memnew_placement(r_dest, Variant(*p_src));
}

void godotsharp_variant_new_string_name(godot_variant *r_dest, const StringName *p_s) {
	memnew_placement(r_dest, Variant(*p_s));
}

void godotsharp_variant_new_node_path(godot_variant *r_dest, const NodePath *p_np) {
	memnew_placement(r_dest, Variant(*p_np));
}

void godotsharp_variant_new_object(godot_variant *r_dest, const Object *p_obj) {
	memnew_placement(r_dest, Variant(p_obj));
}

void godotsharp_variant_new_transform2d(godot_variant *r_dest, const Transform2D *p_t2d) {
	memnew_placement(r_dest, Variant(*p_t2d));
}

void godotsharp_variant_new_basis(godot_variant *r_dest, const Basis *p_basis) {
	memnew_placement(r_dest, Variant(*p_basis));
}

void godotsharp_variant_new_transform3d(godot_variant *r_dest, const Transform3D *p_trans) {
	memnew_placement(r_dest, Variant(*p_trans));
}

void godotsharp_variant_new_projection(godot_variant *r_dest, const Projection *p_proj) {
	memnew_placement(r_dest, Variant(*p_proj));
}

void godotsharp_variant_new_aabb(godot_variant *r_dest, const AABB *p_aabb) {
	memnew_placement(r_dest, Variant(*p_aabb));
}

void godotsharp_variant_new_dictionary(godot_variant *r_dest, const Dictionary *p_dict) {
	memnew_placement(r_dest, Variant(*p_dict));
}

void godotsharp_variant_new_array(godot_variant *r_dest, const Array *p_arr) {
	memnew_placement(r_dest, Variant(*p_arr));
}

void godotsharp_variant_new_packed_byte_array(godot_variant *r_dest, const PackedByteArray *p_pba) {
	memnew_placement(r_dest, Variant(*p_pba));
}

void godotsharp_variant_new_packed_int32_array(godot_variant *r_dest, const PackedInt32Array *p_pia) {
	memnew_placement(r_dest, Variant(*p_pia));
}

void godotsharp_variant_new_packed_int64_array(godot_variant *r_dest, const PackedInt64Array *p_pia) {
	memnew_placement(r_dest, Variant(*p_pia));
}

void godotsharp_variant_new_packed_float32_array(godot_variant *r_dest, const PackedFloat32Array *p_pra) {
	memnew_placement(r_dest, Variant(*p_pra));
}

void godotsharp_variant_new_packed_float64_array(godot_variant *r_dest, const PackedFloat64Array *p_pra) {
	memnew_placement(r_dest, Variant(*p_pra));
}

void godotsharp_variant_new_packed_string_array(godot_variant *r_dest, const PackedStringArray *p_psa) {
	memnew_placement(r_dest, Variant(*p_psa));
}

void godotsharp_variant_new_packed_vector2_array(godot_variant *r_dest, const PackedVector2Array *p_pv2a) {
	memnew_placement(r_dest, Variant(*p_pv2a));
}

void godotsharp_variant_new_packed_vector3_array(godot_variant *r_dest, const PackedVector3Array *p_pv3a) {
	memnew_placement(r_dest, Variant(*p_pv3a));
}

void godotsharp_variant_new_packed_vector4_array(godot_variant *r_dest, const PackedVector4Array *p_pv4a) {
	memnew_placement(r_dest, Variant(*p_pv4a));
}

void godotsharp_variant_new_packed_color_array(godot_variant *r_dest, const PackedColorArray *p_pca) {
	memnew_placement(r_dest, Variant(*p_pca));
}

bool godotsharp_variant_as_bool(const Variant *p_self) {
	return p_self->operator bool();
}

int64_t godotsharp_variant_as_int(const Variant *p_self) {
	return p_self->operator int64_t();
}

double godotsharp_variant_as_float(const Variant *p_self) {
	return p_self->operator double();
}

godot_string godotsharp_variant_as_string(const Variant *p_self) {
	godot_string raw_dest;
	String *dest = (String *)&raw_dest;
	memnew_placement(dest, String(p_self->operator String()));
	return raw_dest;
}

godot_vector2 godotsharp_variant_as_vector2(const Variant *p_self) {
	godot_vector2 raw_dest;
	Vector2 *dest = (Vector2 *)&raw_dest;
	memnew_placement(dest, Vector2(p_self->operator Vector2()));
	return raw_dest;
}

godot_vector2i godotsharp_variant_as_vector2i(const Variant *p_self) {
	godot_vector2i raw_dest;
	Vector2i *dest = (Vector2i *)&raw_dest;
	memnew_placement(dest, Vector2i(p_self->operator Vector2i()));
	return raw_dest;
}

godot_rect2 godotsharp_variant_as_rect2(const Variant *p_self) {
	godot_rect2 raw_dest;
	Rect2 *dest = (Rect2 *)&raw_dest;
	memnew_placement(dest, Rect2(p_self->operator Rect2()));
	return raw_dest;
}

godot_rect2i godotsharp_variant_as_rect2i(const Variant *p_self) {
	godot_rect2i raw_dest;
	Rect2i *dest = (Rect2i *)&raw_dest;
	memnew_placement(dest, Rect2i(p_self->operator Rect2i()));
	return raw_dest;
}

godot_vector3 godotsharp_variant_as_vector3(const Variant *p_self) {
	godot_vector3 raw_dest;
	Vector3 *dest = (Vector3 *)&raw_dest;
	memnew_placement(dest, Vector3(p_self->operator Vector3()));
	return raw_dest;
}

godot_vector3i godotsharp_variant_as_vector3i(const Variant *p_self) {
	godot_vector3i raw_dest;
	Vector3i *dest = (Vector3i *)&raw_dest;
	memnew_placement(dest, Vector3i(p_self->operator Vector3i()));
	return raw_dest;
}

godot_transform2d godotsharp_variant_as_transform2d(const Variant *p_self) {
	godot_transform2d raw_dest;
	Transform2D *dest = (Transform2D *)&raw_dest;
	memnew_placement(dest, Transform2D(p_self->operator Transform2D()));
	return raw_dest;
}

godot_vector4 godotsharp_variant_as_vector4(const Variant *p_self) {
	godot_vector4 raw_dest;
	Vector4 *dest = (Vector4 *)&raw_dest;
	memnew_placement(dest, Vector4(p_self->operator Vector4()));
	return raw_dest;
}

godot_vector4i godotsharp_variant_as_vector4i(const Variant *p_self) {
	godot_vector4i raw_dest;
	Vector4i *dest = (Vector4i *)&raw_dest;
	memnew_placement(dest, Vector4i(p_self->operator Vector4i()));
	return raw_dest;
}

godot_plane godotsharp_variant_as_plane(const Variant *p_self) {
	godot_plane raw_dest;
	Plane *dest = (Plane *)&raw_dest;
	memnew_placement(dest, Plane(p_self->operator Plane()));
	return raw_dest;
}

godot_quaternion godotsharp_variant_as_quaternion(const Variant *p_self) {
	godot_quaternion raw_dest;
	Quaternion *dest = (Quaternion *)&raw_dest;
	memnew_placement(dest, Quaternion(p_self->operator Quaternion()));
	return raw_dest;
}

godot_aabb godotsharp_variant_as_aabb(const Variant *p_self) {
	godot_aabb raw_dest;
	AABB *dest = (AABB *)&raw_dest;
	memnew_placement(dest, AABB(p_self->operator ::AABB()));
	return raw_dest;
}

godot_basis godotsharp_variant_as_basis(const Variant *p_self) {
	godot_basis raw_dest;
	Basis *dest = (Basis *)&raw_dest;
	memnew_placement(dest, Basis(p_self->operator Basis()));
	return raw_dest;
}

godot_transform3d godotsharp_variant_as_transform3d(const Variant *p_self) {
	godot_transform3d raw_dest;
	Transform3D *dest = (Transform3D *)&raw_dest;
	memnew_placement(dest, Transform3D(p_self->operator Transform3D()));
	return raw_dest;
}

godot_projection godotsharp_variant_as_projection(const Variant *p_self) {
	godot_projection raw_dest;
	Projection *dest = (Projection *)&raw_dest;
	memnew_placement(dest, Projection(p_self->operator Projection()));
	return raw_dest;
}

godot_color godotsharp_variant_as_color(const Variant *p_self) {
	godot_color raw_dest;
	Color *dest = (Color *)&raw_dest;
	memnew_placement(dest, Color(p_self->operator Color()));
	return raw_dest;
}

godot_string_name godotsharp_variant_as_string_name(const Variant *p_self) {
	godot_string_name raw_dest;
	StringName *dest = (StringName *)&raw_dest;
	memnew_placement(dest, StringName(p_self->operator StringName()));
	return raw_dest;
}

godot_node_path godotsharp_variant_as_node_path(const Variant *p_self) {
	godot_node_path raw_dest;
	NodePath *dest = (NodePath *)&raw_dest;
	memnew_placement(dest, NodePath(p_self->operator NodePath()));
	return raw_dest;
}

godot_rid godotsharp_variant_as_rid(const Variant *p_self) {
	godot_rid raw_dest;
	RID *dest = (RID *)&raw_dest;
	memnew_placement(dest, RID(p_self->operator ::RID()));
	return raw_dest;
}

godot_callable godotsharp_variant_as_callable(const Variant *p_self) {
	godot_callable raw_dest;
	Callable *dest = (Callable *)&raw_dest;
	memnew_placement(dest, Callable(p_self->operator Callable()));
	return raw_dest;
}

godot_signal godotsharp_variant_as_signal(const Variant *p_self) {
	godot_signal raw_dest;
	Signal *dest = (Signal *)&raw_dest;
	memnew_placement(dest, Signal(p_self->operator Signal()));
	return raw_dest;
}

godot_dictionary godotsharp_variant_as_dictionary(const Variant *p_self) {
	godot_dictionary raw_dest;
	Dictionary *dest = (Dictionary *)&raw_dest;
	memnew_placement(dest, Dictionary(p_self->operator Dictionary()));
	return raw_dest;
}

godot_array godotsharp_variant_as_array(const Variant *p_self) {
	godot_array raw_dest;
	Array *dest = (Array *)&raw_dest;
	memnew_placement(dest, Array(p_self->operator Array()));
	return raw_dest;
}

godot_packed_array godotsharp_variant_as_packed_byte_array(const Variant *p_self) {
	godot_packed_array raw_dest;
	PackedByteArray *dest = (PackedByteArray *)&raw_dest;
	memnew_placement(dest, PackedByteArray(p_self->operator PackedByteArray()));
	return raw_dest;
}

godot_packed_array godotsharp_variant_as_packed_int32_array(const Variant *p_self) {
	godot_packed_array raw_dest;
	PackedInt32Array *dest = (PackedInt32Array *)&raw_dest;
	memnew_placement(dest, PackedInt32Array(p_self->operator PackedInt32Array()));
	return raw_dest;
}

godot_packed_array godotsharp_variant_as_packed_int64_array(const Variant *p_self) {
	godot_packed_array raw_dest;
	PackedInt64Array *dest = (PackedInt64Array *)&raw_dest;
	memnew_placement(dest, PackedInt64Array(p_self->operator PackedInt64Array()));
	return raw_dest;
}

godot_packed_array godotsharp_variant_as_packed_float32_array(const Variant *p_self) {
	godot_packed_array raw_dest;
	PackedFloat32Array *dest = (PackedFloat32Array *)&raw_dest;
	memnew_placement(dest, PackedFloat32Array(p_self->operator PackedFloat32Array()));
	return raw_dest;
}

godot_packed_array godotsharp_variant_as_packed_float64_array(const Variant *p_self) {
	godot_packed_array raw_dest;
	PackedFloat64Array *dest = (PackedFloat64Array *)&raw_dest;
	memnew_placement(dest, PackedFloat64Array(p_self->operator PackedFloat64Array()));
	return raw_dest;
}

godot_packed_array godotsharp_variant_as_packed_string_array(const Variant *p_self) {
	godot_packed_array raw_dest;
	PackedStringArray *dest = (PackedStringArray *)&raw_dest;
	memnew_placement(dest, PackedStringArray(p_self->operator PackedStringArray()));
	return raw_dest;
}

godot_packed_array godotsharp_variant_as_packed_vector2_array(const Variant *p_self) {
	godot_packed_array raw_dest;
	PackedVector2Array *dest = (PackedVector2Array *)&raw_dest;
	memnew_placement(dest, PackedVector2Array(p_self->operator PackedVector2Array()));
	return raw_dest;
}

godot_packed_array godotsharp_variant_as_packed_vector3_array(const Variant *p_self) {
	godot_packed_array raw_dest;
	PackedVector3Array *dest = (PackedVector3Array *)&raw_dest;
	memnew_placement(dest, PackedVector3Array(p_self->operator PackedVector3Array()));
	return raw_dest;
}

godot_packed_array godotsharp_variant_as_packed_vector4_array(const Variant *p_self) {
	godot_packed_array raw_dest;
	PackedVector4Array *dest = (PackedVector4Array *)&raw_dest;
	memnew_placement(dest, PackedVector4Array(p_self->operator PackedVector4Array()));
	return raw_dest;
}

godot_packed_array godotsharp_variant_as_packed_color_array(const Variant *p_self) {
	godot_packed_array raw_dest;
	PackedColorArray *dest = (PackedColorArray *)&raw_dest;
	memnew_placement(dest, PackedColorArray(p_self->operator PackedColorArray()));
	return raw_dest;
}

bool godotsharp_variant_equals(const godot_variant *p_a, const godot_variant *p_b) {
	return *reinterpret_cast<const Variant *>(p_a) == *reinterpret_cast<const Variant *>(p_b);
}

// string.h

void godotsharp_string_new_with_utf16_chars(String *r_dest, const char16_t *p_contents) {
	memnew_placement(r_dest, String());
	r_dest->parse_utf16(p_contents);
}

// string_name.h

void godotsharp_string_name_new_copy(StringName *r_dest, const StringName *p_src) {
	memnew_placement(r_dest, StringName(*p_src));
}

// node_path.h

void godotsharp_node_path_new_copy(NodePath *r_dest, const NodePath *p_src) {
	memnew_placement(r_dest, NodePath(*p_src));
}

// array.h

void godotsharp_array_new(Array *r_dest) {
	memnew_placement(r_dest, Array);
}

void godotsharp_array_new_copy(Array *r_dest, const Array *p_src) {
	memnew_placement(r_dest, Array(*p_src));
}

godot_variant *godotsharp_array_ptrw(godot_array *p_self) {
	return reinterpret_cast<godot_variant *>(&reinterpret_cast<Array *>(p_self)->operator[](0));
}

// dictionary.h

void godotsharp_dictionary_new(Dictionary *r_dest) {
	memnew_placement(r_dest, Dictionary);
}

void godotsharp_dictionary_new_copy(Dictionary *r_dest, const Dictionary *p_src) {
	memnew_placement(r_dest, Dictionary(*p_src));
}

// destroy functions

void godotsharp_packed_byte_array_destroy(PackedByteArray *p_self) {
	p_self->~PackedByteArray();
}

void godotsharp_packed_int32_array_destroy(PackedInt32Array *p_self) {
	p_self->~PackedInt32Array();
}

void godotsharp_packed_int64_array_destroy(PackedInt64Array *p_self) {
	p_self->~PackedInt64Array();
}

void godotsharp_packed_float32_array_destroy(PackedFloat32Array *p_self) {
	p_self->~PackedFloat32Array();
}

void godotsharp_packed_float64_array_destroy(PackedFloat64Array *p_self) {
	p_self->~PackedFloat64Array();
}

void godotsharp_packed_string_array_destroy(PackedStringArray *p_self) {
	p_self->~PackedStringArray();
}

void godotsharp_packed_vector2_array_destroy(PackedVector2Array *p_self) {
	p_self->~PackedVector2Array();
}

void godotsharp_packed_vector3_array_destroy(PackedVector3Array *p_self) {
	p_self->~PackedVector3Array();
}

void godotsharp_packed_vector4_array_destroy(PackedVector4Array *p_self) {
	p_self->~PackedVector4Array();
}

void godotsharp_packed_color_array_destroy(PackedColorArray *p_self) {
	p_self->~PackedColorArray();
}

void godotsharp_variant_destroy(Variant *p_self) {
	p_self->~Variant();
}

void godotsharp_string_destroy(String *p_self) {
	p_self->~String();
}

void godotsharp_string_name_destroy(StringName *p_self) {
	p_self->~StringName();
}

void godotsharp_node_path_destroy(NodePath *p_self) {
	p_self->~NodePath();
}

void godotsharp_signal_destroy(Signal *p_self) {
	p_self->~Signal();
}

void godotsharp_callable_destroy(Callable *p_self) {
	p_self->~Callable();
}

void godotsharp_array_destroy(Array *p_self) {
	p_self->~Array();
}

void godotsharp_dictionary_destroy(Dictionary *p_self) {
	p_self->~Dictionary();
}

// Array

int32_t godotsharp_array_add(Array *p_self, const Variant *p_item) {
	p_self->append(*p_item);
	return p_self->size();
}

int32_t godotsharp_array_add_range(Array *p_self, const Array *p_collection) {
	p_self->append_array(*p_collection);
	return p_self->size();
}

int32_t godotsharp_array_binary_search(const Array *p_self, int32_t p_index, int32_t p_length, const Variant *p_value) {
	ERR_FAIL_COND_V(p_index < 0, -1);
	ERR_FAIL_COND_V(p_length < 0, -1);
	ERR_FAIL_COND_V(p_self->size() - p_index < p_length, -1);

	const Variant &value = *p_value;
	const Array &array = *p_self;

	int lo = p_index;
	int hi = p_index + p_length - 1;
	while (lo <= hi) {
		int mid = lo + ((hi - lo) >> 1);
		const Variant &mid_item = array[mid];

		if (mid_item == value) {
			return mid;
		}
		if (mid_item < value) {
			lo = mid + 1;
		} else {
			hi = mid - 1;
		}
	}

	return ~lo;
}

void godotsharp_array_duplicate(const Array *p_self, bool p_deep, Array *r_dest) {
	memnew_placement(r_dest, Array(p_self->duplicate(p_deep)));
}

void godotsharp_array_fill(Array *p_self, const Variant *p_value) {
	p_self->fill(*p_value);
}

int32_t godotsharp_array_index_of(const Array *p_self, const Variant *p_item, int32_t p_index = 0) {
	return p_self->find(*p_item, p_index);
}

void godotsharp_array_insert(Array *p_self, int32_t p_index, const Variant *p_item) {
	p_self->insert(p_index, *p_item);
}

int32_t godotsharp_array_last_index_of(const Array *p_self, const Variant *p_item, int32_t p_index) {
	return p_self->rfind(*p_item, p_index);
}

void godotsharp_array_make_read_only(Array *p_self) {
	p_self->make_read_only();
}

void godotsharp_array_set_typed(Array *p_self, uint32_t p_elem_type, const StringName *p_elem_class_name, const Ref<CSharpScript> *p_elem_script) {
	Variant elem_script_variant;
	StringName elem_class_name = *p_elem_class_name;
	if (p_elem_script && p_elem_script->is_valid()) {
		elem_script_variant = Variant(p_elem_script->ptr());
		elem_class_name = p_elem_script->ptr()->get_instance_base_type();
	}
	p_self->set_typed(p_elem_type, elem_class_name, p_elem_script->ptr());
}

bool godotsharp_array_is_typed(const Array *p_self) {
	return p_self->is_typed();
}

void godotsharp_array_max(const Array *p_self, Variant *r_value) {
	*r_value = p_self->max();
}

void godotsharp_array_min(const Array *p_self, Variant *r_value) {
	*r_value = p_self->min();
}

void godotsharp_array_pick_random(const Array *p_self, Variant *r_value) {
	*r_value = p_self->pick_random();
}

bool godotsharp_array_recursive_equal(const Array *p_self, const Array *p_other) {
	return p_self->recursive_equal(*p_other, 0);
}

void godotsharp_array_remove_at(Array *p_self, int32_t p_index) {
	p_self->remove_at(p_index);
}

int32_t godotsharp_array_resize(Array *p_self, int32_t p_new_size) {
	return (int32_t)p_self->resize(p_new_size);
}

void godotsharp_array_reverse(Array *p_self) {
	p_self->reverse();
}

void godotsharp_array_shuffle(Array *p_self) {
	p_self->shuffle();
}

void godotsharp_array_slice(Array *p_self, int32_t p_start, int32_t p_end, int32_t p_step, bool p_deep, Array *r_dest) {
	memnew_placement(r_dest, Array(p_self->slice(p_start, p_end, p_step, p_deep)));
}

void godotsharp_array_sort(Array *p_self) {
	p_self->sort();
}

void godotsharp_array_to_string(const Array *p_self, String *r_str) {
	*r_str = Variant(*p_self).operator String();
}

// Dictionary

bool godotsharp_dictionary_try_get_value(const Dictionary *p_self, const Variant *p_key, Variant *r_value) {
	const Variant *ret = p_self->getptr(*p_key);
	if (ret == nullptr) {
		memnew_placement(r_value, Variant());
		return false;
	}
	memnew_placement(r_value, Variant(*ret));
	return true;
}

void godotsharp_dictionary_set_value(Dictionary *p_self, const Variant *p_key, const Variant *p_value) {
	p_self->operator[](*p_key) = *p_value;
}

void godotsharp_dictionary_keys(const Dictionary *p_self, Array *r_dest) {
	memnew_placement(r_dest, Array(p_self->keys()));
}

void godotsharp_dictionary_values(const Dictionary *p_self, Array *r_dest) {
	memnew_placement(r_dest, Array(p_self->values()));
}

int32_t godotsharp_dictionary_count(const Dictionary *p_self) {
	return p_self->size();
}

void godotsharp_dictionary_key_value_pair_at(const Dictionary *p_self, int32_t p_index, Variant *r_key, Variant *r_value) {
	memnew_placement(r_key, Variant(p_self->get_key_at_index(p_index)));
	memnew_placement(r_value, Variant(p_self->get_value_at_index(p_index)));
}

void godotsharp_dictionary_add(Dictionary *p_self, const Variant *p_key, const Variant *p_value) {
	p_self->operator[](*p_key) = *p_value;
}

void godotsharp_dictionary_clear(Dictionary *p_self) {
	p_self->clear();
}

bool godotsharp_dictionary_contains_key(const Dictionary *p_self, const Variant *p_key) {
	return p_self->has(*p_key);
}

void godotsharp_dictionary_duplicate(const Dictionary *p_self, bool p_deep, Dictionary *r_dest) {
	memnew_placement(r_dest, Dictionary(p_self->duplicate(p_deep)));
}

void godotsharp_dictionary_merge(Dictionary *p_self, const Dictionary *p_dictionary, bool p_overwrite) {
	p_self->merge(*p_dictionary, p_overwrite);
}

bool godotsharp_dictionary_recursive_equal(const Dictionary *p_self, const Dictionary *p_other) {
	return p_self->recursive_equal(*p_other, 0);
}

bool godotsharp_dictionary_remove_key(Dictionary *p_self, const Variant *p_key) {
	return p_self->erase(*p_key);
}

void godotsharp_dictionary_make_read_only(Dictionary *p_self) {
	p_self->make_read_only();
}

void godotsharp_dictionary_set_typed(Dictionary *p_self, uint32_t p_key_type, const StringName *p_key_class_name, const Ref<CSharpScript> *p_key_script, uint32_t p_value_type, const StringName *p_value_class_name, const Ref<CSharpScript> *p_value_script) {
	Variant key_script_variant;
	StringName key_class_name = *p_key_class_name;
	if (p_key_script && p_key_script->is_valid()) {
		key_script_variant = Variant(p_key_script->ptr());
		key_class_name = p_key_script->ptr()->get_instance_base_type();
	}
	Variant value_script_variant;
	StringName value_class_name = *p_value_class_name;
	if (p_value_script && p_value_script->is_valid()) {
		value_script_variant = Variant(p_value_script->ptr());
		value_class_name = p_value_script->ptr()->get_instance_base_type();
	}
	p_self->set_typed(p_key_type, key_class_name, p_key_script->ptr(), p_value_type, value_class_name, p_value_script->ptr());
}

bool godotsharp_dictionary_is_typed_key(const Dictionary *p_self) {
	return p_self->is_typed_key();
}

bool godotsharp_dictionary_is_typed_value(const Dictionary *p_self) {
	return p_self->is_typed_value();
}

uint32_t godotsharp_dictionary_get_typed_key_builtin(const Dictionary *p_self) {
	return p_self->get_typed_key_builtin();
}

uint32_t godotsharp_dictionary_get_typed_value_builtin(const Dictionary *p_self) {
	return p_self->get_typed_value_builtin();
}

void godotsharp_dictionary_get_typed_key_class_name(const Dictionary *p_self, StringName *r_dest) {
	memnew_placement(r_dest, StringName(p_self->get_typed_key_class_name()));
}

void godotsharp_dictionary_get_typed_value_class_name(const Dictionary *p_self, StringName *r_dest) {
	memnew_placement(r_dest, StringName(p_self->get_typed_value_class_name()));
}

void godotsharp_dictionary_get_typed_key_script(const Dictionary *p_self, Variant *r_dest) {
	memnew_placement(r_dest, Variant(p_self->get_typed_key_script()));
}

void godotsharp_dictionary_get_typed_value_script(const Dictionary *p_self, Variant *r_dest) {
	memnew_placement(r_dest, Variant(p_self->get_typed_value_script()));
}

void godotsharp_dictionary_to_string(const Dictionary *p_self, String *r_str) {
	*r_str = Variant(*p_self).operator String();
}

void godotsharp_string_simplify_path(const String *p_self, String *r_simplified_path) {
	memnew_placement(r_simplified_path, String(p_self->simplify_path()));
}

void godotsharp_string_to_camel_case(const String *p_self, String *r_camel_case) {
	memnew_placement(r_camel_case, String(p_self->to_camel_case()));
}

void godotsharp_string_to_pascal_case(const String *p_self, String *r_pascal_case) {
	memnew_placement(r_pascal_case, String(p_self->to_pascal_case()));
}

void godotsharp_string_to_snake_case(const String *p_self, String *r_snake_case) {
	memnew_placement(r_snake_case, String(p_self->to_snake_case()));
}

void godotsharp_node_path_get_as_property_path(const NodePath *p_ptr, NodePath *r_dest) {
	memnew_placement(r_dest, NodePath(p_ptr->get_as_property_path()));
}

void godotsharp_node_path_get_concatenated_names(const NodePath *p_self, String *r_subnames) {
	memnew_placement(r_subnames, String(p_self->get_concatenated_names()));
}

void godotsharp_node_path_get_concatenated_subnames(const NodePath *p_self, String *r_subnames) {
	memnew_placement(r_subnames, String(p_self->get_concatenated_subnames()));
}

void godotsharp_node_path_get_name(const NodePath *p_self, uint32_t p_idx, String *r_name) {
	memnew_placement(r_name, String(p_self->get_name(p_idx)));
}

int32_t godotsharp_node_path_get_name_count(const NodePath *p_self) {
	return p_self->get_name_count();
}

void godotsharp_node_path_get_subname(const NodePath *p_self, uint32_t p_idx, String *r_subname) {
	memnew_placement(r_subname, String(p_self->get_subname(p_idx)));
}

int32_t godotsharp_node_path_get_subname_count(const NodePath *p_self) {
	return p_self->get_subname_count();
}

bool godotsharp_node_path_is_absolute(const NodePath *p_self) {
	return p_self->is_absolute();
}

bool godotsharp_node_path_equals(const NodePath *p_self, const NodePath *p_other) {
	return *p_self == *p_other;
}

int godotsharp_node_path_hash(const NodePath *p_self) {
	return p_self->hash();
}

void godotsharp_randomize() {
	Math::randomize();
}

uint32_t godotsharp_randi() {
	return Math::rand();
}

float godotsharp_randf() {
	return Math::randf();
}

int32_t godotsharp_randi_range(int32_t p_from, int32_t p_to) {
	return Math::random(p_from, p_to);
}

double godotsharp_randf_range(double p_from, double p_to) {
	return Math::random(p_from, p_to);
}

double godotsharp_randfn(double p_mean, double p_deviation) {
	return Math::randfn(p_mean, p_deviation);
}

void godotsharp_seed(uint64_t p_seed) {
	Math::seed(p_seed);
}

uint32_t godotsharp_rand_from_seed(uint64_t p_seed, uint64_t *r_new_seed) {
	uint32_t ret = Math::rand_from_seed(&p_seed);
	*r_new_seed = p_seed;
	return ret;
}

void godotsharp_weakref(Object *p_ptr, Ref<RefCounted> *r_weak_ref) {
	if (!p_ptr) {
		return;
	}

	Ref<WeakRef> wref;
	RefCounted *rc = Object::cast_to<RefCounted>(p_ptr);

	if (rc) {
		Ref<RefCounted> r = rc;
		if (r.is_null()) {
			return;
		}

		wref.instantiate();
		wref->set_ref(r);
	} else {
		wref.instantiate();
		wref->set_obj(p_ptr);
	}

	memnew_placement(r_weak_ref, Ref<RefCounted>(wref));
}

void godotsharp_print(const godot_string *p_what) {
	print_line(*reinterpret_cast<const String *>(p_what));
}

void godotsharp_print_rich(const godot_string *p_what) {
	print_line_rich(*reinterpret_cast<const String *>(p_what));
}

void godotsharp_printerr(const godot_string *p_what) {
	print_error(*reinterpret_cast<const String *>(p_what));
}

void godotsharp_printt(const godot_string *p_what) {
	print_line(*reinterpret_cast<const String *>(p_what));
}

void godotsharp_prints(const godot_string *p_what) {
	print_line(*reinterpret_cast<const String *>(p_what));
}

void godotsharp_printraw(const godot_string *p_what) {
	OS::get_singleton()->print("%s", reinterpret_cast<const String *>(p_what)->utf8().get_data());
}

void godotsharp_err_print_error(const godot_string *p_function, const godot_string *p_file, int32_t p_line, const godot_string *p_error, const godot_string *p_message, bool p_editor_notify, ErrorHandlerType p_type) {
	_err_print_error(
			reinterpret_cast<const String *>(p_function)->utf8().get_data(),
			reinterpret_cast<const String *>(p_file)->utf8().get_data(),
			p_line,
			reinterpret_cast<const String *>(p_error)->utf8().get_data(),
			reinterpret_cast<const String *>(p_message)->utf8().get_data(),
			p_editor_notify, p_type);
}

void godotsharp_var_to_str(const godot_variant *p_var, godot_string *r_ret) {
	const Variant &var = *reinterpret_cast<const Variant *>(p_var);
	String &vars = *memnew_placement(r_ret, String);
	VariantWriter::write_to_string(var, vars);
}

void godotsharp_str_to_var(const godot_string *p_str, godot_variant *r_ret) {
	Variant ret;

	VariantParser::StreamString ss;
	ss.s = *reinterpret_cast<const String *>(p_str);

	String errs;
	int line;
	Error err = VariantParser::parse(&ss, ret, errs, line);
	if (err != OK) {
		String err_str = "Parse error at line " + itos(line) + ": " + errs + ".";
		ERR_PRINT(err_str);
		ret = err_str;
	}
	memnew_placement(r_ret, Variant(ret));
}

void godotsharp_var_to_bytes(const godot_variant *p_var, bool p_full_objects, godot_packed_array *r_bytes) {
	const Variant &var = *reinterpret_cast<const Variant *>(p_var);
	PackedByteArray &bytes = *memnew_placement(r_bytes, PackedByteArray);

	int len;
	Error err = encode_variant(var, nullptr, len, p_full_objects);
	ERR_FAIL_COND_MSG(err != OK, "Unexpected error encoding variable to bytes, likely unserializable type found (Object or RID).");

	bytes.resize(len);
	encode_variant(var, bytes.ptrw(), len, p_full_objects);
}

void godotsharp_bytes_to_var(const godot_packed_array *p_bytes, bool p_allow_objects, godot_variant *r_ret) {
	const PackedByteArray *bytes = reinterpret_cast<const PackedByteArray *>(p_bytes);
	Variant ret;
	Error err = decode_variant(ret, bytes->ptr(), bytes->size(), nullptr, p_allow_objects);
	if (err != OK) {
		ret = RTR("Not enough bytes for decoding bytes, or invalid format.");
	}
	memnew_placement(r_ret, Variant(ret));
}

int godotsharp_hash(const godot_variant *p_var) {
	return reinterpret_cast<const Variant *>(p_var)->hash();
}

void godotsharp_convert(const godot_variant *p_what, int32_t p_type, godot_variant *r_ret) {
	const Variant *args[1] = { reinterpret_cast<const Variant *>(p_what) };
	Callable::CallError ce;
	Variant ret;
	Variant::construct(Variant::Type(p_type), ret, args, 1, ce);
	if (ce.error != Callable::CallError::CALL_OK) {
		memnew_placement(r_ret, Variant);
		ERR_FAIL_MSG("Unable to convert parameter from '" +
				Variant::get_type_name(reinterpret_cast<const Variant *>(p_what)->get_type()) +
				"' to '" + Variant::get_type_name(Variant::Type(p_type)) + "'.");
	}
	memnew_placement(r_ret, Variant(ret));
}

Object *godotsharp_instance_from_id(uint64_t p_instance_id) {
	return ObjectDB::get_instance(ObjectID(p_instance_id));
}

void godotsharp_object_to_string(Object *p_ptr, godot_string *r_str) {
#ifdef DEBUG_ENABLED
	// Cannot happen in C#; would get an ObjectDisposedException instead.
	CRASH_COND(p_ptr == nullptr);
#endif
	// Can't call 'Object::to_string()' here, as that can end up calling 'ToString' again resulting in an endless circular loop.
	memnew_placement(r_str,
			String("<" + p_ptr->get_class() + "#" + itos(p_ptr->get_instance_id()) + ">"));
}

#ifdef __cplusplus
}
#endif

// The order in this array must match the declaration order of
// the methods in 'GodotSharp/Core/NativeInterop/NativeFuncs.cs'.
static const void *unmanaged_callbacks[]{
	(void *)godotsharp_dotnet_module_is_initialized,
	(void *)godotsharp_method_bind_get_method,
	(void *)godotsharp_method_bind_get_method_with_compatibility,
	(void *)godotsharp_get_class_constructor,
	(void *)godotsharp_engine_get_singleton,
	(void *)godotsharp_stack_info_vector_resize,
	(void *)godotsharp_stack_info_vector_destroy,
	(void *)godotsharp_internal_editor_file_system_update_files,
	(void *)godotsharp_internal_script_debugger_send_error,
	(void *)godotsharp_internal_script_debugger_is_active,
	(void *)godotsharp_internal_object_get_associated_gchandle,
	(void *)godotsharp_internal_object_disposed,
	(void *)godotsharp_internal_refcounted_disposed,
	(void *)godotsharp_internal_signal_awaiter_connect,
	(void *)godotsharp_internal_tie_native_managed_to_unmanaged,
	(void *)godotsharp_internal_tie_user_managed_to_unmanaged,
	(void *)godotsharp_internal_tie_managed_to_unmanaged_with_pre_setup,
	(void *)godotsharp_internal_unmanaged_get_script_instance_managed,
	(void *)godotsharp_internal_unmanaged_get_instance_binding_managed,
	(void *)godotsharp_internal_unmanaged_instance_binding_create_managed,
	(void *)godotsharp_internal_new_csharp_script,
	(void *)godotsharp_internal_script_load,
	(void *)godotsharp_internal_reload_registered_script,
	(void *)godotsharp_array_filter_godot_objects_by_native,
	(void *)godotsharp_array_filter_godot_objects_by_non_native,
	(void *)godotsharp_ref_new_from_ref_counted_ptr,
	(void *)godotsharp_ref_destroy,
	(void *)godotsharp_string_name_new_from_string,
	(void *)godotsharp_node_path_new_from_string,
	(void *)godotsharp_string_name_as_string,
	(void *)godotsharp_node_path_as_string,
	(void *)godotsharp_packed_byte_array_new_mem_copy,
	(void *)godotsharp_packed_int32_array_new_mem_copy,
	(void *)godotsharp_packed_int64_array_new_mem_copy,
	(void *)godotsharp_packed_float32_array_new_mem_copy,
	(void *)godotsharp_packed_float64_array_new_mem_copy,
	(void *)godotsharp_packed_vector2_array_new_mem_copy,
	(void *)godotsharp_packed_vector3_array_new_mem_copy,
	(void *)godotsharp_packed_vector4_array_new_mem_copy,
	(void *)godotsharp_packed_color_array_new_mem_copy,
	(void *)godotsharp_packed_string_array_add,
	(void *)godotsharp_callable_new_with_delegate,
	(void *)godotsharp_callable_get_data_for_marshalling,
	(void *)godotsharp_callable_call,
	(void *)godotsharp_callable_call_deferred,
	(void *)godotsharp_color_from_ok_hsl,
	(void *)godotsharp_method_bind_ptrcall,
	(void *)godotsharp_method_bind_call,
	(void *)godotsharp_variant_new_string_name,
	(void *)godotsharp_variant_new_copy,
	(void *)godotsharp_variant_new_node_path,
	(void *)godotsharp_variant_new_object,
	(void *)godotsharp_variant_new_transform2d,
	(void *)godotsharp_variant_new_basis,
	(void *)godotsharp_variant_new_transform3d,
	(void *)godotsharp_variant_new_projection,
	(void *)godotsharp_variant_new_aabb,
	(void *)godotsharp_variant_new_dictionary,
	(void *)godotsharp_variant_new_array,
	(void *)godotsharp_variant_new_packed_byte_array,
	(void *)godotsharp_variant_new_packed_int32_array,
	(void *)godotsharp_variant_new_packed_int64_array,
	(void *)godotsharp_variant_new_packed_float32_array,
	(void *)godotsharp_variant_new_packed_float64_array,
	(void *)godotsharp_variant_new_packed_string_array,
	(void *)godotsharp_variant_new_packed_vector2_array,
	(void *)godotsharp_variant_new_packed_vector3_array,
	(void *)godotsharp_variant_new_packed_vector4_array,
	(void *)godotsharp_variant_new_packed_color_array,
	(void *)godotsharp_variant_as_bool,
	(void *)godotsharp_variant_as_int,
	(void *)godotsharp_variant_as_float,
	(void *)godotsharp_variant_as_string,
	(void *)godotsharp_variant_as_vector2,
	(void *)godotsharp_variant_as_vector2i,
	(void *)godotsharp_variant_as_rect2,
	(void *)godotsharp_variant_as_rect2i,
	(void *)godotsharp_variant_as_vector3,
	(void *)godotsharp_variant_as_vector3i,
	(void *)godotsharp_variant_as_transform2d,
	(void *)godotsharp_variant_as_vector4,
	(void *)godotsharp_variant_as_vector4i,
	(void *)godotsharp_variant_as_plane,
	(void *)godotsharp_variant_as_quaternion,
	(void *)godotsharp_variant_as_aabb,
	(void *)godotsharp_variant_as_basis,
	(void *)godotsharp_variant_as_transform3d,
	(void *)godotsharp_variant_as_projection,
	(void *)godotsharp_variant_as_color,
	(void *)godotsharp_variant_as_string_name,
	(void *)godotsharp_variant_as_node_path,
	(void *)godotsharp_variant_as_rid,
	(void *)godotsharp_variant_as_callable,
	(void *)godotsharp_variant_as_signal,
	(void *)godotsharp_variant_as_dictionary,
	(void *)godotsharp_variant_as_array,
	(void *)godotsharp_variant_as_packed_byte_array,
	(void *)godotsharp_variant_as_packed_int32_array,
	(void *)godotsharp_variant_as_packed_int64_array,
	(void *)godotsharp_variant_as_packed_float32_array,
	(void *)godotsharp_variant_as_packed_float64_array,
	(void *)godotsharp_variant_as_packed_string_array,
	(void *)godotsharp_variant_as_packed_vector2_array,
	(void *)godotsharp_variant_as_packed_vector3_array,
	(void *)godotsharp_variant_as_packed_vector4_array,
	(void *)godotsharp_variant_as_packed_color_array,
	(void *)godotsharp_variant_equals,
	(void *)godotsharp_string_new_with_utf16_chars,
	(void *)godotsharp_string_name_new_copy,
	(void *)godotsharp_node_path_new_copy,
	(void *)godotsharp_array_new,
	(void *)godotsharp_array_new_copy,
	(void *)godotsharp_array_ptrw,
	(void *)godotsharp_dictionary_new,
	(void *)godotsharp_dictionary_new_copy,
	(void *)godotsharp_packed_byte_array_destroy,
	(void *)godotsharp_packed_int32_array_destroy,
	(void *)godotsharp_packed_int64_array_destroy,
	(void *)godotsharp_packed_float32_array_destroy,
	(void *)godotsharp_packed_float64_array_destroy,
	(void *)godotsharp_packed_string_array_destroy,
	(void *)godotsharp_packed_vector2_array_destroy,
	(void *)godotsharp_packed_vector3_array_destroy,
	(void *)godotsharp_packed_vector4_array_destroy,
	(void *)godotsharp_packed_color_array_destroy,
	(void *)godotsharp_variant_destroy,
	(void *)godotsharp_string_destroy,
	(void *)godotsharp_string_name_destroy,
	(void *)godotsharp_node_path_destroy,
	(void *)godotsharp_signal_destroy,
	(void *)godotsharp_callable_destroy,
	(void *)godotsharp_array_destroy,
	(void *)godotsharp_dictionary_destroy,
	(void *)godotsharp_array_add,
	(void *)godotsharp_array_add_range,
	(void *)godotsharp_array_binary_search,
	(void *)godotsharp_array_duplicate,
	(void *)godotsharp_array_fill,
	(void *)godotsharp_array_index_of,
	(void *)godotsharp_array_insert,
	(void *)godotsharp_array_last_index_of,
	(void *)godotsharp_array_make_read_only,
	(void *)godotsharp_array_set_typed,
	(void *)godotsharp_array_is_typed,
	(void *)godotsharp_array_max,
	(void *)godotsharp_array_min,
	(void *)godotsharp_array_pick_random,
	(void *)godotsharp_array_recursive_equal,
	(void *)godotsharp_array_remove_at,
	(void *)godotsharp_array_resize,
	(void *)godotsharp_array_reverse,
	(void *)godotsharp_array_shuffle,
	(void *)godotsharp_array_slice,
	(void *)godotsharp_array_sort,
	(void *)godotsharp_array_to_string,
	(void *)godotsharp_dictionary_try_get_value,
	(void *)godotsharp_dictionary_set_value,
	(void *)godotsharp_dictionary_keys,
	(void *)godotsharp_dictionary_values,
	(void *)godotsharp_dictionary_count,
	(void *)godotsharp_dictionary_key_value_pair_at,
	(void *)godotsharp_dictionary_add,
	(void *)godotsharp_dictionary_clear,
	(void *)godotsharp_dictionary_contains_key,
	(void *)godotsharp_dictionary_duplicate,
	(void *)godotsharp_dictionary_merge,
	(void *)godotsharp_dictionary_recursive_equal,
	(void *)godotsharp_dictionary_remove_key,
	(void *)godotsharp_dictionary_make_read_only,
	(void *)godotsharp_dictionary_set_typed,
	(void *)godotsharp_dictionary_is_typed_key,
	(void *)godotsharp_dictionary_is_typed_value,
	(void *)godotsharp_dictionary_get_typed_key_builtin,
	(void *)godotsharp_dictionary_get_typed_value_builtin,
	(void *)godotsharp_dictionary_get_typed_key_class_name,
	(void *)godotsharp_dictionary_get_typed_value_class_name,
	(void *)godotsharp_dictionary_get_typed_key_script,
	(void *)godotsharp_dictionary_get_typed_value_script,
	(void *)godotsharp_dictionary_to_string,
	(void *)godotsharp_string_simplify_path,
	(void *)godotsharp_string_to_camel_case,
	(void *)godotsharp_string_to_pascal_case,
	(void *)godotsharp_string_to_snake_case,
	(void *)godotsharp_node_path_get_as_property_path,
	(void *)godotsharp_node_path_get_concatenated_names,
	(void *)godotsharp_node_path_get_concatenated_subnames,
	(void *)godotsharp_node_path_get_name,
	(void *)godotsharp_node_path_get_name_count,
	(void *)godotsharp_node_path_get_subname,
	(void *)godotsharp_node_path_get_subname_count,
	(void *)godotsharp_node_path_is_absolute,
	(void *)godotsharp_node_path_equals,
	(void *)godotsharp_node_path_hash,
	(void *)godotsharp_bytes_to_var,
	(void *)godotsharp_convert,
	(void *)godotsharp_hash,
	(void *)godotsharp_instance_from_id,
	(void *)godotsharp_print,
	(void *)godotsharp_print_rich,
	(void *)godotsharp_printerr,
	(void *)godotsharp_printraw,
	(void *)godotsharp_prints,
	(void *)godotsharp_printt,
	(void *)godotsharp_randf,
	(void *)godotsharp_randi,
	(void *)godotsharp_randomize,
	(void *)godotsharp_randf_range,
	(void *)godotsharp_randfn,
	(void *)godotsharp_randi_range,
	(void *)godotsharp_rand_from_seed,
	(void *)godotsharp_seed,
	(void *)godotsharp_weakref,
	(void *)godotsharp_str_to_var,
	(void *)godotsharp_var_to_bytes,
	(void *)godotsharp_var_to_str,
	(void *)godotsharp_err_print_error,
	(void *)godotsharp_object_to_string,
};

const void **godotsharp::get_runtime_interop_funcs(int32_t &r_size) {
	r_size = sizeof(unmanaged_callbacks);
	return unmanaged_callbacks;
}
