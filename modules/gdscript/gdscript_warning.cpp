/**************************************************************************/
/*  gdscript_warning.cpp                                                  */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "gdscript_warning.h"

#include "core/variant/variant.h"

#ifdef DEBUG_ENABLED

String GDScriptWarning::get_message() const {
#define CHECK_SYMBOLS(m_amount) ERR_FAIL_COND_V(symbols.size() < m_amount, String());

	switch (code) {
		case UNASSIGNED_VARIABLE:
			CHECK_SYMBOLS(1);
			return vformat(R"(The variable "%s" was used before being assigned a value.)", symbols[0]);
		case UNASSIGNED_VARIABLE_OP_ASSIGN:
			CHECK_SYMBOLS(1);
			return vformat(R"(Using assignment with operation but the variable "%s" was not previously assigned a value.)", symbols[0]);
		case UNUSED_VARIABLE:
			CHECK_SYMBOLS(1);
			return vformat(R"(The local variable "%s" is declared but never used in the block. If this is intended, prefix it with an underscore: "_%s".)", symbols[0], symbols[0]);
		case UNUSED_LOCAL_CONSTANT:
			CHECK_SYMBOLS(1);
			return vformat(R"(The local constant "%s" is declared but never used in the block. If this is intended, prefix it with an underscore: "_%s".)", symbols[0], symbols[0]);
		case UNUSED_PRIVATE_CLASS_VARIABLE:
			CHECK_SYMBOLS(1);
			return vformat(R"(The class variable "%s" is declared but never used in the class.)", symbols[0]);
		case UNUSED_PARAMETER:
			CHECK_SYMBOLS(2);
			return vformat(R"*(The parameter "%s" is never used in the function "%s()". If this is intended, prefix it with an underscore: "_%s".)*", symbols[1], symbols[0], symbols[1]);
		case UNUSED_SIGNAL:
			CHECK_SYMBOLS(1);
			return vformat(R"(The signal "%s" is declared but never explicitly used in the class.)", symbols[0]);
		case SHADOWED_VARIABLE:
			CHECK_SYMBOLS(4);
			return vformat(R"(The local %s "%s" is shadowing an already-declared %s at line %s in the current class.)", symbols[0], symbols[1], symbols[2], symbols[3]);
		case SHADOWED_VARIABLE_BASE_CLASS:
			CHECK_SYMBOLS(4);
			if (symbols.size() > 4) {
				return vformat(R"(The local %s "%s" is shadowing an already-declared %s at line %s in the base class "%s".)", symbols[0], symbols[1], symbols[2], symbols[3], symbols[4]);
			}
			return vformat(R"(The local %s "%s" is shadowing an already-declared %s in the base class "%s".)", symbols[0], symbols[1], symbols[2], symbols[3]);
		case SHADOWED_GLOBAL_IDENTIFIER:
			CHECK_SYMBOLS(3);
			return vformat(R"(The %s "%s" has the same name as a %s.)", symbols[0], symbols[1], symbols[2]);
		case UNREACHABLE_CODE:
			CHECK_SYMBOLS(1);
			return vformat(R"*(Unreachable code (statement after return) in function "%s()".)*", symbols[0]);
		case UNREACHABLE_PATTERN:
			return "Unreachable pattern (pattern after wildcard or bind).";
		case STANDALONE_EXPRESSION:
			return "Standalone expression (the line may have no effect).";
		case STANDALONE_TERNARY:
			return "Standalone ternary operator: the return value is being discarded.";
		case INCOMPATIBLE_TERNARY:
			return "Values of the ternary operator are not mutually compatible.";
		case UNTYPED_DECLARATION:
			CHECK_SYMBOLS(2);
			if (symbols[0] == "Function") {
				return vformat(R"*(%s "%s()" has no static return type.)*", symbols[0], symbols[1]);
			}
			return vformat(R"(%s "%s" has no static type.)", symbols[0], symbols[1]);
		case INFERRED_DECLARATION:
			CHECK_SYMBOLS(2);
			return vformat(R"(%s "%s" has an implicitly inferred static type.)", symbols[0], symbols[1]);
		case UNSAFE_PROPERTY_ACCESS:
			CHECK_SYMBOLS(2);
			return vformat(R"(The property "%s" is not present on the inferred type "%s" (but may be present on a subtype).)", symbols[0], symbols[1]);
		case UNSAFE_METHOD_ACCESS:
			CHECK_SYMBOLS(2);
			return vformat(R"*(The method "%s()" is not present on the inferred type "%s" (but may be present on a subtype).)*", symbols[0], symbols[1]);
		case UNSAFE_CAST:
			CHECK_SYMBOLS(1);
			return vformat(R"(Casting "Variant" to "%s" is unsafe.)", symbols[0]);
		case UNSAFE_CALL_ARGUMENT:
			CHECK_SYMBOLS(5);
			return vformat(R"*(The argument %s of the %s "%s()" requires the subtype "%s" but the supertype "%s" was provided.)*", symbols[0], symbols[1], symbols[2], symbols[3], symbols[4]);
		case UNSAFE_VOID_RETURN:
			CHECK_SYMBOLS(2);
			return vformat(R"*(The method "%s()" returns "void" but it's trying to return a call to "%s()" that can't be ensured to also be "void".)*", symbols[0], symbols[1]);
		case RETURN_VALUE_DISCARDED:
			CHECK_SYMBOLS(1);
			return vformat(R"*(The function "%s()" returns a value that will be discarded if not used.)*", symbols[0]);
		case STATIC_CALLED_ON_INSTANCE:
			CHECK_SYMBOLS(2);
			return vformat(R"*(The function "%s()" is a static function but was called from an instance. Instead, it should be directly called from the type: "%s.%s()".)*", symbols[0], symbols[1], symbols[0]);
		case MISSING_TOOL:
			return R"(The base class script has the "@tool" annotation, but this script does not have it.)";
		case REDUNDANT_STATIC_UNLOAD:
			return R"(The "@static_unload" annotation is redundant because the file does not have a class with static variables.)";
		case REDUNDANT_AWAIT:
			return R"("await" keyword not needed in this case, because the expression isn't a coroutine nor a signal.)";
		case ASSERT_ALWAYS_TRUE:
			return "Assert statement is redundant because the expression is always true.";
		case ASSERT_ALWAYS_FALSE:
			return "Assert statement will raise an error because the expression is always false.";
		case INTEGER_DIVISION:
			return "Integer division, decimal part will be discarded.";
		case NARROWING_CONVERSION:
			return "Narrowing conversion (float is converted to int and loses precision).";
		case INT_AS_ENUM_WITHOUT_CAST:
			return "Integer used when an enum value is expected. If this is intended cast the integer to the enum type.";
		case INT_AS_ENUM_WITHOUT_MATCH:
			CHECK_SYMBOLS(3);
			return vformat(R"(Cannot %s %s as Enum "%s": no enum member has matching value.)", symbols[0], symbols[1], symbols[2]);
		case ENUM_VARIABLE_WITHOUT_DEFAULT:
			CHECK_SYMBOLS(1);
			return vformat(R"(The variable "%s" has an enum type and does not set an explicit default value. The default will be set to "0".)", symbols[0]);
		case EMPTY_FILE:
			return "Empty script file.";
		case DEPRECATED_KEYWORD:
			CHECK_SYMBOLS(2);
			return vformat(R"(The "%s" keyword is deprecated and will be removed in a future release, please replace its uses by "%s".)", symbols[0], symbols[1]);
		case CONFUSABLE_IDENTIFIER:
			CHECK_SYMBOLS(1);
			return vformat(R"(The identifier "%s" has misleading characters and might be confused with something else.)", symbols[0]);
		case CONFUSABLE_LOCAL_DECLARATION:
			CHECK_SYMBOLS(2);
			return vformat(R"(The %s "%s" is declared below in the parent block.)", symbols[0], symbols[1]);
		case CONFUSABLE_LOCAL_USAGE:
			CHECK_SYMBOLS(1);
			return vformat(R"(The identifier "%s" will be shadowed below in the block.)", symbols[0]);
		case CONFUSABLE_CAPTURE_REASSIGNMENT:
			CHECK_SYMBOLS(1);
			return vformat(R"(Reassigning lambda capture does not modify the outer local variable "%s".)", symbols[0]);
		case INFERENCE_ON_VARIANT:
			CHECK_SYMBOLS(1);
			return vformat("The %s type is being inferred from a Variant value, so it will be typed as Variant.", symbols[0]);
		case NATIVE_METHOD_OVERRIDE:
			CHECK_SYMBOLS(2);
			return vformat(R"*(The method "%s()" overrides a method from native class "%s". This won't be called by the engine and may not work as expected.)*", symbols[0], symbols[1]);
		case GET_NODE_DEFAULT_WITHOUT_ONREADY:
			CHECK_SYMBOLS(1);
			return vformat(R"*(The default value is using "%s" which won't return nodes in the scene tree before "_ready()" is called. Use the "@onready" annotation to solve this.)*", symbols[0]);
		case ONREADY_WITH_EXPORT:
			return R"("@onready" will set the default value after "@export" takes effect and will override it.)";
#ifndef DISABLE_DEPRECATED
		// Never produced. These warnings migrated from 3.x by mistake.
		case PROPERTY_USED_AS_FUNCTION: // There is already an error.
		case CONSTANT_USED_AS_FUNCTION: // There is already an error.
		case FUNCTION_USED_AS_PROPERTY: // This is valid, returns `Callable`.
			break;
#endif
		case WARNING_MAX:
			break; // Can't happen, but silences warning.
	}
	ERR_FAIL_V_MSG(String(), vformat(R"(Invalid GDScript warning "%s".)", get_name_from_code(code)));

#undef CHECK_SYMBOLS
}

