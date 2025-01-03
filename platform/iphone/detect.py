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
		('ios_GLES1_override', 'Force legacy GLES (1.1) on iOS', 'no'),
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
	#env['CCFLAGS'] = string.split('-arch armv7 -fno-strict-aliasing -fno-common -D__IPHONE_OS_VERSION_MIN_REQUIRED=20000 -isysroot $IPHONESDK -fvisibility=hidden -mmacosx-version-min=10.5 -DCUSTOM_MATRIX_TRANSFORM_H=\\\"build/iphone/matrix4_iphone.h\\\" -DCUSTOM_VECTOR3_TRANSFORM_H=\\\"build/iphone/vector3_iphone.h\\\" -DNO_THUMB')
	env['CCFLAGS'] = string.split('-fno-objc-arc -arch armv7 -fmessage-length=0 -fno-strict-aliasing -fdiagnostics-print-source-range-info -fdiagnostics-show-category=id -fdiagnostics-parseable-fixits -Wno-trigraphs -fpascal-strings -Wmissing-prototypes -Wreturn-type -Wparentheses -Wswitch -Wno-unused-parameter -Wunused-variable -Wunused-value -Wno-shorten-64-to-32 -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.sdk -gdwarf-2 -fvisibility=hidden -Wno-sign-conversion -mthumb "-DIBOutlet=__attribute__((iboutlet))" "-DIBOutletCollection(ClassName)=__attribute__((iboutletcollection(ClassName)))" "-DIBAction=void)__attribute__((ibaction)" -miphoneos-version-min=4.3 -MMD -MT dependencies -isysroot $IPHONESDK')


