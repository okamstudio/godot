using System;
using Godot;
using Godot.Collections;
using Array = Godot.Collections.Array;

public class MustBeVariantGD0301
{
    public void MethodCallsError()
    {
        // This raises a GD0301 diagnostic error: object is not Variant (and Method<T> requires a variant generic type).
        Method<{|GD0301:object|}>();
    }

    public void MethodCallsOk()
    {
        // All these calls are valid because they are Variant types.
        Method<bool>();
        Method<char>();
        Method<sbyte>();
        Method<byte>();
        Method<short>();
        Method<ushort>();
        Method<int>();
        Method<uint>();
        Method<long>();
        Method<ulong>();
        Method<float>();
        Method<double>();
        Method<string>();
        Method<Vector2>();
        Method<Vector2I>();
        Method<Rect2>();
        Method<Rect2I>();
        Method<Transform2D>();
        Method<Vector3>();
        Method<Vector3I>();
        Method<Vector4>();
        Method<Vector4I>();
        Method<Basis>();
        Method<Quaternion>();
        Method<Transform3D>();
        Method<Projection>();
        Method<Aabb>();
        Method<Color>();
        Method<Plane>();
        Method<Callable>();
        Method<Signal>();
        Method<GodotObject>();
        Method<StringName>();
        Method<NodePath>();
        Method<Rid>();
        Method<Dictionary>();
        Method<Array>();
        Method<byte[]>();
        Method<int[]>();
        Method<long[]>();
        Method<float[]>();
        Method<double[]>();
        Method<string[]>();
        Method<Vector2[]>();
        Method<Vector3[]>();
        Method<Color[]>();
        Method<GodotObject[]>();
        Method<StringName[]>();
        Method<NodePath[]>();
        Method<Rid[]>();
    }

    public void MethodCallDynamic()
    {
        dynamic self = this;
        self.Method<object>();
    }

    public void Method<[MustBeVariant] T>()
    {
    }

    public void MustBeVariantClasses()
    {
        new ClassWithGenericVariant<bool>();
        new ClassWithGenericVariant<char>();
        new ClassWithGenericVariant<sbyte>();
        new ClassWithGenericVariant<byte>();
        new ClassWithGenericVariant<short>();
        new ClassWithGenericVariant<ushort>();
        new ClassWithGenericVariant<int>();
        new ClassWithGenericVariant<uint>();
        new ClassWithGenericVariant<long>();
        new ClassWithGenericVariant<ulong>();
        new ClassWithGenericVariant<float>();
        new ClassWithGenericVariant<double>();
        new ClassWithGenericVariant<string>();
        new ClassWithGenericVariant<Vector2>();
        new ClassWithGenericVariant<Vector2I>();
        new ClassWithGenericVariant<Rect2>();
        new ClassWithGenericVariant<Rect2I>();
        new ClassWithGenericVariant<Transform2D>();
        new ClassWithGenericVariant<Vector3>();
        new ClassWithGenericVariant<Vector3I>();
        new ClassWithGenericVariant<Vector4>();
        new ClassWithGenericVariant<Vector4I>();
        new ClassWithGenericVariant<Basis>();
        new ClassWithGenericVariant<Quaternion>();
        new ClassWithGenericVariant<Transform3D>();
        new ClassWithGenericVariant<Projection>();
        new ClassWithGenericVariant<Aabb>();
        new ClassWithGenericVariant<Color>();
        new ClassWithGenericVariant<Plane>();
        new ClassWithGenericVariant<Callable>();
        new ClassWithGenericVariant<Signal>();
        new ClassWithGenericVariant<GodotObject>();
        new ClassWithGenericVariant<StringName>();
        new ClassWithGenericVariant<NodePath>();
        new ClassWithGenericVariant<Rid>();
        new ClassWithGenericVariant<Dictionary>();
        new ClassWithGenericVariant<Array>();
        new ClassWithGenericVariant<byte[]>();
        new ClassWithGenericVariant<int[]>();
        new ClassWithGenericVariant<long[]>();
        new ClassWithGenericVariant<float[]>();
        new ClassWithGenericVariant<double[]>();
        new ClassWithGenericVariant<string[]>();
        new ClassWithGenericVariant<Vector2[]>();
        new ClassWithGenericVariant<Vector3[]>();
        new ClassWithGenericVariant<Color[]>();
        new ClassWithGenericVariant<GodotObject[]>();
        new ClassWithGenericVariant<StringName[]>();
        new ClassWithGenericVariant<NodePath[]>();
        new ClassWithGenericVariant<Rid[]>();

        // This class fails because generic type is not Variant-compatible.
        new ClassWithGenericVariant<{|GD0301:object|}>();
    }
}

