import os
import sys


def is_active():
	return True

def get_name():
	return "iOS"

def can_build():

	import sys
	if sys.platform == 'darwin':
		return True

	return False

def get_opts():

	return [
		('IPHONEPLATFORM', 'name of the iphone platform', 'iPhoneOS'),
		('IPHONEPATH', 'the path to iphone toolchain', '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain'),
		('IOS_SDK_VERSION', 'The SDK version', 'iPhoneOS'),
		('IPHONESDK', 'path to the iphone SDK', '/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/${IOS_SDK_VERSION}.sdk/'),
		('game_center', 'Support for game center', 'yes'),
		('store_kit', 'Support for in-app store', 'yes'),
		('ios_gles22_override', 'Force GLES2.0 on iOS', 'yes'),
		('ios_appirater', 'Enable Appirater', 'no'),
		('ios_exceptions', 'Use exceptions when compiling on playbook', 'yes'),
	]

def get_flags():

	return [
		('tools', 'no'),
		('webp', 'yes'),
		('openssl','builtin'), #use builtin openssl
	]



def configure(env):

	env.Append(CPPPATH=['#platform/iphone', '#platform/iphone/include'])

	env['ENV']['PATH'] = env['IPHONEPATH']+"/Developer/usr/bin/:"+env['ENV']['PATH']

