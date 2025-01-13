using System;

namespace Godot.SourceGenerators
{
    // TODO: May need to think about compatibility here. Could Godot change these values between minor versions?

    internal enum VariantType
    {
        Nil = 0,
        Bool = 1,
        Int = 2,
        Float = 3,
        String = 4,
        Vector2 = 5,
        Vector2I = 6,
        Rect2 = 7,
        Rect2I = 8,
        Vector3 = 9,
        Vector3I = 10,
        Transform2D = 11,
        Vector4 = 12,
        Vector4I = 13,
        Plane = 14,
        Quaternion = 15,
        Aabb = 16,
        Basis = 17,
        Transform3D = 18,
        Projection = 19,
        Color = 20,
        StringName = 21,
        NodePath = 22,
        Rid = 23,
        Object = 24,
        Callable = 25,
        Signal = 26,
        Dictionary = 27,
        Array = 28,
        PackedByteArray = 29,
        PackedInt32Array = 30,
        PackedInt64Array = 31,
        PackedFloat32Array = 32,
        PackedFloat64Array = 33,
        PackedStringArray = 34,
        PackedVector2Array = 35,
        PackedVector3Array = 36,
        PackedColorArray = 37,
        PackedVector4Array = 38,
        Max = 39
    }

    internal enum PropertyHint
    {
        None = 0,
        Range = 1,
        Enum = 2,
        EnumSuggestion = 3,
        ExpEasing = 4,
        Link = 5,
        Flags = 6,
        Layers2DRender = 7,
        Layers2DPhysics = 8,
        Layers2DNavigation = 9,
        Layers3DRender = 10,
        Layers3DPhysics = 11,
        Layers3DNavigation = 12,
        File = 13,
        Dir = 14,
        GlobalFile = 15,
        GlobalDir = 16,
        ResourceType = 17,
        MultilineText = 18,
        Expression = 19,
        PlaceholderText = 20,
        ColorNoAlpha = 21,
        ObjectId = 22,
        TypeString = 23,
        NodePathToEditedNode = 24,
        ObjectTooBig = 25,
        NodePathValidTypes = 26,
        SaveFile = 27,
        GlobalSaveFile = 28,
        IntIsObjectid = 29,
        IntIsPointer = 30,
        ArrayType = 31,
        LocaleId = 32,
        LocalizableString = 33,
        NodeType = 34,
        HideQuaternionEdit = 35,
        Password = 36,
        LayersAvoidance = 37,
        DictionaryType = 38,
        ToolButton = 39,
        Max = 40
    }

    [Flags]
    internal enum PropertyUsageFlags
    {
        None = 0,
        Storage = 2,
        Editor = 4,
        Internal = 8,
        Checkable = 16,
        Checked = 32,
        Group = 64,
        Category = 128,
        Subgroup = 256,
        ClassIsBitfield = 512,
        NoInstanceState = 1024,
        RestartIfChanged = 2048,
        ScriptVariable = 4096,
        StoreIfNull = 8192,
        UpdateAllIfModified = 16384,
        ScriptDefaultValue = 32768,
        ClassIsEnum = 65536,
        NilIsVariant = 131072,
        Array = 262144,
        AlwaysDuplicate = 524288,
        NeverDuplicate = 1048576,
        HighEndGfx = 2097152,
        NodePathFromSceneRoot = 4194304,
        ResourceNotPersistent = 8388608,
        KeyingIncrements = 16777216,
        DeferredSetResource = 33554432,
        EditorInstantiateObject = 67108864,
        EditorBasicSetting = 134217728,
        ReadOnly = 268435456,
        Default = 6,
        NoEditor = 2
    }

    [Flags]
    public enum MethodFlags
    {
        Normal = 1,
        Editor = 2,
        Const = 4,
        Virtual = 8,
        Vararg = 16,
        Static = 32,
        ObjectCore = 64,
        Default = 1
    }
}
