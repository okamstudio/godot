using System.Text;
using System.Linq;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.Text;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;

namespace Godot.SourceGenerators.Internal;

[Generator]
public class UnmanagedCallbacksGenerator : ISourceGenerator
{
    public void Initialize(GeneratorInitializationContext context)
    {
        context.RegisterForPostInitialization(ctx => { GenerateAttribute(ctx); });
    }

    public void Execute(GeneratorExecutionContext context)
    {
        INamedTypeSymbol[] unmanagedCallbacksClasses = context
            .Compilation.SyntaxTrees
            .SelectMany(tree =>
                tree.GetRoot().DescendantNodes()
                    .OfType<ClassDeclarationSyntax>()
                    .SelectUnmanagedCallbacksClasses(context.Compilation)
                    // Report and skip non-partial classes
                    .Where(x =>
                    {
                        if (x.cds.IsPartial())
                        {
                            if (x.cds.IsNested() && !x.cds.AreAllOuterTypesPartial(out var typeMissingPartial))
                            {
                                Common.ReportNonPartialUnmanagedCallbacksOuterClass(context, typeMissingPartial!);
                                return false;
                            }

                            return true;
                        }

                        Common.ReportNonPartialUnmanagedCallbacksClass(context, x.cds, x.symbol);
                        return false;
                    })
                    .Select(x => x.symbol)
            )
            .Distinct<INamedTypeSymbol>(SymbolEqualityComparer.Default)
            .ToArray();

        foreach (var symbol in unmanagedCallbacksClasses)
        {
            var attr = symbol.GetGenerateUnmanagedCallbacksAttribute();
            if (attr == null || attr.ConstructorArguments.Length != 1)
            {
                // TODO: Report error or throw exception, this is an invalid case and should never be reached
                System.Diagnostics.Debug.Fail("FAILED!");
                continue;
            }

            var funcStructType = (INamedTypeSymbol?)attr.ConstructorArguments[0].Value;
            if (funcStructType == null)
            {
                // TODO: Report error or throw exception, this is an invalid case and should never be reached
                System.Diagnostics.Debug.Fail("FAILED!");
                continue;
            }

            var data = new CallbacksData(symbol, funcStructType);
            GenerateInteropMethodImplementations(context, data);
            GenerateUnmanagedCallbacksStruct(context, data);
        }
    }

    private void GenerateAttribute(GeneratorPostInitializationContext context)
    {
        string source = @"using System;

namespace Godot.SourceGenerators.Internal
{
internal class GenerateUnmanagedCallbacksAttribute : Attribute
{
    public Type FuncStructType { get; }

    public GenerateUnmanagedCallbacksAttribute(Type funcStructType)
    {
        FuncStructType = funcStructType;
    }
}
}";

        context.AddSource("GenerateUnmanagedCallbacksAttribute.generated",
            SourceText.From(source, Encoding.UTF8));
    }

