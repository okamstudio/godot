using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using System.Linq;
using System.Text;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;

namespace Godot.SourceGenerators
{
    internal static class ExtensionMethods
    {
        public static bool TryGetGlobalAnalyzerProperty(
            this GeneratorExecutionContext context, string property, out string? value
        ) => context.AnalyzerConfigOptions.GlobalOptions
            .TryGetValue("build_property." + property, out value);

        public static bool AreGodotSourceGeneratorsDisabled(this GeneratorExecutionContext context)
            => context.TryGetGlobalAnalyzerProperty("GodotSourceGenerators", out string? toggle) &&
               toggle != null &&
               toggle.Equals("disabled", StringComparison.OrdinalIgnoreCase);

        public static bool IsGodotToolsProject(this GeneratorExecutionContext context)
            => context.TryGetGlobalAnalyzerProperty("IsGodotToolsProject", out string? toggle) &&
               toggle != null &&
               toggle.Equals("true", StringComparison.OrdinalIgnoreCase);

        public static bool IsGodotSourceGeneratorDisabled(this GeneratorExecutionContext context, string generatorName) =>
            AreGodotSourceGeneratorsDisabled(context) ||
            (context.TryGetGlobalAnalyzerProperty("GodotDisabledSourceGenerators", out string? disabledGenerators) &&
            disabledGenerators != null &&
            disabledGenerators.Split(';').Contains(generatorName));

        public static bool InheritsFrom(this ITypeSymbol? symbol, string assemblyName, string typeFullName)
        {
            while (symbol != null)
            {
                if (symbol.ContainingAssembly?.Name == assemblyName &&
                    symbol.FullQualifiedNameOmitGlobal() == typeFullName)
                {
                    return true;
                }

                symbol = symbol.BaseType;
            }

            return false;
        }

        public static INamedTypeSymbol? GetGodotScriptNativeClass(this INamedTypeSymbol classTypeSymbol)
        {
            var symbol = classTypeSymbol;

            while (symbol != null)
            {
                if (symbol.ContainingAssembly?.Name == "GodotSharp")
                    return symbol;

                symbol = symbol.BaseType;
            }

            return null;
        }

        public static string? GetGodotScriptNativeClassName(this INamedTypeSymbol classTypeSymbol)
        {
            var nativeType = classTypeSymbol.GetGodotScriptNativeClass();

            if (nativeType == null)
                return null;

            var godotClassNameAttr = nativeType.GetAttributes()
                .FirstOrDefault(a => a.AttributeClass?.IsGodotClassNameAttribute() ?? false);

            string? godotClassName = null;

            if (godotClassNameAttr is { ConstructorArguments: { Length: > 0 } })
                godotClassName = godotClassNameAttr.ConstructorArguments[0].Value?.ToString();

            return godotClassName ?? nativeType.Name;
        }

        private static bool TryGetGodotScriptClass(
            this ClassDeclarationSyntax cds, Compilation compilation,
            out INamedTypeSymbol? symbol
        )
        {
            var sm = compilation.GetSemanticModel(cds.SyntaxTree);

            var classTypeSymbol = sm.GetDeclaredSymbol(cds);

            if (classTypeSymbol?.BaseType == null
                || !classTypeSymbol.BaseType.InheritsFrom("GodotSharp", GodotClasses.GodotObject))
            {
                symbol = null;
                return false;
            }

            symbol = classTypeSymbol;
            return true;
        }

        public static IEnumerable<(ClassDeclarationSyntax cds, INamedTypeSymbol symbol)> SelectGodotScriptClasses(
            this IEnumerable<ClassDeclarationSyntax> source,
            Compilation compilation
        )
        {
            foreach (var cds in source)
            {
                if (cds.TryGetGodotScriptClass(compilation, out var symbol))
                    yield return (cds, symbol!);
            }
        }

        public static bool IsNested(this TypeDeclarationSyntax cds)
            => cds.Parent is TypeDeclarationSyntax;

        public static bool IsPartial(this TypeDeclarationSyntax cds)
            => cds.Modifiers.Any(SyntaxKind.PartialKeyword);

        public static bool AreAllOuterTypesPartial(
            this TypeDeclarationSyntax cds,
            out TypeDeclarationSyntax? typeMissingPartial
        )
        {
            SyntaxNode? outerSyntaxNode = cds.Parent;

            while (outerSyntaxNode is TypeDeclarationSyntax outerTypeDeclSyntax)
            {
                if (!outerTypeDeclSyntax.IsPartial())
                {
                    typeMissingPartial = outerTypeDeclSyntax;
                    return false;
                }

                outerSyntaxNode = outerSyntaxNode.Parent;
            }

            typeMissingPartial = null;
            return true;
        }

        public static string GetDeclarationKeyword(this INamedTypeSymbol namedTypeSymbol)
        {
            string? keyword = namedTypeSymbol.DeclaringSyntaxReferences
                .OfType<TypeDeclarationSyntax>().FirstOrDefault()?
                .Keyword.Text;

            return keyword ?? namedTypeSymbol.TypeKind switch
            {
                TypeKind.Interface => "interface",
                TypeKind.Struct => "struct",
                _ => "class"
            };
        }

