# 
# 	tested on               | Windows native    | Linux cross-compilation
#	------------------------+-------------------+---------------------------
#	MSVS C++ 2010 Express   | WORKS             | n/a
#	Mingw-w64               | WORKS             | WORKS
#	Mingw-w32               | WORKS             | WORKS
#	MinGW                   | WORKS             | untested
#
#####
# Notes about MSVS C++ :
#
# 	- MSVC2010-Express compiles to 32bits only.
#
#####
# Notes about Mingw-w64 and Mingw-w32 under Windows :
#
#	- both can be installed using the official installer :
#		http://mingw-w64.sourceforge.net/download.php#mingw-builds
#
#	- if you want to compile both 32bits and 64bits, don't forget to
#	run the installer twice to install them both.
#
#	- install them into a path that does not contain spaces
#		( example : "C:/Mingw-w32", "C:/Mingw-w64" )
#
#	- if you want to compile faster using the "-j" option, don't forget
#	to install the appropriate version of the Pywin32 python extension 
#	available from : http://sourceforge.net/projects/pywin32/files/
#
#	- before running scons, you must add into the environment path 
#	the path to the "/bin" directory of the Mingw version you want 
#	to use :
#
#		set PATH=C:/Mingw-w32/bin;%PATH%
#
#	- then, scons should be able to detect gcc.
#	- Mingw-w32 only compiles 32bits.
#	- Mingw-w64 only compiles 64bits.
#
#	- it is possible to add them both at the same time into the PATH env, 
#	if you also define the MINGW32_PREFIX and MINGW64_PREFIX environment 
#	variables. 
#	For instance, you could store that set of commands into a .bat script
#	that you would run just before scons :
#
#			set PATH=C:\mingw-w32\bin;%PATH%
#			set PATH=C:\mingw-w64\bin;%PATH%
#			set MINGW32_PREFIX=C:\mingw-w32\bin\
#			set MINGW64_PREFIX=C:\mingw-w64\bin\
#
#####
# Notes about Mingw, Mingw-w64 and Mingw-w32 under Linux :
#
#	- default toolchain prefixes are :
#		"i586-mingw32msvc-" for MinGW
#		"i686-w64-mingw32-"	for Mingw-w32
#		"x86_64-w64-mingw32-" for Mingw-w64
#
#	- if both MinGW and Mingw-w32 are installed on your system
#	Mingw-w32 should take the priority over MinGW.
#
#	- it is possible to manually override prefixes by defining
#	the MINGW32_PREFIX and MINGW64_PREFIX environment variables.
#	
#####
# Notes about Mingw under Windows :
#
#	- this is the MinGW version from http://mingw.org/ 
#	- install it into a path that does not contain spaces
#		( example : "C:/MinGW" )
#	- several DirectX headers might be missing. You can copy them into 
#	the C:/MinGW/include" directory from this page :
#	 https://code.google.com/p/mingw-lib/source/browse/trunk/working/avcodec_to_widget_5/directx_include/
#	- before running scons, add the path to the "/bin" directory :
#		set PATH=C:/MinGW/bin;%PATH%
#	- scons should be able to detect gcc.
#

#####
# TODO :
#
#	- finish to cleanup this script to remove all the remains of previous hacks and workarounds
#	- make it work with the Windows7 SDK that is supposed to enable 64bits compilation for MSVC2010-Express
#	- confirm it works well with other Visual Studio versions.
#	- update the wiki about the pywin32 extension required for the "-j" option under Windows.
#	- update the wiki to document MINGW32_PREFIX and MINGW64_PREFIX
# 	

import os

import sys	


def is_active():
	return True
        
def get_name():
        return "Windows"

def can_build():
	
	if (os.name=="nt"):
		#building natively on windows!
		if (os.getenv("VSINSTALLDIR")):
			return True 
		else:
			print("\nMSVC not detected, attempting Mingw.")
			mingw32 = ""
			mingw64 = ""
			if ( os.getenv("MINGW32_PREFIX") ) :
				mingw32 = os.getenv("MINGW32_PREFIX")
			if ( os.getenv("MINGW64_PREFIX") ) :
				mingw64 = os.getenv("MINGW64_PREFIX")
				
			test = "gcc --version > NUL 2>&1"
			if os.system(test)!= 0 and os.system(mingw32+test)!=0 and os.system(mingw64+test)!=0 :
				print("- could not detect gcc.")
				print("Please, make sure a path to a Mingw /bin directory is accessible into the environment PATH.\n")
				return False
			else:
				print("- gcc detected.")
				
			return True
			
	if (os.name=="posix"):

		mingw = "i586-mingw32msvc-"
		mingw64 = "x86_64-w64-mingw32-"
		mingw32 = "i686-w64-mingw32-"
		
		if (os.getenv("MINGW32_PREFIX")):
			mingw32=os.getenv("MINGW32_PREFIX")
			mingw = mingw32
		if (os.getenv("MINGW64_PREFIX")):
			mingw64=os.getenv("MINGW64_PREFIX")
			
		test = "gcc --version >/dev/null"
		if os.system(mingw+test) == 0 or os.system(mingw64+test) ==0 or os.system(mingw32+test) ==0 :
			return True
			
	return False
		