public class ClassWithGenericVariant<[MustBeVariant] T>
{
}

public class MustBeVariantAnnotatedMethods
{
    [GenericTypeAttribute<bool>()]
    public void MethodWithAttributeBool()
    {
    }

    [GenericTypeAttribute<char>()]
    public void MethodWithAttributeChar()
    {
    }

    [GenericTypeAttribute<sbyte>()]
    public void MethodWithAttributeSByte()
    {
    }

    [GenericTypeAttribute<byte>()]
    public void MethodWithAttributeByte()
    {
    }

    [GenericTypeAttribute<short>()]
    public void MethodWithAttributeInt16()
    {
    }

    [GenericTypeAttribute<ushort>()]
    public void MethodWithAttributeUInt16()
    {
    }

    [GenericTypeAttribute<int>()]
    public void MethodWithAttributeInt32()
    {
    }

    [GenericTypeAttribute<uint>()]
    public void MethodWithAttributeUInt32()
    {
    }

    [GenericTypeAttribute<long>()]
    public void MethodWithAttributeInt64()
    {
    }

    [GenericTypeAttribute<ulong>()]
    public void MethodWithAttributeUInt64()
    {
    }

    [GenericTypeAttribute<float>()]
    public void MethodWithAttributeSingle()
    {
    }

    [GenericTypeAttribute<double>()]
    public void MethodWithAttributeDouble()
    {
    }

    [GenericTypeAttribute<string>()]
    public void MethodWithAttributeString()
    {
    }

    [GenericTypeAttribute<Vector2>()]
    public void MethodWithAttributeVector2()
    {
    }

    [GenericTypeAttribute<Vector2I>()]
    public void MethodWithAttributeVector2I()
    {
    }

    [GenericTypeAttribute<Rect2>()]
    public void MethodWithAttributeRect2()
    {
    }

    [GenericTypeAttribute<Rect2I>()]
    public void MethodWithAttributeRect2I()
    {
    }

    [GenericTypeAttribute<Transform2D>()]
    public void MethodWithAttributeTransform2D()
    {
    }

    [GenericTypeAttribute<Vector3>()]
    public void MethodWithAttributeVector3()
    {
    }

    [GenericTypeAttribute<Vector3I>()]
    public void MethodWithAttributeVector3I()
    {
    }

    [GenericTypeAttribute<Vector4>()]
    public void MethodWithAttributeVector4()
    {
    }

    [GenericTypeAttribute<Vector4I>()]
    public void MethodWithAttributeVector4I()
    {
    }

    [GenericTypeAttribute<Basis>()]
    public void MethodWithAttributeBasis()
    {
    }

    [GenericTypeAttribute<Quaternion>()]
    public void MethodWithAttributeQuaternion()
    {
    }

    [GenericTypeAttribute<Transform3D>()]
    public void MethodWithAttributeTransform3D()
    {
    }

    [GenericTypeAttribute<Projection>()]
    public void MethodWithAttributeProjection()
    {
    }

    [GenericTypeAttribute<Aabb>()]
    public void MethodWithAttributeAabb()
    {
    }

    [GenericTypeAttribute<Color>()]
    public void MethodWithAttributeColor()
    {
    }

    [GenericTypeAttribute<Plane>()]
    public void MethodWithAttributePlane()
    {
    }

    [GenericTypeAttribute<Callable>()]
    public void MethodWithAttributeCallable()
    {
    }

    [GenericTypeAttribute<Signal>()]
    public void MethodWithAttributeSignal()
    {
    }

    [GenericTypeAttribute<GodotObject>()]
    public void MethodWithAttributeGodotObject()
    {
    }

