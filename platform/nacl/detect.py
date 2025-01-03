import os
import sys

def is_active():
	return True
	
def get_name():
	return "NaCl"

def can_build():

	import os
	if not os.environ.has_key("NACLPATH"):
		return False
	return True

def get_opts():

	return [
		('NACLPATH', 'the path to nacl', os.environ.get("NACLPATH", 0)),
		('nacl_arch', 'The architecture for Nacl build (can be i686 or x86_64', 'i686'),
	]

def get_flags():

	return [
		('nedmalloc', 'no'),
		('tools', 'no'),
	]



def configure(env):

	env.Append(CPPPATH=['#platform/nacl'])

	env['OBJSUFFIX'] = ".nacl.${nacl_arch}.o"
	env['LIBSUFFIX'] = ".nacl.${nacl_arch}.a"
	env['PROGSUFFIX'] = ".${nacl_arch}.nexe"

	env['ENV']['PATH'] = env['ENV']['PATH']+":"+env['NACLPATH']+"/toolchain/linux_x86_newlib/bin"

	env['CC'] = '${nacl_arch}-nacl-gcc'
	env['CXX'] = '${nacl_arch}-nacl-g++'
	env['AR'] = '${nacl_arch}-nacl-ar'

	env.Append(CCFLAGS=['-fexceptions', '-Wno-long-long', '-pthread', '-DXP_UNIX'])

	env.Append(CPPPATH=env['NACLPATH'])

	if (env["target"]=="release"):

		env.Append(CCFLAGS=['-O2','-ffast-math','-fomit-frame-pointer', '-ffunction-sections', '-fdata-sections', '-fno-default-inline'])

	elif (env["target"]=="debug"):

		env.Append(CCFLAGS=['-g', '-O0', '-DDEBUG_ENABLED'])


	elif (env["target"]=="profile"):

		env.Append(CCFLAGS=['-g','-pg'])
		env.Append(LINKFLAGS=['-pg'])

	env.Append(CCFLAGS=['-DNACL_ENABLED', '-DGLES2_ENABLED'])

	env.Append(LIBFLAGS=['m32'])
	env.Append(LIBS=env.Split('ppapi ppapi_cpp pthread srpc ppapi_gles22'))

	import methods
	env.Append( BUILDERS = { 'GLSL120GLES' : env.Builder(action = methods.build_gles2_headers, suffix = 'glsl.h',src_suffix = '.glsl') } )
