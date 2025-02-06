partial class ClassAllDoc
{
#pragma warning disable CS0109 // Disable warning about redundant 'new' keyword
#if TOOLS
    [global::System.ComponentModel.EditorBrowsable(global::System.ComponentModel.EditorBrowsableState.Never)]
    internal new static global::Godot.Collections.Dictionary GetGodotClassDocs()
    {
        var docs = new global::Godot.Collections.Dictionary();
        docs.Add("name","\"ClassAllDoc.cs\"");
        docs.Add("description","class description\ntest");

        var propertyDocs = new global::Godot.Collections.Array();
        propertyDocs.Add(new global::Godot.Collections.Dictionary { { "name", PropertyName.PropertyDocTest}, { "description", "property description\ntest" } });
        propertyDocs.Add(new global::Godot.Collections.Dictionary { { "name", PropertyName._fieldDocTest}, { "description", "field description\ntest" } });
        docs.Add("properties", propertyDocs);

        var signalDocs  = new global::Godot.Collections.Array();
        signalDocs.Add(new global::Godot.Collections.Dictionary { { "name", SignalName.SignalDocTest}, { "description", "signal description\ntest" } });
        docs.Add("signals", signalDocs);

        return docs;
    }

#endif // TOOLS
#pragma warning restore CS0109
}
