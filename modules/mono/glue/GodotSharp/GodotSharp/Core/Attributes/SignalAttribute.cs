using System;

namespace Godot
{
    /// <summary>
    /// Declares a <see langword="delegate"/> as a signal. This allows any
    /// connected <see cref="Callable"/> (and, by extension, their respective
    /// objects) to listen and react to events, without directly referencing
    /// one another.
    /// </summary>
    /// <example>
    /// <code>
    /// public partial class MyNode2D : Node2D
    /// {
    ///     private int _health = 10;
    ///
    ///     [Signal]
    ///     public delegate void HealthDepletedEventHandler();
    ///
    ///     [Signal]
    ///     public delegate void HealthChangedEventHandler(int oldValue, int newValue);
    /// }
    /// </code>
    /// </example>
    [AttributeUsage(AttributeTargets.Delegate)]
    public sealed class SignalAttribute : Attribute { }
}
