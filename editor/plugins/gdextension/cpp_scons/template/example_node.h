#ifndef EXAMPLE_NODE_H
#define EXAMPLE_NODE_H

#include "__LIBRARY_NAME___defines.h"

#if GDEXTENSION
#include <godot_cpp/classes/node.hpp>
#elif GODOT_MODULE
#include "scene/main/node.h"
#endif

class ExampleNode : public Node {
	GDCLASS(ExampleNode, Node);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	void print_hello() const;
	String return_hello() const;
};

#endif // EXAMPLE_NODE_H
