#pragma warning disable CA1707 // Identifiers should not contain underscores

using System;
using System.Runtime.InteropServices;
using Godot.Collections;
using Array = System.Array;

// ReSharper disable InconsistentNaming

// We want to use full name qualifiers here even if redundant for clarity
// ReSharper disable RedundantNameQualifier

#nullable enable

namespace Godot.NativeInterop
{
    public static class Marshaling
    {
        internal static Variant.Type ConvertManagedTypeToVariantType(Type type, out bool r_nil_is_variant)
        {
            r_nil_is_variant = false;

            switch (Type.GetTypeCode(type))
            {
                case TypeCode.Boolean:
                    return Variant.Type.Bool;
                case TypeCode.Char:
                    return Variant.Type.Int;
                case TypeCode.SByte:
                    return Variant.Type.Int;
                case TypeCode.Int16:
                    return Variant.Type.Int;
                case TypeCode.Int32:
                    return Variant.Type.Int;
                case TypeCode.Int64:
                    return Variant.Type.Int;
                case TypeCode.Byte:
                    return Variant.Type.Int;
                case TypeCode.UInt16:
                    return Variant.Type.Int;
                case TypeCode.UInt32:
                    return Variant.Type.Int;
                case TypeCode.UInt64:
                    return Variant.Type.Int;
                case TypeCode.Single:
                    return Variant.Type.Float;
                case TypeCode.Double:
                    return Variant.Type.Float;
                case TypeCode.String:
                    return Variant.Type.String;
                default:
                {
                    if (type == typeof(Vector2))
                        return Variant.Type.Vector2;

                    if (type == typeof(Vector2I))
                        return Variant.Type.Vector2I;

                    if (type == typeof(Rect2))
                        return Variant.Type.Rect2;

                    if (type == typeof(Rect2I))
                        return Variant.Type.Rect2I;

                    if (type == typeof(Transform2D))
                        return Variant.Type.Transform2D;

                    if (type == typeof(Vector3))
                        return Variant.Type.Vector3;

                    if (type == typeof(Vector3I))
                        return Variant.Type.Vector3I;

                    if (type == typeof(Vector4))
                        return Variant.Type.Vector4;

                    if (type == typeof(Vector4I))
                        return Variant.Type.Vector4I;

                    if (type == typeof(Basis))
                        return Variant.Type.Basis;

                    if (type == typeof(Quaternion))
                        return Variant.Type.Quaternion;

                    if (type == typeof(Transform3D))
                        return Variant.Type.Transform3D;

                    if (type == typeof(Projection))
                        return Variant.Type.Projection;

                    if (type == typeof(Aabb))
                        return Variant.Type.Aabb;

                    if (type == typeof(Color))
                        return Variant.Type.Color;

                    if (type == typeof(Plane))
                        return Variant.Type.Plane;

                    if (type == typeof(Callable))
                        return Variant.Type.Callable;

                    if (type == typeof(Signal))
                        return Variant.Type.Signal;

                    if (type.IsEnum)
                        return Variant.Type.Int;

                    if (type.IsArray || type.IsSZArray)
                    {
                        if (type == typeof(byte[]))
                            return Variant.Type.PackedByteArray;

                        if (type == typeof(int[]))
                            return Variant.Type.PackedInt32Array;

                        if (type == typeof(long[]))
                            return Variant.Type.PackedInt64Array;

                        if (type == typeof(float[]))
                            return Variant.Type.PackedFloat32Array;

                        if (type == typeof(double[]))
                            return Variant.Type.PackedFloat64Array;

                        if (type == typeof(string[]))
                            return Variant.Type.PackedStringArray;

                        if (type == typeof(Vector2[]))
                            return Variant.Type.PackedVector2Array;

                        if (type == typeof(Vector3[]))
                            return Variant.Type.PackedVector3Array;

                        if (type == typeof(Vector4[]))
                            return Variant.Type.PackedVector4Array;

                        if (type == typeof(Color[]))
                            return Variant.Type.PackedColorArray;

                        if (type == typeof(StringName[]))
                            return Variant.Type.Array;

                        if (type == typeof(NodePath[]))
                            return Variant.Type.Array;

                        if (type == typeof(Rid[]))
                            return Variant.Type.Array;

                        if (typeof(GodotObject[]).IsAssignableFrom(type))
                            return Variant.Type.Array;
                    }
                    else if (type.IsGenericType)
                    {
                        if (typeof(GodotObject).IsAssignableFrom(type))
                            return Variant.Type.Object;

                        // We use `IsAssignableFrom` with our helper interfaces to detect generic Godot collections
                        // because `GetGenericTypeDefinition` is not supported in NativeAOT reflection-free mode.

                        if (typeof(IGenericGodotDictionary).IsAssignableFrom(type))
                            return Variant.Type.Dictionary;

                        if (typeof(IGenericGodotArray).IsAssignableFrom(type))
                            return Variant.Type.Array;
                    }
                    else if (type == typeof(Variant))
                    {
                        r_nil_is_variant = true;
                        return Variant.Type.Nil;
                    }
                    else
                    {
                        if (typeof(GodotObject).IsAssignableFrom(type))
                            return Variant.Type.Object;

                        if (typeof(StringName) == type)
                            return Variant.Type.StringName;

                        if (typeof(NodePath) == type)
                            return Variant.Type.NodePath;

                        if (typeof(Rid) == type)
                            return Variant.Type.Rid;

                        if (typeof(Collections.Dictionary) == type)
                            return Variant.Type.Dictionary;

                        if (typeof(Collections.Array) == type)
                            return Variant.Type.Array;
                    }

                    break;
                }
            }

            // Unknown
            return Variant.Type.Nil;
        }

