using System;
using System.Diagnostics.CodeAnalysis;
using Godot;
using Godot.Collections;
using GodotTools.Internals;
using Path = System.IO.Path;

namespace GodotTools.Build
{
    [Serializable]
    public sealed partial class BuildInfo : RefCounted // TODO Remove RefCounted once we have proper serialization
    {
        public string Solution { get; private set; }
        public string Project { get; private set; }
        public string Configuration { get; private set; }
        public string? RuntimeIdentifier { get; private set; }
        public string? PublishOutputDir { get; private set; }
        public bool Restore { get; private set; }
        public bool Rebuild { get; private set; }
        public bool OnlyClean { get; private set; }

        // TODO Use List once we have proper serialization
        public Godot.Collections.Array CustomProperties { get; private set; } = new();

        // Arguments specified by the user in ProjectSettings. Passed last to the build process to override any previous arguments.
        public Godot.Collections.Array CustomArguments { get; private set; } = new();

        public string LogsDirPath => GodotSharpDirs.LogsDirPathFor(Solution, Configuration);

        public override bool Equals([NotNullWhen(true)] object? obj)
        {
            return obj is BuildInfo other &&
                other.Solution == Solution &&
                other.Project == Project &&
                other.Configuration == Configuration && other.RuntimeIdentifier == RuntimeIdentifier &&
                other.PublishOutputDir == PublishOutputDir && other.Restore == Restore &&
                other.Rebuild == Rebuild && other.OnlyClean == OnlyClean &&
                other.CustomProperties == CustomProperties &&
                other.LogsDirPath == LogsDirPath;
        }

        public override int GetHashCode()
        {
            var hash = new HashCode();
            hash.Add(Solution);
            hash.Add(Project);
            hash.Add(Configuration);
            hash.Add(RuntimeIdentifier);
            hash.Add(PublishOutputDir);
            hash.Add(Restore);
            hash.Add(Rebuild);
            hash.Add(OnlyClean);
            hash.Add(CustomProperties);
            hash.Add(LogsDirPath);
            return hash.ToHashCode();
        }

        // Needed for instantiation from Godot, after reloading assemblies
        private BuildInfo()
        {
            Solution = string.Empty;
            Project = string.Empty;
            Configuration = string.Empty;
        }

        public BuildInfo(string solution, string project, string configuration, bool restore, bool rebuild, bool onlyClean)
        {
            Solution = solution;
            Project = project;
            Configuration = configuration;
            Restore = restore;
            Rebuild = rebuild;
            OnlyClean = onlyClean;
        }

        public BuildInfo(string solution, string project, string configuration, string runtimeIdentifier,
            string publishOutputDir, bool restore, bool rebuild, bool onlyClean)
        {
            Solution = solution;
            Project = project;
            Configuration = configuration;
            RuntimeIdentifier = runtimeIdentifier;
            PublishOutputDir = publishOutputDir;
            Restore = restore;
            Rebuild = rebuild;
            OnlyClean = onlyClean;
        }
    }
}
