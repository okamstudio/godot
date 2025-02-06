using System;
using System.IO;
using System.Threading.Tasks;
using Godot;
using GodotTools.Internals;
using static GodotTools.Internals.Globals;
using File = GodotTools.Utils.File;

namespace GodotTools.Build
{
    public partial class MSBuildPanel : MarginContainer, ISerializationListener
    {
        [Signal]
        public delegate void BuildStateChangedEventHandler();

#nullable disable
        private MenuButton _buildMenuButton;
        private Button _openLogsFolderButton;

        private BuildProblemsView _problemsView;
        private BuildOutputView _outputView;
#nullable enable

        public BuildInfo? LastBuildInfo { get; private set; }
        public bool IsBuildingOngoing { get; private set; }
        public BuildResult? BuildResult { get; private set; }

        private readonly object _pendingBuildLogTextLock = new object();
        private string _pendingBuildLogText = string.Empty;

        public Texture2D? GetBuildStateIcon()
        {
            if (IsBuildingOngoing)
                return GetThemeIcon("Stop", "EditorIcons");

            if (_problemsView.WarningCount > 0 && _problemsView.ErrorCount > 0)
                return GetThemeIcon("ErrorWarning", "EditorIcons");

            if (_problemsView.WarningCount > 0)
                return GetThemeIcon("Warning", "EditorIcons");

            if (_problemsView.ErrorCount > 0)
                return GetThemeIcon("Error", "EditorIcons");

            return null;
        }

        private enum BuildMenuOptions
        {
            BuildProject,
            RebuildProject,
            CleanProject,
        }

        private void BuildMenuOptionPressed(long id)
        {
            switch ((BuildMenuOptions)id)
            {
                case BuildMenuOptions.BuildProject:
                    _ = BuildProject();
                    break;

                case BuildMenuOptions.RebuildProject:
                    _ = RebuildProject();
                    break;

                case BuildMenuOptions.CleanProject:
                    _ = CleanProject();
                    break;

                default:
                    throw new ArgumentOutOfRangeException(nameof(id), id, "Invalid build menu option");
            }
        }

        public async Task BuildProject()
        {
            if (!File.Exists(GodotSharpDirs.ProjectCsProjPath))
                return; // No project to build.

            if (!await BuildManager.BuildProjectAsync("Debug"))
                return; // Build failed.

            // Notify running game for hot-reload.
            Internal.EditorDebuggerNodeReloadScripts();

            // Hot-reload in the editor.
            GodotSharpEditor.Instance.GetNode<HotReloadAssemblyWatcher>("HotReloadAssemblyWatcher").RestartTimer();

            if (Internal.IsAssembliesReloadingNeeded())
            {
                BuildManager.UpdateLastValidBuildDateTime();
                Internal.ReloadAssemblies(softReload: false);
            }
        }

        private async Task RebuildProject()
        {
            if (!File.Exists(GodotSharpDirs.ProjectCsProjPath))
                return; // No project to build.

            if (!await BuildManager.BuildProjectAsync("Debug", rebuild: true))
                return; // Build failed.

            // Notify running game for hot-reload.
            Internal.EditorDebuggerNodeReloadScripts();

            // Hot-reload in the editor.
            GodotSharpEditor.Instance.GetNode<HotReloadAssemblyWatcher>("HotReloadAssemblyWatcher").RestartTimer();

            if (Internal.IsAssembliesReloadingNeeded())
            {
                BuildManager.UpdateLastValidBuildDateTime();
                Internal.ReloadAssemblies(softReload: false);
            }
        }

        private async Task CleanProject()
        {
            if (!File.Exists(GodotSharpDirs.ProjectCsProjPath))
                return; // No project to build.

            _ = await BuildManager.CleanProjectAsync("Debug");
        }

        private void OpenLogsFolder() => OS.ShellOpen(
            $"file://{GodotSharpDirs.LogsDirPathFor("Debug")}"
        );

        private void BuildLaunchFailed(BuildInfo buildInfo, string cause)
        {
            IsBuildingOngoing = false;
            BuildResult = Build.BuildResult.Error;

            _problemsView.Clear();
            _outputView.Clear();

            var diagnostic = new BuildDiagnostic
            {
                Type = BuildDiagnostic.DiagnosticType.Error,
                Message = cause,
            };

            _problemsView.SetDiagnostics(new[] { diagnostic });
            _buildMenuButton.Disabled = IsBuildingOngoing;

            EmitSignal(SignalName.BuildStateChanged);
        }

        private void BuildStarted(BuildInfo buildInfo)
        {
            LastBuildInfo = buildInfo;
            IsBuildingOngoing = true;
            BuildResult = null;

            _problemsView.Clear();
            _outputView.Clear();

            _problemsView.UpdateProblemsView();
            _buildMenuButton.Disabled = IsBuildingOngoing;

            EmitSignal(SignalName.BuildStateChanged);
        }