        // String

        public static unsafe godot_string ConvertStringToNative(string? p_mono_string)
        {
            if (p_mono_string == null)
                return new godot_string();

            fixed (char* methodChars = p_mono_string)
            {
                NativeFuncs.godotsharp_string_new_with_utf16_chars(out godot_string dest, methodChars);
                return dest;
            }
        }

        public static unsafe string ConvertStringToManaged(in godot_string p_string)
        {
            if (p_string.Buffer == IntPtr.Zero)
                return string.Empty;

            const int SizeOfChar32 = 4;
            byte* bytes = (byte*)p_string.Buffer;
            int size = p_string.Size;
            if (size == 0)
                return string.Empty;
            size -= 1; // zero at the end
            int sizeInBytes = size * SizeOfChar32;
            return System.Text.Encoding.UTF32.GetString(bytes, sizeInBytes);
        }

        // Callable

        public static godot_callable ConvertCallableToNative(in Callable p_managed_callable)
        {
            if (p_managed_callable.Delegate != null)
            {
                var gcHandle = CustomGCHandle.AllocStrong(p_managed_callable.Delegate);

                IntPtr objectPtr = p_managed_callable.Target != null ?
                    GodotObject.GetPtr(p_managed_callable.Target) :
                    IntPtr.Zero;

                unsafe
                {
                    NativeFuncs.godotsharp_callable_new_with_delegate(
                        GCHandle.ToIntPtr(gcHandle), (IntPtr)p_managed_callable.Trampoline,
                        objectPtr, out godot_callable callable);

                    return callable;
                }
            }
            else
            {
                godot_string_name method;

                if (p_managed_callable.Method != null && !p_managed_callable.Method.IsEmpty)
                {
                    var src = (godot_string_name)p_managed_callable.Method.NativeValue;
                    method = NativeFuncs.godotsharp_string_name_new_copy(src);
                }
                else
                {
                    method = default;
                }

                return new godot_callable(method /* Takes ownership of disposable */,
                    p_managed_callable.Target?.GetInstanceId() ?? 0);
            }
        }

        public static Callable ConvertCallableToManaged(in godot_callable p_callable)
        {
            if (NativeFuncs.godotsharp_callable_get_data_for_marshalling(p_callable,
                    out IntPtr delegateGCHandle, out IntPtr trampoline,
                    out IntPtr godotObject, out godot_string_name name).ToBool())
            {
                if (delegateGCHandle != IntPtr.Zero)
                {
                    unsafe
                    {
                        return Callable.CreateWithUnsafeTrampoline(
                            (Delegate?)GCHandle.FromIntPtr(delegateGCHandle).Target,
                            (delegate* managed<object, NativeVariantPtrArgs, out godot_variant, void>)trampoline);
                    }
                }

                return new Callable(
                    InteropUtils.UnmanagedGetManaged(godotObject),
                    StringName.CreateTakingOwnershipOfDisposableValue(name));
            }

            // Some other unsupported callable
            return new Callable();
        }

        // Signal

        public static godot_signal ConvertSignalToNative(in Signal p_managed_signal)
        {
            ulong ownerId = p_managed_signal.Owner.GetInstanceId();
            godot_string_name name;

            if (p_managed_signal.Name != null && !p_managed_signal.Name.IsEmpty)
            {
                var src = (godot_string_name)p_managed_signal.Name.NativeValue;
                name = NativeFuncs.godotsharp_string_name_new_copy(src);
            }
            else
            {
                name = default;
            }

            return new godot_signal(name, ownerId);
        }