    [GenericTypeAttribute<StringName>()]
    public void MethodWithAttributeStringName()
    {
    }

    [GenericTypeAttribute<NodePath>()]
    public void MethodWithAttributeNodePath()
    {
    }

    [GenericTypeAttribute<Rid>()]
    public void MethodWithAttributeRid()
    {
    }

    [GenericTypeAttribute<Dictionary>()]
    public void MethodWithAttributeDictionary()
    {
    }

    [GenericTypeAttribute<Array>()]
    public void MethodWithAttributeArray()
    {
    }

    [GenericTypeAttribute<byte[]>()]
    public void MethodWithAttributeByteArray()
    {
    }

    [GenericTypeAttribute<int[]>()]
    public void MethodWithAttributeInt32Array()
    {
    }

    [GenericTypeAttribute<long[]>()]
    public void MethodWithAttributeInt64Array()
    {
    }

    [GenericTypeAttribute<float[]>()]
    public void MethodWithAttributeSingleArray()
    {
    }

    [GenericTypeAttribute<double[]>()]
    public void MethodWithAttributeDoubleArray()
    {
    }

    [GenericTypeAttribute<string[]>()]
    public void MethodWithAttributeStringArray()
    {
    }

    [GenericTypeAttribute<Vector2[]>()]
    public void MethodWithAttributeVector2Array()
    {
    }

    [GenericTypeAttribute<Vector3[]>()]
    public void MethodWithAttributeVector3Array()
    {
    }

    [GenericTypeAttribute<Color[]>()]
    public void MethodWithAttributeColorArray()
    {
    }

    [GenericTypeAttribute<GodotObject[]>()]
    public void MethodWithAttributeGodotObjectArray()
    {
    }

    [GenericTypeAttribute<StringName[]>()]
    public void MethodWithAttributeStringNameArray()
    {
    }

    [GenericTypeAttribute<NodePath[]>()]
    public void MethodWithAttributeNodePathArray()
    {
    }

    [GenericTypeAttribute<Rid[]>()]
    public void MethodWithAttributeRidArray()
    {
    }

    // This method definition fails because generic type is not Variant-compatible.
    [GenericTypeAttribute<{|GD0301:object|}>()]
    public void MethodWithWrongAttribute()
    {
    }

    [NestedGenericTypeAttributeContainer.NestedGenericTypeAttribute<bool>()]
    public void MethodWithNestedAttribute()
    {
    }
}

[GenericTypeAttribute<bool>()]
public class ClassVariantAnnotatedBool
{
}

[GenericTypeAttribute<char>()]
public class ClassVariantAnnotatedChar
{
}

[GenericTypeAttribute<sbyte>()]
public class ClassVariantAnnotatedSByte
{
}

[GenericTypeAttribute<byte>()]
public class ClassVariantAnnotatedByte
{
}

[GenericTypeAttribute<short>()]
public class ClassVariantAnnotatedInt16
{
}

[GenericTypeAttribute<ushort>()]
public class ClassVariantAnnotatedUInt16
{
}

[GenericTypeAttribute<int>()]
public class ClassVariantAnnotatedInt32
{
}

[GenericTypeAttribute<uint>()]
public class ClassVariantAnnotatedUInt32
{
}

[GenericTypeAttribute<long>()]
public class ClassVariantAnnotatedInt64
{
}

[GenericTypeAttribute<ulong>()]
public class ClassVariantAnnotatedUInt64
{
}

[GenericTypeAttribute<float>()]
public class ClassVariantAnnotatedSingle
{
}

[GenericTypeAttribute<double>()]
public class ClassVariantAnnotatedDouble
{
}

[GenericTypeAttribute<string>()]
public class ClassVariantAnnotatedString
{
}

[GenericTypeAttribute<Vector2>()]
public class ClassVariantAnnotatedVector2
{
}

[GenericTypeAttribute<Vector2I>()]
public class ClassVariantAnnotatedVector2I
{
}

[GenericTypeAttribute<Rect2>()]
public class ClassVariantAnnotatedRect2
{
}

[GenericTypeAttribute<Rect2I>()]
public class ClassVariantAnnotatedRect2I
{
}

