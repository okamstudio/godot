#
#   tested on               | Windows native    | Linux cross-compilation
#   ------------------------+-------------------+---------------------------
#   MSVS C++ 2010 Express   | WORKS             | n/a
#   Mingw-w64               | WORKS             | WORKS
#   Mingw-w32               | WORKS             | WORKS
#   MinGW                   | WORKS             | untested
#
#####
# Notes about MSVS C++ :
#
#   - MSVC2010-Express compiles to 32bits only.
#
#####
# Notes about Mingw-w64 and Mingw-w32 under Windows :
#
#   - both can be installed using the official installer :
#       http://mingw-w64.sourceforge.net/download.php#mingw-builds
#
#   - if you want to compile both 32bits and 64bits, don't forget to
#   run the installer twice to install them both.
#
#   - install them into a path that does not contain spaces
#       ( example : "C:/Mingw-w32", "C:/Mingw-w64" )
#
#   - if you want to compile faster using the "-j" option, don't forget
#   to install the appropriate version of the Pywin32 python extension
#   available from : http://sourceforge.net/projects/pywin32/files/
#
#   - before running scons, you must add into the environment path
#   the path to the "/bin" directory of the Mingw version you want
#   to use :
#
#       set PATH=C:/Mingw-w32/bin;%PATH%
#
#   - then, scons should be able to detect gcc.
#   - Mingw-w32 only compiles 32bits.
#   - Mingw-w64 only compiles 64bits.
#
#   - it is possible to add them both at the same time into the PATH env,
#   if you also define the MINGW32_PREFIX and MINGW64_PREFIX environment
#   variables.
#   For instance, you could store that set of commands into a .bat script
#   that you would run just before scons :
#
#           set PATH=C:\mingw-w32\bin;%PATH%
#           set PATH=C:\mingw-w64\bin;%PATH%
#           set MINGW32_PREFIX=C:\mingw-w32\bin\
#           set MINGW64_PREFIX=C:\mingw-w64\bin\
#
#####
# Notes about Mingw, Mingw-w64 and Mingw-w32 under Linux :
#
#   - default toolchain prefixes are :
#       "i586-mingw32msvc-" for MinGW
#       "i686-w64-mingw32-" for Mingw-w32
#       "x86_64-w64-mingw32-" for Mingw-w64
#
#   - if both MinGW and Mingw-w32 are installed on your system
#   Mingw-w32 should take the priority over MinGW.
#
#   - it is possible to manually override prefixes by defining
#   the MINGW32_PREFIX and MINGW64_PREFIX environment variables.
#
#####
# Notes about Mingw under Windows :
#
#   - this is the MinGW version from http://mingw.org/
#   - install it into a path that does not contain spaces
#       ( example : "C:/MinGW" )
#   - several DirectX headers might be missing. You can copy them into
#   the C:/MinGW/include" directory from this page :
#    https://code.google.com/p/mingw-lib/source/browse/trunk/working/avcodec_to_widget_5/directx_include/
#   - before running scons, add the path to the "/bin" directory :
#       set PATH=C:/MinGW/bin;%PATH%
#   - scons should be able to detect gcc.
#

#####
# TODO :
#
#   - finish to cleanup this script to remove all the remains of previous hacks and workarounds
#   - make it work with the Windows7 SDK that is supposed to enable 64bits compilation for MSVC2010-Express
#   - confirm it works well with other Visual Studio versions.
#   - update the wiki about the pywin32 extension required for the "-j" option under Windows.
#   - update the wiki to document MINGW32_PREFIX and MINGW64_PREFIX
#

import os

import sys

import methods

def is_active():
    return True


def get_name():
    return "Windows"


def can_build():

    if (os.name == "nt"):
        # building natively on windows!
        if (os.getenv("VCINSTALLDIR")):
            return True
        else:
            print("\nMSVC not detected, attempting Mingw.")
            mingw32 = ""
            mingw64 = ""
            if (os.getenv("MINGW32_PREFIX")):
                mingw32 = os.getenv("MINGW32_PREFIX")
            if (os.getenv("MINGW64_PREFIX")):
                mingw64 = os.getenv("MINGW64_PREFIX")

            test = "gcc --version > NUL 2>&1"
            if os.system(test) != 0 and os.system(mingw32 + test) != 0 and os.system(mingw64 + test) != 0:
                print("- could not detect gcc.")
                print("Please, make sure a path to a Mingw /bin directory is accessible into the environment PATH.\n")
                return False
            else:
                print("- gcc detected.")

            return True

    if (os.name == "posix"):

        mingw = "i586-mingw32msvc-"
        mingw64 = "x86_64-w64-mingw32-"
        mingw32 = "i686-w64-mingw32-"

        if (os.getenv("MINGW32_PREFIX")):
            mingw32 = os.getenv("MINGW32_PREFIX")
            mingw = mingw32
        if (os.getenv("MINGW64_PREFIX")):
            mingw64 = os.getenv("MINGW64_PREFIX")

        test = "gcc --version > /dev/null 2>&1"
        if (os.system(mingw + test) == 0 or os.system(mingw64 + test) == 0 or os.system(mingw32 + test) == 0):
            return True

    return False