    private void GenerateInteropMethodImplementations(GeneratorExecutionContext context, CallbacksData data)
    {
        var symbol = data.NativeTypeSymbol;

        INamespaceSymbol namespaceSymbol = symbol.ContainingNamespace;
        string classNs = namespaceSymbol != null && !namespaceSymbol.IsGlobalNamespace ?
            namespaceSymbol.FullQualifiedNameOmitGlobal() :
            string.Empty;
        bool hasNamespace = classNs.Length != 0;
        bool isInnerClass = symbol.ContainingType != null;

        var source = new StringBuilder();
        var methodSource = new StringBuilder();
        var methodCallArguments = new StringBuilder();
        var methodSourceAfterCall = new StringBuilder();

        source.Append(
            @"using System;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using Godot.Bridge;
using Godot.NativeInterop;

#pragma warning disable CA1707 // Disable warning: Identifiers should not contain underscores

");

        if (hasNamespace)
        {
            source.Append("namespace ");
            source.Append(classNs);
            source.Append("\n{\n");
        }

        if (isInnerClass)
        {
            var containingType = symbol.ContainingType;
            AppendPartialContainingTypeDeclarations(containingType);

            void AppendPartialContainingTypeDeclarations(INamedTypeSymbol? containingType)
            {
                if (containingType == null)
                    return;

                AppendPartialContainingTypeDeclarations(containingType.ContainingType);

                source.Append("partial ");
                source.Append(containingType.GetDeclarationKeyword());
                source.Append(" ");
                source.Append(containingType.NameWithTypeParameters());
                source.Append("\n{\n");
            }
        }

        source.Append("[System.Runtime.CompilerServices.SkipLocalsInit]\n");
        source.Append($"unsafe partial class {symbol.Name}\n");
        source.Append("{\n");
        source.Append($"    private static {data.FuncStructSymbol.FullQualifiedNameIncludeGlobal()} _unmanagedCallbacks;\n\n");

        foreach (var callback in data.Methods)
        {
            methodSource.Clear();
            methodCallArguments.Clear();
            methodSourceAfterCall.Clear();

            source.Append("    [global::System.Runtime.CompilerServices.MethodImpl(global::System.Runtime.CompilerServices.MethodImplOptions.AggressiveInlining)]\n");
            source.Append($"    {SyntaxFacts.GetText(callback.DeclaredAccessibility)} ");

            if (callback.IsStatic)
                source.Append("static ");

            source.Append("partial ");
            source.Append(callback.ReturnType.FullQualifiedNameIncludeGlobal());
            source.Append(' ');
            source.Append(callback.Name);
            source.Append('(');

            for (int i = 0; i < callback.Parameters.Length; i++)
            {
                var parameter = callback.Parameters[i];

                AppendRefKind(source, parameter.RefKind, parameter.ScopedKind);
                source.Append(' ');
                source.Append(parameter.Type.FullQualifiedNameIncludeGlobal());
                source.Append(' ');
                source.Append(parameter.Name);

                if (parameter.RefKind == RefKind.Out)
                {
                    // Only assign default if the parameter won't be passed by-ref or copied later.
                    if (IsGodotInteropStruct(parameter.Type))
                        methodSource.Append($"        {parameter.Name} = default;\n");
                }

                if (IsByRefParameter(parameter))
                {
                    if (IsGodotInteropStruct(parameter.Type))
                    {
                        methodSource.Append("        ");
                        AppendCustomUnsafeAsPointer(methodSource, parameter, out string varName);
                        methodCallArguments.Append(varName);
                    }
                    else if (parameter.Type.IsValueType)
                    {
                        methodSource.Append("        ");
                        AppendCopyToStackAndGetPointer(methodSource, parameter, out string varName);
                        methodCallArguments.Append($"&{varName}");

                        if (parameter.RefKind is RefKind.Out or RefKind.Ref)
                        {
                            methodSourceAfterCall.Append($"        {parameter.Name} = {varName};\n");
                        }
                    }
                    else
                    {
                        // If it's a by-ref param and we can't get the pointer
                        // just pass it by-ref and let it be pinned.
                        AppendRefKind(methodCallArguments, parameter.RefKind, parameter.ScopedKind)
                            .Append(' ')
                            .Append(parameter.Name);
                    }
                }
                else
                {
                    methodCallArguments.Append(parameter.Name);
                }

                if (i < callback.Parameters.Length - 1)
                {
                    source.Append(", ");
                    methodCallArguments.Append(", ");
                }
            }

            source.Append(")\n");
            source.Append("    {\n");

            source.Append(methodSource);
            source.Append("        ");

            if (!callback.ReturnsVoid)
            {
                if (methodSourceAfterCall.Length != 0)
                    source.Append($"{callback.ReturnType.FullQualifiedNameIncludeGlobal()} ret = ");
                else
                    source.Append("return ");
            }

            source.Append($"_unmanagedCallbacks.{callback.Name}(");
            source.Append(methodCallArguments);
            source.Append(");\n");

            if (methodSourceAfterCall.Length != 0)
            {
                source.Append(methodSourceAfterCall);

                if (!callback.ReturnsVoid)
                    source.Append("        return ret;\n");
            }

            source.Append("    }\n\n");
        }

        source.Append("}\n");

        if (isInnerClass)
        {
            var containingType = symbol.ContainingType;

            while (containingType != null)
            {
                source.Append("}\n"); // outer class

                containingType = containingType.ContainingType;
            }
        }

        if (hasNamespace)
            source.Append("\n}");

        source.Append("\n\n#pragma warning restore CA1707\n");

        context.AddSource($"{data.NativeTypeSymbol.FullQualifiedNameOmitGlobal().SanitizeQualifiedNameForUniqueHint()}.generated",
            SourceText.From(source.ToString(), Encoding.UTF8));
    }

    private void GenerateUnmanagedCallbacksStruct(GeneratorExecutionContext context, CallbacksData data)
    {
        var symbol = data.FuncStructSymbol;

        INamespaceSymbol namespaceSymbol = symbol.ContainingNamespace;
        string classNs = namespaceSymbol != null && !namespaceSymbol.IsGlobalNamespace ?
            namespaceSymbol.FullQualifiedNameOmitGlobal() :
            string.Empty;
        bool hasNamespace = classNs.Length != 0;
        bool isInnerClass = symbol.ContainingType != null;

        var source = new StringBuilder();

        source.Append(
            @"using System.Runtime.InteropServices;
using Godot.NativeInterop;

#pragma warning disable CA1707 // Disable warning: Identifiers should not contain underscores

");
        if (hasNamespace)
        {
            source.Append("namespace ");
            source.Append(classNs);
            source.Append("\n{\n");
        }

        if (isInnerClass)
        {
            var containingType = symbol.ContainingType;
            AppendPartialContainingTypeDeclarations(containingType);

            void AppendPartialContainingTypeDeclarations(INamedTypeSymbol? containingType)
            {
                if (containingType == null)
                    return;

                AppendPartialContainingTypeDeclarations(containingType.ContainingType);

                source.Append("partial ");
                source.Append(containingType.GetDeclarationKeyword());
                source.Append(" ");
                source.Append(containingType.NameWithTypeParameters());
                source.Append("\n{\n");
            }
        }

        source.Append("[StructLayout(LayoutKind.Sequential)]\n");
        source.Append($"unsafe partial struct {symbol.Name}\n{{\n");

        foreach (var callback in data.Methods)
        {
            source.Append("    ");
            source.Append(callback.DeclaredAccessibility == Accessibility.Public ? "public " : "internal ");

            source.Append("delegate* unmanaged<");

            foreach (var parameter in callback.Parameters)
            {
                if (IsByRefParameter(parameter))
                {
                    if (IsGodotInteropStruct(parameter.Type) || parameter.Type.IsValueType)
                    {
                        AppendPointerType(source, parameter.Type);
                    }
                    else
                    {
                        // If it's a by-ref param and we can't get the pointer
                        // just pass it by-ref and let it be pinned.
                        AppendRefKind(source, parameter.RefKind, parameter.ScopedKind)
                            .Append(' ')
                            .Append(parameter.Type.FullQualifiedNameIncludeGlobal());
                    }
                }
                else
                {
                    source.Append(parameter.Type.FullQualifiedNameIncludeGlobal());
                }

                source.Append(", ");
            }

            source.Append(callback.ReturnType.FullQualifiedNameIncludeGlobal());
            source.Append($"> {callback.Name};\n");
        }

        source.Append("}\n");

        if (isInnerClass)
        {
            var containingType = symbol.ContainingType;

            while (containingType != null)
            {
                source.Append("}\n"); // outer class

                containingType = containingType.ContainingType;
            }
        }

        if (hasNamespace)
            source.Append("}\n");

        source.Append("\n#pragma warning restore CA1707\n");

        context.AddSource($"{symbol.FullQualifiedNameOmitGlobal().SanitizeQualifiedNameForUniqueHint()}.generated",
            SourceText.From(source.ToString(), Encoding.UTF8));
    }

    private static bool IsGodotInteropStruct(ITypeSymbol type) =>
        _godotInteropStructs.Contains(type.FullQualifiedNameOmitGlobal());

    private static bool IsByRefParameter(IParameterSymbol parameter) =>
        parameter.RefKind is RefKind.In or RefKind.Out or RefKind.Ref;

    private static StringBuilder AppendRefKind(StringBuilder source, RefKind refKind, ScopedKind scopedKind)
    {
        return (refKind, scopedKind) switch
        {
            (RefKind.Out, _) => source.Append("out"),
            (RefKind.In, ScopedKind.ScopedRef) => source.Append("scoped in"),
            (RefKind.In, _) => source.Append("in"),
            (RefKind.Ref, ScopedKind.ScopedRef) => source.Append("scoped ref"),
            (RefKind.Ref, _) => source.Append("ref"),
            _ => source,
        };
    }

    private static void AppendPointerType(StringBuilder source, ITypeSymbol type)
    {
        source.Append(type.FullQualifiedNameIncludeGlobal());
        source.Append('*');
    }

    private static void AppendCustomUnsafeAsPointer(StringBuilder source, IParameterSymbol parameter,
        out string varName)
    {
        varName = $"{parameter.Name}_ptr";

        AppendPointerType(source, parameter.Type);
        source.Append(' ');
        source.Append(varName);
        source.Append(" = ");

        source.Append('(');
        AppendPointerType(source, parameter.Type);
        source.Append(')');

        if (parameter.RefKind == RefKind.In)
            source.Append("CustomUnsafe.ReadOnlyRefAsPointer(in ");
        else
            source.Append("CustomUnsafe.AsPointer(ref ");

        source.Append(parameter.Name);

        source.Append(");\n");
    }

    private static void AppendCopyToStackAndGetPointer(StringBuilder source, IParameterSymbol parameter,
        out string varName)
    {
        varName = $"{parameter.Name}_copy";

        source.Append(parameter.Type.FullQualifiedNameIncludeGlobal());
        source.Append(' ');
        source.Append(varName);
        if (parameter.RefKind is RefKind.In or RefKind.Ref)
        {
            source.Append(" = ");
            source.Append(parameter.Name);
        }

        source.Append(";\n");
    }

    private static readonly string[] _godotInteropStructs =
    {
        "Godot.NativeInterop.godot_ref",
        "Godot.NativeInterop.godot_variant_call_error",
        "Godot.NativeInterop.godot_variant",
        "Godot.NativeInterop.godot_string",
        "Godot.NativeInterop.godot_string_name",
        "Godot.NativeInterop.godot_node_path",
        "Godot.NativeInterop.godot_signal",
        "Godot.NativeInterop.godot_callable",
        "Godot.NativeInterop.godot_array",
        "Godot.NativeInterop.godot_dictionary",
        "Godot.NativeInterop.godot_packed_byte_array",
        "Godot.NativeInterop.godot_packed_int32_array",
        "Godot.NativeInterop.godot_packed_int64_array",
        "Godot.NativeInterop.godot_packed_float32_array",
        "Godot.NativeInterop.godot_packed_float64_array",
        "Godot.NativeInterop.godot_packed_string_array",
        "Godot.NativeInterop.godot_packed_vector2_array",
        "Godot.NativeInterop.godot_packed_vector3_array",
        "Godot.NativeInterop.godot_packed_vector4_array",
        "Godot.NativeInterop.godot_packed_color_array",
    };
}
