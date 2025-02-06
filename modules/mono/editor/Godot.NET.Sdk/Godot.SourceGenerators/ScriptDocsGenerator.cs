using System;
using System.Linq;
using System.Text;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp.Syntax;
using Microsoft.CodeAnalysis.Text;

namespace Godot.SourceGenerators
{
    [Generator]
    public class ScriptDocsGenerator : ISourceGenerator
    {
        public void Initialize(GeneratorInitializationContext context)
        {
        }

        public void Execute(GeneratorExecutionContext context)
        {
            if (context.IsGodotSourceGeneratorDisabled("ScriptDocs"))
                return;

            // NOTE: NotNullWhen diagnostics don't work on projects targeting .NET Standard 2.0
            // ReSharper disable once ReplaceWithStringIsNullOrEmpty
            if (!context.TryGetGlobalAnalyzerProperty("GodotProjectDirBase64", out string? godotProjectDir) || godotProjectDir!.Length == 0)
            {
                if (!context.TryGetGlobalAnalyzerProperty("GodotProjectDir", out godotProjectDir) || godotProjectDir!.Length == 0)
                {
                    throw new InvalidOperationException("Property 'GodotProjectDir' is null or empty.");
                }
            }
            else
            {
                // Workaround for https://github.com/dotnet/roslyn/issues/51692
                godotProjectDir = Encoding.UTF8.GetString(Convert.FromBase64String(godotProjectDir));
            }

            INamedTypeSymbol[] godotClasses = context
                .Compilation.SyntaxTrees
                .SelectMany(tree =>
                    tree.GetRoot().DescendantNodes()
                        .OfType<ClassDeclarationSyntax>()
                        .SelectGodotScriptClasses(context.Compilation)
                        // Report and skip non-partial classes
                        .Where(x =>
                        {
                            if (x.cds.IsPartial())
                            {
                                if (x.cds.IsNested() && !x.cds.AreAllOuterTypesPartial(out _))
                                {
                                    return false;
                                }

                                return true;
                            }

                            return false;
                        })
                        .Select(x => x.symbol)
                )
                .Distinct<INamedTypeSymbol>(SymbolEqualityComparer.Default)
                .ToArray();

            if (godotClasses.Length > 0)
            {
                var typeCache = new MarshalUtils.TypeCache(context.Compilation);

                foreach (var godotClass in godotClasses)
                {
                    VisitGodotScriptClass(context, typeCache, godotClass, godotProjectDir);
                }
            }
        }

