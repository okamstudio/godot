extends Node

func my_func_1(_foo, _bar):
	pass

func my_func_2(_foo, _bar, _baz):
	pass

static func my_static_func_1(_foo, _bar):
	pass

static func my_static_func_2(_foo, _bar, _baz):
	pass

@rpc
func my_rpc_func_1(_foo, _bar):
	pass

@rpc
func my_rpc_func_2(_foo, _bar, _baz):
	pass

func test():
	# Test built-in methods.
	var builtin_callable_1 : Callable = add_to_group
	print(builtin_callable_1.get_argument_count()) # Should print 2.
	var builtin_callable_2 : Callable = find_child
	print(builtin_callable_2.get_argument_count()) # Should print 3.

	# Test built-in vararg methods.
	var builtin_vararg_callable_1 : Callable = call_thread_safe
	print(builtin_vararg_callable_1.get_argument_count()) # Should print 1.
	var builtin_vararg_callable_2 : Callable = rpc_id
	print(builtin_vararg_callable_2.get_argument_count()) # Should print 2.

	# Test plain methods.
	var callable_1 : Callable = my_func_1
	print(callable_1.get_argument_count()) # Should print 2.
	var callable_2 : Callable = my_func_2
	print(callable_2.get_argument_count()) # Should print 3.

	# Test static methods.
	var static_callable_1 : Callable = my_static_func_1
	print(static_callable_1.get_argument_count()) # Should print 2.
	var static_callable_2 : Callable = my_static_func_2
	print(static_callable_2.get_argument_count()) # Should print 3.

	# Test rpc methods.
	var rpc_callable_1 : Callable = my_rpc_func_1
	print(rpc_callable_1.get_argument_count()) # Should print 2.
	var rpc_callable_2 : Callable = my_rpc_func_2
	print(rpc_callable_2.get_argument_count()) # Should print 3.

	# Test lambdas.
	var lambda_callable_1 : Callable = func(_foo, _bar): pass
	print(lambda_callable_1.get_argument_count()) # Should print 2.
	var lambda_callable_2 : Callable = func(_foo, _bar, _baz): pass
	print(lambda_callable_2.get_argument_count()) # Should print 3.

	# Test lambdas with self.
	var lambda_self_callable_1 : Callable = func(_foo, _bar): return self
	print(lambda_self_callable_1.get_argument_count()) # Should print 2.
	var lambda_self_callable_2 : Callable = func(_foo, _bar, _baz): return self
	print(lambda_self_callable_2.get_argument_count()) # Should print 3.

	# Test bind.
	var bind_callable_1 : Callable = my_func_2.bind(1)
	print(bind_callable_1.get_argument_count()) # Should print 2.
	var bind_callable_2 : Callable = my_func_2.bind(1, 2)
	print(bind_callable_2.get_argument_count()) # Should print 1.

	# Test unbind.
	var unbind_callable_1 : Callable = my_func_2.unbind(1)
	print(unbind_callable_1.get_argument_count()) # Should print 4.
	var unbind_callable_2 : Callable = my_func_2.unbind(2)
	print(unbind_callable_2.get_argument_count()) # Should print 5.

	# Test variant callables.
	var string_tmp := String()
	var variant_callable_1 : Callable = string_tmp.replace
	print(variant_callable_1.get_argument_count()) # Should print 2.
	var variant_callable_2 : Callable = string_tmp.rsplit
	print(variant_callable_2.get_argument_count()) # Should print 3.

	# Test variant vararg callables.
	var callable_tmp := Callable()
	var variant_vararg_callable_1 : Callable = callable_tmp.call
	print(variant_vararg_callable_1.get_argument_count()) # Should print 0.
	var variant_vararg_callable_2 : Callable = callable_tmp.rpc_id
	print(variant_vararg_callable_2.get_argument_count()) # Should print 1.

	# Test global methods.
	var global_callable_1 = is_equal_approx
	print(global_callable_1.get_argument_count()) # Should print 2.
	var global_callable_2 = inverse_lerp
	print(global_callable_2.get_argument_count()) # Should print 3.

	# Test GDScript methods.
	var gdscript_callable_1 = char
	print(gdscript_callable_1.get_argument_count()) # Should print 1.
	var gdscript_callable_2 = is_instance_of
	print(gdscript_callable_2.get_argument_count()) # Should print 2.
