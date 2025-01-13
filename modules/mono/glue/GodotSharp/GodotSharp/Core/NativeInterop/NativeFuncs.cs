#pragma warning disable CA1707 // Identifiers should not contain underscores
#pragma warning disable IDE1006 // Naming rule violation
// ReSharper disable InconsistentNaming

using System;
using System.Runtime.CompilerServices;
using Godot.SourceGenerators.Internal;


namespace Godot.NativeInterop
{
    /*
     * IMPORTANT:
     * The order of the methods defined in NativeFuncs must match the order
     * in the array defined at the bottom of 'glue/runtime_interop.cpp'.
     */

    [GenerateUnmanagedCallbacks(typeof(UnmanagedCallbacks))]
    public static unsafe partial class NativeFuncs
    {
        private static bool initialized;

        // ReSharper disable once ParameterOnlyUsedForPreconditionCheck.Global
        public static void Initialize(IntPtr unmanagedCallbacks, int unmanagedCallbacksSize)
        {
            if (initialized)
                throw new InvalidOperationException("Already initialized.");
            initialized = true;

            if (unmanagedCallbacksSize != sizeof(UnmanagedCallbacks))
                throw new ArgumentException("Unmanaged callbacks size mismatch.", nameof(unmanagedCallbacksSize));

            _unmanagedCallbacks = Unsafe.AsRef<UnmanagedCallbacks>((void*)unmanagedCallbacks);
        }

        private partial struct UnmanagedCallbacks
        {
        }

        // Custom functions

        internal static partial godot_bool godotsharp_dotnet_module_is_initialized();

        public static partial IntPtr godotsharp_method_bind_get_method(in godot_string_name p_classname,
            in godot_string_name p_methodname);

        public static partial IntPtr godotsharp_method_bind_get_method_with_compatibility(
            in godot_string_name p_classname, in godot_string_name p_methodname, ulong p_hash);

        public static partial delegate* unmanaged<godot_bool, IntPtr> godotsharp_get_class_constructor(
            in godot_string_name p_classname);

        public static partial IntPtr godotsharp_engine_get_singleton(in godot_string p_name);


        internal static partial Error godotsharp_stack_info_vector_resize(
            ref DebuggingUtils.godot_stack_info_vector p_stack_info_vector, int p_size);

        internal static partial void godotsharp_stack_info_vector_destroy(
            ref DebuggingUtils.godot_stack_info_vector p_stack_info_vector);

        internal static partial void godotsharp_internal_editor_file_system_update_files(in godot_packed_string_array p_script_paths);

        internal static partial void godotsharp_internal_script_debugger_send_error(in godot_string p_func,
            in godot_string p_file, int p_line, in godot_string p_err, in godot_string p_descr,
            godot_error_handler_type p_type, in DebuggingUtils.godot_stack_info_vector p_stack_info_vector);

        internal static partial godot_bool godotsharp_internal_script_debugger_is_active();

        internal static partial IntPtr godotsharp_internal_object_get_associated_gchandle(IntPtr ptr);

        internal static partial void godotsharp_internal_object_disposed(IntPtr ptr, IntPtr gcHandleToFree);

        internal static partial void godotsharp_internal_refcounted_disposed(IntPtr ptr, IntPtr gcHandleToFree,
            godot_bool isFinalizer);

        internal static partial Error godotsharp_internal_signal_awaiter_connect(IntPtr source,
            in godot_string_name signal,
            IntPtr target, IntPtr awaiterHandlePtr);

        internal static partial void godotsharp_internal_tie_native_managed_to_unmanaged(IntPtr gcHandleIntPtr,
            IntPtr unmanaged, in godot_string_name nativeName, godot_bool refCounted);

        internal static partial void godotsharp_internal_tie_user_managed_to_unmanaged(IntPtr gcHandleIntPtr,
            IntPtr unmanaged, godot_ref* scriptPtr, godot_bool refCounted);