        private void BuildFinished(BuildResult result)
        {
            IsBuildingOngoing = false;
            BuildResult = result;

            string csvFile = Path.Combine(LastBuildInfo!.LogsDirPath, BuildManager.MsBuildIssuesFileName);
            _problemsView.SetDiagnosticsFromFile(csvFile);

            _problemsView.UpdateProblemsView();
            _buildMenuButton.Disabled = IsBuildingOngoing;

            EmitSignal(SignalName.BuildStateChanged);
        }

        private void UpdateBuildLogText()
        {
            lock (_pendingBuildLogTextLock)
            {
                _outputView.Append(_pendingBuildLogText);
                _pendingBuildLogText = string.Empty;
            }
        }

        private void StdOutputReceived(string? text)
        {
            lock (_pendingBuildLogTextLock)
            {
                if (_pendingBuildLogText.Length == 0)
                    CallDeferred(nameof(UpdateBuildLogText));
                _pendingBuildLogText += text + "\n";
            }
        }

        private void StdErrorReceived(string? text)
        {
            lock (_pendingBuildLogTextLock)
            {
                if (_pendingBuildLogText.Length == 0)
                    CallDeferred(nameof(UpdateBuildLogText));
                _pendingBuildLogText += text + "\n";
            }
        }

        public override void _Ready()
        {
            base._Ready();

            var bottomPanelStylebox = EditorInterface.Singleton.GetBaseControl().GetThemeStylebox("BottomPanel", "EditorStyles");
            AddThemeConstantOverride("margin_top", -(int)bottomPanelStylebox.ContentMarginTop);
            AddThemeConstantOverride("margin_left", -(int)bottomPanelStylebox.ContentMarginLeft);
            AddThemeConstantOverride("margin_right", -(int)bottomPanelStylebox.ContentMarginRight);

            var tabs = new TabContainer();
            AddChild(tabs);

            var tabActions = new HBoxContainer
            {
                SizeFlagsVertical = SizeFlags.ExpandFill,
                SizeFlagsHorizontal = SizeFlags.ExpandFill,
                Alignment = BoxContainer.AlignmentMode.End,
            };
            tabActions.SetAnchorsAndOffsetsPreset(LayoutPreset.FullRect);
            tabs.GetTabBar().AddChild(tabActions);

            _buildMenuButton = new MenuButton
            {
                TooltipText = "Build".TTR(),
                Flat = true,
            };
            tabActions.AddChild(_buildMenuButton);

            var buildMenu = _buildMenuButton.GetPopup();
            buildMenu.AddItem("Build Project".TTR(), (int)BuildMenuOptions.BuildProject);
            buildMenu.AddItem("Rebuild Project".TTR(), (int)BuildMenuOptions.RebuildProject);
            buildMenu.AddItem("Clean Project".TTR(), (int)BuildMenuOptions.CleanProject);
            buildMenu.IdPressed += BuildMenuOptionPressed;

            _openLogsFolderButton = new Button
            {
                TooltipText = "Show Logs in File Manager".TTR(),
                Flat = true,
            };
            _openLogsFolderButton.Pressed += OpenLogsFolder;
            tabActions.AddChild(_openLogsFolderButton);

            _problemsView = new BuildProblemsView();
            tabs.AddChild(_problemsView);

            _outputView = new BuildOutputView();
            tabs.AddChild(_outputView);

            UpdateTheme();

            AddBuildEventListeners();
        }

        public override void _Notification(int what)
        {
            base._Notification(what);

            if (what == NotificationThemeChanged)
            {
                UpdateTheme();
            }
        }

        private void UpdateTheme()
        {
            // Nodes will be null until _Ready is called.
            if (_buildMenuButton == null)
                return;

            _buildMenuButton.Icon = GetThemeIcon("BuildCSharp", "EditorIcons");
            _openLogsFolderButton.Icon = GetThemeIcon("Filesystem", "EditorIcons");
        }

        private void AddBuildEventListeners()
        {
            BuildManager.BuildLaunchFailed += BuildLaunchFailed;
            BuildManager.BuildStarted += BuildStarted;
            BuildManager.BuildFinished += BuildFinished;
            // StdOutput/Error can be received from different threads, so we need to use CallDeferred.
            BuildManager.StdOutputReceived += StdOutputReceived;
            BuildManager.StdErrorReceived += StdErrorReceived;
        }

        public void OnBeforeSerialize()
        {
            // In case it didn't update yet. We don't want to have to serialize any pending output.
            UpdateBuildLogText();

            // NOTE:
            // Currently, GodotTools is loaded in its own load context. This load context is not reloaded, but the script still are.
            // Until that changes, we need workarounds like this one because events keep strong references to disposed objects.
            BuildManager.BuildLaunchFailed -= BuildLaunchFailed;
            BuildManager.BuildStarted -= BuildStarted;
            BuildManager.BuildFinished -= BuildFinished;
            // StdOutput/Error can be received from different threads, so we need to use CallDeferred
            BuildManager.StdOutputReceived -= StdOutputReceived;
            BuildManager.StdErrorReceived -= StdErrorReceived;
        }

        public void OnAfterDeserialize()
        {
            AddBuildEventListeners(); // Re-add them.
        }
    }
}
