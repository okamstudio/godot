#include "example_node.h"

void ExampleNode::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
#if GDEXTENSION
#ifdef GODOT_PRINT_STRING_HPP
			// Printing easily requires this godot-cpp PR: https://github.com/godotengine/godot-cpp/pull/1653
			print_line("The example GDExtension node has been added to the scene tree and is now ready!");
#endif // GODOT_PRINT_STRING_HPP
#elif GODOT_MODULE
			print_line("The example engine module node has been added to the scene tree and is now ready!");
#endif
		} break;
	}
}

void ExampleNode::print_hello() const {
#if GDEXTENSION
#ifdef GODOT_PRINT_STRING_HPP
	// Printing easily requires this godot-cpp PR: https://github.com/godotengine/godot-cpp/pull/1653
	print_line("Hello, World! From GDExtension.");
#endif // GODOT_PRINT_STRING_HPP
#elif GODOT_MODULE
	print_line("Hello, World! From a module.");
#endif
}

String ExampleNode::return_hello() const {
#if GDEXTENSION
	return "Hello, World! From GDExtension.";
#elif GODOT_MODULE
	return "Hello, World! From a module.";
#endif
}

void ExampleNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("print_hello"), &ExampleNode::print_hello);
	ClassDB::bind_method(D_METHOD("return_hello"), &ExampleNode::return_hello);
}