        internal static partial void godotsharp_internal_tie_managed_to_unmanaged_with_pre_setup(
            IntPtr gcHandleIntPtr, IntPtr unmanaged);

        internal static partial IntPtr godotsharp_internal_unmanaged_get_script_instance_managed(IntPtr p_unmanaged,
            out godot_bool r_has_cs_script_instance);

        internal static partial IntPtr godotsharp_internal_unmanaged_get_instance_binding_managed(IntPtr p_unmanaged);

        internal static partial IntPtr godotsharp_internal_unmanaged_instance_binding_create_managed(IntPtr p_unmanaged,
            IntPtr oldGCHandlePtr);

        internal static partial void godotsharp_internal_new_csharp_script(godot_ref* r_dest);

        internal static partial godot_bool godotsharp_internal_script_load(in godot_string p_path, godot_ref* r_dest);

        internal static partial void godotsharp_internal_reload_registered_script(IntPtr scriptPtr);

        internal static partial void godotsharp_array_filter_godot_objects_by_native(scoped in godot_string_name p_native_name,
            scoped in godot_array p_input, out godot_array r_output);

        internal static partial void godotsharp_array_filter_godot_objects_by_non_native(scoped in godot_array p_input,
            out godot_array r_output);

        public static partial void godotsharp_ref_new_from_ref_counted_ptr(out godot_ref r_dest,
            IntPtr p_ref_counted_ptr);

        public static partial void godotsharp_ref_destroy(ref godot_ref p_instance);

        public static partial void godotsharp_string_name_new_from_string(out godot_string_name r_dest,
            scoped in godot_string p_name);

        public static partial void godotsharp_node_path_new_from_string(out godot_node_path r_dest,
            scoped in godot_string p_name);

        public static partial void
            godotsharp_string_name_as_string(out godot_string r_dest, scoped in godot_string_name p_name);

        public static partial void godotsharp_node_path_as_string(out godot_string r_dest, scoped in godot_node_path p_np);

        public static partial godot_packed_byte_array godotsharp_packed_byte_array_new_mem_copy(byte* p_src,
            int p_length);

        public static partial godot_packed_int32_array godotsharp_packed_int32_array_new_mem_copy(int* p_src,
            int p_length);

        public static partial godot_packed_int64_array godotsharp_packed_int64_array_new_mem_copy(long* p_src,
            int p_length);

        public static partial godot_packed_float32_array godotsharp_packed_float32_array_new_mem_copy(float* p_src,
            int p_length);

        public static partial godot_packed_float64_array godotsharp_packed_float64_array_new_mem_copy(double* p_src,
            int p_length);

        public static partial godot_packed_vector2_array godotsharp_packed_vector2_array_new_mem_copy(Vector2* p_src,
            int p_length);

        public static partial godot_packed_vector3_array godotsharp_packed_vector3_array_new_mem_copy(Vector3* p_src,
            int p_length);

        public static partial godot_packed_vector4_array godotsharp_packed_vector4_array_new_mem_copy(Vector4* p_src,
            int p_length);

        public static partial godot_packed_color_array godotsharp_packed_color_array_new_mem_copy(Color* p_src,
            int p_length);

        public static partial void godotsharp_packed_string_array_add(ref godot_packed_string_array r_dest,
            in godot_string p_element);

        public static partial void godotsharp_callable_new_with_delegate(IntPtr p_delegate_handle, IntPtr p_trampoline,
            IntPtr p_object, out godot_callable r_callable);

        internal static partial godot_bool godotsharp_callable_get_data_for_marshalling(scoped in godot_callable p_callable,
            out IntPtr r_delegate_handle, out IntPtr r_trampoline, out IntPtr r_object, out godot_string_name r_name);

        internal static partial godot_variant godotsharp_callable_call(scoped in godot_callable p_callable,
            godot_variant** p_args, int p_arg_count, out godot_variant_call_error p_call_error);

        internal static partial void godotsharp_callable_call_deferred(in godot_callable p_callable,
            godot_variant** p_args, int p_arg_count);