        public static Signal ConvertSignalToManaged(in godot_signal p_signal)
        {
            var owner = GodotObject.InstanceFromId(p_signal.ObjectId);
            var name = StringName.CreateTakingOwnershipOfDisposableValue(
                NativeFuncs.godotsharp_string_name_new_copy(p_signal.Name));
            return new Signal(owner, name);
        }

        // Array

        internal static T[] ConvertNativeGodotArrayToSystemArrayOfGodotObjectType<T>(in godot_array p_array)
            where T : GodotObject
        {
            var array = Collections.Array.CreateTakingOwnershipOfDisposableValue(
                NativeFuncs.godotsharp_array_new_copy(p_array));

            int length = array.Count;
            var ret = new T[length];

            for (int i = 0; i < length; i++)
                ret[i] = (T)array[i].AsGodotObject();

            return ret;
        }

        internal static StringName[] ConvertNativeGodotArrayToSystemArrayOfStringName(in godot_array p_array)
        {
            var array = Collections.Array.CreateTakingOwnershipOfDisposableValue(
                NativeFuncs.godotsharp_array_new_copy(p_array));

            int length = array.Count;
            var ret = new StringName[length];

            for (int i = 0; i < length; i++)
                ret[i] = array[i].AsStringName();

            return ret;
        }

        internal static NodePath[] ConvertNativeGodotArrayToSystemArrayOfNodePath(in godot_array p_array)
        {
            var array = Collections.Array.CreateTakingOwnershipOfDisposableValue(
                NativeFuncs.godotsharp_array_new_copy(p_array));

            int length = array.Count;
            var ret = new NodePath[length];

            for (int i = 0; i < length; i++)
                ret[i] = array[i].AsNodePath();

            return ret;
        }

        internal static Rid[] ConvertNativeGodotArrayToSystemArrayOfRid(in godot_array p_array)
        {
            var array = Collections.Array.CreateTakingOwnershipOfDisposableValue(
                NativeFuncs.godotsharp_array_new_copy(p_array));

            int length = array.Count;
            var ret = new Rid[length];

            for (int i = 0; i < length; i++)
                ret[i] = array[i].AsRid();

            return ret;
        }

        // PackedByteArray

        public static unsafe byte[] ConvertNativePackedByteArrayToSystemArray(in godot_packed_byte_array p_array)
        {
            byte* buffer = p_array.Buffer;
            int size = p_array.Size;
            if (size == 0)
                return Array.Empty<byte>();
            var array = new byte[size];
            fixed (byte* dest = array)
                Buffer.MemoryCopy(buffer, dest, size, size);
            return array;
        }

        public static godot_packed_byte_array ConvertSystemArrayToNativePackedByteArray(Span<byte> p_array)
        {
            return ConvertSystemArrayToNativePackedByteArray((ReadOnlySpan<byte>)p_array);
        }

        public static unsafe godot_packed_byte_array ConvertSystemArrayToNativePackedByteArray(ReadOnlySpan<byte> p_array)
        {
            if (p_array.IsEmpty)
                return new godot_packed_byte_array();
            fixed (byte* src = p_array)
                return NativeFuncs.godotsharp_packed_byte_array_new_mem_copy(src, p_array.Length);
        }

        // PackedInt32Array

        public static unsafe int[] ConvertNativePackedInt32ArrayToSystemArray(godot_packed_int32_array p_array)
        {
            int* buffer = p_array.Buffer;
            int size = p_array.Size;
            if (size == 0)
                return Array.Empty<int>();
            int sizeInBytes = size * sizeof(int);
            var array = new int[size];
            fixed (int* dest = array)
                Buffer.MemoryCopy(buffer, dest, sizeInBytes, sizeInBytes);
            return array;
        }

        public static godot_packed_int32_array ConvertSystemArrayToNativePackedInt32Array(Span<int> p_array)
        {
            return ConvertSystemArrayToNativePackedInt32Array((ReadOnlySpan<int>)p_array);
        }

        public static unsafe godot_packed_int32_array ConvertSystemArrayToNativePackedInt32Array(ReadOnlySpan<int> p_array)
        {
            if (p_array.IsEmpty)
                return new godot_packed_int32_array();
            fixed (int* src = p_array)
                return NativeFuncs.godotsharp_packed_int32_array_new_mem_copy(src, p_array.Length);
        }

        // PackedInt64Array

