namespace Godot.SourceGenerators.Sample;

/// <summary>
/// class description
/// test
/// </summary>
public partial class ClassDoc : GodotObject
{
    /// <summary>
    /// field description
    /// test
    /// </summary>
    [Export]
    private int _fieldDocTest = 1;

    /// <summary>
    /// property description
    /// test
    /// </summary>
    [Export]
    public int PropertyDocTest { get; set; }

    /// <summary>
    /// signal description
    /// test
    /// </summary>
    /// <param name="num"></param>
    [Signal]
    public delegate void SignalDocTestEventHandler(int num);
}
