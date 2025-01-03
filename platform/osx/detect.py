
import os
import sys


def is_active():
	return True
	
def get_name():
	return "OSX"

def can_build():

	if (sys.platform == "darwin" or ("OSXCROSS_ROOT" in os.environ)):
		return True

	return False

def get_opts():

	return [
	    ('force_64_bits','Force 64 bits binary','no'),
	    ('osxcross_sdk', 'OSXCross SDK version', 'darwin14'),
	 ]

def get_flags():

	return [
	('opengl', 'no'),
	('legacygl', 'yes'),
	('builtin_zlib', 'no'),
	('freetype','builtin'), #use builtin freetype
	]



def configure(env):

	env.Append(CPPPATH=['#platform/osx'])

	if (env["bits"]=="default"):
	    env["bits"]="32"

	if (env["target"]=="release"):

		env.Append(CCFLAGS=['-O2','-ffast-math','-fomit-frame-pointer','-ftree-vectorize','-msse2'])

	elif (env["target"]=="release_debug"):

		env.Append(CCFLAGS=['-O2','-DDEBUG_ENABLED'])

	elif (env["target"]=="debug"):

		env.Append(CCFLAGS=['-g3', '-DDEBUG_ENABLED','-DDEBUG_MEMORY_ENABLED'])


	if (env["freetype"]!="no"):
		env.Append(CCFLAGS=['-DFREETYPE_ENABLED'])
		env.Append(CPPPATH=['#tools/freetype'])
		env.Append(CPPPATH=['#tools/freetype/freetype/include'])

	if ("OSXCROSS_ROOT" not in os.environ):
		# regular native build
		if (env["bits"]=="64"):
			env.Append(CCFLAGS=['-arch', 'x86_64'])
			env.Append(LINKFLAGS=['-arch', 'x86_64'])
		else:
			env.Append(CCFLAGS=['-arch', 'i386'])
			env.Append(LINKFLAGS=['-arch', 'i386'])
	else:
		# osxcross build
		root = os.environ.get("OSXCROSS_ROOT", 0)
		arch = "i686" if env["bits"]=="32" else "x86_64"
		basecmd = root + "/target/bin/" + arch + "-apple-" + env["osxcross_sdk"] + "-"

		env['CC'] = basecmd + "cc"
		env['CXX'] = basecmd + "c++"
		env['AR'] = basecmd + "ar"
		env['RANLIB'] = basecmd + "ranlib"
		env['AS'] = basecmd + "as"

#	env.Append(CPPPATH=['#platform/osx/include/freetype2', '#platform/osx/include'])
#	env.Append(LIBPATH=['#platform/osx/lib'])


	#if env['opengl'] == 'yes':
	#	env.Append(CPPFLAGS=['-DOPENGL_ENABLED','-DGLEW_ENABLED'])

	env.Append(CPPFLAGS=["-DAPPLE_STYLE_KEYS"])
	env.Append(CPPFLAGS=['-DUNIX_ENABLED','-DGLES2_ENABLED','-DGLEW_ENABLED', '-DOSX_ENABLED'])
	env.Append(LIBS=['pthread'])
	#env.Append(CPPFLAGS=['-F/Developer/SDKs/MacOSX10.4u.sdk/System/Library/Frameworks', '-isysroot', '/Developer/SDKs/MacOSX10.4u.sdk', '-mmacosx-version-min=10.4'])
	#env.Append(LINKFLAGS=['-mmacosx-version-min=10.4', '-isysroot', '/Developer/SDKs/MacOSX10.4u.sdk', '-Wl,-syslibroot,/Developer/SDKs/MacOSX10.4u.sdk'])
	env.Append(LINKFLAGS=['-framework', 'Cocoa', '-framework', 'OpenGL', '-framework', 'AGL', '-framework', 'AudioUnit','-lz'])

	if (env["CXX"]=="clang++"):
		env.Append(CPPFLAGS=['-DTYPED_METHOD_BIND'])
		env["CC"]="clang"
		env["LD"]="clang++"

	import methods

	env.Append( BUILDERS = { 'GLSL120' : env.Builder(action = methods.build_legacygl_headers, suffix = 'glsl.h',src_suffix = '.glsl') } )
	env.Append( BUILDERS = { 'GLSL' : env.Builder(action = methods.build_glsl_headers, suffix = 'glsl.h',src_suffix = '.glsl') } )
	env.Append( BUILDERS = { 'GLSL120GLES' : env.Builder(action = methods.build_gles2_headers, suffix = 'glsl.h',src_suffix = '.glsl') } )
	#env.Append( BUILDERS = { 'HLSL9' : env.Builder(action = methods.build_hlsl_dx9_headers, suffix = 'hlsl.h',src_suffix = '.hlsl') } )