def get_opts():

    mingw = ""
    mingw32 = ""
    mingw64 = ""
    if (os.name == "posix"):
        mingw = "i586-mingw32msvc-"
        mingw32 = "i686-w64-mingw32-"
        mingw64 = "x86_64-w64-mingw32-"

        if os.system(mingw32 + "gcc --version > /dev/null 2>&1") != 0:
            mingw32 = mingw

    if (os.getenv("MINGW32_PREFIX")):
        mingw32 = os.getenv("MINGW32_PREFIX")
        mingw = mingw32
    if (os.getenv("MINGW64_PREFIX")):
        mingw64 = os.getenv("MINGW64_PREFIX")

    return [
        ('mingw_prefix', 'Mingw Prefix', mingw32),
        ('mingw_prefix_64', 'Mingw Prefix 64 bits', mingw64),
        # Targeted Windows version: Vista (and later)
        ('target_win_version', 'Targeted Windows version, >= 0x0600 (Vista)', '0x0600'),
    ]


def get_flags():

    return [
    ]


def build_res_file(target, source, env):

    cmdbase = ""
    if (env["bits"] == "32"):
        cmdbase = env['mingw_prefix']
    else:
        cmdbase = env['mingw_prefix_64']
    CPPPATH = env['CPPPATH']
    cmdbase = cmdbase + 'windres --include-dir . '
    import subprocess
    for x in range(len(source)):
        cmd = cmdbase + '-i ' + str(source[x]) + ' -o ' + str(target[x])
        try:
            out = subprocess.Popen(cmd, shell=True, stderr=subprocess.PIPE).communicate()
            if len(out[1]):
                return 1
        except:
            return 1
    return 0


