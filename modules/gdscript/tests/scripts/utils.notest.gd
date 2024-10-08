class_name Utils


# `assert()` is not evaluated in non-debug builds. Do not use `assert()`
# for anything other than testing the `assert()` itself.
static func check(condition: Variant) -> void:
	if not condition:
		printerr("Check failed.")


static func get_type(property: Dictionary, is_return: bool = false) -> String:
	match property.type:
		TYPE_NIL:
			if property.usage & PROPERTY_USAGE_NIL_IS_VARIANT:
				return "Variant"
			return "void" if is_return else "null"
		TYPE_INT:
			if property.usage & PROPERTY_USAGE_CLASS_IS_ENUM:
				if property.class_name == &"":
					return "<unknown enum>"
				return property.class_name
		TYPE_ARRAY:
			if property.hint == PROPERTY_HINT_ARRAY_TYPE:
				if str(property.hint_string).is_empty():
					return "Array[<unknown type>]"
				return "Array[%s]" % property.hint_string
		TYPE_DICTIONARY:
			if property.hint == PROPERTY_HINT_DICTIONARY_TYPE:
				if str(property.hint_string).is_empty():
					return "Dictionary[<unknown type>, <unknown type>]"
				return "Dictionary[%s]" % str(property.hint_string).replace(";", ", ")
		TYPE_OBJECT:
			if not str(property.class_name).is_empty():
				return property.class_name
	return type_string(property.type)


static func get_property_signature(property: Dictionary, base: Object = null, is_static: bool = false) -> String:
	if property.usage & PROPERTY_USAGE_CATEGORY:
		return '@export_category("%s")' % str(property.name).c_escape()
	if property.usage & PROPERTY_USAGE_GROUP:
		return '@export_group("%s")' % str(property.name).c_escape()
	if property.usage & PROPERTY_USAGE_SUBGROUP:
		return '@export_subgroup("%s")' % str(property.name).c_escape()

	var result: String = ""
	if not (property.usage & PROPERTY_USAGE_SCRIPT_VARIABLE):
		printerr("Missing `PROPERTY_USAGE_SCRIPT_VARIABLE` flag.")
	if is_static:
		result += "static "
	result += "var " + property.name + ": " + get_type(property)
	if is_instance_valid(base):
		result += " = " + var_to_str(base.get(property.name))
	return result


static func get_human_readable_hint_string(property: Dictionary) -> String:
	if property.type >= TYPE_ARRAY and property.hint == PROPERTY_HINT_TYPE_STRING:
		var type_hint_prefixes: String = ""
		var hint_string: String = property.hint_string

		while true:
			if not hint_string.contains(":"):
				printerr("Invalid PROPERTY_HINT_TYPE_STRING format.")
			var elem_type_hint: String = hint_string.get_slice(":", 0)
			hint_string = hint_string.substr(elem_type_hint.length() + 1)

			var elem_type: int
			var elem_hint: int

			if elem_type_hint.is_valid_int():
				elem_type = elem_type_hint.to_int()
				type_hint_prefixes += "<%s>:" % type_string(elem_type)
			else:
				if elem_type_hint.count("/") != 1:
					printerr("Invalid PROPERTY_HINT_TYPE_STRING format.")
				elem_type = elem_type_hint.get_slice("/", 0).to_int()
				elem_hint = elem_type_hint.get_slice("/", 1).to_int()
				type_hint_prefixes += "<%s>/<%s>:" % [
					type_string(elem_type),
					get_property_hint_name(elem_hint).trim_prefix("PROPERTY_HINT_"),
				]

			if elem_type < TYPE_ARRAY or hint_string.is_empty():
				break

		return type_hint_prefixes + hint_string

	return property.hint_string


static func print_property_extended_info(property: Dictionary, base: Object = null, is_static: bool = false) -> void:
	print(get_property_signature(property, base, is_static))
	print('  hint=%s hint_string="%s" usage=%s class_name=&"%s"' % [
		get_property_hint_name(property.hint).trim_prefix("PROPERTY_HINT_"),
		get_human_readable_hint_string(property).c_escape(),
		get_property_usage_string(property.usage).replace("PROPERTY_USAGE_", ""),
		property.class_name.c_escape(),
	])


static func get_method_signature(method: Dictionary, is_signal: bool = false) -> String:
	var result: String = ""
	if method.flags & METHOD_FLAG_STATIC:
		result += "static "
	result += ("signal " if is_signal else "func ") + method.name + "("

	var args: Array[Dictionary] = method.args
	var default_args: Array = method.default_args
	var mandatory_argc: int = args.size() - default_args.size()
	for i in args.size():
		if i > 0:
			result += ", "
		var arg: Dictionary = args[i]
		result += arg.name + ": " + get_type(arg)
		if i >= mandatory_argc:
			result += " = " + var_to_str(default_args[i - mandatory_argc])

	result += ")"
	if is_signal:
		if get_type(method.return, true) != "void":
			printerr("Signal return type must be `void`.")
	else:
		result += " -> " + get_type(method.return, true)
	return result


