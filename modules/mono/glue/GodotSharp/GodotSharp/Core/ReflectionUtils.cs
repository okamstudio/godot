using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;

#nullable enable

namespace Godot;

internal class ReflectionUtils
{
    private static readonly HashSet<Type>? _tupleTypeSet;
    private static readonly Dictionary<Type, string>? _builtinTypeNameDictionary;
    internal static readonly bool IsEditorHintCached;

    static ReflectionUtils()
    {
        IsEditorHintCached = Engine.IsEditorHint();
        if (!IsEditorHintCached)
        {
            return;
        }

        _tupleTypeSet = new HashSet<Type>
        {
            // ValueTuple with only one element should be treated as normal generic type.
            //typeof(ValueTuple<>),
            typeof(ValueTuple<,>),
            typeof(ValueTuple<,,>),
            typeof(ValueTuple<,,,>),
            typeof(ValueTuple<,,,,>),
            typeof(ValueTuple<,,,,,>),
            typeof(ValueTuple<,,,,,,>),
            typeof(ValueTuple<,,,,,,,>),
        };

        _builtinTypeNameDictionary ??= new Dictionary<Type, string>
        {
            { typeof(sbyte), "sbyte" },
            { typeof(byte), "byte" },
            { typeof(short), "short" },
            { typeof(ushort), "ushort" },
            { typeof(int), "int" },
            { typeof(uint), "uint" },
            { typeof(long), "long" },
            { typeof(ulong), "ulong" },
            { typeof(nint), "nint" },
            { typeof(nuint), "nuint" },
            { typeof(float), "float" },
            { typeof(double), "double" },
            { typeof(decimal), "decimal" },
            { typeof(bool), "bool" },
            { typeof(char), "char" },
            { typeof(string), "string" },
            { typeof(object), "object" },
        };
    }

    public static Type? FindTypeInLoadedAssemblies(string assemblyName, string typeFullName)
    {
        return AppDomain.CurrentDomain.GetAssemblies()
            .FirstOrDefault(a => a.GetName().Name == assemblyName)?
            .GetType(typeFullName);
    }

    public static string ConstructTypeName(Type type)
    {
        if (!IsEditorHintCached)
        {
            return type.Name;
        }

        if (type is { IsArray: false, IsGenericType: false })
        {
            return GetSimpleTypeName(type);
        }

        var typeNameBuilder = new StringBuilder();
        AppendType(typeNameBuilder, type);
        return typeNameBuilder.ToString();

        static void AppendType(StringBuilder sb, Type type)
        {
            if (type.IsArray)
            {
                AppendArray(sb, type);
            }
            else if (type.IsGenericType)
            {
                AppendGeneric(sb, type);
            }
            else
            {
                sb.Append(GetSimpleTypeName(type));
            }
        }

        static void AppendArray(StringBuilder sb, Type type)
        {
            // Append inner most non-array element.
            var elementType = type.GetElementType()!;
            while (elementType.IsArray)
            {
                elementType = elementType.GetElementType()!;
            }

            AppendType(sb, elementType);
            // Append brackets.
            AppendArrayBrackets(sb, type);

            static void AppendArrayBrackets(StringBuilder sb, Type? type)
            {
                while (type != null && type.IsArray)
                {
                    int rank = type.GetArrayRank();
                    sb.Append('[');
                    sb.Append(',', rank - 1);
                    sb.Append(']');
                    type = type.GetElementType();
                }
            }
        }

        static void AppendGeneric(StringBuilder sb, Type type)
        {
            var genericArgs = type.GenericTypeArguments;
            var genericDefinition = type.GetGenericTypeDefinition();

            // Nullable<T>
            if (genericDefinition == typeof(Nullable<>))
            {
                AppendType(sb, genericArgs[0]);
                sb.Append('?');
                return;
            }

            // ValueTuple
            Debug.Assert(_tupleTypeSet != null);
            if (_tupleTypeSet.Contains(genericDefinition))
            {
                sb.Append('(');
                while (true)
                {
                    // We assume that ValueTuple has 1~8 elements.
                    // And the 8th element (TRest) is always another ValueTuple.

                    // This is a hard coded tuple element length check.
                    if (genericArgs.Length != 8)
                    {
                        AppendParamTypes(sb, genericArgs);
                        break;
                    }
                    else
                    {
                        AppendParamTypes(sb, genericArgs.AsSpan(0, 7));
                        sb.Append(", ");

                        // TRest should be a ValueTuple!
                        var nextTuple = genericArgs[7];

                        genericArgs = nextTuple.GenericTypeArguments;
                    }
                }
                sb.Append(')');
                return;
            }

            // Normal generic
            var typeName = type.Name.AsSpan();
            sb.Append(typeName[..typeName.LastIndexOf('`')]);
            sb.Append('<');
            AppendParamTypes(sb, genericArgs);
            sb.Append('>');

            static void AppendParamTypes(StringBuilder sb, ReadOnlySpan<Type> genericArgs)
            {
                int n = genericArgs.Length - 1;
                for (int i = 0; i < n; i += 1)
                {
                    AppendType(sb, genericArgs[i]);
                    sb.Append(", ");
                }

                AppendType(sb, genericArgs[n]);
            }
        }

        static string GetSimpleTypeName(Type type)
        {
            Debug.Assert(_builtinTypeNameDictionary != null);
            return _builtinTypeNameDictionary.TryGetValue(type, out string? name) ? name : type.Name;
        }
    }
}