        public static string GetAccessibilityKeyword(this INamedTypeSymbol namedTypeSymbol)
        {
            if (namedTypeSymbol.DeclaredAccessibility == Accessibility.NotApplicable)
            {
                // Accessibility not specified. Get the default accessibility.
                return namedTypeSymbol.ContainingSymbol switch
                {
                    null or INamespaceSymbol => "internal",
                    ITypeSymbol { TypeKind: TypeKind.Class or TypeKind.Struct } => "private",
                    ITypeSymbol { TypeKind: TypeKind.Interface } => "public",
                    _ => "",
                };
            }

            return namedTypeSymbol.DeclaredAccessibility switch
            {
                Accessibility.Private => "private",
                Accessibility.Protected => "protected",
                Accessibility.Internal => "internal",
                Accessibility.ProtectedAndInternal => "private",
                Accessibility.ProtectedOrInternal => "private",
                Accessibility.Public => "public",
                _ => "",
            };
        }

        public static string NameWithTypeParameters(this INamedTypeSymbol symbol)
        {
            return symbol.IsGenericType ?
                string.Concat(symbol.Name, "<", string.Join(", ", symbol.TypeParameters), ">") :
                symbol.Name;
        }

        private static SymbolDisplayFormat FullyQualifiedFormatOmitGlobal { get; } =
            SymbolDisplayFormat.FullyQualifiedFormat
                .WithGlobalNamespaceStyle(SymbolDisplayGlobalNamespaceStyle.Omitted);

        private static SymbolDisplayFormat FullyQualifiedFormatIncludeGlobal { get; } =
            SymbolDisplayFormat.FullyQualifiedFormat
                .WithGlobalNamespaceStyle(SymbolDisplayGlobalNamespaceStyle.Included);

        public static string FullQualifiedNameOmitGlobal(this ITypeSymbol symbol)
            => symbol.ToDisplayString(NullableFlowState.NotNull, FullyQualifiedFormatOmitGlobal);

        public static string FullQualifiedNameOmitGlobal(this INamespaceSymbol namespaceSymbol)
            => namespaceSymbol.ToDisplayString(FullyQualifiedFormatOmitGlobal);

        public static string FullQualifiedNameIncludeGlobal(this ITypeSymbol symbol)
            => symbol.ToDisplayString(NullableFlowState.NotNull, FullyQualifiedFormatIncludeGlobal);

        public static string FullQualifiedNameIncludeGlobal(this INamespaceSymbol namespaceSymbol)
            => namespaceSymbol.ToDisplayString(FullyQualifiedFormatIncludeGlobal);

        public static string FullQualifiedSyntax(this SyntaxNode node, SemanticModel sm)
        {
            StringBuilder sb = new();
            FullQualifiedSyntax(node, sm, sb, true);
            return sb.ToString();
        }

        private static void FullQualifiedSyntax(SyntaxNode node, SemanticModel sm, StringBuilder sb, bool isFirstNode)
        {
            if (node is NameSyntax ns && isFirstNode)
            {
                SymbolInfo nameInfo = sm.GetSymbolInfo(ns);
                sb.Append(nameInfo.Symbol?.ToDisplayString(FullyQualifiedFormatIncludeGlobal) ?? ns.ToString());
                return;
            }

            bool innerIsFirstNode = true;
            foreach (var child in node.ChildNodesAndTokens())
            {
                if (child.HasLeadingTrivia)
                {
                    sb.Append(child.GetLeadingTrivia());
                }

                if (child.IsNode)
                {
                    var childNode = child.AsNode()!;

                    if (node is InterpolationSyntax && childNode is ExpressionSyntax)
                    {
                        ParenEnclosedFullQualifiedSyntax(childNode, sm, sb, isFirstNode: innerIsFirstNode);
                    }
                    else
                    {
                        FullQualifiedSyntax(childNode, sm, sb, isFirstNode: innerIsFirstNode);
                    }

                    innerIsFirstNode = false;
                }
                else
                {
                    sb.Append(child);
                }

                if (child.HasTrailingTrivia)
                {
                    sb.Append(child.GetTrailingTrivia());
                }
            }

            static void ParenEnclosedFullQualifiedSyntax(SyntaxNode node, SemanticModel sm, StringBuilder sb, bool isFirstNode)
            {
                sb.Append(SyntaxFactory.Token(SyntaxKind.OpenParenToken));
                FullQualifiedSyntax(node, sm, sb, isFirstNode);
                sb.Append(SyntaxFactory.Token(SyntaxKind.CloseParenToken));
            }
        }

        public static string SanitizeQualifiedNameForUniqueHint(this string qualifiedName)
            => qualifiedName
                // AddSource() doesn't support angle brackets
                .Replace("<", "(Of ")
                .Replace(">", ")");

