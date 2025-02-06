#ifndef TEST_EXAMPLE_NODE_H
#define TEST_EXAMPLE_NODE_H

#include "../example_node.h"
#include "tests/test_macros.h"

namespace TestExampleNode {
TEST_CASE("[ExampleNode] Hello") {
	ExampleNode test = ExampleNode();
	const String hello_text = test.return_hello();
	REQUIRE(hello_text == "Hello, World! From a module.");
}
} // namespace TestExampleNode

#endif // TEST_EXAMPLE_NODE_H
