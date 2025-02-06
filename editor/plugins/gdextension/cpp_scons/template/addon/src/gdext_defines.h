#ifndef __LIBRARY_NAME_UPPER___DEFINES_H
#define __LIBRARY_NAME_UPPER___DEFINES_H

// This file should be included before any other files.

// The build system already defines GDEXTENSION, but this helps IDEs detect the build mode.
#define GDEXTENSION 1

// Extremely common classes used by most files. Customize for your extension as needed.
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>
// Including the namespace helps make GDExtension code more similar to module code.
// Remove this if you prefer to use the `godot::` namespace explicitly.
using namespace godot;

#endif // __LIBRARY_NAME_UPPER___DEFINES_H