        public static bool IsGodotExportAttribute(this INamedTypeSymbol symbol)
            => symbol.FullQualifiedNameOmitGlobal() == GodotClasses.ExportAttr;

        public static bool IsGodotSignalAttribute(this INamedTypeSymbol symbol)
            => symbol.FullQualifiedNameOmitGlobal() == GodotClasses.SignalAttr;

        public static bool IsGodotMustBeVariantAttribute(this INamedTypeSymbol symbol)
            => symbol.FullQualifiedNameOmitGlobal() == GodotClasses.MustBeVariantAttr;

        public static bool IsGodotClassNameAttribute(this INamedTypeSymbol symbol)
            => symbol.FullQualifiedNameOmitGlobal() == GodotClasses.GodotClassNameAttr;

        public static bool IsGodotGlobalClassAttribute(this INamedTypeSymbol symbol)
            => symbol.FullQualifiedNameOmitGlobal() == GodotClasses.GlobalClassAttr;

        public static bool IsGodotExportToolButtonAttribute(this INamedTypeSymbol symbol)
            => symbol.FullQualifiedNameOmitGlobal() == GodotClasses.ExportToolButtonAttr;

        public static bool IsGodotToolAttribute(this INamedTypeSymbol symbol)
            => symbol.FullQualifiedNameOmitGlobal() == GodotClasses.ToolAttr;

        public static bool IsSystemFlagsAttribute(this INamedTypeSymbol symbol)
            => symbol.FullQualifiedNameOmitGlobal() == GodotClasses.SystemFlagsAttr;

        public static GodotMethodData? HasGodotCompatibleSignature(
            this IMethodSymbol method,
            MarshalUtils.TypeCache typeCache
        )
        {
            if (method.IsGenericMethod)
                return null;

            var retSymbol = method.ReturnType;
            var retType = method.ReturnsVoid ?
                null :
                MarshalUtils.ConvertManagedTypeToMarshalType(method.ReturnType, typeCache);

            if (retType == null && !method.ReturnsVoid)
                return null;

            var parameters = method.Parameters;

            var paramTypes = parameters
                // Currently we don't support `ref`, `out`, `in`, `ref readonly` parameters (and we never may)
                .Where(p => p.RefKind == RefKind.None)
                // Attempt to determine the variant type
                .Select(p => MarshalUtils.ConvertManagedTypeToMarshalType(p.Type, typeCache))
                // Discard parameter types that couldn't be determined (null entries)
                .Where(t => t != null).Cast<MarshalType>().ToImmutableArray();

            // If any parameter type was incompatible, it was discarded so the length won't match
            if (parameters.Length > paramTypes.Length)
                return null; // Ignore incompatible method

            return new GodotMethodData(method, paramTypes,
                parameters.Select(p => p.Type).ToImmutableArray(),
                retType != null ? (retType.Value, retSymbol) : null);
        }

        public static IEnumerable<GodotMethodData> WhereHasGodotCompatibleSignature(
            this IEnumerable<IMethodSymbol> methods,
            MarshalUtils.TypeCache typeCache
        )
        {
            foreach (var method in methods)
            {
                var methodData = HasGodotCompatibleSignature(method, typeCache);

                if (methodData != null)
                    yield return methodData.Value;
            }
        }

        public static IEnumerable<GodotPropertyData> WhereIsGodotCompatibleType(
            this IEnumerable<IPropertySymbol> properties,
            MarshalUtils.TypeCache typeCache
        )
        {
            foreach (var property in properties)
            {
                var marshalType = MarshalUtils.ConvertManagedTypeToMarshalType(property.Type, typeCache);

                if (marshalType == null)
                    continue;

                yield return new GodotPropertyData(property, marshalType.Value);
            }
        }

        public static IEnumerable<GodotFieldData> WhereIsGodotCompatibleType(
            this IEnumerable<IFieldSymbol> fields,
            MarshalUtils.TypeCache typeCache
        )
        {
            foreach (var field in fields)
            {
                // TODO: We should still restore read-only fields after reloading assembly. Two possible ways: reflection or turn RestoreGodotObjectData into a constructor overload.
                var marshalType = MarshalUtils.ConvertManagedTypeToMarshalType(field.Type, typeCache);

                if (marshalType == null)
                    continue;

                yield return new GodotFieldData(field, marshalType.Value);
            }
        }

        public static Location? FirstLocationWithSourceTreeOrDefault(this IEnumerable<Location> locations)
        {
            return locations.FirstOrDefault(location => location.SourceTree != null) ?? locations.FirstOrDefault();
        }

        public static string Path(this Location location)
            => location.SourceTree?.GetLineSpan(location.SourceSpan).Path
               ?? location.GetLineSpan().Path;

        public static int StartLine(this Location location)
            => location.SourceTree?.GetLineSpan(location.SourceSpan).StartLinePosition.Line
               ?? location.GetLineSpan().StartLinePosition.Line;
    }
}
