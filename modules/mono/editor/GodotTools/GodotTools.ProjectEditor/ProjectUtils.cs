using System;
using System.Collections.Generic;
using System.Linq;
using System.Text.RegularExpressions;
using Microsoft.Build.Construction;
using Microsoft.Build.Evaluation;
using Microsoft.Build.Locator;
using NuGet.Frameworks;

namespace GodotTools.ProjectEditor
{
    public sealed class MSBuildProject
    {
        internal ProjectRootElement Root { get; set; }

        public bool HasUnsavedChanges { get; set; }

        public void Save() => Root.Save();

        public MSBuildProject(ProjectRootElement root)
        {
            Root = root;
        }
    }

    public static partial class ProjectUtils
    {
        [GeneratedRegex(@"\s*'\$\(GodotTargetPlatform\)'\s*==\s*'(?<platform>[A-z]+)'\s*", RegexOptions.IgnoreCase)]
        private static partial Regex GodotTargetPlatformConditionRegex();

        private static readonly string[] _platformNames =
        {
            "windows",
            "linuxbsd",
            "macos",
            "android",
            "ios",
            "web",
        };

        public static void MSBuildLocatorRegisterLatest(out Version version, out string path)
        {
            var instance = MSBuildLocator.QueryVisualStudioInstances()
                .OrderByDescending(x => x.Version)
                .First();
            MSBuildLocator.RegisterInstance(instance);
            version = instance.Version;
            path = instance.MSBuildPath;
        }

        public static void MSBuildLocatorRegisterMSBuildPath(string msbuildPath)
            => MSBuildLocator.RegisterMSBuildPath(msbuildPath);

        public static MSBuildProject? Open(string path)
        {
            var root = ProjectRootElement.Open(path, ProjectCollection.GlobalProjectCollection, preserveFormatting: true);
            return root != null ? new MSBuildProject(root) : null;
        }

        public static void UpgradeProjectIfNeeded(MSBuildProject project, string projectName)
        {
            // NOTE: The order in which changes are made to the project is important.

            // Migrate to MSBuild project Sdks style if using the old style.
            MigrateToProjectSdksStyle(project, projectName);

            EnsureGodotSdkIsUpToDate(project);
            EnsureTargetFrameworkMatchesMinimumRequirement(project);
        }

        private static void MigrateToProjectSdksStyle(MSBuildProject project, string projectName)
        {
            var origRoot = project.Root;

            if (!string.IsNullOrEmpty(origRoot.Sdk))
                return;

            project.Root = ProjectGenerator.GenGameProject(projectName);
            project.Root.FullPath = origRoot.FullPath;
            project.HasUnsavedChanges = true;
        }

        public static void EnsureGodotSdkIsUpToDate(MSBuildProject project)
        {
            var root = project.Root;
            string godotSdkAttrValue = ProjectGenerator.GodotSdkAttrValue;

            if (!string.IsNullOrEmpty(root.Sdk) &&
                root.Sdk.Trim().Equals(godotSdkAttrValue, StringComparison.OrdinalIgnoreCase))
                return;

            root.Sdk = godotSdkAttrValue;
            project.HasUnsavedChanges = true;
        }

        private static void EnsureTargetFrameworkMatchesMinimumRequirement(MSBuildProject project)
        {
            var root = project.Root;
            string minTfmValue = ProjectGenerator.GodotMinimumRequiredTfm;
            var minTfmVersion = NuGetFramework.Parse(minTfmValue).Version;

            ProjectPropertyGroupElement? mainPropertyGroup = null;
            ProjectPropertyElement? mainTargetFrameworkProperty = null;

            var propertiesToChange = new List<ProjectPropertyElement>();

            foreach (var propertyGroup in root.PropertyGroups)
            {
                bool groupHasCondition = !string.IsNullOrEmpty(propertyGroup.Condition);

                // Check if the property group should be excluded from checking for 'TargetFramework' properties.
                if (groupHasCondition && !ConditionMatchesGodotPlatform(propertyGroup.Condition))
                {
                    continue;
                }

                // Store a reference to the first property group without conditions,
                // in case we need to add a new 'TargetFramework' property later.
                if (mainPropertyGroup == null && !groupHasCondition)
                {
                    mainPropertyGroup = propertyGroup;
                }

                foreach (var property in propertyGroup.Properties)
                {
                    // We are looking for 'TargetFramework' properties.
                    if (property.Name != "TargetFramework")
                    {
                        continue;
                    }

                    bool propertyHasCondition = !string.IsNullOrEmpty(property.Condition);

                    // Check if the property should be excluded.
                    if (propertyHasCondition && !ConditionMatchesGodotPlatform(property.Condition))
                    {
                        continue;
                    }

                    if (!groupHasCondition && !propertyHasCondition)
                    {
                        // Store a reference to the 'TargetFramework' that has no conditions
                        // because it applies to all platforms.
                        if (mainTargetFrameworkProperty == null)
                        {
                            mainTargetFrameworkProperty = property;
                        }
                        continue;
                    }

                    // If the 'TargetFramework' property is conditional, it may no longer be needed
                    // when the main one is upgraded to the new minimum version.
                    var tfmVersion = NuGetFramework.Parse(property.Value).Version;
                    if (tfmVersion <= minTfmVersion)
                    {
                        propertiesToChange.Add(property);
                    }
                }
            }

            if (mainTargetFrameworkProperty == null)
            {
                // We haven't found a 'TargetFramework' property without conditions,
                // we'll just add one in the first property group without conditions.
                if (mainPropertyGroup == null)
                {
                    // We also don't have a property group without conditions,
                    // so we'll add a new one to the project.
                    mainPropertyGroup = root.AddPropertyGroup();
                }

                mainTargetFrameworkProperty = mainPropertyGroup.AddProperty("TargetFramework", minTfmValue);
                project.HasUnsavedChanges = true;
            }
            else
            {
                var tfmVersion = NuGetFramework.Parse(mainTargetFrameworkProperty.Value).Version;
                if (tfmVersion < minTfmVersion)
                {
                    mainTargetFrameworkProperty.Value = minTfmValue;
                    project.HasUnsavedChanges = true;
                }
            }

            var mainTfmVersion = NuGetFramework.Parse(mainTargetFrameworkProperty.Value).Version;
            foreach (var property in propertiesToChange)
            {
                // If the main 'TargetFramework' property targets a version newer than
                // the minimum required by Godot, we don't want to remove the conditional
                // 'TargetFramework' properties, only upgrade them to the new minimum.
                // Otherwise, it can be removed.
                if (mainTfmVersion > minTfmVersion)
                {
                    property.Value = minTfmValue;
                }
                else
                {
                    property.Parent.RemoveChild(property);
                }

                project.HasUnsavedChanges = true;
            }

            static bool ConditionMatchesGodotPlatform(string condition)
            {
                // Check if the condition is checking the 'GodotTargetPlatform' for one of the
                // Godot platforms with built-in support in the Godot.NET.Sdk.
                var match = GodotTargetPlatformConditionRegex().Match(condition);
                if (match.Success)
                {
                    string platform = match.Groups["platform"].Value;
                    return _platformNames.Contains(platform, StringComparer.OrdinalIgnoreCase);
                }

                return false;
            }
        }
    }
}
