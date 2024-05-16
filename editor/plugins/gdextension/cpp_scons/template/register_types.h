#ifndef REGISTER_TYPES_H
#define REGISTER_TYPES_H

#include "__LIBRARY_NAME___defines.h"

#if GDEXTENSION
#include <godot_cpp/core/class_db.hpp>
#elif GODOT_MODULE
#include "modules/register_module_types.h"
#else
#error "Must build as Godot GDExtension or Godot module."
#endif

void initialize___BASE_NAME___module(ModuleInitializationLevel p_level);
void uninitialize___BASE_NAME___module(ModuleInitializationLevel p_level);

#endif // REGISTER_TYPES_H