        internal static partial Color godotsharp_color_from_ok_hsl(float p_h, float p_s, float p_l, float p_alpha);

        internal static partial float godotsharp_color_get_ok_hsl_h(in Color p_self);

        internal static partial float godotsharp_color_get_ok_hsl_s(in Color p_self);

        internal static partial float godotsharp_color_get_ok_hsl_l(in Color p_self);

        // GDNative functions

        // gdnative.h

        public static partial void godotsharp_method_bind_ptrcall(IntPtr p_method_bind, IntPtr p_instance, void** p_args,
            void* p_ret);

        public static partial godot_variant godotsharp_method_bind_call(IntPtr p_method_bind, IntPtr p_instance,
            godot_variant** p_args, int p_arg_count, out godot_variant_call_error p_call_error);

        // variant.h

        public static partial void
            godotsharp_variant_new_string_name(out godot_variant r_dest, scoped in godot_string_name p_s);

        public static partial void godotsharp_variant_new_copy(out godot_variant r_dest, scoped in godot_variant p_src);

        public static partial void godotsharp_variant_new_node_path(out godot_variant r_dest, scoped in godot_node_path p_np);

        public static partial void godotsharp_variant_new_object(out godot_variant r_dest, IntPtr p_obj);

        public static partial void godotsharp_variant_new_transform2d(out godot_variant r_dest, scoped in Transform2D p_t2d);

        public static partial void godotsharp_variant_new_basis(out godot_variant r_dest, scoped in Basis p_basis);

        public static partial void godotsharp_variant_new_transform3d(out godot_variant r_dest, scoped in Transform3D p_trans);

        public static partial void godotsharp_variant_new_projection(out godot_variant r_dest, scoped in Projection p_proj);

        public static partial void godotsharp_variant_new_aabb(out godot_variant r_dest, scoped in Aabb p_aabb);

        public static partial void godotsharp_variant_new_dictionary(out godot_variant r_dest,
            scoped in godot_dictionary p_dict);

        public static partial void godotsharp_variant_new_array(out godot_variant r_dest, scoped in godot_array p_arr);

        public static partial void godotsharp_variant_new_packed_byte_array(out godot_variant r_dest,
            scoped in godot_packed_byte_array p_pba);

        public static partial void godotsharp_variant_new_packed_int32_array(out godot_variant r_dest,
            scoped in godot_packed_int32_array p_pia);

        public static partial void godotsharp_variant_new_packed_int64_array(out godot_variant r_dest,
            scoped in godot_packed_int64_array p_pia);

        public static partial void godotsharp_variant_new_packed_float32_array(out godot_variant r_dest,
            scoped in godot_packed_float32_array p_pra);

        public static partial void godotsharp_variant_new_packed_float64_array(out godot_variant r_dest,
            scoped in godot_packed_float64_array p_pra);

        public static partial void godotsharp_variant_new_packed_string_array(out godot_variant r_dest,
            scoped in godot_packed_string_array p_psa);

        public static partial void godotsharp_variant_new_packed_vector2_array(out godot_variant r_dest,
            scoped in godot_packed_vector2_array p_pv2a);

        public static partial void godotsharp_variant_new_packed_vector3_array(out godot_variant r_dest,
            scoped in godot_packed_vector3_array p_pv3a);

        public static partial void godotsharp_variant_new_packed_vector4_array(out godot_variant r_dest,
            scoped in godot_packed_vector4_array p_pv4a);

        public static partial void godotsharp_variant_new_packed_color_array(out godot_variant r_dest,
            scoped in godot_packed_color_array p_pca);

        public static partial godot_bool godotsharp_variant_as_bool(scoped in godot_variant p_self);

        public static partial Int64 godotsharp_variant_as_int(scoped in godot_variant p_self);

        public static partial double godotsharp_variant_as_float(scoped in godot_variant p_self);

