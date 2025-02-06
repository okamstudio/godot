using System;
using Xunit;

namespace Godot.SourceGenerators.Tests;

public class ScriptSerializationGeneratorTests
{
    [Fact]
    public async void DisableGenerator()
    {
        var verifier = CSharpSourceGeneratorVerifier<ScriptSerializationGenerator>.MakeVerifier(
            new string[] { "ScriptBoilerplate.cs" },
            Array.Empty<string>()
        );
        verifier.TestState.AddGlobalConfig(Utils.DisabledGenerators("ScriptSerialization"));
        await verifier.RunAsync();
    }

    [Fact]
    public async void ScriptBoilerplate()
    {
        await CSharpSourceGeneratorVerifier<ScriptSerializationGenerator>.VerifyNoCompilerDiagnostics(
            "ScriptBoilerplate.cs",
            "ScriptBoilerplate_ScriptSerialization.generated.cs", "OuterClass.NestedClass_ScriptSerialization.generated.cs"
        );
    }
}
