using System;
using System.Runtime.CompilerServices;
using Godot.NativeInterop;

namespace Godot;

#nullable enable

// TODO: Disabled because it is a false positive, see https://github.com/dotnet/roslyn-analyzers/issues/6151
#pragma warning disable CA1001 // Types that own disposable fields should be disposable
public partial struct Variant : IDisposable
#pragma warning restore CA1001
{
    internal godot_variant.movable NativeVar;
    private object? _obj;
    private Disposer? _disposer;

    private sealed class Disposer : IDisposable
    {
        private godot_variant.movable _native;

        private WeakReference<IDisposable>? _weakReferenceToSelf;

        public Disposer(in godot_variant.movable nativeVar)
        {
            _native = nativeVar;
            _weakReferenceToSelf = DisposablesTracker.RegisterDisposable(this);
        }

        ~Disposer()
        {
            Dispose(false);
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        private void Dispose(bool disposing)
        {
            _native.DangerousSelfRef.Dispose();

            if (_weakReferenceToSelf != null)
            {
                DisposablesTracker.UnregisterDisposable(_weakReferenceToSelf);
            }
        }
    }

    private Variant(in godot_variant nativeVar)
    {
        NativeVar = (godot_variant.movable)nativeVar;
        _obj = null;

        switch (nativeVar.Type)
        {
            case Type.Nil:
            case Type.Bool:
            case Type.Int:
            case Type.Float:
            case Type.Vector2:
            case Type.Vector2I:
            case Type.Rect2:
            case Type.Rect2I:
            case Type.Vector3:
            case Type.Vector3I:
            case Type.Vector4:
            case Type.Vector4I:
            case Type.Plane:
            case Type.Quaternion:
            case Type.Color:
            case Type.Rid:
                _disposer = null;
                break;
            default:
            {
                _disposer = new Disposer(NativeVar);
                break;
            }
        }
    }

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    // Explicit name to make it very clear
    public static Variant CreateTakingOwnershipOfDisposableValue(in godot_variant nativeValueToOwn) =>
        new(nativeValueToOwn);

    // Explicit name to make it very clear
    public static Variant CreateCopyingBorrowed(in godot_variant nativeValueToOwn) =>
        new(NativeFuncs.godotsharp_variant_new_copy(nativeValueToOwn));

    /// <summary>
    /// Constructs a new <see cref="Godot.NativeInterop.godot_variant"/> from this instance.
    /// The caller is responsible of disposing the new instance to avoid memory leaks.
    /// </summary>
    public godot_variant CopyNativeVariant() =>
        NativeFuncs.godotsharp_variant_new_copy((godot_variant)NativeVar);

    public void Dispose()
    {
        _disposer?.Dispose();
        NativeVar = default;
        _obj = null;
    }

    // TODO: Consider renaming Variant.Type to VariantType and this property to Type. VariantType would also avoid ambiguity with System.Type.
    public Type VariantType => NativeVar.DangerousSelfRef.Type;

    public override string ToString() => AsString();

    public object? Obj =>
        _obj ??= NativeVar.DangerousSelfRef.Type switch
        {
            Type.Bool => AsBool(),
            Type.Int => AsInt64(),
            Type.Float => AsDouble(),
            Type.String => AsString(),
            Type.Vector2 => AsVector2(),
            Type.Vector2I => AsVector2I(),
            Type.Rect2 => AsRect2(),
            Type.Rect2I => AsRect2I(),
            Type.Vector3 => AsVector3(),
            Type.Vector3I => AsVector3I(),
            Type.Transform2D => AsTransform2D(),
            Type.Vector4 => AsVector4(),
            Type.Vector4I => AsVector4I(),
            Type.Plane => AsPlane(),
            Type.Quaternion => AsQuaternion(),
            Type.Aabb => AsAabb(),
            Type.Basis => AsBasis(),
            Type.Transform3D => AsTransform3D(),
            Type.Projection => AsProjection(),
            Type.Color => AsColor(),
            Type.StringName => AsStringName(),
            Type.NodePath => AsNodePath(),
            Type.Rid => AsRid(),
            Type.Object => AsGodotObject(),
            Type.Callable => AsCallable(),
            Type.Signal => AsSignal(),
            Type.Dictionary => AsGodotDictionary(),
            Type.Array => AsGodotArray(),
            Type.PackedByteArray => AsByteArray(),
            Type.PackedInt32Array => AsInt32Array(),
            Type.PackedInt64Array => AsInt64Array(),
            Type.PackedFloat32Array => AsFloat32Array(),
            Type.PackedFloat64Array => AsFloat64Array(),
            Type.PackedStringArray => AsStringArray(),
            Type.PackedVector2Array => AsVector2Array(),
            Type.PackedVector3Array => AsVector3Array(),
            Type.PackedVector4Array => AsVector4Array(),
            Type.PackedColorArray => AsColorArray(),
            Type.Nil => null,
            Type.Max or _ =>
                throw new InvalidOperationException($"Invalid Variant type: {NativeVar.DangerousSelfRef.Type}"),
        };

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant From<[MustBeVariant] T>(in T from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFrom(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public T Cast<[MustBeVariant] T>() where T : GodotObject =>
        VariantUtils.ConvertTo<T>(NativeVar.DangerousSelfRef, true);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public T As<[MustBeVariant] T>() =>
        VariantUtils.ConvertTo<T>(NativeVar.DangerousSelfRef, false);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public bool AsBool() =>
        VariantUtils.ConvertToBool((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public char AsChar() =>
        (char)VariantUtils.ConvertToUInt16((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public sbyte AsSByte() =>
        VariantUtils.ConvertToInt8((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public short AsInt16() =>
        VariantUtils.ConvertToInt16((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public int AsInt32() =>
        VariantUtils.ConvertToInt32((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public long AsInt64() =>
        VariantUtils.ConvertToInt64((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public byte AsByte() =>
        VariantUtils.ConvertToUInt8((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public ushort AsUInt16() =>
        VariantUtils.ConvertToUInt16((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public uint AsUInt32() =>
        VariantUtils.ConvertToUInt32((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public ulong AsUInt64() =>
        VariantUtils.ConvertToUInt64((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public float AsSingle() =>
        VariantUtils.ConvertToFloat32((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public double AsDouble() =>
        VariantUtils.ConvertToFloat64((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public string AsString() =>
        VariantUtils.ConvertToString((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Vector2 AsVector2() =>
        VariantUtils.ConvertToVector2((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Vector2I AsVector2I() =>
        VariantUtils.ConvertToVector2I((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Rect2 AsRect2() =>
        VariantUtils.ConvertToRect2((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Rect2I AsRect2I() =>
        VariantUtils.ConvertToRect2I((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Transform2D AsTransform2D() =>
        VariantUtils.ConvertToTransform2D((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Vector3 AsVector3() =>
        VariantUtils.ConvertToVector3((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Vector3I AsVector3I() =>
        VariantUtils.ConvertToVector3I((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Basis AsBasis() =>
        VariantUtils.ConvertToBasis((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Quaternion AsQuaternion() =>
        VariantUtils.ConvertToQuaternion((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Transform3D AsTransform3D() =>
        VariantUtils.ConvertToTransform3D((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Vector4 AsVector4() =>
        VariantUtils.ConvertToVector4((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Vector4I AsVector4I() =>
        VariantUtils.ConvertToVector4I((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Projection AsProjection() =>
        VariantUtils.ConvertToProjection((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Aabb AsAabb() =>
        VariantUtils.ConvertToAabb((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Color AsColor() =>
        VariantUtils.ConvertToColor((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Plane AsPlane() =>
        VariantUtils.ConvertToPlane((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Callable AsCallable() =>
        VariantUtils.ConvertToCallable((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Signal AsSignal() =>
        VariantUtils.ConvertToSignal((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public byte[] AsByteArray() =>
        VariantUtils.ConvertAsPackedByteArrayToSystemArray((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public int[] AsInt32Array() =>
        VariantUtils.ConvertAsPackedInt32ArrayToSystemArray((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public long[] AsInt64Array() =>
        VariantUtils.ConvertAsPackedInt64ArrayToSystemArray((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public float[] AsFloat32Array() =>
        VariantUtils.ConvertAsPackedFloat32ArrayToSystemArray((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public double[] AsFloat64Array() =>
        VariantUtils.ConvertAsPackedFloat64ArrayToSystemArray((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public string[] AsStringArray() =>
        VariantUtils.ConvertAsPackedStringArrayToSystemArray((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Vector2[] AsVector2Array() =>
        VariantUtils.ConvertAsPackedVector2ArrayToSystemArray((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Vector3[] AsVector3Array() =>
        VariantUtils.ConvertAsPackedVector3ArrayToSystemArray((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Vector4[] AsVector4Array() =>
        VariantUtils.ConvertAsPackedVector4ArrayToSystemArray((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Color[] AsColorArray() =>
        VariantUtils.ConvertAsPackedColorArrayToSystemArray((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public T[] AsGodotObjectArray<T>()
        where T : GodotObject =>
        VariantUtils.ConvertToSystemArrayOfGodotObject<T>((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Collections.Dictionary<TKey, TValue> AsGodotDictionary<[MustBeVariant] TKey, [MustBeVariant] TValue>() =>
        VariantUtils.ConvertToDictionary<TKey, TValue>((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Collections.Array<T> AsGodotArray<[MustBeVariant] T>() =>
        VariantUtils.ConvertToArray<T>((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public StringName[] AsSystemArrayOfStringName() =>
        VariantUtils.ConvertToSystemArrayOfStringName((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public NodePath[] AsSystemArrayOfNodePath() =>
        VariantUtils.ConvertToSystemArrayOfNodePath((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Rid[] AsSystemArrayOfRid() =>
        VariantUtils.ConvertToSystemArrayOfRid((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public GodotObject AsGodotObject() =>
        VariantUtils.ConvertToGodotObject((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public StringName AsStringName() =>
        VariantUtils.ConvertToStringName((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public NodePath AsNodePath() =>
        VariantUtils.ConvertToNodePath((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Rid AsRid() =>
        VariantUtils.ConvertToRid((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Collections.Dictionary AsGodotDictionary() =>
        VariantUtils.ConvertToDictionary((godot_variant)NativeVar);

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public Collections.Array AsGodotArray() =>
        VariantUtils.ConvertToArray((godot_variant)NativeVar);

    // Explicit conversion operators to supported types

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator bool(Variant from) => from.AsBool();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator char(Variant from) => from.AsChar();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator sbyte(Variant from) => from.AsSByte();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator short(Variant from) => from.AsInt16();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator int(Variant from) => from.AsInt32();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator long(Variant from) => from.AsInt64();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator byte(Variant from) => from.AsByte();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator ushort(Variant from) => from.AsUInt16();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator uint(Variant from) => from.AsUInt32();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator ulong(Variant from) => from.AsUInt64();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator float(Variant from) => from.AsSingle();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator double(Variant from) => from.AsDouble();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator string(Variant from) => from.AsString();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Vector2(Variant from) => from.AsVector2();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Vector2I(Variant from) => from.AsVector2I();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Rect2(Variant from) => from.AsRect2();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Rect2I(Variant from) => from.AsRect2I();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Transform2D(Variant from) => from.AsTransform2D();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Vector3(Variant from) => from.AsVector3();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Vector3I(Variant from) => from.AsVector3I();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Basis(Variant from) => from.AsBasis();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Quaternion(Variant from) => from.AsQuaternion();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Transform3D(Variant from) => from.AsTransform3D();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Vector4(Variant from) => from.AsVector4();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Vector4I(Variant from) => from.AsVector4I();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Projection(Variant from) => from.AsProjection();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Aabb(Variant from) => from.AsAabb();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Color(Variant from) => from.AsColor();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Plane(Variant from) => from.AsPlane();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Callable(Variant from) => from.AsCallable();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Signal(Variant from) => from.AsSignal();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator byte[](Variant from) => from.AsByteArray();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator int[](Variant from) => from.AsInt32Array();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator long[](Variant from) => from.AsInt64Array();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator float[](Variant from) => from.AsFloat32Array();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator double[](Variant from) => from.AsFloat64Array();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator string[](Variant from) => from.AsStringArray();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Vector2[](Variant from) => from.AsVector2Array();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Vector3[](Variant from) => from.AsVector3Array();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Vector4[](Variant from) => from.AsVector4Array();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Color[](Variant from) => from.AsColorArray();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator StringName[](Variant from) => from.AsSystemArrayOfStringName();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator NodePath[](Variant from) => from.AsSystemArrayOfNodePath();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Rid[](Variant from) => from.AsSystemArrayOfRid();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator GodotObject(Variant from) => from.AsGodotObject();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator StringName(Variant from) => from.AsStringName();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator NodePath(Variant from) => from.AsNodePath();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Rid(Variant from) => from.AsRid();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Collections.Dictionary(Variant from) => from.AsGodotDictionary();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static explicit operator Collections.Array(Variant from) => from.AsGodotArray();

    // While we provide implicit conversion operators, normal methods are still needed for
    // casts that are not done implicitly (e.g.: raw array to Span, enum to integer, etc).

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(bool from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(char from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(sbyte from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(short from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(int from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(long from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(byte from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(ushort from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(uint from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(ulong from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(float from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(double from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(string from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Vector2 from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Vector2I from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Rect2 from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Rect2I from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Transform2D from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Vector3 from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Vector3I from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Basis from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Quaternion from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Transform3D from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Vector4 from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Vector4I from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Projection from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Aabb from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Color from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Plane from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Callable from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Signal from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Span<byte> from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Span<int> from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Span<long> from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Span<float> from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Span<double> from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Span<string> from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Span<Vector2> from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Span<Vector3> from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Span<Vector4> from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Span<Color> from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(GodotObject[] from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom<[MustBeVariant] TKey, [MustBeVariant] TValue>(Collections.Dictionary<TKey, TValue> from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromDictionary(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom<[MustBeVariant] T>(Collections.Array<T> from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromArray(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Span<StringName> from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Span<NodePath> from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Span<Rid> from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(GodotObject from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(StringName from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(NodePath from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Rid from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Collections.Dictionary from) => from;

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static Variant CreateFrom(Collections.Array from) => from;

    // Implicit conversion operators

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(bool from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromBool(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(char from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromInt(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(sbyte from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromInt(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(short from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromInt(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(int from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromInt(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(long from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromInt(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(byte from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromInt(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(ushort from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromInt(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(uint from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromInt(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(ulong from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromInt(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(float from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromFloat(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(double from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromFloat(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(string from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromString(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Vector2 from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromVector2(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Vector2I from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromVector2I(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Rect2 from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromRect2(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Rect2I from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromRect2I(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Transform2D from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromTransform2D(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Vector3 from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromVector3(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Vector3I from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromVector3I(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Basis from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromBasis(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Quaternion from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromQuaternion(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Transform3D from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromTransform3D(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Vector4 from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromVector4(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Vector4I from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromVector4I(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Projection from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromProjection(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Aabb from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromAabb(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Color from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromColor(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Plane from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromPlane(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Callable from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromCallable(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Signal from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromSignal(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(byte[] from) =>
        (Variant)from.AsSpan();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(int[] from) =>
        (Variant)from.AsSpan();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(long[] from) =>
        (Variant)from.AsSpan();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(float[] from) =>
        (Variant)from.AsSpan();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(double[] from) =>
        (Variant)from.AsSpan();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(string[] from) =>
        (Variant)from.AsSpan();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Vector2[] from) =>
        (Variant)from.AsSpan();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Vector3[] from) =>
        (Variant)from.AsSpan();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Vector4[] from) =>
        (Variant)from.AsSpan();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Color[] from) =>
        (Variant)from.AsSpan();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(GodotObject[] from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromSystemArrayOfGodotObject(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(StringName[] from) =>
        (Variant)from.AsSpan();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(NodePath[] from) =>
        (Variant)from.AsSpan();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Rid[] from) =>
        (Variant)from.AsSpan();

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Span<byte> from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromPackedByteArray(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Span<int> from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromPackedInt32Array(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Span<long> from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromPackedInt64Array(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Span<float> from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromPackedFloat32Array(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Span<double> from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromPackedFloat64Array(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Span<string> from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromPackedStringArray(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Span<Vector2> from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromPackedVector2Array(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Span<Vector3> from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromPackedVector3Array(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Span<Vector4> from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromPackedVector4Array(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Span<Color> from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromPackedColorArray(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Span<StringName> from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromSystemArrayOfStringName(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Span<NodePath> from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromSystemArrayOfNodePath(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Span<Rid> from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromSystemArrayOfRid(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(GodotObject from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromGodotObject(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(StringName from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromStringName(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(NodePath from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromNodePath(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Rid from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromRid(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Collections.Dictionary from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromDictionary(from));

    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public static implicit operator Variant(Collections.Array from) =>
        CreateTakingOwnershipOfDisposableValue(VariantUtils.CreateFromArray(from));
}