        public static partial godot_string godotsharp_variant_as_string(scoped in godot_variant p_self);

        public static partial Vector2 godotsharp_variant_as_vector2(scoped in godot_variant p_self);

        public static partial Vector2I godotsharp_variant_as_vector2i(scoped in godot_variant p_self);

        public static partial Rect2 godotsharp_variant_as_rect2(scoped in godot_variant p_self);

        public static partial Rect2I godotsharp_variant_as_rect2i(scoped in godot_variant p_self);

        public static partial Vector3 godotsharp_variant_as_vector3(scoped in godot_variant p_self);

        public static partial Vector3I godotsharp_variant_as_vector3i(scoped in godot_variant p_self);

        public static partial Transform2D godotsharp_variant_as_transform2d(scoped in godot_variant p_self);

        public static partial Vector4 godotsharp_variant_as_vector4(scoped in godot_variant p_self);

        public static partial Vector4I godotsharp_variant_as_vector4i(scoped in godot_variant p_self);

        public static partial Plane godotsharp_variant_as_plane(scoped in godot_variant p_self);

        public static partial Quaternion godotsharp_variant_as_quaternion(scoped in godot_variant p_self);

        public static partial Aabb godotsharp_variant_as_aabb(scoped in godot_variant p_self);

        public static partial Basis godotsharp_variant_as_basis(scoped in godot_variant p_self);

        public static partial Transform3D godotsharp_variant_as_transform3d(scoped in godot_variant p_self);

        public static partial Projection godotsharp_variant_as_projection(scoped in godot_variant p_self);

        public static partial Color godotsharp_variant_as_color(scoped in godot_variant p_self);

        public static partial godot_string_name godotsharp_variant_as_string_name(scoped in godot_variant p_self);

        public static partial godot_node_path godotsharp_variant_as_node_path(scoped in godot_variant p_self);

        public static partial Rid godotsharp_variant_as_rid(scoped in godot_variant p_self);

        public static partial godot_callable godotsharp_variant_as_callable(scoped in godot_variant p_self);

        public static partial godot_signal godotsharp_variant_as_signal(scoped in godot_variant p_self);

        public static partial godot_dictionary godotsharp_variant_as_dictionary(scoped in godot_variant p_self);

        public static partial godot_array godotsharp_variant_as_array(scoped in godot_variant p_self);

        public static partial godot_packed_byte_array godotsharp_variant_as_packed_byte_array(scoped in godot_variant p_self);

        public static partial godot_packed_int32_array godotsharp_variant_as_packed_int32_array(scoped in godot_variant p_self);

        public static partial godot_packed_int64_array godotsharp_variant_as_packed_int64_array(scoped in godot_variant p_self);

        public static partial godot_packed_float32_array godotsharp_variant_as_packed_float32_array(scoped in godot_variant p_self);

        public static partial godot_packed_float64_array godotsharp_variant_as_packed_float64_array(scoped in godot_variant p_self);

        public static partial godot_packed_string_array godotsharp_variant_as_packed_string_array(scoped in godot_variant p_self);

        public static partial godot_packed_vector2_array godotsharp_variant_as_packed_vector2_array(scoped in godot_variant p_self);

        public static partial godot_packed_vector3_array godotsharp_variant_as_packed_vector3_array(scoped in godot_variant p_self);

        public static partial godot_packed_vector4_array godotsharp_variant_as_packed_vector4_array(
            in godot_variant p_self);

        public static partial godot_packed_color_array godotsharp_variant_as_packed_color_array(scoped in godot_variant p_self);

        public static partial godot_bool godotsharp_variant_equals(scoped in godot_variant p_a, scoped in godot_variant p_b);

        // string.h

        public static partial void godotsharp_string_new_with_utf16_chars(out godot_string r_dest, char* p_contents);

        // string_name.h

        public static partial void godotsharp_string_name_new_copy(out godot_string_name r_dest,
            scoped in godot_string_name p_src);

        // node_path.h

