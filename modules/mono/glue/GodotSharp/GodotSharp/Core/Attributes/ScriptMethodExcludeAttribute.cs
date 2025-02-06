using System;

namespace Godot
{
    /// <summary>
    /// An attribute that excluding method generation from source generators
    /// </summary>
    [AttributeUsage(AttributeTargets.Method)]
    public sealed class ScriptMethodExcludeAttribute : Attribute { }
}