        private static void VisitGodotScriptClass(
            GeneratorExecutionContext context,
            MarshalUtils.TypeCache typeCache,
            INamedTypeSymbol symbol,
            string godotProjectDir
        )
        {
            string? classDescription = symbol.GetDocumentationSummaryText();

            StringBuilder docPropertyString = new StringBuilder();

            var members = symbol.GetMembers();

            var exportedProperties = members
                .Where(s => s.Kind == SymbolKind.Property)
                .Cast<IPropertySymbol>()
                .Where(s => s.GetAttributes()
                    .Any(a => a.AttributeClass?.IsGodotExportAttribute() ?? false))
                .ToArray();

            var exportedFields = members
                .Where(s => s.Kind == SymbolKind.Field && !s.IsImplicitlyDeclared)
                .Cast<IFieldSymbol>()
                .Where(s => s.GetAttributes()
                    .Any(a => a.AttributeClass?.IsGodotExportAttribute() ?? false))
                .ToArray();

            foreach (var property in exportedProperties)
            {
                GeneratePropertyDoc(docPropertyString, property);
            }

            foreach (var field in exportedFields)
            {
                GeneratePropertyDoc(docPropertyString, field);
            }

            StringBuilder docSignalString = new StringBuilder();

            var signalDelegateSymbols = members
                .Where(s => s.Kind == SymbolKind.NamedType)
                .Cast<INamedTypeSymbol>()
                .Where(namedTypeSymbol => namedTypeSymbol.TypeKind == TypeKind.Delegate)
                .Where(s => s.GetAttributes()
                    .Any(a => a.AttributeClass?.IsGodotSignalAttribute() ?? false))
                .ToArray();

            foreach (var signalDelegateSymbol in signalDelegateSymbols)
            {
                GenerateSignalDoc(docSignalString, signalDelegateSymbol);
            }

            if (string.IsNullOrWhiteSpace(classDescription) && docPropertyString.Length == 0 && docSignalString.Length == 0)
            {
                // Script has no docs.
                return;
            }

            INamespaceSymbol namespaceSymbol = symbol.ContainingNamespace;
            string classNs = namespaceSymbol is { IsGlobalNamespace: false }
                ? namespaceSymbol.FullQualifiedNameOmitGlobal()
                : string.Empty;
            bool hasNamespace = classNs.Length != 0;

            bool isInnerClass = symbol.ContainingType != null;

            string uniqueHint = symbol.FullQualifiedNameOmitGlobal().SanitizeQualifiedNameForUniqueHint()
                                + "_ScriptDocs.generated";

            var source = new StringBuilder();

            if (hasNamespace)
            {
                source.Append("namespace ");
                source.Append(classNs);
                source.Append(" {\n\n");
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

            source.Append("partial class ");
            source.Append(symbol.NameWithTypeParameters());
            source.Append("\n{\n");

            source.Append("#pragma warning disable CS0109 // Disable warning about redundant 'new' keyword\n");
            source.Append("#if TOOLS\n");

            source.Append("    [global::System.ComponentModel.EditorBrowsable(global::System.ComponentModel.EditorBrowsableState.Never)]\n");
            source.Append("    internal new static global::Godot.Collections.Dictionary GetGodotClassDocs()\n    {\n");
            source.Append("        var docs = new global::Godot.Collections.Dictionary();\n");
            source.Append("        docs.Add(\"name\",\"");
            if (symbol.GetAttributes().Any(a => a.AttributeClass?.IsGodotGlobalClassAttribute() ?? false))
            {
                source.Append(symbol.Name);
            }
            else
            {
                source.Append($"\\\"{ScriptPathAttributeGenerator.RelativeToDir(symbol.DeclaringSyntaxReferences.First().SyntaxTree.FilePath, godotProjectDir)}\\\"");
                if (isInnerClass)
                {
                    source.Append($".{symbol.Name}");
                }
            }
            source.Append("\");\n");
            source.Append("        docs.Add(\"description\",\"");
            source.Append(classDescription);
            source.Append("\");\n\n");

            if (docPropertyString.Length > 0)
            {
                source.Append("        var propertyDocs = new global::Godot.Collections.Array();\n");
                source.Append(docPropertyString);
                source.Append("        docs.Add(\"properties\", propertyDocs);\n\n");
            }

            if (docSignalString.Length > 0)
            {
                source.Append("        var signalDocs  = new global::Godot.Collections.Array();\n");
                source.Append(docSignalString);
                source.Append("        docs.Add(\"signals\", signalDocs);\n\n");
            }

            source.Append("        return docs;\n    }\n\n");

            source.Append("#endif // TOOLS\n");

            source.Append("#pragma warning restore CS0109\n");

            source.Append("}\n"); // partial class

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
            {
                source.Append("\n}\n");
            }

            context.AddSource(uniqueHint, SourceText.From(source.ToString(), Encoding.UTF8));
        }

        private static void GeneratePropertyDoc(StringBuilder docPropertyString, ISymbol symbol)
        {
            string? text = symbol.GetDocumentationSummaryText();
            if (!string.IsNullOrWhiteSpace(text))
            {
                docPropertyString.Append("        propertyDocs.Add(new global::Godot.Collections.Dictionary { { \"name\", PropertyName.")
                    .Append(symbol.Name)
                    .Append("}, { \"description\", \"")
                    .Append(text)
                    .Append("\" } });\n");
            }
        }

        private static void GenerateSignalDoc(StringBuilder docSignalString, ISymbol symbol)
        {
            string signalName = symbol.Name;
            string SignalDelegateSuffix = "EventHandler";
            signalName = signalName.Substring(0, signalName.Length - SignalDelegateSuffix.Length);

            string? text = symbol.GetDocumentationSummaryText();
            if (!string.IsNullOrWhiteSpace(text))
            {
                docSignalString.Append("        signalDocs.Add(new global::Godot.Collections.Dictionary { { \"name\", SignalName.")
                    .Append(signalName)
                    .Append("}, { \"description\", \"")
                    .Append(text)
                    .Append("\" } });\n");
            }
        }
    }
}