static func get_property_hint_name(hint: PropertyHint) -> String:
	match hint:
		PROPERTY_HINT_NONE:
			return "PROPERTY_HINT_NONE"
		PROPERTY_HINT_RANGE:
			return "PROPERTY_HINT_RANGE"
		PROPERTY_HINT_ENUM:
			return "PROPERTY_HINT_ENUM"
		PROPERTY_HINT_ENUM_SUGGESTION:
			return "PROPERTY_HINT_ENUM_SUGGESTION"
		PROPERTY_HINT_EXP_EASING:
			return "PROPERTY_HINT_EXP_EASING"
		PROPERTY_HINT_LINK:
			return "PROPERTY_HINT_LINK"
		PROPERTY_HINT_FLAGS:
			return "PROPERTY_HINT_FLAGS"
		PROPERTY_HINT_LAYERS_2D_RENDER:
			return "PROPERTY_HINT_LAYERS_2D_RENDER"
		PROPERTY_HINT_LAYERS_2D_PHYSICS:
			return "PROPERTY_HINT_LAYERS_2D_PHYSICS"
		PROPERTY_HINT_LAYERS_2D_NAVIGATION:
			return "PROPERTY_HINT_LAYERS_2D_NAVIGATION"
		PROPERTY_HINT_LAYERS_3D_RENDER:
			return "PROPERTY_HINT_LAYERS_3D_RENDER"
		PROPERTY_HINT_LAYERS_3D_PHYSICS:
			return "PROPERTY_HINT_LAYERS_3D_PHYSICS"
		PROPERTY_HINT_LAYERS_3D_NAVIGATION:
			return "PROPERTY_HINT_LAYERS_3D_NAVIGATION"
		PROPERTY_HINT_LAYERS_AVOIDANCE:
			return "PROPERTY_HINT_LAYERS_AVOIDANCE"
		PROPERTY_HINT_FILE:
			return "PROPERTY_HINT_FILE"
		PROPERTY_HINT_DIR:
			return "PROPERTY_HINT_DIR"
		PROPERTY_HINT_GLOBAL_FILE:
			return "PROPERTY_HINT_GLOBAL_FILE"
		PROPERTY_HINT_GLOBAL_DIR:
			return "PROPERTY_HINT_GLOBAL_DIR"
		PROPERTY_HINT_RESOURCE_TYPE:
			return "PROPERTY_HINT_RESOURCE_TYPE"
		PROPERTY_HINT_MULTILINE_TEXT:
			return "PROPERTY_HINT_MULTILINE_TEXT"
		PROPERTY_HINT_EXPRESSION:
			return "PROPERTY_HINT_EXPRESSION"
		PROPERTY_HINT_PLACEHOLDER_TEXT:
			return "PROPERTY_HINT_PLACEHOLDER_TEXT"
		PROPERTY_HINT_COLOR_NO_ALPHA:
			return "PROPERTY_HINT_COLOR_NO_ALPHA"
		PROPERTY_HINT_OBJECT_ID:
			return "PROPERTY_HINT_OBJECT_ID"
		PROPERTY_HINT_TYPE_STRING:
			return "PROPERTY_HINT_TYPE_STRING"
		PROPERTY_HINT_NODE_PATH_TO_EDITED_NODE:
			return "PROPERTY_HINT_NODE_PATH_TO_EDITED_NODE"
		PROPERTY_HINT_OBJECT_TOO_BIG:
			return "PROPERTY_HINT_OBJECT_TOO_BIG"
		PROPERTY_HINT_NODE_PATH_VALID_TYPES:
			return "PROPERTY_HINT_NODE_PATH_VALID_TYPES"
		PROPERTY_HINT_SAVE_FILE:
			return "PROPERTY_HINT_SAVE_FILE"
		PROPERTY_HINT_GLOBAL_SAVE_FILE:
			return "PROPERTY_HINT_GLOBAL_SAVE_FILE"
		PROPERTY_HINT_INT_IS_OBJECTID:
			return "PROPERTY_HINT_INT_IS_OBJECTID"
		PROPERTY_HINT_INT_IS_POINTER:
			return "PROPERTY_HINT_INT_IS_POINTER"
		PROPERTY_HINT_ARRAY_TYPE:
			return "PROPERTY_HINT_ARRAY_TYPE"
		PROPERTY_HINT_DICTIONARY_TYPE:
			return "PROPERTY_HINT_DICTIONARY_TYPE"
		PROPERTY_HINT_LOCALE_ID:
			return "PROPERTY_HINT_LOCALE_ID"
		PROPERTY_HINT_LOCALIZABLE_STRING:
			return "PROPERTY_HINT_LOCALIZABLE_STRING"
		PROPERTY_HINT_NODE_TYPE:
			return "PROPERTY_HINT_NODE_TYPE"
		PROPERTY_HINT_HIDE_QUATERNION_EDIT:
			return "PROPERTY_HINT_HIDE_QUATERNION_EDIT"
		PROPERTY_HINT_PASSWORD:
			return "PROPERTY_HINT_PASSWORD"
		PROPERTY_HINT_TOOL_BUTTON:
			return "PROPERTY_HINT_TOOL_BUTTON"

	printerr("Argument `hint` is invalid. Use `PROPERTY_HINT_*` constants.")
	return "<invalid hint>"


