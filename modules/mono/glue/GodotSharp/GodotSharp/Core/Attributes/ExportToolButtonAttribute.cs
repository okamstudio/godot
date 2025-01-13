using System;

#nullable enable

namespace Godot
{
    /// <summary>
    /// Exports the annotated <see cref="Callable"/> as a clickable button.
    /// </summary>
    [AttributeUsage(AttributeTargets.Field | AttributeTargets.Property)]
    public sealed class ExportToolButtonAttribute : Attribute
    {
        /// <summary>
        /// The label of the button.
        /// </summary>
        public string Text { get; }

        /// <summary>
        /// If defined, used to fetch an icon for the button via <see cref="Control.GetThemeIcon"/>,
        /// from the <code>EditorIcons</code> theme type.
        /// </summary>
        public string? Icon { get; init; }

        /// <summary>
        /// Exports the annotated <see cref="Callable"/> as a clickable button.
        /// </summary>
        /// <param name="text">The label of the button.</param>
        public ExportToolButtonAttribute(string text)
        {
            Text = text;
        }
    }
}
