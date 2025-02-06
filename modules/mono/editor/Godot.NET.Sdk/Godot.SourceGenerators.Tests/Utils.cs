using System.Collections.Generic;

namespace Godot.SourceGenerators.Tests;

public static class Utils
{
    public static Dictionary<string, string> DisabledGenerators(params string[] generators)
    {
        return new Dictionary<string, string>()
        {
            { "build_property.GodotDisabledSourceGenerators", string.Join(";", generators) }
        };
    }
}