        public static unsafe long[] ConvertNativePackedInt64ArrayToSystemArray(godot_packed_int64_array p_array)
        {
            long* buffer = p_array.Buffer;
            int size = p_array.Size;
            if (size == 0)
                return Array.Empty<long>();
            int sizeInBytes = size * sizeof(long);
            var array = new long[size];
            fixed (long* dest = array)
                Buffer.MemoryCopy(buffer, dest, sizeInBytes, sizeInBytes);
            return array;
        }

        public static godot_packed_int64_array ConvertSystemArrayToNativePackedInt64Array(Span<long> p_array)
        {
            return ConvertSystemArrayToNativePackedInt64Array((ReadOnlySpan<long>)p_array);
        }

        public static unsafe godot_packed_int64_array ConvertSystemArrayToNativePackedInt64Array(ReadOnlySpan<long> p_array)
        {
            if (p_array.IsEmpty)
                return new godot_packed_int64_array();
            fixed (long* src = p_array)
                return NativeFuncs.godotsharp_packed_int64_array_new_mem_copy(src, p_array.Length);
        }

        // PackedFloat32Array

        public static unsafe float[] ConvertNativePackedFloat32ArrayToSystemArray(godot_packed_float32_array p_array)
        {
            float* buffer = p_array.Buffer;
            int size = p_array.Size;
            if (size == 0)
                return Array.Empty<float>();
            int sizeInBytes = size * sizeof(float);
            var array = new float[size];
            fixed (float* dest = array)
                Buffer.MemoryCopy(buffer, dest, sizeInBytes, sizeInBytes);
            return array;
        }

        public static godot_packed_float32_array ConvertSystemArrayToNativePackedFloat32Array(Span<float> p_array)
        {
            return ConvertSystemArrayToNativePackedFloat32Array((ReadOnlySpan<float>)p_array);
        }

        public static unsafe godot_packed_float32_array ConvertSystemArrayToNativePackedFloat32Array(
            ReadOnlySpan<float> p_array)
        {
            if (p_array.IsEmpty)
                return new godot_packed_float32_array();
            fixed (float* src = p_array)
                return NativeFuncs.godotsharp_packed_float32_array_new_mem_copy(src, p_array.Length);
        }

        // PackedFloat64Array

        public static unsafe double[] ConvertNativePackedFloat64ArrayToSystemArray(godot_packed_float64_array p_array)
        {
            double* buffer = p_array.Buffer;
            int size = p_array.Size;
            if (size == 0)
                return Array.Empty<double>();
            int sizeInBytes = size * sizeof(double);
            var array = new double[size];
            fixed (double* dest = array)
                Buffer.MemoryCopy(buffer, dest, sizeInBytes, sizeInBytes);
            return array;
        }

        public static godot_packed_float64_array ConvertSystemArrayToNativePackedFloat64Array(Span<double> p_array)
        {
            return ConvertSystemArrayToNativePackedFloat64Array((ReadOnlySpan<double>)p_array);
        }

        public static unsafe godot_packed_float64_array ConvertSystemArrayToNativePackedFloat64Array(
            ReadOnlySpan<double> p_array)
        {
            if (p_array.IsEmpty)
                return new godot_packed_float64_array();
            fixed (double* src = p_array)
                return NativeFuncs.godotsharp_packed_float64_array_new_mem_copy(src, p_array.Length);
        }

        // PackedStringArray

        public static unsafe string[] ConvertNativePackedStringArrayToSystemArray(godot_packed_string_array p_array)
        {
            godot_string* buffer = p_array.Buffer;
            int size = p_array.Size;
            if (size == 0)
                return Array.Empty<string>();
            var array = new string[size];
            for (int i = 0; i < size; i++)
                array[i] = ConvertStringToManaged(buffer[i]);
            return array;
        }

        public static godot_packed_string_array ConvertSystemArrayToNativePackedStringArray(Span<string> p_array)
        {
            return ConvertSystemArrayToNativePackedStringArray((ReadOnlySpan<string>)p_array);
        }

        public static godot_packed_string_array ConvertSystemArrayToNativePackedStringArray(ReadOnlySpan<string> p_array)
        {
            godot_packed_string_array dest = new godot_packed_string_array();

            if (p_array.IsEmpty)
                return dest;

            /* TODO: Replace godotsharp_packed_string_array_add with a single internal call to
             get the write address. We can't use `dest._ptr` directly for writing due to COW. */

            for (int i = 0; i < p_array.Length; i++)
            {
                using godot_string godotStrElem = ConvertStringToNative(p_array[i]);
                NativeFuncs.godotsharp_packed_string_array_add(ref dest, godotStrElem);
            }

            return dest;
        }

        // PackedVector2Array