        public static partial void godotsharp_node_path_new_copy(out godot_node_path r_dest, scoped in godot_node_path p_src);

        // array.h

        public static partial void godotsharp_array_new(out godot_array r_dest);

        public static partial void godotsharp_array_new_copy(out godot_array r_dest, scoped in godot_array p_src);

        public static partial godot_variant* godotsharp_array_ptrw(ref godot_array p_self);

        // dictionary.h

        public static partial void godotsharp_dictionary_new(out godot_dictionary r_dest);

        public static partial void godotsharp_dictionary_new_copy(out godot_dictionary r_dest,
            scoped in godot_dictionary p_src);

        // destroy functions

        public static partial void godotsharp_packed_byte_array_destroy(ref godot_packed_byte_array p_self);

        public static partial void godotsharp_packed_int32_array_destroy(ref godot_packed_int32_array p_self);

        public static partial void godotsharp_packed_int64_array_destroy(ref godot_packed_int64_array p_self);

        public static partial void godotsharp_packed_float32_array_destroy(ref godot_packed_float32_array p_self);

        public static partial void godotsharp_packed_float64_array_destroy(ref godot_packed_float64_array p_self);

        public static partial void godotsharp_packed_string_array_destroy(ref godot_packed_string_array p_self);

        public static partial void godotsharp_packed_vector2_array_destroy(ref godot_packed_vector2_array p_self);

        public static partial void godotsharp_packed_vector3_array_destroy(ref godot_packed_vector3_array p_self);

        public static partial void godotsharp_packed_vector4_array_destroy(ref godot_packed_vector4_array p_self);

        public static partial void godotsharp_packed_color_array_destroy(ref godot_packed_color_array p_self);

        public static partial void godotsharp_variant_destroy(ref godot_variant p_self);

        public static partial void godotsharp_string_destroy(ref godot_string p_self);

        public static partial void godotsharp_string_name_destroy(ref godot_string_name p_self);

        public static partial void godotsharp_node_path_destroy(ref godot_node_path p_self);

        public static partial void godotsharp_signal_destroy(ref godot_signal p_self);

        public static partial void godotsharp_callable_destroy(ref godot_callable p_self);

        public static partial void godotsharp_array_destroy(ref godot_array p_self);

        public static partial void godotsharp_dictionary_destroy(ref godot_dictionary p_self);

        // Array

        public static partial int godotsharp_array_add(ref godot_array p_self, in godot_variant p_item);

        public static partial int godotsharp_array_add_range(ref godot_array p_self, in godot_array p_collection);

        public static partial int godotsharp_array_binary_search(ref godot_array p_self, int p_index, int p_count, in godot_variant p_value);

        public static partial void godotsharp_array_duplicate(scoped ref godot_array p_self, godot_bool p_deep, out godot_array r_dest);

        public static partial void godotsharp_array_fill(ref godot_array p_self, in godot_variant p_value);

        public static partial int godotsharp_array_index_of(ref godot_array p_self, in godot_variant p_item, int p_index = 0);

        public static partial void godotsharp_array_insert(ref godot_array p_self, int p_index, in godot_variant p_item);

        public static partial int godotsharp_array_last_index_of(ref godot_array p_self, in godot_variant p_item, int p_index);

        public static partial void godotsharp_array_make_read_only(ref godot_array p_self);

        public static partial void godotsharp_array_set_typed(
            ref godot_array p_self,
            uint p_elem_type,
            in godot_string_name p_elem_class_name,
            in godot_ref p_elem_script);

        public static partial godot_bool godotsharp_array_is_typed(ref godot_array p_self);

        public static partial void godotsharp_array_max(scoped ref godot_array p_self, out godot_variant r_value);

        public static partial void godotsharp_array_min(scoped ref godot_array p_self, out godot_variant r_value);

        public static partial void godotsharp_array_pick_random(scoped ref godot_array p_self, out godot_variant r_value);

        public static partial godot_bool godotsharp_array_recursive_equal(ref godot_array p_self, in godot_array p_other);

