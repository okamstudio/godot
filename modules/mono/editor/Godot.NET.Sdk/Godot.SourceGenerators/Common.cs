using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp.Syntax;

namespace Godot.SourceGenerators
{
    public static partial class Common
    {
        private static readonly string _helpLinkFormat = $"{VersionDocsUrl}/tutorials/scripting/c_sharp/diagnostics/{{0}}.html";

        internal static readonly DiagnosticDescriptor ClassPartialModifierRule =
            new DiagnosticDescriptor(id: "GD0001",
                title: $"Missing partial modifier on declaration of type that derives from '{GodotClasses.GodotObject}'",
                messageFormat: $"Missing partial modifier on declaration of type '{{0}}' that derives from '{GodotClasses.GodotObject}'",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                $"Classes that derive from '{GodotClasses.GodotObject}' must be declared with the partial modifier.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0001"));

        internal static readonly DiagnosticDescriptor OuterClassPartialModifierRule =
            new DiagnosticDescriptor(id: "GD0002",
                title: $"Missing partial modifier on declaration of type which contains nested classes that derive from '{GodotClasses.GodotObject}'",
                messageFormat: $"Missing partial modifier on declaration of type '{{0}}' which contains nested classes that derive from '{GodotClasses.GodotObject}'",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                $"Classes that derive from '{GodotClasses.GodotObject}' and their containing types must be declared with the partial modifier.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0002"));

        public static readonly DiagnosticDescriptor MultipleClassesInGodotScriptRule =
            new DiagnosticDescriptor(id: "GD0003",
                title: "Found multiple classes with the same name in the same script file",
                messageFormat: "Found multiple classes with the name '{0}' in the same script file",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "Found multiple classes with the same name in the same script file. A script file must only contain one class with a name that matches the file name.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0003"));

        public static readonly DiagnosticDescriptor ExportedMemberIsStaticRule =
            new DiagnosticDescriptor(id: "GD0101",
                title: "The exported member is static",
                messageFormat: "The exported member '{0}' is static",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "The exported member is static. Only instance fields and properties can be exported. Remove the 'static' modifier, or the '[Export]' attribute.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0101"));

        public static readonly DiagnosticDescriptor ExportedMemberTypeIsNotSupportedRule =
            new DiagnosticDescriptor(id: "GD0102",
                title: "The type of the exported member is not supported",
                messageFormat: "The type of the exported member '{0}' is not supported",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "The type of the exported member is not supported. Use a supported type, or remove the '[Export]' attribute.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0102"));

        public static readonly DiagnosticDescriptor ExportedMemberIsReadOnlyRule =
            new DiagnosticDescriptor(id: "GD0103",
                title: "The exported member is read-only",
                messageFormat: "The exported member '{0}' is read-only",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "The exported member is read-only. Exported member must be writable.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0103"));

        public static readonly DiagnosticDescriptor ExportedPropertyIsWriteOnlyRule =
            new DiagnosticDescriptor(id: "GD0104",
                title: "The exported property is write-only",
                messageFormat: "The exported property '{0}' is write-only",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "The exported property is write-only. Exported properties must be readable.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0104"));

        public static readonly DiagnosticDescriptor ExportedMemberIsIndexerRule =
            new DiagnosticDescriptor(id: "GD0105",
                title: "The exported property is an indexer",
                messageFormat: "The exported property '{0}' is an indexer",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "The exported property is an indexer. Remove the '[Export]' attribute.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0105"));

        public static readonly DiagnosticDescriptor ExportedMemberIsExplicitInterfaceImplementationRule =
            new DiagnosticDescriptor(id: "GD0106",
                title: "The exported property is an explicit interface implementation",
                messageFormat: "The exported property '{0}' is an explicit interface implementation",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "The exported property is an explicit interface implementation. Remove the '[Export]' attribute.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0106"));

        public static readonly DiagnosticDescriptor OnlyNodesShouldExportNodesRule =
            new DiagnosticDescriptor(id: "GD0107",
                title: "Types not derived from Node should not export Node members",
                messageFormat: "Types not derived from Node should not export Node members",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "Types not derived from Node should not export Node members. Node export is only supported in Node-derived classes.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0107"));

        public static readonly DiagnosticDescriptor OnlyToolClassesShouldUseExportToolButtonRule =
            new DiagnosticDescriptor(id: "GD0108",
                title: "The exported tool button is not in a tool class",
                messageFormat: "The exported tool button '{0}' is not in a tool class",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "The exported tool button is not in a tool class. Annotate the class with the '[Tool]' attribute, or remove the '[ExportToolButton]' attribute.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0108"));

        public static readonly DiagnosticDescriptor ExportToolButtonShouldNotBeUsedWithExportRule =
            new DiagnosticDescriptor(id: "GD0109",
                title: "The '[ExportToolButton]' attribute cannot be used with another '[Export]' attribute",
                messageFormat: "The '[ExportToolButton]' attribute cannot be used with another '[Export]' attribute on '{0}'",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "The '[ExportToolButton]' attribute cannot be used with the '[Export]' attribute. Remove one of the attributes.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0109"));

        public static readonly DiagnosticDescriptor ExportToolButtonIsNotCallableRule =
            new DiagnosticDescriptor(id: "GD0110",
                title: "The exported tool button is not a Callable",
                messageFormat: "The exported tool button '{0}' is not a Callable",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "The exported tool button is not a Callable. The '[ExportToolButton]' attribute is only supported on members of type Callable.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0110"));

        public static readonly DiagnosticDescriptor SignalDelegateMissingSuffixRule =
            new DiagnosticDescriptor(id: "GD0201",
                title: "The name of the delegate must end with 'EventHandler'",
                messageFormat: "The name of the delegate '{0}' must end with 'EventHandler'",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "The name of the delegate must end with 'EventHandler'. Rename the delegate accordingly, or remove the '[Signal]' attribute.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0201"));

        public static readonly DiagnosticDescriptor SignalParameterTypeNotSupportedRule =
            new DiagnosticDescriptor(id: "GD0202",
                title: "The parameter of the delegate signature of the signal is not supported",
                messageFormat: "The parameter of the delegate signature of the signal '{0}' is not supported",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "The parameter of the delegate signature of the signal is not supported. Use supported types only, or remove the '[Signal]' attribute.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0202"));

        public static readonly DiagnosticDescriptor SignalDelegateSignatureMustReturnVoidRule =
            new DiagnosticDescriptor(id: "GD0203",
                title: "The delegate signature of the signal must return void",
                messageFormat: "The delegate signature of the signal '{0}' must return void",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "The delegate signature of the signal must return void. Return void, or remove the '[Signal]' attribute.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0203"));

        public static readonly DiagnosticDescriptor GenericTypeArgumentMustBeVariantRule =
            new DiagnosticDescriptor(id: "GD0301",
                title: "The generic type argument must be a Variant compatible type",
                messageFormat: "The generic type argument '{0}' must be a Variant compatible type",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "The generic type argument must be a Variant compatible type. Use a Variant compatible type as the generic type argument.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0301"));

        public static readonly DiagnosticDescriptor GenericTypeParameterMustBeVariantAnnotatedRule =
            new DiagnosticDescriptor(id: "GD0302",
                title: "The generic type parameter must be annotated with the '[MustBeVariant]' attribute",
                messageFormat: "The generic type parameter '{0}' must be annotated with the '[MustBeVariant]' attribute",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "The generic type parameter must be annotated with the '[MustBeVariant]' attribute. Add the '[MustBeVariant]' attribute to the generic type parameter.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0302"));

        public static readonly DiagnosticDescriptor TypeArgumentParentSymbolUnhandledRule =
            new DiagnosticDescriptor(id: "GD0303",
                title: "The parent symbol of a type argument that must be Variant compatible was not handled",
                messageFormat: "The parent symbol '{0}' of a type argument that must be Variant compatible was not handled",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "The parent symbol of a type argument that must be Variant compatible was not handled. This is an issue in the engine, and should be reported.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0303"));

        public static readonly DiagnosticDescriptor GlobalClassMustDeriveFromGodotObjectRule =
            new DiagnosticDescriptor(id: "GD0401",
                title: $"The class must derive from {GodotClasses.GodotObject} or a derived class",
                messageFormat: $"The class '{{0}}' must derive from {GodotClasses.GodotObject} or a derived class",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                $"The class must derive from {GodotClasses.GodotObject} or a derived class. Change the base type, or remove the '[GlobalClass]' attribute.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0401"));

        public static readonly DiagnosticDescriptor GlobalClassMustNotBeGenericRule =
            new DiagnosticDescriptor(id: "GD0402",
                title: "The class must not be generic",
                messageFormat: "The class '{0}' must not be generic",
                category: "Usage",
                DiagnosticSeverity.Error,
                isEnabledByDefault: true,
                "The class must not be generic. Make the class non-generic, or remove the '[GlobalClass]' attribute.",
                helpLinkUri: string.Format(_helpLinkFormat, "GD0402"));
    }
}