[GenericTypeAttribute<Transform2D>()]
public class ClassVariantAnnotatedTransform2D
{
}

[GenericTypeAttribute<Vector3>()]
public class ClassVariantAnnotatedVector3
{
}

[GenericTypeAttribute<Vector3I>()]
public class ClassVariantAnnotatedVector3I
{
}

[GenericTypeAttribute<Vector4>()]
public class ClassVariantAnnotatedVector4
{
}

[GenericTypeAttribute<Vector4I>()]
public class ClassVariantAnnotatedVector4I
{
}

[GenericTypeAttribute<Basis>()]
public class ClassVariantAnnotatedBasis
{
}

[GenericTypeAttribute<Quaternion>()]
public class ClassVariantAnnotatedQuaternion
{
}

[GenericTypeAttribute<Transform3D>()]
public class ClassVariantAnnotatedTransform3D
{
}

[GenericTypeAttribute<Projection>()]
public class ClassVariantAnnotatedProjection
{
}

[GenericTypeAttribute<Aabb>()]
public class ClassVariantAnnotatedAabb
{
}

[GenericTypeAttribute<Color>()]
public class ClassVariantAnnotatedColor
{
}

[GenericTypeAttribute<Plane>()]
public class ClassVariantAnnotatedPlane
{
}

[GenericTypeAttribute<Callable>()]
public class ClassVariantAnnotatedCallable
{
}

[GenericTypeAttribute<Signal>()]
public class ClassVariantAnnotatedSignal
{
}

[GenericTypeAttribute<GodotObject>()]
public class ClassVariantAnnotatedGodotObject
{
}

[GenericTypeAttribute<StringName>()]
public class ClassVariantAnnotatedStringName
{
}

[GenericTypeAttribute<NodePath>()]
public class ClassVariantAnnotatedNodePath
{
}

[GenericTypeAttribute<Rid>()]
public class ClassVariantAnnotatedRid
{
}

[GenericTypeAttribute<Dictionary>()]
public class ClassVariantAnnotatedDictionary
{
}

[GenericTypeAttribute<Array>()]
public class ClassVariantAnnotatedArray
{
}

[GenericTypeAttribute<byte[]>()]
public class ClassVariantAnnotatedByteArray
{
}

[GenericTypeAttribute<int[]>()]
public class ClassVariantAnnotatedInt32Array
{
}

[GenericTypeAttribute<long[]>()]
public class ClassVariantAnnotatedInt64Array
{
}

[GenericTypeAttribute<float[]>()]
public class ClassVariantAnnotatedSingleArray
{
}

[GenericTypeAttribute<double[]>()]
public class ClassVariantAnnotatedDoubleArray
{
}

[GenericTypeAttribute<string[]>()]
public class ClassVariantAnnotatedStringArray
{
}

[GenericTypeAttribute<Vector2[]>()]
public class ClassVariantAnnotatedVector2Array
{
}

[GenericTypeAttribute<Vector3[]>()]
public class ClassVariantAnnotatedVector3Array
{
}

[GenericTypeAttribute<Color[]>()]
public class ClassVariantAnnotatedColorArray
{
}

[GenericTypeAttribute<GodotObject[]>()]
public class ClassVariantAnnotatedGodotObjectArray
{
}

[GenericTypeAttribute<StringName[]>()]
public class ClassVariantAnnotatedStringNameArray
{
}

[GenericTypeAttribute<NodePath[]>()]
public class ClassVariantAnnotatedNodePathArray
{
}

[GenericTypeAttribute<Rid[]>()]
public class ClassVariantAnnotatedRidArray
{
}

// This class definition fails because generic type is not Variant-compatible.
[GenericTypeAttribute<{|GD0301:object|}>()]
public class ClassNonVariantAnnotated
{
}

[AttributeUsage(AttributeTargets.Class | AttributeTargets.Method, AllowMultiple = true)]
public class GenericTypeAttribute<[MustBeVariant] T> : Attribute
{
}

public class NestedGenericTypeAttributeContainer
{
    [AttributeUsage(AttributeTargets.Method, AllowMultiple = true)]
    public class NestedGenericTypeAttribute<[MustBeVariant] T> : Attribute
    {
    }
}