def configure(env):

    env.Append(CPPPATH=['#platform/windows'])

    env['is_mingw'] = False
    if (os.name == "nt" and os.getenv("VCINSTALLDIR")):
        # build using visual studio
        env['ENV']['TMP'] = os.environ['TMP']
        env.Append(CPPPATH=['#platform/windows/include'])
        env.Append(LIBPATH=['#platform/windows/lib'])
        env.Append(CCFLAGS=['/DWINVER=%s' % env['target_win_version'], '/D_WIN32_WINNT=%s' % env['target_win_version']])

        if (env["target"] == "release"):

            env.Append(CCFLAGS=['/O2'])
            env.Append(LINKFLAGS=['/SUBSYSTEM:WINDOWS'])
            env.Append(LINKFLAGS=['/ENTRY:mainCRTStartup'])

        elif (env["target"] == "release_debug"):

            env.Append(CCFLAGS=['/O2', '/DDEBUG_ENABLED'])
            env.Append(LINKFLAGS=['/SUBSYSTEM:CONSOLE'])
        elif (env["target"] == "debug_release"):

            env.Append(CCFLAGS=['/Z7', '/Od'])
            env.Append(LINKFLAGS=['/DEBUG'])
            env.Append(LINKFLAGS=['/SUBSYSTEM:WINDOWS'])
            env.Append(LINKFLAGS=['/ENTRY:mainCRTStartup'])

        elif (env["target"] == "debug"):

            env.Append(CCFLAGS=['/Z7', '/DDEBUG_ENABLED', '/DDEBUG_MEMORY_ENABLED', '/DD3D_DEBUG_INFO', '/Od', '/EHsc'])
            env.Append(LINKFLAGS=['/SUBSYSTEM:CONSOLE'])
            env.Append(LINKFLAGS=['/DEBUG'])

        env.Append(CCFLAGS=['/MT', '/Gd', '/GR', '/nologo'])
        env.Append(CXXFLAGS=['/TP'])
        env.Append(CPPFLAGS=['/DMSVC', '/GR', ])
        env.Append(CCFLAGS=['/I' + os.getenv("WindowsSdkDir") + "/Include"])
        env.Append(CCFLAGS=['/DWINDOWS_ENABLED'])
        env.Append(CCFLAGS=['/DRTAUDIO_ENABLED'])
        env.Append(CCFLAGS=['/DWASAPI_ENABLED'])
        env.Append(CCFLAGS=['/DWIN32'])
        env.Append(CCFLAGS=['/DTYPED_METHOD_BIND'])

        env.Append(CCFLAGS=['/DGLES2_ENABLED'])

        LIBS = ['winmm', 'opengl32', 'dsound', 'kernel32', 'ole32', 'oleaut32', 'user32', 'gdi32', 'IPHLPAPI', 'Shlwapi', 'wsock32', 'ws2_32', 'shell32', 'advapi32', 'dinput8', 'dxguid']
        env.Append(LINKFLAGS=[p + env["LIBSUFFIX"] for p in LIBS])

        env.Append(LIBPATH=[os.getenv("WindowsSdkDir") + "/Lib"])
        if (os.getenv("DXSDK_DIR")):
            DIRECTX_PATH = os.getenv("DXSDK_DIR")
        else:
            DIRECTX_PATH = "C:/Program Files/Microsoft DirectX SDK (March 2009)"

        if (os.getenv("VCINSTALLDIR")):
            VC_PATH = os.getenv("VCINSTALLDIR")
        else:
            VC_PATH = ""

        env.Append(CCFLAGS=["/I" + p for p in os.getenv("INCLUDE").split(";")])
        env.Append(LIBPATH=[p for p in os.getenv("LIB").split(";")])
        env.Append(CCFLAGS=["/I" + DIRECTX_PATH + "/Include"])
        env.Append(LIBPATH=[DIRECTX_PATH + "/Lib/x86"])
        env['ENV'] = os.environ

        # This detection function needs the tools env (that is env['ENV'], not SCons's env), and that is why it's this far bellow in the code
        compiler_version_str = methods.detect_visual_c_compiler_version(env['ENV'])

        # Note: this detection/override code from here onward should be here instead of in SConstruct because it's platform and compiler specific (MSVC/Windows)
        if(env["bits"] != "default"):
            print("Error: bits argument is disabled for MSVC")
            print ("Bits argument is not supported for MSVC compilation. Architecture depends on the Native/Cross Compile Tools Prompt/Developer Console (or Visual Studio settings)"
                   + " that is being used to run SCons. As a consequence, bits argument is disabled. Run scons again without bits argument (example: scons p=windows) and SCons will attempt to detect what MSVC compiler"
                   + " will be executed and inform you.")
            sys.exit()

        # Forcing bits argument because MSVC does not have a flag to set this through SCons... it's different compilers (cl.exe's) called from the propper command prompt
        # that decide the architecture that is build for. Scons can only detect the os.getenviron (because vsvarsall.bat sets a lot of stuff for cl.exe to work with)
        env["bits"] = "32"
        env["x86_libtheora_opt_vc"] = True

        print("Detected MSVC compiler: " + compiler_version_str)
        # If building for 64bit architecture, disable assembly optimisations for 32 bit builds (theora as of writting)... vc compiler for 64bit can not compile _asm
        if(compiler_version_str == "amd64" or compiler_version_str == "x86_amd64"):
            env["bits"] = "64"
            env["x86_libtheora_opt_vc"] = False
            print("Compiled program architecture will be a 64 bit executable (forcing bits=64).")
        elif (compiler_version_str == "x86" or compiler_version_str == "amd64_x86"):
            print("Compiled program architecture will be a 32 bit executable. (forcing bits=32).")
        else:
            print("Failed to detect MSVC compiler architecture version... Defaulting to 32bit executable settings (forcing bits=32). Compilation attempt will continue, but SCons can not detect for what architecture this build is compiled for. You should check your settings/compilation setup.")
        if env["bits"] == "64":
            env.Append(CCFLAGS=['/D_WIN64'])
    else:

        # Workaround for MinGW. See:
        # http://www.scons.org/wiki/LongCmdLinesOnWin32
        env.use_windows_spawn_fix()

        # build using mingw
        env.Append(CCFLAGS=['-DWINVER=%s' % env['target_win_version'], '-D_WIN32_WINNT=%s' % env['target_win_version']])
        if (os.name == "nt"):
            env['ENV']['TMP'] = os.environ['TMP']  # way to go scons, you can be so stupid sometimes
        else:
            env["PROGSUFFIX"] = env["PROGSUFFIX"] + ".exe"  # for linux cross-compilation

        mingw_prefix = ""

        if (env["bits"] == "default"):
            env["bits"] = "64" if "PROGRAMFILES(X86)" in os.environ else "32"

        if (env["bits"] == "32"):
            env.Append(LINKFLAGS=['-static'])
            env.Append(LINKFLAGS=['-static-libgcc'])
            env.Append(LINKFLAGS=['-static-libstdc++'])
            mingw_prefix = env["mingw_prefix"]
        else:
            env.Append(LINKFLAGS=['-static'])
            mingw_prefix = env["mingw_prefix_64"]

        if (env["target"] == "release"):

            env.Append(CCFLAGS=['-msse2'])

            if (env["bits"] == "64"):
                env.Append(CCFLAGS=['-O3'])

                if (os.system(mingw_prefix + "gcc --version > /dev/null 2>&1") == 0): # GCC
                    # Hack to prevent segfaults due to UB when dereferencing NULL `this` in `Object::cast_to` with `-O3`.
                    # This is fixed in 3.0, for 2.1 we just prevent using the known affected GCC versions (6 and 7).
                    # GCC 5 or GCC 8 and later seem to be fine.
                    import subprocess
                    gcc_major = int(subprocess.check_output([mingw_prefix + 'gcc', '-dumpversion']).decode('ascii').split('.')[0])
                    if (gcc_major == 6 or gcc_major == 7):
                        print("Your configured compiler appears to be GCC %d, which triggers issues in release builds for this version of Godot (fixed in Godot 3.0+)." % gcc_major)
                        print("You can use the Clang compiler instead with the `use_llvm=yes` option, or configure another compiler such as GCC 5 using the CC, CXX and LD flags.")
                        print("Aborting..")
                        sys.exit(255)
            else:
                env.Append(CCFLAGS=['-O2'])

            env.Append(LINKFLAGS=['-Wl,--subsystem,windows'])

        elif (env["target"] == "release_debug"):

            env.Append(CCFLAGS=['-O2', '-DDEBUG_ENABLED'])

        elif (env["target"] == "debug"):

            env.Append(CCFLAGS=['-g', '-DDEBUG_ENABLED', '-DDEBUG_MEMORY_ENABLED'])

        env["CC"] = mingw_prefix + "gcc"
        env['AS'] = mingw_prefix + "as"
        env['CXX'] = mingw_prefix + "g++"
        env['AR'] = mingw_prefix + "ar"
        env['RANLIB'] = mingw_prefix + "ranlib"
        env['LD'] = mingw_prefix + "g++"
        env["x86_libtheora_opt_gcc"] = True

        #env['CC'] = "winegcc"
        #env['CXX'] = "wineg++"

        env.Append(CCFLAGS=['-DWINDOWS_ENABLED', '-mwindows'])
        env.Append(CPPFLAGS=['-DRTAUDIO_ENABLED'])
        env.Append(CPPFLAGS=['-DWASAPI_ENABLED'])
        env.Append(CCFLAGS=['-DGLES2_ENABLED'])
        env.Append(LIBS=['mingw32', 'opengl32', 'dsound', 'ole32', 'd3d9', 'winmm', 'gdi32', 'iphlpapi', 'shlwapi', 'wsock32', 'ws2_32', 'kernel32', 'oleaut32', 'dinput8', 'dxguid', 'ksuser'])

        # if (env["bits"]=="32"):
        # env.Append(LIBS=['gcc_s'])
        # #--with-arch=i686
        # env.Append(CPPFLAGS=['-march=i686'])
        # env.Append(LINKFLAGS=['-march=i686'])

        #'d3dx9d'
        env.Append(CPPFLAGS=['-DMINGW_ENABLED'])
        # env.Append(LINKFLAGS=['-g'])

        # resrc
        env['is_mingw'] = True
        env.Append(BUILDERS={'RES': env.Builder(action=build_res_file, suffix='.o', src_suffix='.rc')})

    env.Append(BUILDERS={'GLSL120': env.Builder(action=methods.build_legacygl_headers, suffix='glsl.gen.h', src_suffix='.glsl')})
    env.Append(BUILDERS={'GLSL': env.Builder(action=methods.build_glsl_headers, suffix='glsl.gen.h', src_suffix='.glsl')})
    env.Append(BUILDERS={'HLSL9': env.Builder(action=methods.build_hlsl_dx9_headers, suffix='hlsl.gen.h', src_suffix='.hlsl')})
    env.Append(BUILDERS={'GLSL120GLES': env.Builder(action=methods.build_gles2_headers, suffix='glsl.gen.h', src_suffix='.glsl')})
