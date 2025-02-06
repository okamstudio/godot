#ifndef __LIBRARY_NAME_UPPER___DEFINES_H
#define __LIBRARY_NAME_UPPER___DEFINES_H
// This file should be included before any other files.

// Uncomment one of these to help IDEs detect the build mode.
// The build system already defines one of these, so keep them
// commented out when committing.
#ifndef GDEXTENSION
//#define GDEXTENSION 1
#endif // GDEXTENSION

#ifndef GODOT_MODULE
//#define GODOT_MODULE 1
#endif // GODOT_MODULE

#if GDEXTENSION
// Extremely common classes used by most files. Customize for your extension as needed.
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>
// Including the namespace helps make GDExtension code more similar to module code.
using namespace godot;
#elif GODOT_MODULE
#include "core/object/class_db.h"
#include "core/string/ustring.h"
#endif

#endif // __LIBRARY_NAME_UPPER___DEFINES_H
