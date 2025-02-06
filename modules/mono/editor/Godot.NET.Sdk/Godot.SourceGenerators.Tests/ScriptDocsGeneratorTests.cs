using Xunit;

namespace Godot.SourceGenerators.Tests;

public class ScriptDocsGeneratorTests
{
    [Fact]
    public async void Docs()
    {
        await CSharpSourceGeneratorVerifier<ScriptDocsGenerator>.Verify(
            "ClassDoc.cs",
            "ClassDoc_ScriptDocs.generated.cs"
        );
    }

    [Fact]
    public async void AllDocs()
    {
        await CSharpSourceGeneratorVerifier<ScriptDocsGenerator>.Verify(
            "ClassAllDoc.cs",
            "ClassAllDoc_ScriptDocs.generated.cs"
        );
    }
}
