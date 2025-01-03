import os
import sys
import string
import platform

def is_active():
	return True
	
def get_name():
	return "Android"

def can_build():

        import os
        if (not os.environ.has_key("ANDROID_NDK_ROOT")):
        	return False

	return True

def get_opts():

	return [
	     ('ANDROID_NDK_ROOT', 'the path to Android NDK', os.environ.get("ANDROID_NDK_ROOT", 0)), 
             ('NDK_TOOLCHAIN', 'toolchain to use for the NDK',"arm-eabi-4.4.0"), 	                      
             #android 2.3       
		 ('ndk_platform', 'compile for platform: (2.2,2.3)',"2.2"),
		 ('NDK_TARGET', 'toolchain to use for the NDK',"arm-linux-androideabi-4.8"),
		('android_stl','enable STL support in android port (for modules)','no'),
		('armv6','compile for older phones running arm v6 (instead of v7+neon+smp)','no'),
		 ('x86','Xompile for Android-x86','no')

	]

def get_flags():

	return [
		('tools', 'no'),
		('nedmalloc', 'no'),
		('builtin_zlib', 'no'),
                ('openssl','builtin'), #use builtin openssl
		('theora','no'), #use builtin openssl

        ]


def create(env):
	tools = env['TOOLS']
	if "mingw" in tools:
		tools.remove('mingw')
	if "applelink" in tools:
		tools.remove("applelink")
		env.Tool('gcc')
	return env.Clone(tools=tools);

def configure(env):

	if env['x86']=='yes':
		env['NDK_TARGET']='x86-4.8'

	if env['PLATFORM'] == 'win32':
		import methods
		env.Tool('gcc')
		env['SPAWN'] = methods.win32_spawn
		env['SHLIBSUFFIX'] = '.so'

#	env.android_source_modules.append("../libs/apk_expansion")	
	env.android_source_modules.append("../libs/google_play_services")	
	env.android_source_modules.append("../libs/downloader_library")	
	env.android_source_modules.append("../libs/play_licensing")	
	
	ndk_platform=""

	ndk_platform="android-15"

	print("Godot Android!!!!!")

	env.Append(CPPPATH=['#platform/android'])
	
	if env['x86']=='yes':
		env.extra_suffix=".x86"
	
	gcc_path=env["ANDROID_NDK_ROOT"]+"/toolchains/"+env["NDK_TARGET"]+"/prebuilt/";
	
	import os
	if (sys.platform.find("linux")==0):
		if (platform.architecture()[0]=='64bit' or os.path.isdir(gcc_path+"linux-x86_64/bin")): # check was not working
			gcc_path=gcc_path+"/linux-x86_64/bin"
		else:
			gcc_path=gcc_path+"/linux-x86/bin"
	elif (sys.platform=="darwin"):
		gcc_path=gcc_path+"/darwin-x86_64/bin" #this may be wrong
		env['SHLINKFLAGS'][1] = '-shared'
	elif (os.name=="nt"):
		gcc_path=gcc_path+"/windows/bin" #this may be wrong
	
	

	env['ENV']['PATH'] = gcc_path+":"+env['ENV']['PATH']
	if env['x86']=='yes':
		env['CC'] = gcc_path+'/i686-linux-android-gcc'
		env['CXX'] = gcc_path+'/i686-linux-android-g++'
		env['AR'] = gcc_path+"/i686-linux-android-ar"
		env['RANLIB'] = gcc_path+"/i686-linux-android-ranlib"
		env['AS'] = gcc_path+"/i686-linux-android-as"
	else:
		env['CC'] = gcc_path+'/arm-linux-androideabi-gcc'
		env['CXX'] = gcc_path+'/arm-linux-androideabi-g++'
		env['AR'] = gcc_path+"/arm-linux-androideabi-ar"
		env['RANLIB'] = gcc_path+"/arm-linux-androideabi-ranlib"
		env['AS'] = gcc_path+"/arm-linux-androideabi-as"

	if env['x86']=='yes':
		env['ARCH'] = 'arch-x86'
	else:
		env['ARCH'] = 'arch-arm'

	import string
	#include path
	gcc_include=env["ANDROID_NDK_ROOT"]+"/platforms/"+ndk_platform+"/"+env['ARCH'] +"/usr/include"
	ld_sysroot=env["ANDROID_NDK_ROOT"]+"/platforms/"+ndk_platform+"/"+env['ARCH']
	#glue_include=env["ANDROID_NDK_ROOT"]+"/sources/android/native_app_glue"
	ld_path=env["ANDROID_NDK_ROOT"]+"/platforms/"+ndk_platform+"/"+env['ARCH']+"/usr/lib"
	env.Append(CPPPATH=[gcc_include])
