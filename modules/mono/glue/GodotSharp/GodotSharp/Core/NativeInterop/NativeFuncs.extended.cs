#pragma warning disable CA1707 // Identifiers should not contain underscores
#pragma warning disable IDE1006 // Naming rule violation
// ReSharper disable InconsistentNaming

namespace Godot.NativeInterop
{
    public static partial class NativeFuncs
    {
        public static godot_variant godotsharp_variant_new_copy(scoped in godot_variant src)
        {
            switch (src.Type)
            {
                case Variant.Type.Nil:
                    return default;
                case Variant.Type.Bool:
                    return new godot_variant() { Bool = src.Bool, Type = Variant.Type.Bool };
                case Variant.Type.Int:
                    return new godot_variant() { Int = src.Int, Type = Variant.Type.Int };
                case Variant.Type.Float:
                    return new godot_variant() { Float = src.Float, Type = Variant.Type.Float };
                case Variant.Type.Vector2:
                    return new godot_variant() { Vector2 = src.Vector2, Type = Variant.Type.Vector2 };
                case Variant.Type.Vector2I:
                    return new godot_variant() { Vector2I = src.Vector2I, Type = Variant.Type.Vector2I };
                case Variant.Type.Rect2:
                    return new godot_variant() { Rect2 = src.Rect2, Type = Variant.Type.Rect2 };
                case Variant.Type.Rect2I:
                    return new godot_variant() { Rect2I = src.Rect2I, Type = Variant.Type.Rect2I };
                case Variant.Type.Vector3:
                    return new godot_variant() { Vector3 = src.Vector3, Type = Variant.Type.Vector3 };
                case Variant.Type.Vector3I:
                    return new godot_variant() { Vector3I = src.Vector3I, Type = Variant.Type.Vector3I };
                case Variant.Type.Vector4:
                    return new godot_variant() { Vector4 = src.Vector4, Type = Variant.Type.Vector4 };
                case Variant.Type.Vector4I:
                    return new godot_variant() { Vector4I = src.Vector4I, Type = Variant.Type.Vector4I };
                case Variant.Type.Plane:
                    return new godot_variant() { Plane = src.Plane, Type = Variant.Type.Plane };
                case Variant.Type.Quaternion:
                    return new godot_variant() { Quaternion = src.Quaternion, Type = Variant.Type.Quaternion };
                case Variant.Type.Color:
                    return new godot_variant() { Color = src.Color, Type = Variant.Type.Color };
                case Variant.Type.Rid:
                    return new godot_variant() { Rid = src.Rid, Type = Variant.Type.Rid };
            }

            godotsharp_variant_new_copy(out godot_variant ret, src);
            return ret;
        }

        public static godot_string_name godotsharp_string_name_new_copy(scoped in godot_string_name src)
        {
            if (src.IsEmpty)
                return default;
            godotsharp_string_name_new_copy(out godot_string_name ret, src);
            return ret;
        }

        public static godot_node_path godotsharp_node_path_new_copy(scoped in godot_node_path src)
        {
            if (src.IsEmpty)
                return default;
            godotsharp_node_path_new_copy(out godot_node_path ret, src);
            return ret;
        }

        public static godot_array godotsharp_array_new()
        {
            godotsharp_array_new(out godot_array ret);
            return ret;
        }

        public static godot_array godotsharp_array_new_copy(scoped in godot_array src)
        {
            godotsharp_array_new_copy(out godot_array ret, src);
            return ret;
        }

        public static godot_dictionary godotsharp_dictionary_new()
        {
            godotsharp_dictionary_new(out godot_dictionary ret);
            return ret;
        }

        public static godot_dictionary godotsharp_dictionary_new_copy(scoped in godot_dictionary src)
        {
            godotsharp_dictionary_new_copy(out godot_dictionary ret, src);
            return ret;
        }

        public static godot_string_name godotsharp_string_name_new_from_string(string name)
        {
            using godot_string src = Marshaling.ConvertStringToNative(name);
            godotsharp_string_name_new_from_string(out godot_string_name ret, src);
            return ret;
        }

        public static godot_node_path godotsharp_node_path_new_from_string(string name)
        {
            using godot_string src = Marshaling.ConvertStringToNative(name);
            godotsharp_node_path_new_from_string(out godot_node_path ret, src);
            return ret;
        }
    }
}