#	env['CC'] = '$IPHONEPATH/Developer/usr/bin/gcc'
#	env['CXX'] = '$IPHONEPATH/Developer/usr/bin/g++'
	env['CC'] = '$IPHONEPATH/usr/bin/clang'
	env['CXX'] = '$IPHONEPATH/usr/bin/clang++'
	env['AR'] = 'ar'

	import string
	if (env["bits"]=="64"):
		#env['CCFLAGS'] = string.split('-arch arm64 -fmessage-length=0 -fdiagnostics-show-note-include-stack -fmacro-backtrace-limit=0 -Wno-trigraphs -fpascal-strings -O0 -Wno-missing-field-initializers -Wno-missing-prototypes -Wno-return-type -Wno-non-virtual-dtor -Wno-overloaded-virtual -Wno-exit-time-destructors -Wno-missing-braces -Wparentheses -Wswitch -Wno-unused-function -Wno-unused-label -Wno-unused-parameter -Wno-unused-variable -Wunused-value -Wno-empty-body -Wno-uninitialized -Wno-unknown-pragmas -Wno-shadow -Wno-four-char-constants -Wno-conversion -Wno-constant-conversion -Wno-int-conversion -Wno-bool-conversion -Wno-enum-conversion -Wshorten-64-to-32 -Wno-newline-eof -Wno-c++11-extensions -fstrict-aliasing -Wdeprecated-declarations -Winvalid-offsetof -g -Wno-sign-conversion -miphoneos-version-min=5.1.1 -Wmost -Wno-four-char-constants -Wno-unknown-pragmas -Wno-invalid-offsetof -ffast-math -m64 -DDEBUG -D_DEBUG -MMD -MT dependencies -isysroot $IPHONESDK')
		env['CCFLAGS'] = string.split('-fno-objc-arc -arch arm64 -fmessage-length=0 -fno-strict-aliasing -fdiagnostics-print-source-range-info -fdiagnostics-show-category=id -fdiagnostics-parseable-fixits -Wno-trigraphs -fpascal-strings -Wmissing-prototypes -Wreturn-type -Wparentheses -Wswitch -Wno-unused-parameter -Wunused-variable -Wunused-value -Wno-shorten-64-to-32 -gdwarf-2 -fvisibility=hidden -Wno-sign-conversion -MMD -MT dependencies -miphoneos-version-min=5.1.1 -isysroot $IPHONESDK')		
		env.Append(CPPFLAGS=['-DNEED_LONG_INT'])
		env.Append(CPPFLAGS=['-DLIBYUV_DISABLE_NEON'])
	else:
		env['CCFLAGS'] = string.split('-fno-objc-arc -arch armv7 -fmessage-length=0 -fno-strict-aliasing -fdiagnostics-print-source-range-info -fdiagnostics-show-category=id -fdiagnostics-parseable-fixits -Wno-trigraphs -fpascal-strings -Wmissing-prototypes -Wreturn-type -Wparentheses -Wswitch -Wno-unused-parameter -Wunused-variable -Wunused-value -Wno-shorten-64-to-32 -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.sdk -gdwarf-2 -fvisibility=hidden -Wno-sign-conversion -mthumb "-DIBOutlet=__attribute__((iboutlet))" "-DIBOutletCollection(ClassName)=__attribute__((iboutletcollection(ClassName)))" "-DIBAction=void)__attribute__((ibaction)" -miphoneos-version-min=4.3 -MMD -MT dependencies -isysroot $IPHONESDK')

	if (env["bits"]=="64"):
		env.Append(LINKFLAGS=['-arch', 'arm64', '-Wl,-dead_strip', '-miphoneos-version-min=5.1.1',
							'-isysroot', '$IPHONESDK',
							#'-stdlib=libc++',
							'-framework', 'Foundation',
							'-framework', 'UIKit',
							'-framework', 'CoreGraphics',
							'-framework', 'OpenGLES',
							'-framework', 'QuartzCore',
							'-framework', 'CoreAudio',
							'-framework', 'AudioToolbox',
							'-framework', 'SystemConfiguration',
							'-framework', 'Security',
							#'-framework', 'AdSupport',
							'-framework', 'MediaPlayer',
							'-framework', 'AVFoundation',
							'-framework', 'CoreMedia',
							])
	else:
		env.Append(LINKFLAGS=['-arch', 'armv7', '-Wl,-dead_strip', '-miphoneos-version-min=4.3',
							'-isysroot', '$IPHONESDK',
							'-framework', 'Foundation',
							'-framework', 'UIKit',
							'-framework', 'CoreGraphics',
							'-framework', 'OpenGLES',
							'-framework', 'QuartzCore',
							'-framework', 'CoreAudio',
							'-framework', 'AudioToolbox',
							'-framework', 'SystemConfiguration',
							'-framework', 'Security',
							#'-framework', 'AdSupport',
							'-framework', 'MediaPlayer',
							'-framework', 'AVFoundation',
							'-framework', 'CoreMedia',
							])

	if env['game_center'] == 'yes':
		env.Append(CPPFLAGS=['-fblocks', '-DGAME_CENTER_ENABLED'])
		env.Append(LINKFLAGS=['-framework', 'GameKit'])

	if env['store_kit'] == 'yes':
		env.Append(CPPFLAGS=['-DSTOREKIT_ENABLED'])
		env.Append(LINKFLAGS=['-framework', 'StoreKit'])

	env.Append(CPPPATH = ['$IPHONESDK/usr/include', '$IPHONESDK/System/Library/Frameworks/OpenGLES.framework/Headers', '$IPHONESDK/System/Library/Frameworks/AudioUnit.framework/Headers'])

	if (env["target"]=="release"):

		env.Append(CCFLAGS=['-O3', '-ffast-math', '-DNS_BLOCK_ASSERTIONS=1'])
		env.Append(LINKFLAGS=['-O3', '-ffast-math'])

	elif env["target"] == "release_debug":
		env.Append(CCFLAGS=['-Os', '-ffast-math', '-DNS_BLOCK_ASSERTIONS=1','-DDEBUG_ENABLED'])
		env.Append(LINKFLAGS=['-Os', '-ffast-math'])
		env.Append(CPPFLAGS=['-DDEBUG_MEMORY_ENABLED'])

	elif (env["target"]=="debug"):

		env.Append(CCFLAGS=['-D_DEBUG', '-DDEBUG=1', '-gdwarf-2', '-O0', '-DDEBUG_ENABLED'])
		env.Append(CPPFLAGS=['-DDEBUG_MEMORY_ENABLED'])

	elif (env["target"]=="profile"):

		env.Append(CCFLAGS=['-g','-pg', '-Os', '-ffast-math'])
		env.Append(LINKFLAGS=['-pg'])


	env['ENV']['CODESIGN_ALLOCATE'] = '/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/codesign_allocate'
	env.Append(CPPFLAGS=['-DIPHONE_ENABLED', '-DUNIX_ENABLED', '-DGLES2_ENABLED', '-DMPC_FIXED_POINT'])
	if env['ios_exceptions'] == 'yes':
		env.Append(CPPFLAGS=['-fexceptions'])
	else:
		env.Append(CPPFLAGS=['-fno-exceptions'])
	#env['neon_enabled']=True
	env['S_compiler'] = '$IPHONEPATH/Developer/usr/bin/gcc'
	
	import methods
	env.Append( BUILDERS = { 'GLSL120' : env.Builder(action = methods.build_legacygl_headers, suffix = 'glsl.h',src_suffix = '.glsl') } )
	env.Append( BUILDERS = { 'GLSL' : env.Builder(action = methods.build_glsl_headers, suffix = 'glsl.h',src_suffix = '.glsl') } )
	env.Append( BUILDERS = { 'GLSL120GLES' : env.Builder(action = methods.build_gles2_headers, suffix = 'glsl.h',src_suffix = '.glsl') } )


