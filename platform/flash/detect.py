import os
import sys
import string

def is_active():
	return True
	
def get_name():
	return "Flash"

def can_build():

	#import os
	if (not os.environ.has_key("FLASCC_ROOT")):
		return False
	return True

def get_opts():

	return []
	

def get_flags():

	return [
		('lua', 'no'),
		('tools', 'no'),
		('nedmalloc', 'no'),
		('theora', 'no'),
		('squirrel', 'yes'),
		('gdscript', 'yes'),
		('minizip', 'no'),
		('vorbis', 'yes'),
		('squish', 'yes'),
		('speex', 'yes'),
		('xml', 'yes'),
		('png', 'yes'),
		('jpg', 'yes'),
		('webp', 'yes'),
		('dds', 'yes'),
		('pvr', 'yes'),
		('musepack', 'yes'),
		('default_gui_theme', 'yes'),
		('old_scenes', 'no'),
	]


def configure(env):

	ccroot = os.environ["FLASCC_ROOT"]

	if (ccroot.find("/cygdrive")==0):
		unit = ccroot[ ccroot.find("/") + 1 ]
		ccroot=ccroot.replace("/cygdrive/"+unit,unit+":")
		
	env['FLASCC_ROOT'] = ccroot
	if env['PLATFORM'] == 'win32':
		import methods
		env.Tool('mingw')
		#env['SPAWN'] = methods.win32_spawn

	env['ENV']['PATH'] = os.environ["PATH"];
	env.PrependENVPath('PATH', env['FLASCC_ROOT']+'/usr/bin')
	#os.environ['PATH'] = env['ENV']['PATH']

	env.Append(CPPPATH=['#platform/flash', '#platform/flash/include'])
	env.Append(LIBPATH=['#platform/flash/lib'])

	env['CC'] = ccroot+'/sdk/usr/bin/gcc'
	env['CXX'] = ccroot+'/sdk/usr/bin/g++'
	env['AR'] = ccroot+'/sdk/usr/bin/ar'
	env['LINK'] = ccroot+'/sdk/usr/bin/gcc'

	
	env['OBJSUFFIX'] = ".fl.o"
	env['LIBSUFFIX'] = ".fl.a"
	env['PROGSUFFIX'] = "_flash"
	
	#env["CXX"]='gcc-4'
	import string
	#include path
	env['CCFLAGS'] = string.split('-fno-strict-aliasing -fno-rtti -fno-common -finline-limit=30000 -fno-exceptions -DNO_SAFE_CAST -DNO_THREADS -DNO_NETWORK -DNO_STATVFS')

	#env.Append(LDPATH=[ld_path])
	env.Append(LIBS=['m', 'Flash++', 'AS3++', 'GL'])

	env.Append(LINKFLAGS=['-symbol-abc=platform/flash/Console.abc'])
	#env["LINKFLAGS"]= string.split(" -g --sysroot="+ld_sysroot+" -Wl,--no-undefined -Wl,-z,noexecstack -lsupc++ ")

	#env.Append(CXXFLAGS=['-fno-access-control'])

	if (env["target"]=="release"):

		env.Append(CCFLAGS=['-O4', '-ffast-math','-fomit-frame-pointer'])
		env['OBJSUFFIX'] = ".fo.o"
		env['LIBSUFFIX'] = ".fo.a"
		env['PROGSUFFIX'] = "_opt_flash"

	elif (env["target"]=="debug"):

		env.Append(CCFLAGS=['-D_DEBUG', '-g0', '-O0', '-DDEBUG_ENABLED'])
		env.Append(CPPFLAGS=['-DDEBUG_MEMORY_ALLOC'])

	env.Append(CPPFLAGS=['-DFLASH_ENABLED', '-DGLES1_ENABLED', '-DNO_FCNTL', '-DUNIX_ENABLED'])
#	env.Append(CPPFLAGS=['-DANDROID_ENABLED', '-DUNIX_ENABLED','-DMPC_FIXED_POINT'])
