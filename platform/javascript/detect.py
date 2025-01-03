import os
import sys
import string


def is_active():
    return True


def get_name():
    return "JavaScript"


def can_build():

    return ("EMSCRIPTEN_ROOT" in os.environ or "EMSCRIPTEN" in os.environ)


def get_opts():

    return [
        ['compress', 'Compress JS Executable', 'no']
    ]


def get_flags():

    return [
        ('tools', 'no'),
        ('module_etc1_enabled', 'no'),
        ('module_mpc_enabled', 'no'),
        ('module_speex_enabled', 'no'),
    ]


def configure(env):
    env['ENV'] = os.environ
    env.use_windows_spawn_fix('javascript')

    env.Append(CPPPATH=['#platform/javascript'])

    env['ENV'] = os.environ
    if ("EMSCRIPTEN_ROOT" in os.environ):
        em_path = os.environ["EMSCRIPTEN_ROOT"]
    elif ("EMSCRIPTEN" in os.environ):
        em_path = os.environ["EMSCRIPTEN"]

    env['ENV']['PATH'] = em_path + ":" + env['ENV']['PATH']

    env['CC'] = em_path + '/emcc'
    env['CXX'] = em_path + '/emcc'
    env['AR'] = em_path + "/emar"
    env['RANLIB'] = em_path + "/emranlib"

    env['OBJSUFFIX'] = '.bc'
    env['LIBSUFFIX'] = '.bc'

#	env.Append(LIBS=['c','m','stdc++','log','GLESv1_CM','GLESv2'])

#	env["LINKFLAGS"]= string.split(" -g --sysroot="+ld_sysroot+" -Wl,--no-undefined -Wl,-z,noexecstack ")

    if (env["target"] == "release"):
        env.Append(CCFLAGS=['-O2'])
    elif (env["target"] == "release_debug"):
        env.Append(CCFLAGS=['-O2', '-DDEBUG_ENABLED'])
    elif (env["target"] == "debug"):
        env.Append(CCFLAGS=['-D_DEBUG', '-O2', '-DDEBUG_ENABLED'])
        env.Append(CPPFLAGS=['-DDEBUG_MEMORY_ALLOC'])

    # TODO: Move that to opus module's config
    if("module_opus_enabled" in env and env["module_opus_enabled"] != "no"):
        env.opus_fixed_point = "yes"

    env.Append(CPPFLAGS=["-fno-exceptions", '-DNO_SAFE_CAST', '-fno-rtti'])
    env.Append(CPPFLAGS=['-DJAVASCRIPT_ENABLED', '-DUNIX_ENABLED', '-DPTHREAD_NO_RENAME', '-DNO_FCNTL', '-DMPC_FIXED_POINT', '-DTYPED_METHOD_BIND', '-DNO_THREADS'])
    env.Append(CPPFLAGS=['-DGLES2_ENABLED'])
    env.Append(CPPFLAGS=['-DGLES_NO_CLIENT_ARRAYS'])
    env.Append(CPPFLAGS=['-s', 'ASM_JS=1'])
    env.Append(CPPFLAGS=['-s', 'FULL_ES2=1'])
#	env.Append(CPPFLAGS=['-DANDROID_ENABLED', '-DUNIX_ENABLED','-DMPC_FIXED_POINT'])
    if (env["compress"] == "yes"):
        lzma_binpath = em_path + "/third_party/lzma.js/lzma-native"
        lzma_decoder = em_path + "/third_party/lzma.js/lzma-decoder.js"
        lzma_dec = "LZMA.decompress"
        env.Append(LINKFLAGS=['--compression', lzma_binpath + "," + lzma_decoder + "," + lzma_dec])

    # We use IDBFS in javascript_main.cpp. Since Emscripten 1.39.1 it needs to
    # be linked explicitly.
    env.Append(LIBS=['idbfs.js'])

    env.Append(LINKFLAGS=['-s', 'ASM_JS=1'])
    env.Append(LINKFLAGS=['-s', 'WASM=0'])
    env.Append(LINKFLAGS=['-s', 'EXTRA_EXPORTED_RUNTIME_METHODS="[\'FS\']"'])
    env.Append(LINKFLAGS=['--separate-asm'])
    env.Append(LINKFLAGS=['-O2'])
    # env.Append(LINKFLAGS=['-g4'])

    # FIXME: This used to be the default in older emscripten, but now it would error out.
    # We have two undefined symbols in the current 2.1 branch (since years):
    # error: undefined symbol: popen
    # error: undefined symbol: sem_getvalue
    env.Append(LINKFLAGS=['-s', 'ERROR_ON_UNDEFINED_SYMBOLS=0'])

    # print "CCCOM is:", env.subst('$CCCOM')
    # print "P: ", env['p'], " Platofrm: ", env['platform']

    import methods

    env.Append(BUILDERS={'GLSL120': env.Builder(action=methods.build_legacygl_headers, suffix='glsl.gen.h', src_suffix='.glsl')})
    env.Append(BUILDERS={'GLSL': env.Builder(action=methods.build_glsl_headers, suffix='glsl.gen.h', src_suffix='.glsl')})
    env.Append(BUILDERS={'GLSL120GLES': env.Builder(action=methods.build_gles2_headers, suffix='glsl.gen.h', src_suffix='.glsl')})
    #env.Append( BUILDERS = { 'HLSL9' : env.Builder(action = methods.build_hlsl_dx9_headers, suffix = 'hlsl.gen.h',src_suffix = '.hlsl') } )
