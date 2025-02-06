"""
Load this file to your LLDB session to enable pretty-printing of some Godot C++ types.

LLDB command: `command script import ./misc/utility/godot_lldb_pretty_print.py`.

To load these automatically in VSCodium using the CodeLLDB plugin, add a lldb init command to your workspace settings:
```json
{
    ...
    "lldb.launch.initCommands": [
        "command script import ./misc/utility/godot_lldb_pretty_print.py"
    ]
    ...
}
```
Other UIs that use LLDB under the hood are likely to have their own ways to achieve this. It can be added to a local `.lldbinit` file if lldb was configured to load those.
"""

import lldb


class BaseLinkedChildrenProvider:
    def __init__(
        self, valobj, size_expression_path, first_expression_path, next_expression_path, value_expression_path
    ):
        self.valobj = valobj
        self.size_expression_path = size_expression_path
        self.first_expression_path = first_expression_path
        self.next_expression_path = next_expression_path
        self.value_expression_path = value_expression_path

        self.element_cache = []
        self.next_element = None

    def update(self):
        self.element_cache = []
        self.next_element = self.valobj.GetValueForExpressionPath(self.first_expression_path)
        return False

    def num_children(self, max_children=None):
        return self.valobj.GetValueForExpressionPath(self.size_expression_path).signed

    def get_child_at_index(self, index):
        if index < 0:
            return None
        if index >= self.num_children():
            return None

        while index >= len(self.element_cache):
            if not self.next_element:
                return None

            node = self.next_element
            value = node.GetValueForExpressionPath(self.value_expression_path)
            self.element_cache.append(value)

            self.next_element = node.GetValueForExpressionPath(self.next_expression_path)
            if not self.next_element.GetValueAsUnsigned(0):
                self.next_element = None

        value = self.element_cache[index]
        return self.valobj.CreateValueFromData("[%d]" % index, value.GetData(), value.GetType())

    def get_child_index(self, name):
        try:
            return int(name.lstrip("[").rstrip("]"))
        except Exception:
            return -1


class ListChildrenProvider(BaseLinkedChildrenProvider):
    def __init__(self, valobj, internal_dict):
        super().__init__(valobj, "._data->size_cache", "._data->first", ".next_ptr", ".value")


class HashMapChildrenProvider(BaseLinkedChildrenProvider):
    def __init__(self, valobj, internal_dict):
        super().__init__(valobj, ".num_elements", ".head_element", ".next", ".data")


class VectorChildrenProvider:
    def __init__(self, valobj, internal_dict):
        self.valobj = valobj

    def num_children(self, max_children):
        return self.valobj.EvaluateExpression("this.size()").unsigned

    def get_child_index(self, name):
        logger = lldb.formatters.Logger.Logger()
        logger >> "Requesting child index for " + str(name)
        try:
            return int(name.lstrip("[").rstrip("]"))
        except Exception:
            return -1

    def get_child_at_index(self, index):
        value = self.valobj.EvaluateExpression("this.get(%d)" % index)
        return self.valobj.CreateValueFromData("[%d]" % index, value.GetData(), value.GetType())


def string_summary(valobj, internal_dict, options):
    ptr = valobj.GetValueForExpressionPath("._cowdata._ptr")
    if not ptr.GetValueAsUnsigned(0):
        return '""'
    return ptr.GetSummary().lstrip("U")


def string_name_summary(valobj, internal_dict, options):
    ptr = valobj.GetValueForExpressionPath("._data.name._cowdata._ptr")
    if not ptr.GetValueAsUnsigned(0):
        cname = valobj.GetValueForExpressionPath("._data.cname")
        if not cname.GetValueAsUnsigned(0):
            return '&""'
        ptr = cname
    return "&" + ptr.GetSummary().lstrip("U")


def __lldb_init_module(debugger, dict):
    # Uncomment to debug this script:
    # lldb.formatters.Logger._lldb_formatters_debug_level = 2

    # Easy Summary strings ========================================================================================================================

    # debugger.HandleCommand('type summary add --summary-string "&${var._data.name._cowdata._ptr%S}" StringName')
    # debugger.HandleCommand('type summary add --summary-string "${var._cowdata._ptr%S}" String')
    debugger.HandleCommand("type summary add -F godot_lldb_pretty_print.string_summary String")
    debugger.HandleCommand("type summary add -F godot_lldb_pretty_print.string_name_summary StringName")

    # List ========================================================================================================================================

    # Special attention: The regex must not match the ::Element subtype.
    LIST_REGEX = "^List<.+>$"

    debugger.HandleCommand(
        'type synthetic add --python-class godot_lldb_pretty_print.ListChildrenProvider -x "' + LIST_REGEX + '"'
    )
    debugger.HandleCommand('type summary add --summary-string "${svar%#} items" -x "' + LIST_REGEX + '"')

    # HashMap =====================================================================================================================================
    HASH_MAP_REGEX = "^HashMap<.+>$"

    debugger.HandleCommand(
        'type synthetic add --python-class godot_lldb_pretty_print.HashMapChildrenProvider -x "' + HASH_MAP_REGEX + '"'
    )
    debugger.HandleCommand('type summary add --summary-string "${svar%#} items" -x "' + HASH_MAP_REGEX + '"')

    # Vector ======================================================================================================================================
    VECTOR_REGEX = "^Vector<.+>$"

    debugger.HandleCommand(
        'type synthetic add --python-class godot_lldb_pretty_print.VectorChildrenProvider -x "' + VECTOR_REGEX + '"'
    )
    debugger.HandleCommand('type summary add --summary-string "${svar%#} items" -x "' + VECTOR_REGEX + '"')