        public static unsafe Vector2[] ConvertNativePackedVector2ArrayToSystemArray(godot_packed_vector2_array p_array)
        {
            Vector2* buffer = p_array.Buffer;
            int size = p_array.Size;
            if (size == 0)
                return Array.Empty<Vector2>();
            int sizeInBytes = size * sizeof(Vector2);
            var array = new Vector2[size];
            fixed (Vector2* dest = array)
                Buffer.MemoryCopy(buffer, dest, sizeInBytes, sizeInBytes);
            return array;
        }

        public static godot_packed_vector2_array ConvertSystemArrayToNativePackedVector2Array(Span<Vector2> p_array)
        {
            return ConvertSystemArrayToNativePackedVector2Array((ReadOnlySpan<Vector2>)p_array);
        }

        public static unsafe godot_packed_vector2_array ConvertSystemArrayToNativePackedVector2Array(
            ReadOnlySpan<Vector2> p_array)
        {
            if (p_array.IsEmpty)
                return new godot_packed_vector2_array();
            fixed (Vector2* src = p_array)
                return NativeFuncs.godotsharp_packed_vector2_array_new_mem_copy(src, p_array.Length);
        }

        // PackedVector3Array

        public static unsafe Vector3[] ConvertNativePackedVector3ArrayToSystemArray(godot_packed_vector3_array p_array)
        {
            Vector3* buffer = p_array.Buffer;
            int size = p_array.Size;
            if (size == 0)
                return Array.Empty<Vector3>();
            int sizeInBytes = size * sizeof(Vector3);
            var array = new Vector3[size];
            fixed (Vector3* dest = array)
                Buffer.MemoryCopy(buffer, dest, sizeInBytes, sizeInBytes);
            return array;
        }

        public static godot_packed_vector3_array ConvertSystemArrayToNativePackedVector3Array(Span<Vector3> p_array)
        {
            return ConvertSystemArrayToNativePackedVector3Array((ReadOnlySpan<Vector3>)p_array);
        }

        public static unsafe godot_packed_vector3_array ConvertSystemArrayToNativePackedVector3Array(
            ReadOnlySpan<Vector3> p_array)
        {
            if (p_array.IsEmpty)
                return new godot_packed_vector3_array();
            fixed (Vector3* src = p_array)
                return NativeFuncs.godotsharp_packed_vector3_array_new_mem_copy(src, p_array.Length);
        }

        // PackedVector4Array

        public static unsafe Vector4[] ConvertNativePackedVector4ArrayToSystemArray(godot_packed_vector4_array p_array)
        {
            Vector4* buffer = p_array.Buffer;
            int size = p_array.Size;
            if (size == 0)
                return Array.Empty<Vector4>();
            int sizeInBytes = size * sizeof(Vector4);
            var array = new Vector4[size];
            fixed (Vector4* dest = array)
                Buffer.MemoryCopy(buffer, dest, sizeInBytes, sizeInBytes);
            return array;
        }

        public static godot_packed_vector4_array ConvertSystemArrayToNativePackedVector4Array(Span<Vector4> p_array)
        {
            return ConvertSystemArrayToNativePackedVector4Array((ReadOnlySpan<Vector4>)p_array);
        }

        public static unsafe godot_packed_vector4_array ConvertSystemArrayToNativePackedVector4Array(
            ReadOnlySpan<Vector4> p_array)
        {
            if (p_array.IsEmpty)
                return new godot_packed_vector4_array();
            fixed (Vector4* src = p_array)
                return NativeFuncs.godotsharp_packed_vector4_array_new_mem_copy(src, p_array.Length);
        }

        // PackedColorArray

        public static unsafe Color[] ConvertNativePackedColorArrayToSystemArray(godot_packed_color_array p_array)
        {
            Color* buffer = p_array.Buffer;
            int size = p_array.Size;
            if (size == 0)
                return Array.Empty<Color>();
            int sizeInBytes = size * sizeof(Color);
            var array = new Color[size];
            fixed (Color* dest = array)
                Buffer.MemoryCopy(buffer, dest, sizeInBytes, sizeInBytes);
            return array;
        }

        public static godot_packed_color_array ConvertSystemArrayToNativePackedColorArray(Span<Color> p_array)
        {
            return ConvertSystemArrayToNativePackedColorArray((ReadOnlySpan<Color>)p_array);
        }

        public static unsafe godot_packed_color_array ConvertSystemArrayToNativePackedColorArray(ReadOnlySpan<Color> p_array)
        {
            if (p_array.IsEmpty)
                return new godot_packed_color_array();
            fixed (Color* src = p_array)
                return NativeFuncs.godotsharp_packed_color_array_new_mem_copy(src, p_array.Length);
        }
    }
}