#/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/clang++ fno-objc-arc -arch armv7 -fmessage-length=0 -fdiagnostics-print-source-range-info -fdiagnostics-show-category=id -fdiagnostics-parseable-fixits                       -Wno-trigraphs -fpascal-strings     -Wmissing-prototypes -Wreturn-type -Wparentheses -Wswitch -Wno-unused-parameter -Wunused-variable -Wunused-value -Wno-shorten-64-to-32 -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.sdk -gdwarf-2 -fvisibility=hidden -Wno-sign-conversion -mthumb "-DIBOutlet=__attribute__((iboutlet))" "-DIBOutletCollection(ClassName)=__attribute__((iboutletcollection(ClassName)))" "-DIBAction=void)__attribute__((ibaction)" -miphoneos-version-min=4.3                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                         -MMD -MT dependencies -v -Os -ffast-math -DSTOREKIT_ENABLED -DIPHONE_ENABLED -DUNIX_ENABLED -DGLES2_ENABLED -DNO_THREADS -DMODULE_GRIDMAP_ENABLED -DMUSEPACK_ENABLED -DOLD_SCENE_FORMAT_ENABLED -DSQUIRREL_ENABLED -DVORBIS_ENABLED -DTHEORA_ENABLED -DPNG_ENABLED -DDDS_ENABLED -DPVR_ENABLED -DJPG_ENABLED -DSPEEX_ENABLED -DTOOLS_ENABLED -DGDSCRIPT_ENABLED -DMINIZIP_ENABLED -DXML_ENABLED -Icore -Icore/math -Itools -Idrivers -I. -Iplatform/iphone -Iplatform/iphone/include -Iplatform/iphone/scoreloop/SDKs -I/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.sdk/System/Library/Frameworks/OpenGLES.framework/Headers -Iscript/squirrel/src -Iscript/vorbis script/gdscript/gd_script.cpp
#/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/clang -x objective-c -arch armv7 -fmessage-length=0 -fdiagnostics-print-source-range-info -fdiagnostics-show-category=id -fdiagnostics-parseable-fixits -std=gnu99 -fobjc-arc -Wno-trigraphs -fpascal-strings -Os -Wmissing-prototypes -Wreturn-type -Wparentheses -Wswitch -Wno-unused-parameter -Wunused-variable -Wunused-value -Wno-shorten-64-to-32 -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.0.sdk -gdwarf-2 -fvisibility=hidden -Wno-sign-conversion -mthumb "-DIBOutlet=__attribute__((iboutlet))" "-DIBOutletCollection(ClassName)=__attribute__((iboutletcollection(ClassName)))" "-DIBAction=void)__attribute__((ibaction)" -miphoneos-version-min=4.3 -iquote /Users/red/test2/build/test2.build/Release-iphoneos/test2.build/test2-generated-files.hmap -I/Users/red/test2/build/test2.build/Release-iphoneos/test2.build/test2-own-target-headers.hmap -I/Users/red/test2/build/test2.build/Release-iphoneos/test2.build/test2-all-target-headers.hmap -iquote /Users/red/test2/build/test2.build/Release-iphoneos/test2.build/test2-project-headers.hmap -I/Users/red/test2/build/Release-iphoneos/include -I/Users/red/test2/build/test2.build/Release-iphoneos/test2.build/DerivedSources/armv7 -I/Users/red/test2/build/test2.build/Release-iphoneos/test2.build/DerivedSources -F/Users/red/test2/build/Release-iphoneos -DNS_BLOCK_ASSERTIONS=1 -include /var/folders/LX/LXYXHTeSHSqbkhuPJRIsuE+++TI/-Caches-/com.apple.Xcode.501/SharedPrecompiledHeaders/test2-Prefix-dvdhnltoisfpmyalexovdrmfyeky/test2-Prefix.pch -MMD -MT dependencies -MF /Users/red/test2/build/test2.build/Release-iphoneos/test2.build/Objects-normal/armv7/main.d -c /Users/red/test2/test2/main.m -o /Users/red/test2/build/test2.build/Release-iphoneos/test2.build/Objects-normal/armv7/main.o





	env.Append(LINKFLAGS=['-arch', 'armv7', '-Wl,-dead_strip', '-miphoneos-version-min=4.3',
							'-isysroot', '$IPHONESDK',
							#'-mmacosx-version-min=10.5',
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
	env.Append(CPPFLAGS=['-DIPHONE_ENABLED', '-DUNIX_ENABLED', '-DGLES2_ENABLED', '-DGLES1_ENABLED', '-DMPC_FIXED_POINT'])
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

# /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang -x objective-c-header -arch armv7s -fmessage-length=0 -std=gnu99 -fobjc-arc -Wno-trigraphs -fpascal-strings -Os -Wno-missing-field-initializers -Wno-missing-prototypes -Wreturn-type -Wno-implicit-atomic-properties -Wno-receiver-is-weak -Wduplicate-method-match -Wformat -Wno-missing-braces -Wparentheses -Wswitch -Wno-unused-function -Wno-unused-label -Wno-unused-parameter -Wunused-variable -Wunused-value -Wempty-body -Wuninitialized -Wno-unknown-pragmas -Wno-shadow -Wno-four-char-constants -Wno-conversion -Wno-shorten-64-to-32 -Wpointer-sign -Wno-newline-eof -Wno-selector -Wno-strict-selector-match -Wno-undeclared-selector -Wno-deprecated-implementations -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS6.0.sdk -Wprotocol -Wdeprecated-declarations -g -fvisibility=hidden -Wno-sign-conversion "-DIBOutlet=__attribute__((iboutlet))" "-DIBOutletCollection(ClassName)=__attribute__((iboutletcollection(ClassName)))" "-DIBAction=void)__attribute__((ibaction)" -miphoneos-version-min=4.3 -iquote /Users/lucasgondolo/test/build/test.build/Release-iphoneos/test.build/test-generated-files.hmap -I/Users/lucasgondolo/test/build/test.build/Release-iphoneos/test.build/test-own-target-headers.hmap -I/Users/lucasgondolo/test/build/test.build/Release-iphoneos/test.build/test-all-target-headers.hmap -iquote /Users/lucasgondolo/test/build/test.build/Release-iphoneos/test.build/test-project-headers.hmap -I/Users/lucasgondolo/test/build/Release-iphoneos/include -I/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include -I/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include -I/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/include -I/Users/lucasgondolo/test/build/test.build/Release-iphoneos/test.build/DerivedSources/armv7s -I/Users/lucasgondolo/test/build/test.build/Release-iphoneos/test.build/DerivedSources -F/Users/lucasgondolo/test/build/Release-iphoneos -DNS_BLOCK_ASSERTIONS=1 --serialize-diagnostics /var/folders/9r/_65jj9457bgb4n4nxcsm0xl80000gn/C/com.apple.Xcode.501/SharedPrecompiledHeaders/test-Prefix-esrzoamhgruxcxbhemvvlrjmmvoh/test-Prefix.pch.dia -c /Users/lucasgondolo/test/test/test-Prefix.pch -o /var/folders/9r/_65jj9457bgb4n4nxcsm0xl80000gn/C/com.apple.Xcode.501/SharedPrecompiledHeaders/test-Prefix-esrzoamhgruxcxbhemvvlrjmmvoh/test-Prefix.pch.pth -MMD -MT dependencies -MF /var/folders/9r/_65jj9457bgb4n4nxcsm0xl80000gn/C/com.apple.Xcode.501/SharedPrecompiledHeaders/test-Prefix-esrzoamhgruxcxbhemvvlrjmmvoh/test-Prefix.pch.d