        public static partial void godotsharp_array_remove_at(ref godot_array p_self, int p_index);

        public static partial Error godotsharp_array_resize(ref godot_array p_self, int p_new_size);

        public static partial void godotsharp_array_reverse(ref godot_array p_self);

        public static partial void godotsharp_array_shuffle(ref godot_array p_self);

        public static partial void godotsharp_array_slice(scoped ref godot_array p_self, int p_start, int p_end,
            int p_step, godot_bool p_deep, out godot_array r_dest);

        public static partial void godotsharp_array_sort(ref godot_array p_self);

        public static partial void godotsharp_array_to_string(ref godot_array p_self, out godot_string r_str);

        // Dictionary

        public static partial godot_bool godotsharp_dictionary_try_get_value(scoped ref godot_dictionary p_self,
            scoped in godot_variant p_key,
            out godot_variant r_value);

        public static partial void godotsharp_dictionary_set_value(ref godot_dictionary p_self, in godot_variant p_key,
            in godot_variant p_value);

        public static partial void godotsharp_dictionary_keys(scoped ref godot_dictionary p_self, out godot_array r_dest);

        public static partial void godotsharp_dictionary_values(scoped ref godot_dictionary p_self, out godot_array r_dest);

        public static partial int godotsharp_dictionary_count(ref godot_dictionary p_self);

        public static partial void godotsharp_dictionary_key_value_pair_at(scoped ref godot_dictionary p_self, int p_index,
            out godot_variant r_key, out godot_variant r_value);

        public static partial void godotsharp_dictionary_add(ref godot_dictionary p_self, in godot_variant p_key,
            in godot_variant p_value);

        public static partial void godotsharp_dictionary_clear(ref godot_dictionary p_self);

        public static partial godot_bool godotsharp_dictionary_contains_key(ref godot_dictionary p_self,
            in godot_variant p_key);

        public static partial void godotsharp_dictionary_duplicate(scoped ref godot_dictionary p_self, godot_bool p_deep,
            out godot_dictionary r_dest);

        public static partial void godotsharp_dictionary_merge(ref godot_dictionary p_self, in godot_dictionary p_dictionary, godot_bool p_overwrite);

        public static partial godot_bool godotsharp_dictionary_recursive_equal(ref godot_dictionary p_self, in godot_dictionary p_other);

        public static partial godot_bool godotsharp_dictionary_remove_key(ref godot_dictionary p_self,
            in godot_variant p_key);

        public static partial void godotsharp_dictionary_make_read_only(ref godot_dictionary p_self);

        public static partial void godotsharp_dictionary_set_typed(
            ref godot_dictionary p_self,
            uint p_key_type,
            in godot_string_name p_key_class_name,
            in godot_ref p_key_script,
            uint p_value_type,
            in godot_string_name p_value_class_name,
            in godot_ref p_value_script);

        public static partial godot_bool godotsharp_dictionary_is_typed_key(ref godot_dictionary p_self);

        public static partial godot_bool godotsharp_dictionary_is_typed_value(ref godot_dictionary p_self);

        public static partial uint godotsharp_dictionary_get_typed_key_builtin(ref godot_dictionary p_self);

        public static partial uint godotsharp_dictionary_get_typed_value_builtin(ref godot_dictionary p_self);

        public static partial void godotsharp_dictionary_get_typed_key_class_name(ref godot_dictionary p_self, out godot_string_name r_dest);

        public static partial void godotsharp_dictionary_get_typed_value_class_name(ref godot_dictionary p_self, out godot_string_name r_dest);

        public static partial void godotsharp_dictionary_get_typed_key_script(ref godot_dictionary p_self, out godot_variant r_dest);

        public static partial void godotsharp_dictionary_get_typed_value_script(ref godot_dictionary p_self, out godot_variant r_dest);

        public static partial void godotsharp_dictionary_to_string(scoped ref godot_dictionary p_self, out godot_string r_str);