static func get_property_usage_string(usage: int) -> String:
	if usage == PROPERTY_USAGE_NONE:
		return "PROPERTY_USAGE_NONE"

	const FLAGS: Array[Array] = [
		[PROPERTY_USAGE_STORAGE, "PROPERTY_USAGE_STORAGE"],
		[PROPERTY_USAGE_EDITOR, "PROPERTY_USAGE_EDITOR"],
		[PROPERTY_USAGE_INTERNAL, "PROPERTY_USAGE_INTERNAL"],
		[PROPERTY_USAGE_CHECKABLE, "PROPERTY_USAGE_CHECKABLE"],
		[PROPERTY_USAGE_CHECKED, "PROPERTY_USAGE_CHECKED"],
		[PROPERTY_USAGE_GROUP, "PROPERTY_USAGE_GROUP"],
		[PROPERTY_USAGE_CATEGORY, "PROPERTY_USAGE_CATEGORY"],
		[PROPERTY_USAGE_SUBGROUP, "PROPERTY_USAGE_SUBGROUP"],
		[PROPERTY_USAGE_CLASS_IS_BITFIELD, "PROPERTY_USAGE_CLASS_IS_BITFIELD"],
		[PROPERTY_USAGE_NO_INSTANCE_STATE, "PROPERTY_USAGE_NO_INSTANCE_STATE"],
		[PROPERTY_USAGE_RESTART_IF_CHANGED, "PROPERTY_USAGE_RESTART_IF_CHANGED"],
		[PROPERTY_USAGE_SCRIPT_VARIABLE, "PROPERTY_USAGE_SCRIPT_VARIABLE"],
		[PROPERTY_USAGE_STORE_IF_NULL, "PROPERTY_USAGE_STORE_IF_NULL"],
		[PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED, "PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED"],
		[PROPERTY_USAGE_SCRIPT_DEFAULT_VALUE, "PROPERTY_USAGE_SCRIPT_DEFAULT_VALUE"],
		[PROPERTY_USAGE_CLASS_IS_ENUM, "PROPERTY_USAGE_CLASS_IS_ENUM"],
		[PROPERTY_USAGE_NIL_IS_VARIANT, "PROPERTY_USAGE_NIL_IS_VARIANT"],
		[PROPERTY_USAGE_ARRAY, "PROPERTY_USAGE_ARRAY"],
		[PROPERTY_USAGE_ALWAYS_DUPLICATE, "PROPERTY_USAGE_ALWAYS_DUPLICATE"],
		[PROPERTY_USAGE_NEVER_DUPLICATE, "PROPERTY_USAGE_NEVER_DUPLICATE"],
		[PROPERTY_USAGE_HIGH_END_GFX, "PROPERTY_USAGE_HIGH_END_GFX"],
		[PROPERTY_USAGE_NODE_PATH_FROM_SCENE_ROOT, "PROPERTY_USAGE_NODE_PATH_FROM_SCENE_ROOT"],
		[PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT, "PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT"],
		[PROPERTY_USAGE_KEYING_INCREMENTS, "PROPERTY_USAGE_KEYING_INCREMENTS"],
		[PROPERTY_USAGE_DEFERRED_SET_RESOURCE, "PROPERTY_USAGE_DEFERRED_SET_RESOURCE"],
		[PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT, "PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT"],
		[PROPERTY_USAGE_EDITOR_BASIC_SETTING, "PROPERTY_USAGE_EDITOR_BASIC_SETTING"],
		[PROPERTY_USAGE_READ_ONLY, "PROPERTY_USAGE_READ_ONLY"],
		[PROPERTY_USAGE_SECRET, "PROPERTY_USAGE_SECRET"],
	]

	var result: String = ""

	if (usage & PROPERTY_USAGE_DEFAULT) == PROPERTY_USAGE_DEFAULT:
		result += "PROPERTY_USAGE_DEFAULT|"
		usage &= ~PROPERTY_USAGE_DEFAULT

	for flag in FLAGS:
		if usage & flag[0]:
			result += flag[1] + "|"
			usage &= ~flag[0]

	if usage != PROPERTY_USAGE_NONE:
		printerr("Argument `usage` is invalid. Use `PROPERTY_USAGE_*` constants.")
		return "<invalid usage flags>"

	return result.left(-1)
