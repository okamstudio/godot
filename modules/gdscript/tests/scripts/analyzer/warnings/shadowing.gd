class_name ShadowedClass
extends ShadowingBase

var member: int = 0

var print_debug := 'print_debug'
@warning_ignore('shadowed_global_identifier')
var print := 'print'

@warning_ignore_start('unused_variable', 'unused_local_constant')
func test():
	var Array := 'Array'
	var Node := 'Node'
	var is_same := 'is_same'
	var sqrt := 'sqrt'
	var member := 'member'
	var reference := 'reference'
	var ShadowedClass := 'ShadowedClass'
	var base_variable_member
	const base_function_member = 1
	var base_const_member

	print('warn')