        // StringExtensions

        public static partial void godotsharp_string_simplify_path(scoped in godot_string p_self,
            out godot_string r_simplified_path);

        public static partial void godotsharp_string_to_camel_case(scoped in godot_string p_self,
            out godot_string r_camel_case);

        public static partial void godotsharp_string_to_pascal_case(scoped in godot_string p_self,
            out godot_string r_pascal_case);

        public static partial void godotsharp_string_to_snake_case(scoped in godot_string p_self,
            out godot_string r_snake_case);

        // NodePath

        public static partial void godotsharp_node_path_get_as_property_path(in godot_node_path p_self,
            ref godot_node_path r_dest);

        public static partial void godotsharp_node_path_get_concatenated_names(scoped in godot_node_path p_self,
            out godot_string r_names);

        public static partial void godotsharp_node_path_get_concatenated_subnames(scoped in godot_node_path p_self,
            out godot_string r_subnames);

        public static partial void godotsharp_node_path_get_name(scoped in godot_node_path p_self, int p_idx,
            out godot_string r_name);

        public static partial int godotsharp_node_path_get_name_count(in godot_node_path p_self);

        public static partial void godotsharp_node_path_get_subname(scoped in godot_node_path p_self, int p_idx,
            out godot_string r_subname);

        public static partial int godotsharp_node_path_get_subname_count(in godot_node_path p_self);

        public static partial godot_bool godotsharp_node_path_is_absolute(in godot_node_path p_self);

        public static partial godot_bool godotsharp_node_path_equals(in godot_node_path p_self, in godot_node_path p_other);

        public static partial int godotsharp_node_path_hash(in godot_node_path p_self);

        // GD, etc

        internal static partial void godotsharp_bytes_to_var(scoped in godot_packed_byte_array p_bytes,
            godot_bool p_allow_objects,
            out godot_variant r_ret);

        internal static partial void godotsharp_convert(scoped in godot_variant p_what, int p_type,
            out godot_variant r_ret);

        internal static partial int godotsharp_hash(in godot_variant p_var);

        internal static partial IntPtr godotsharp_instance_from_id(ulong p_instance_id);

        internal static partial void godotsharp_print(in godot_string p_what);

        public static partial void godotsharp_print_rich(in godot_string p_what);

        internal static partial void godotsharp_printerr(in godot_string p_what);

        internal static partial void godotsharp_printraw(in godot_string p_what);

        internal static partial void godotsharp_prints(in godot_string p_what);

        internal static partial void godotsharp_printt(in godot_string p_what);

        internal static partial float godotsharp_randf();

        internal static partial uint godotsharp_randi();

        internal static partial void godotsharp_randomize();

        internal static partial double godotsharp_randf_range(double from, double to);

        internal static partial double godotsharp_randfn(double mean, double deviation);

        internal static partial int godotsharp_randi_range(int from, int to);

        internal static partial uint godotsharp_rand_from_seed(ulong seed, out ulong newSeed);

        internal static partial void godotsharp_seed(ulong seed);

        internal static partial void godotsharp_weakref(IntPtr p_obj, out godot_ref r_weak_ref);

        internal static partial void godotsharp_str_to_var(scoped in godot_string p_str, out godot_variant r_ret);

        internal static partial void godotsharp_var_to_bytes(scoped in godot_variant p_what, godot_bool p_full_objects,
            out godot_packed_byte_array r_bytes);

        internal static partial void godotsharp_var_to_str(scoped in godot_variant p_var, out godot_string r_ret);

        internal static partial void godotsharp_err_print_error(in godot_string p_function, in godot_string p_file, int p_line, in godot_string p_error, in godot_string p_message = default, godot_bool p_editor_notify = godot_bool.False, godot_error_handler_type p_type = godot_error_handler_type.ERR_HANDLER_ERROR);

        // Object

        public static partial void godotsharp_object_to_string(IntPtr ptr, out godot_string r_str);
    }
}
