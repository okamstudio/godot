using System;

namespace Godot
{
    /// <summary>
    /// Allows the annotated class to execute in the editor.
    /// </summary>
    /// <example>
    /// <code>
    /// [Tool]
    /// public partial class MySprite : Sprite2D
    /// {
    ///     public override void _Process(double delta)
    ///     {
    ///         Rotation += Mathf.Pi * (float)delta;
    ///     }
    /// }
    /// </code>
    /// </example>
    [AttributeUsage(AttributeTargets.Class)]
    public sealed class ToolAttribute : Attribute { }
}
