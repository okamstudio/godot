using System;
using Xunit;

namespace Godot.SourceGenerators.Tests;

public class ScriptSignalsGeneratorTests
{
    [Fact]
    public async void DisableGenerator()
    {
        var verifier = CSharpSourceGeneratorVerifier<ScriptSignalsGenerator>.MakeVerifier(
            new string[] { "SimpleEventSignals.cs" },
            Array.Empty<string>()
        );
        verifier.TestState.AddGlobalConfig(Utils.DisabledGenerators("ScriptSignals"));
        await verifier.RunAsync();
    }

    [Fact]
    public async void EventSignals()
    {
        await CSharpSourceGeneratorVerifier<ScriptSignalsGenerator>.Verify(
            "EventSignals.cs",
            "EventSignals_ScriptSignals.generated.cs"
        );
    }
}
