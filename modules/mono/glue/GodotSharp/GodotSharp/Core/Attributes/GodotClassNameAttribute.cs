using System;

namespace Godot
{
    /// <summary>
    /// Attribute that specifies the engine class name when it's not the same
    /// as the generated C# class name. This allows introspection code to find
    /// the name associated with the class. If the attribute is not present,
    /// the C# class name can be used instead.
    /// </summary>
    [AttributeUsage(AttributeTargets.Class)]
    public class GodotClassNameAttribute : Attribute
    {
        /// <summary>
        /// Original engine class name.
        /// </summary>
        public string Name { get; }

        /// <summary>
        /// Specify the name that represents the original engine class.
        /// </summary>
        /// <param name="name">Name of the original engine class.</param>
        public GodotClassNameAttribute(string name)
        {
            Name = name;
        }
    }
}
