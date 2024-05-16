#include "__UPDIR_DOTS__/../register_types.h"

#include <gdextension_interface.h>

extern "C" {
// Initialization.
GDExtensionBool GDE_EXPORT __LIBRARY_NAME___library_init(GDExtensionInterfaceGetProcAddress p_get_proc_address, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_initialization) {
	godot::GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);

	init_obj.register_initializer(initialize___BASE_NAME___module);
	init_obj.register_terminator(uninitialize___BASE_NAME___module);
	init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);

	return init_obj.init();
}
} // extern "C"