int GDScriptWarning::get_default_value(Code p_code) {
	ERR_FAIL_INDEX_V_MSG(p_code, WARNING_MAX, WarnLevel::IGNORE, "Getting default value of invalid warning code.");
	return default_warning_levels[p_code];
}

PropertyInfo GDScriptWarning::get_property_info(Code p_code) {
	return PropertyInfo(Variant::INT, get_settings_path_from_code(p_code), PROPERTY_HINT_ENUM, "Ignore,Warn,Error");
}

String GDScriptWarning::get_name() const {
	return get_name_from_code(code);
}

String GDScriptWarning::get_name_from_code(Code p_code) {
	ERR_FAIL_COND_V(p_code < 0 || p_code >= WARNING_MAX, String());

	static const char *names[] = {
		"UNASSIGNED_VARIABLE",
		"UNASSIGNED_VARIABLE_OP_ASSIGN",
		"UNUSED_VARIABLE",
		"UNUSED_LOCAL_CONSTANT",
		"UNUSED_PRIVATE_CLASS_VARIABLE",
		"UNUSED_PARAMETER",
		"UNUSED_SIGNAL",
		"SHADOWED_VARIABLE",
		"SHADOWED_VARIABLE_BASE_CLASS",
		"SHADOWED_GLOBAL_IDENTIFIER",
		"UNREACHABLE_CODE",
		"UNREACHABLE_PATTERN",
		"STANDALONE_EXPRESSION",
		"STANDALONE_TERNARY",
		"INCOMPATIBLE_TERNARY",
		"UNTYPED_DECLARATION",
		"INFERRED_DECLARATION",
		"UNSAFE_PROPERTY_ACCESS",
		"UNSAFE_METHOD_ACCESS",
		"UNSAFE_CAST",
		"UNSAFE_CALL_ARGUMENT",
		"UNSAFE_VOID_RETURN",
		"RETURN_VALUE_DISCARDED",
		"STATIC_CALLED_ON_INSTANCE",
		"MISSING_TOOL",
		"REDUNDANT_STATIC_UNLOAD",
		"REDUNDANT_AWAIT",
		"ASSERT_ALWAYS_TRUE",
		"ASSERT_ALWAYS_FALSE",
		"INTEGER_DIVISION",
		"NARROWING_CONVERSION",
		"INT_AS_ENUM_WITHOUT_CAST",
		"INT_AS_ENUM_WITHOUT_MATCH",
		"ENUM_VARIABLE_WITHOUT_DEFAULT",
		"EMPTY_FILE",
		"DEPRECATED_KEYWORD",
		"CONFUSABLE_IDENTIFIER",
		"CONFUSABLE_LOCAL_DECLARATION",
		"CONFUSABLE_LOCAL_USAGE",
		"CONFUSABLE_CAPTURE_REASSIGNMENT",
		"INFERENCE_ON_VARIANT",
		"NATIVE_METHOD_OVERRIDE",
		"GET_NODE_DEFAULT_WITHOUT_ONREADY",
		"ONREADY_WITH_EXPORT",
#ifndef DISABLE_DEPRECATED
		"PROPERTY_USED_AS_FUNCTION",
		"CONSTANT_USED_AS_FUNCTION",
		"FUNCTION_USED_AS_PROPERTY",
#endif
	};

	static_assert(std::size(names) == WARNING_MAX, "Amount of warning types don't match the amount of warning names.");

	return names[(int)p_code];
}

String GDScriptWarning::get_settings_path_from_code(Code p_code) {
	return "debug/gdscript/warnings/" + get_name_from_code(p_code).to_lower();
}

GDScriptWarning::Code GDScriptWarning::get_code_from_name(const String &p_name) {
	for (int i = 0; i < WARNING_MAX; i++) {
		if (get_name_from_code((Code)i) == p_name) {
			return (Code)i;
		}
	}

	return WARNING_MAX;
}

#endif // DEBUG_ENABLED
