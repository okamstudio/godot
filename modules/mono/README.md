# How to build and run

1. Build Godot with the module enabled: `module_mono_enabled=yes`.
2. After building Godot, use it to generate the C# glue code:
   ```sh
   <godot_binary> --generate-mono-glue ./modules/mono/glue
   ```
3. Build the C# solutions:
   ```sh
   ./modules/mono/build_scripts/build_assemblies.py --godot-output-dir ./bin
   ```

The paths specified in these examples assume the command is being run from
the Godot source root.

# How to deal with NuGet packages

We distribute the API assemblies, our source generators, and our custom
MSBuild project SDK as NuGet packages. This is all transparent to the user,
but it can make things complicated during development.

In order to use Godot with a development of those packages, we must create
a local NuGet source where MSBuild can find them. This can be done with
the .NET CLI:

```sh
dotnet nuget add source ~/MyLocalNugetSource --name MyLocalNugetSource
```

The Godot NuGet packages must be added to that local source. Additionally,
we must make sure there are no other versions of the package in the NuGet
cache, as MSBuild may pick one of those instead.

In order to simplify this process, the `build_assemblies.py` script provides
the following `--push-nupkgs-local` option:

```sh
./modules/mono/build_scripts/build_assemblies.py --godot-output-dir ./bin \
    --push-nupkgs-local ~/MyLocalNugetSource
```

This option ensures the packages will be added to the specified local NuGet
source and that conflicting versions of the package are removed from the
NuGet cache. It's recommended to always use this option when building the
C# solutions during development to avoid mistakes.

# Double Precision Support (REAL_T_IS_DOUBLE)

Follow the above instructions but build Godot with the precision=double argument to scons

When building the NuGet packages, specify `--precision=double` - for example:
```sh
./modules/mono/build_scripts/build_assemblies.py --godot-output-dir ./bin \
    --push-nupkgs-local ~/MyLocalNugetSource --precision=double
```