def get_opts():

	mingw=""
	mingw32=""
	mingw64=""
	if ( os.name == "posix" ):
		mingw = "i586-mingw32msvc-"
		mingw32 = "i686-w64-mingw32-"
		mingw64 = "x86_64-w64-mingw32-"
		
		if os.system(mingw32+"gcc --version >/dev/null") != 0 :
			mingw32 = mingw
	
	if (os.getenv("MINGW32_PREFIX")):
		mingw32=os.getenv("MINGW32_PREFIX")
		mingw = mingw32
	if (os.getenv("MINGW64_PREFIX")):
		mingw64=os.getenv("MINGW64_PREFIX")


	return [
		('mingw_prefix','Mingw Prefix',mingw32),
		('mingw_prefix_64','Mingw Prefix 64 bits',mingw64),
	]
  
def get_flags():

	return [
		('freetype','builtin'), #use builtin freetype
		('openssl','builtin'), #use builtin openssl
		('theora','no'),
	]
			


def configure(env):

	env.Append(CPPPATH=['#platform/windows'])


	if (os.name=="nt" and os.getenv("VSINSTALLDIR")!=None):
		#build using visual studio
		env['ENV']['TMP'] = os.environ['TMP']
		env.Append(CPPPATH=['#platform/windows/include'])
		env.Append(LIBPATH=['#platform/windows/lib'])

		if (env["freetype"]!="no"):
			env.Append(CCFLAGS=['/DFREETYPE_ENABLED'])
			env.Append(CPPPATH=['#tools/freetype'])
			env.Append(CPPPATH=['#tools/freetype/freetype/include'])

		if (env["target"]=="release"):

			env.Append(CCFLAGS=['/O2'])
			env.Append(LINKFLAGS=['/SUBSYSTEM:WINDOWS'])
			env.Append(LINKFLAGS=['/ENTRY:mainCRTStartup'])

		elif (env["target"]=="release_debug"):

			env.Append(CCFLAGS=['/O2','/DDEBUG_ENABLED'])
			env.Append(LINKFLAGS=['/SUBSYSTEM:CONSOLE'])

		elif (env["target"]=="debug"):

			env.Append(CCFLAGS=['/Zi','/DDEBUG_ENABLED','/DDEBUG_MEMORY_ENABLED','/DD3D_DEBUG_INFO','/Od'])
			env.Append(LINKFLAGS=['/SUBSYSTEM:CONSOLE'])
			env.Append(LINKFLAGS=['/DEBUG'])


		env.Append(CCFLAGS=['/MT','/Gd','/GR','/nologo'])
		env.Append(CXXFLAGS=['/TP'])
		env.Append(CPPFLAGS=['/DMSVC', '/GR', ])
		env.Append(CCFLAGS=['/I'+os.getenv("WindowsSdkDir")+"/Include"])
		env.Append(CCFLAGS=['/DWINDOWS_ENABLED'])
		env.Append(CCFLAGS=['/DRTAUDIO_ENABLED'])
		env.Append(CCFLAGS=['/DWIN32'])
		env.Append(CCFLAGS=['/DTYPED_METHOD_BIND'])

		env.Append(CCFLAGS=['/DGLES2_ENABLED'])

		env.Append(CCFLAGS=['/DGLEW_ENABLED'])
		LIBS=['winmm','opengl32','dsound','kernel32','ole32','user32','gdi32', 'IPHLPAPI','Shlwapi', 'wsock32', 'shell32','advapi32']
		env.Append(LINKFLAGS=[p+env["LIBSUFFIX"] for p in LIBS])
		
		env.Append(LIBPATH=[os.getenv("WindowsSdkDir")+"/Lib"])
                if (os.getenv("DXSDK_DIR")):
                        DIRECTX_PATH=os.getenv("DXSDK_DIR")
                else:
                        DIRECTX_PATH="C:/Program Files/Microsoft DirectX SDK (March 2009)"

                if (os.getenv("VCINSTALLDIR")):
                        VC_PATH=os.getenv("VCINSTALLDIR")
                else:
                        VC_PATH=""

		env.Append(CCFLAGS=["/I" + p for p in os.getenv("INCLUDE").split(";")])
		env.Append(LIBPATH=[p for p in os.getenv("LIB").split(";")])
		env.Append(CCFLAGS=["/I"+DIRECTX_PATH+"/Include"])
		env.Append(LIBPATH=[DIRECTX_PATH+"/Lib/x86"])
		env['ENV'] = os.environ;
	else:

		# Workaround for MinGW. See:
		# http://www.scons.org/wiki/LongCmdLinesOnWin32
		if (os.name=="nt"):
			import subprocess
			
			def mySubProcess(cmdline,env):
				#print "SPAWNED : " + cmdline
				startupinfo = subprocess.STARTUPINFO()
				startupinfo.dwFlags |= subprocess.STARTF_USESHOWWINDOW
				proc = subprocess.Popen(cmdline, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
					stderr=subprocess.PIPE, startupinfo=startupinfo, shell = False, env = env)
				data, err = proc.communicate()
				rv = proc.wait()
				if rv:
					print "====="
					print err
					print "====="
				return rv
				
			def mySpawn(sh, escape, cmd, args, env):
								
				newargs = ' '.join(args[1:])
				cmdline = cmd + " " + newargs
				
				rv=0
				if len(cmdline) > 32000 and cmd.endswith("ar") :
					cmdline = cmd + " " + args[1] + " " + args[2] + " "
					for i in range(3,len(args)) :
						rv = mySubProcess( cmdline + args[i], env )
						if rv :
							break	
				else:				
					rv = mySubProcess( cmdline, env )
					
				return rv
				
			env['SPAWN'] = mySpawn

		#build using mingw
		if (os.name=="nt"):
			env['ENV']['TMP'] = os.environ['TMP'] #way to go scons, you can be so stupid sometimes
		else:
			env["PROGSUFFIX"]=env["PROGSUFFIX"]+".exe" # for linux cross-compilation

		mingw_prefix=""

		if (env["bits"]=="default"):
			env["bits"]="32"

		if (env["bits"]=="32"):
			env.Append(LINKFLAGS=['-static'])
			env.Append(LINKFLAGS=['-static-libgcc'])
			env.Append(LINKFLAGS=['-static-libstdc++'])
			mingw_prefix=env["mingw_prefix"];
		else:
			env.Append(LINKFLAGS=['-static'])
			mingw_prefix=env["mingw_prefix_64"];

		nulstr=""

		if (os.name=="posix"):
		    nulstr=">/dev/null"
		else:
		    nulstr=">nul"



		# if os.system(mingw_prefix+"gcc --version"+nulstr)!=0:
			# #not really super consistent but..
			# print("Can't find Windows compiler: "+mingw_prefix)
			# sys.exit(255)

		if (env["target"]=="release"):
			
			env.Append(CCFLAGS=['-O3','-ffast-math','-fomit-frame-pointer','-msse2'])
			env.Append(LINKFLAGS=['-Wl,--subsystem,windows'])

		elif (env["target"]=="release_debug"):

			env.Append(CCFLAGS=['-O2','-DDEBUG_ENABLED'])

		elif (env["target"]=="debug"):
					
			env.Append(CCFLAGS=['-g', '-DDEBUG_ENABLED','-DDEBUG_MEMORY_ENABLED'])

		if (env["freetype"]!="no"):
			env.Append(CCFLAGS=['-DFREETYPE_ENABLED'])
			env.Append(CPPPATH=['#tools/freetype'])
			env.Append(CPPPATH=['#tools/freetype/freetype/include'])

		env["CC"]=mingw_prefix+"gcc"
		env['AS']=mingw_prefix+"as"
		env['CXX'] = mingw_prefix+"g++"
		env['AR'] = mingw_prefix+"ar"
		env['RANLIB'] = mingw_prefix+"ranlib"
		env['LD'] = mingw_prefix+"g++"

		#env['CC'] = "winegcc"
		#env['CXX'] = "wineg++"

		env.Append(CCFLAGS=['-DWINDOWS_ENABLED','-mwindows'])
		env.Append(CPPFLAGS=['-DRTAUDIO_ENABLED'])
		env.Append(CCFLAGS=['-DGLES2_ENABLED','-DGLEW_ENABLED'])
		env.Append(LIBS=['mingw32','opengl32', 'dsound', 'ole32', 'd3d9','winmm','gdi32','iphlpapi','shlwapi','wsock32','kernel32'])

		# if (env["bits"]=="32"):
# #			env.Append(LIBS=['gcc_s'])
			# #--with-arch=i686
			# env.Append(CPPFLAGS=['-march=i686'])
			# env.Append(LINKFLAGS=['-march=i686'])




		#'d3dx9d'
		env.Append(CPPFLAGS=['-DMINGW_ENABLED'])
		env.Append(LINKFLAGS=['-g'])

	import methods
	env.Append( BUILDERS = { 'GLSL120' : env.Builder(action = methods.build_legacygl_headers, suffix = 'glsl.h',src_suffix = '.glsl') } )
	env.Append( BUILDERS = { 'GLSL' : env.Builder(action = methods.build_glsl_headers, suffix = 'glsl.h',src_suffix = '.glsl') } )
	env.Append( BUILDERS = { 'HLSL9' : env.Builder(action = methods.build_hlsl_dx9_headers, suffix = 'hlsl.h',src_suffix = '.hlsl') } )
	env.Append( BUILDERS = { 'GLSL120GLES' : env.Builder(action = methods.build_gles2_headers, suffix = 'glsl.h',src_suffix = '.glsl') } )

	