#	env['CCFLAGS'] = string.split('-DNO_THREADS -MMD -MP -MF -fpic -ffunction-sections -funwind-tables -fstack-protector -D__ARM_ARCH_5__ -D__ARM_ARCH_5T__ -D__ARM_ARCH_5E__ -D__ARM_ARCH_5TE__  -Wno-psabi -march=armv5te -mtune=xscale -msoft-float  -fno-exceptions -mthumb -fno-strict-aliasing -DANDROID -Wa,--noexecstack -DGLES2_ENABLED ')

	if env['x86']=='yes':
		env['CCFLAGS'] = string.split('-DNO_STATVFS -MMD -MP -MF -fpic -ffunction-sections -funwind-tables -fstack-protector -D__GLIBC__  -Wno-psabi -ftree-vectorize -funsafe-math-optimizations -fno-strict-aliasing -DANDROID -Wa,--noexecstack -DGLES2_ENABLED -DGLES1_ENABLED')
	elif env["armv6"]!="no":
		env['CCFLAGS'] = string.split('-DNO_STATVFS -MMD -MP -MF -fpic -ffunction-sections -funwind-tables -fstack-protector -D__ARM_ARCH_6__ -D__GLIBC__  -Wno-psabi -march=armv6 -mfpu=vfp -mfloat-abi=softfp -funsafe-math-optimizations -fno-strict-aliasing -DANDROID -Wa,--noexecstack -DGLES2_ENABLED -DGLES1_ENABLED')
	else:
		env['CCFLAGS'] = string.split('-DNO_STATVFS -MMD -MP -MF -fpic -ffunction-sections -funwind-tables -fstack-protector -D__ARM_ARCH_7__ -D__GLIBC__  -Wno-psabi -march=armv6 -mfpu=neon -mfloat-abi=softfp -ftree-vectorize -funsafe-math-optimizations -fno-strict-aliasing -DANDROID -Wa,--noexecstack -DGLES2_ENABLED -DGLES1_ENABLED')

	env.Append(LDPATH=[ld_path])
	env.Append(LIBS=['OpenSLES'])
#	env.Append(LIBS=['c','m','stdc++','log','EGL','GLESv1_CM','GLESv2','OpenSLES','supc++','android'])
	if (env["ndk_platform"]!="2.2"):
		env.Append(LIBS=['EGL','OpenSLES','android'])
	env.Append(LIBS=['c','m','stdc++','log','GLESv1_CM','GLESv2', 'z'])

	env["LINKFLAGS"]= string.split(" -g --sysroot="+ld_sysroot+" -Wl,--no-undefined -Wl,-z,noexecstack ")
	env.Append(LINKFLAGS=["-Wl,-soname,libgodot_android.so"])

	if (env["target"]=="release"):

		env.Append(CCFLAGS=['-O2', '-ffast-math','-fomit-frame-pointer'])

	elif (env["target"]=="release_debug"):

		env.Append(CCFLAGS=['-O2', '-ffast-math','-DDEBUG_ENABLED'])

	elif (env["target"]=="debug"):

		env.Append(CCFLAGS=['-D_DEBUG', '-g1', '-O0', '-DDEBUG_ENABLED'])
		env.Append(CPPFLAGS=['-DDEBUG_MEMORY_ALLOC'])

	if env["armv6"] == "no" and env['x86'] != 'yes':
		env['neon_enabled']=True

	env.Append(CPPFLAGS=['-DANDROID_ENABLED', '-DUNIX_ENABLED', '-DNO_FCNTL','-DMPC_FIXED_POINT'])
#	env.Append(CPPFLAGS=['-DANDROID_ENABLED', '-DUNIX_ENABLED','-DMPC_FIXED_POINT'])

	if (env['android_stl']=='yes'):
		#env.Append(CCFLAGS=[env["ANDROID_NDK_ROOT"]+"/sources/cxx-stl/system/include"])
		env.Append(CPPPATH=[env["ANDROID_NDK_ROOT"]+"/sources/cxx-stl/gnu-libstdc++/4.4.3/include"])
		env.Append(CPPPATH=[env["ANDROID_NDK_ROOT"]+"/sources/cxx-stl/gnu-libstdc++/4.4.3/libs/armeabi/include"])
		env.Append(LIBPATH=[env["ANDROID_NDK_ROOT"]+"/sources/cxx-stl/gnu-libstdc++/4.4.3/libs/armeabi"])
		env.Append(LIBS=["gnustl_static","supc++"])
		env.Append(CPPPATH=[env["ANDROID_NDK_ROOT"]+"/sources/cpufeatures"])

		#env.Append(CCFLAGS=["-I"+env["ANDROID_NDK_ROOT"]+"/sources/cxx-stl/stlport/stlport"])
		#env.Append(CCFLAGS=["-I"+env["ANDROID_NDK_ROOT"]+"/sources/cxx-stl/gnu-libstdc++/libs/armeabi/include"])
		#env.Append(LINKFLAGS=[env["ANDROID_NDK_ROOT"]+"/sources/cxx-stl/gnu-libstdc++/libs/armeabi/libstdc++.a"])
	else:

		env.Append(CPPPATH=[env["ANDROID_NDK_ROOT"]+"/sources/cxx-stl/gabi++/include"])
		env.Append(CPPPATH=[env["ANDROID_NDK_ROOT"]+"/sources/cpufeatures"])
		if env['x86']=='yes':
			env.Append(LIBPATH=[env["ANDROID_NDK_ROOT"]+"/sources/cxx-stl/gabi++/libs/x86"])
		else:
			env.Append(LIBPATH=[env["ANDROID_NDK_ROOT"]+"/sources/cxx-stl/gabi++/libs/armeabi"])
		env.Append(LIBS=['gabi++_static'])
		env.Append(CCFLAGS=["-fno-exceptions",'-DNO_SAFE_CAST'])

	import methods
	env.Append( BUILDERS = { 'GLSL120' : env.Builder(action = methods.build_legacygl_headers, suffix = 'glsl.h',src_suffix = '.glsl') } )
	env.Append( BUILDERS = { 'GLSL' : env.Builder(action = methods.build_glsl_headers, suffix = 'glsl.h',src_suffix = '.glsl') } )
	env.Append( BUILDERS = { 'GLSL120GLES' : env.Builder(action = methods.build_gles2_headers, suffix = 'glsl.h',src_suffix = '.glsl') } )
