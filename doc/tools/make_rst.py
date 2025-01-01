#!/usr/bin/env python3

# This script makes RST files from the XML class reference for use with the online docs.

import argparse
import os
import re
import sys
import xml.etree.ElementTree as ET
from collections import OrderedDict
from typing import Any, Dict, List, Optional, TextIO, Tuple, Union

sys.path.insert(0, root_directory := os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../"))

import version
from methods import Ansi, toggle_color

# $DOCS_URL/path/to/page.html(#fragment-tag)
GODOT_DOCS_PATTERN = re.compile(r"^\$DOCS_URL/(.*)\.html(#.*)?$")

# Based on reStructuredText inline markup recognition rules
# https://docutils.sourceforge.io/docs/ref/rst/restructuredtext.html#inline-markup-recognition-rules
MARKUP_ALLOWED_PRECEDENT = " -:/'\"<([{"
MARKUP_ALLOWED_SUBSEQUENT = " -.,:;!?\\/'\")]}>"

# Used to translate section headings and other hardcoded strings when required with
# the --lang argument. The BASE_STRINGS list should be synced with what we actually
# write in this script (check `translate()` uses), and also hardcoded in
# `scripts/extract_classes.py` (godotengine/godot-editor-l10n repo) to include them in the source POT file.
BASE_STRINGS = [
    "All classes",
    "Globals",
    "Nodes",
    "Resources",
    "Editor-only",
    "Other objects",
    "Variant types",
    "Description",
    "Tutorials",
    "Properties",
    "Constructors",
    "Methods",
    "Operators",
    "Theme Properties",
    "Signals",
    "Enumerations",
    "Constants",
    "Annotations",
    "Property Descriptions",
    "Constructor Descriptions",
    "Method Descriptions",
    "Operator Descriptions",
    "Theme Property Descriptions",
    "Inherits:",
    "Inherited By:",
    "(overrides %s)",
    "Default",
    "Setter",
    "value",
    "Getter",
    "This method should typically be overridden by the user to have any effect.",
    "This method has no side effects. It doesn't modify any of the instance's member variables.",
    "This method accepts any number of arguments after the ones described here.",
    "This method is used to construct a type.",
    "This method doesn't need an instance to be called, so it can be called directly using the class name.",
    "This method describes a valid operator to use with this type as left-hand operand.",
    "This value is an integer composed as a bitmask of the following flags.",
    "No return value.",
    "There is currently no description for this class. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!",
    "There is currently no description for this signal. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!",
    "There is currently no description for this enum. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!",
    "There is currently no description for this constant. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!",
    "There is currently no description for this annotation. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!",
    "There is currently no description for this property. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!",
    "There is currently no description for this constructor. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!",
    "There is currently no description for this method. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!",
    "There is currently no description for this operator. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!",
    "There is currently no description for this theme property. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!",
    "There are notable differences when using this API with C#. See :ref:`doc_c_sharp_differences` for more information.",
    "Deprecated:",
    "Experimental:",
    "This signal may be changed or removed in future versions.",
    "This constant may be changed or removed in future versions.",
    "This property may be changed or removed in future versions.",
    "This constructor may be changed or removed in future versions.",
    "This method may be changed or removed in future versions.",
    "This operator may be changed or removed in future versions.",
    "This theme property may be changed or removed in future versions.",
    "[b]Note:[/b] The returned array is [i]copied[/i] and any changes to it will not update the original property value. See [%s] for more details.",
]
strings_l10n: Dict[str, str] = {}

CLASS_GROUPS: Dict[str, str] = {
    "global": "Globals",
    "node": "Nodes",
    "resource": "Resources",
    "object": "Other objects",
    "editor": "Editor-only",
    "variant": "Variant types",
}
CLASS_GROUPS_BASE: Dict[str, str] = {
    "node": "Node",
    "resource": "Resource",
    "object": "Object",
    "variant": "Variant",
}
# Sync with editor\register_editor_types.cpp
EDITOR_CLASSES: List[str] = [
    "FileSystemDock",
    "ScriptCreateDialog",
    "ScriptEditor",
    "ScriptEditorBase",
]
# Sync with the types mentioned in https://docs.godotengine.org/en/stable/tutorials/scripting/c_sharp/c_sharp_differences.html
CLASSES_WITH_CSHARP_DIFFERENCES: List[str] = [
    "@GlobalScope",
    "String",
    "StringName",
    "NodePath",
    "Signal",
    "Callable",
    "RID",
    "Basis",
    "Transform2D",
    "Transform3D",
    "Rect2",
    "Rect2i",
    "AABB",
    "Quaternion",
    "Projection",
    "Color",
    "Array",
    "Dictionary",
    "PackedByteArray",
    "PackedColorArray",
    "PackedFloat32Array",
    "PackedFloat64Array",
    "PackedInt32Array",
    "PackedInt64Array",
    "PackedStringArray",
    "PackedVector2Array",
    "PackedVector3Array",
    "PackedVector4Array",
    "Variant",
]

PACKED_ARRAY_TYPES: List[str] = [
    "PackedByteArray",
    "PackedColorArray",
    "PackedFloat32Array",
    "PackedFloat64Array",
    "PackedInt32Array",
    "PackedInt64Array",
    "PackedStringArray",
    "PackedVector2Array",
    "PackedVector3Array",
    "PackedVector4Array",
]


class State:
    def __init__(self) -> None:
        self.num_errors = 0
        self.num_warnings = 0
        self.classes: OrderedDict[str, ClassDef] = OrderedDict()
        self.current_class: str = ""

        # Additional content and structure checks and validators.
        self.script_language_parity_check: ScriptLanguageParityCheck = ScriptLanguageParityCheck()

    def parse_class(self, class_root: ET.Element, filepath: str) -> None:
        class_name = class_root.attrib["name"]
        self.current_class = class_name

        class_def = ClassDef(class_name)
        self.classes[class_name] = class_def
        class_def.filepath = filepath

        inherits = class_root.get("inherits")
        if inherits is not None:
            class_def.inherits = inherits

        class_def.deprecated = class_root.get("deprecated")
        class_def.experimental = class_root.get("experimental")

        brief_desc = class_root.find("brief_description")
        if brief_desc is not None and brief_desc.text:
            class_def.brief_description = brief_desc.text

        desc = class_root.find("description")
        if desc is not None and desc.text:
            class_def.description = desc.text

        keywords = class_root.get("keywords")
        if keywords is not None:
            class_def.keywords = keywords

        properties = class_root.find("members")
        if properties is not None:
            for property in properties:
                assert property.tag == "member"

                property_name = property.attrib["name"]
                if property_name in class_def.properties:
                    print_error(f'{class_name}.xml: Duplicate property "{property_name}".', self)
                    continue

                type_name = TypeName.from_element(property)
                setter = property.get("setter") or None  # Use or None so '' gets turned into None.
                getter = property.get("getter") or None
                default_value = property.get("default") or None
                if default_value is not None:
                    default_value = f"``{default_value}``"
                overrides = property.get("overrides") or None

                property_def = PropertyDef(
                    property_name, type_name, setter, getter, property.text, default_value, overrides
                )
                property_def.deprecated = property.get("deprecated")
                property_def.experimental = property.get("experimental")
                class_def.properties[property_name] = property_def

        constructors = class_root.find("constructors")
        if constructors is not None:
            for constructor in constructors:
                assert constructor.tag == "constructor"

                method_name = constructor.attrib["name"]
                qualifiers = constructor.get("qualifiers")

                return_element = constructor.find("return")
                if return_element is not None:
                    return_type = TypeName.from_element(return_element)
                else:
                    return_type = TypeName("void")

                params = self.parse_params(constructor, "constructor")

                desc_element = constructor.find("description")
                method_desc = None
                if desc_element is not None:
                    method_desc = desc_element.text

                method_def = MethodDef(method_name, return_type, params, method_desc, qualifiers)
                method_def.definition_name = "constructor"
                method_def.deprecated = constructor.get("deprecated")
                method_def.experimental = constructor.get("experimental")
                if method_name not in class_def.constructors:
                    class_def.constructors[method_name] = []

                class_def.constructors[method_name].append(method_def)

        methods = class_root.find("methods")
        if methods is not None:
            for method in methods:
                assert method.tag == "method"

                method_name = method.attrib["name"]
                qualifiers = method.get("qualifiers")

                return_element = method.find("return")
                if return_element is not None:
                    return_type = TypeName.from_element(return_element)

                else:
                    return_type = TypeName("void")

                params = self.parse_params(method, "method")

                desc_element = method.find("description")
                method_desc = None
                if desc_element is not None:
                    method_desc = desc_element.text

                method_def = MethodDef(method_name, return_type, params, method_desc, qualifiers)
                method_def.deprecated = method.get("deprecated")
                method_def.experimental = method.get("experimental")
                if method_name not in class_def.methods:
                    class_def.methods[method_name] = []

                class_def.methods[method_name].append(method_def)

        operators = class_root.find("operators")
        if operators is not None:
            for operator in operators:
                assert operator.tag == "operator"

                method_name = operator.attrib["name"]
                qualifiers = operator.get("qualifiers")

                return_element = operator.find("return")
                if return_element is not None:
                    return_type = TypeName.from_element(return_element)

                else:
                    return_type = TypeName("void")

                params = self.parse_params(operator, "operator")

                desc_element = operator.find("description")
                method_desc = None
                if desc_element is not None:
                    method_desc = desc_element.text

                method_def = MethodDef(method_name, return_type, params, method_desc, qualifiers)
                method_def.definition_name = "operator"
                method_def.deprecated = operator.get("deprecated")
                method_def.experimental = operator.get("experimental")
                if method_name not in class_def.operators:
                    class_def.operators[method_name] = []

                class_def.operators[method_name].append(method_def)

        constants = class_root.find("constants")
        if constants is not None:
            for constant in constants:
                assert constant.tag == "constant"

                constant_name = constant.attrib["name"]
                value = constant.attrib["value"]
                enum = constant.get("enum")
                is_bitfield = constant.get("is_bitfield") == "true"
                constant_def = ConstantDef(constant_name, value, constant.text, is_bitfield)
                constant_def.deprecated = constant.get("deprecated")
                constant_def.experimental = constant.get("experimental")
                if enum is None:
                    if constant_name in class_def.constants:
                        print_error(f'{class_name}.xml: Duplicate constant "{constant_name}".', self)
                        continue

                    class_def.constants[constant_name] = constant_def

                else:
                    if enum in class_def.enums:
                        enum_def = class_def.enums[enum]

                    else:
                        enum_def = EnumDef(enum, TypeName("int", enum), is_bitfield)
                        class_def.enums[enum] = enum_def

                    enum_def.values[constant_name] = constant_def

        annotations = class_root.find("annotations")
        if annotations is not None:
            for annotation in annotations:
                assert annotation.tag == "annotation"

                annotation_name = annotation.attrib["name"]
                qualifiers = annotation.get("qualifiers")

                params = self.parse_params(annotation, "annotation")

                desc_element = annotation.find("description")
                annotation_desc = None
                if desc_element is not None:
                    annotation_desc = desc_element.text

                annotation_def = AnnotationDef(annotation_name, params, annotation_desc, qualifiers)
                if annotation_name not in class_def.annotations:
                    class_def.annotations[annotation_name] = []

                class_def.annotations[annotation_name].append(annotation_def)

        signals = class_root.find("signals")
        if signals is not None:
            for signal in signals:
                assert signal.tag == "signal"

                signal_name = signal.attrib["name"]

                if signal_name in class_def.signals:
                    print_error(f'{class_name}.xml: Duplicate signal "{signal_name}".', self)
                    continue

                params = self.parse_params(signal, "signal")

                desc_element = signal.find("description")
                signal_desc = None
                if desc_element is not None:
                    signal_desc = desc_element.text

                signal_def = SignalDef(signal_name, params, signal_desc)
                signal_def.deprecated = signal.get("deprecated")
                signal_def.experimental = signal.get("experimental")
                class_def.signals[signal_name] = signal_def

        theme_items = class_root.find("theme_items")
        if theme_items is not None:
            for theme_item in theme_items:
                assert theme_item.tag == "theme_item"

                theme_item_name = theme_item.attrib["name"]
                theme_item_data_name = theme_item.attrib["data_type"]
                theme_item_id = "{}_{}".format(theme_item_data_name, theme_item_name)
                if theme_item_id in class_def.theme_items:
                    print_error(
                        f'{class_name}.xml: Duplicate theme property "{theme_item_name}" of type "{theme_item_data_name}".',
                        self,
                    )
                    continue

                default_value = theme_item.get("default") or None
                if default_value is not None:
                    default_value = f"``{default_value}``"

                theme_item_def = ThemeItemDef(
                    theme_item_name,
                    TypeName.from_element(theme_item),
                    theme_item_data_name,
                    theme_item.text,
                    default_value,
                )
                class_def.theme_items[theme_item_name] = theme_item_def

        tutorials = class_root.find("tutorials")
        if tutorials is not None:
            for link in tutorials:
                assert link.tag == "link"

                if link.text is not None:
                    class_def.tutorials.append((link.text.strip(), link.get("title", "")))

        self.current_class = ""

    def parse_params(self, root: ET.Element, context: str) -> List["ParameterDef"]:
        param_elements = root.findall("param")
        params: Any = [None] * len(param_elements)

        for param_index, param_element in enumerate(param_elements):
            param_name = param_element.attrib["name"]
            index = int(param_element.attrib["index"])
            type_name = TypeName.from_element(param_element)
            default = param_element.get("default")

            if param_name.strip() == "" or param_name.startswith("_unnamed_arg"):
                print_error(
                    f'{self.current_class}.xml: Empty argument name in {context} "{root.attrib["name"]}" at position {param_index}.',
                    self,
                )

            params[index] = ParameterDef(param_name, type_name, default)

        cast: List[ParameterDef] = params

        return cast

    def sort_classes(self) -> None:
        self.classes = OrderedDict(sorted(self.classes.items(), key=lambda t: t[0].lower()))


class TagState:
    def __init__(self, raw: str, name: str, arguments: str, closing: bool) -> None:
        self.raw = raw

        self.name = name
        self.arguments = arguments
        self.closing = closing


class TypeName:
    def __init__(self, type_name: str, enum: Optional[str] = None, is_bitfield: bool = False) -> None:
        self.type_name = type_name
        self.enum = enum
        self.is_bitfield = is_bitfield

    def to_rst(self, state: State) -> str:
        if self.enum is not None:
            return make_enum(self.enum, self.is_bitfield, state)
        elif self.type_name == "void":
            return "|void|"
        else:
            return make_type(self.type_name, state)

    @classmethod
    def from_element(cls, element: ET.Element) -> "TypeName":
        return cls(element.attrib["type"], element.get("enum"), element.get("is_bitfield") == "true")


class DefinitionBase:
    def __init__(
        self,
        definition_name: str,
        name: str,
    ) -> None:
        self.definition_name = definition_name
        self.name = name
        self.deprecated: Optional[str] = None
        self.experimental: Optional[str] = None


class PropertyDef(DefinitionBase):
    def __init__(
        self,
        name: str,
        type_name: TypeName,
        setter: Optional[str],
        getter: Optional[str],
        text: Optional[str],
        default_value: Optional[str],
        overrides: Optional[str],
    ) -> None:
        super().__init__("property", name)

        self.type_name = type_name
        self.setter = setter
        self.getter = getter
        self.text = text
        self.default_value = default_value
        self.overrides = overrides


class ParameterDef(DefinitionBase):
    def __init__(self, name: str, type_name: TypeName, default_value: Optional[str]) -> None:
        super().__init__("parameter", name)

        self.type_name = type_name
        self.default_value = default_value


class SignalDef(DefinitionBase):
    def __init__(self, name: str, parameters: List[ParameterDef], description: Optional[str]) -> None:
        super().__init__("signal", name)

        self.parameters = parameters
        self.description = description


class AnnotationDef(DefinitionBase):
    def __init__(
        self,
        name: str,
        parameters: List[ParameterDef],
        description: Optional[str],
        qualifiers: Optional[str],
    ) -> None:
        super().__init__("annotation", name)

        self.parameters = parameters
        self.description = description
        self.qualifiers = qualifiers


class MethodDef(DefinitionBase):
    def __init__(
        self,
        name: str,
        return_type: TypeName,
        parameters: List[ParameterDef],
        description: Optional[str],
        qualifiers: Optional[str],
    ) -> None:
        super().__init__("method", name)

        self.return_type = return_type
        self.parameters = parameters
        self.description = description
        self.qualifiers = qualifiers


class ConstantDef(DefinitionBase):
    def __init__(self, name: str, value: str, text: Optional[str], bitfield: bool) -> None:
        super().__init__("constant", name)

        self.value = value
        self.text = text
        self.is_bitfield = bitfield


class EnumDef(DefinitionBase):
    def __init__(self, name: str, type_name: TypeName, bitfield: bool) -> None:
        super().__init__("enum", name)

        self.type_name = type_name
        self.values: OrderedDict[str, ConstantDef] = OrderedDict()
        self.is_bitfield = bitfield


class ThemeItemDef(DefinitionBase):
    def __init__(
        self, name: str, type_name: TypeName, data_name: str, text: Optional[str], default_value: Optional[str]
    ) -> None:
        super().__init__("theme property", name)

        self.type_name = type_name
        self.data_name = data_name
        self.text = text
        self.default_value = default_value


class ClassDef(DefinitionBase):
    def __init__(self, name: str) -> None:
        super().__init__("class", name)

        self.class_group = "variant"
        self.editor_class = self._is_editor_class()

        self.constants: OrderedDict[str, ConstantDef] = OrderedDict()
        self.enums: OrderedDict[str, EnumDef] = OrderedDict()
        self.properties: OrderedDict[str, PropertyDef] = OrderedDict()
        self.constructors: OrderedDict[str, List[MethodDef]] = OrderedDict()
        self.methods: OrderedDict[str, List[MethodDef]] = OrderedDict()
        self.operators: OrderedDict[str, List[MethodDef]] = OrderedDict()
        self.signals: OrderedDict[str, SignalDef] = OrderedDict()
        self.annotations: OrderedDict[str, List[AnnotationDef]] = OrderedDict()
        self.theme_items: OrderedDict[str, ThemeItemDef] = OrderedDict()
        self.inherits: Optional[str] = None
        self.brief_description: Optional[str] = None
        self.description: Optional[str] = None
        self.tutorials: List[Tuple[str, str]] = []
        self.keywords: Optional[str] = None

        # Used to match the class with XML source for output filtering purposes.
        self.filepath: str = ""

    def _is_editor_class(self) -> bool:
        if self.name.startswith("Editor"):
            return True
        if self.name in EDITOR_CLASSES:
            return True

        return False

    def update_class_group(self, state: State) -> None:
        group_name = "variant"

        if self.name.startswith("@"):
            group_name = "global"
        elif self.inherits:
            inherits = self.inherits.strip()

            while inherits in state.classes:
                if inherits == "Node":
                    group_name = "node"
                    break
                if inherits == "Resource":
                    group_name = "resource"
                    break
                if inherits == "Object":
                    group_name = "object"
                    break

                inode = state.classes[inherits].inherits
                if inode:
                    inherits = inode.strip()
                else:
                    break

        self.class_group = group_name


# Checks if code samples have both GDScript and C# variations.
# For simplicity we assume that a GDScript example is always present, and ignore contexts
# which don't necessarily need C# examples.
class ScriptLanguageParityCheck:
    def __init__(self) -> None:
        self.hit_map: OrderedDict[str, List[Tuple[DefinitionBase, str]]] = OrderedDict()
        self.hit_count = 0

    def add_hit(self, class_name: str, context: DefinitionBase, error: str, state: State) -> None:
        if class_name in ["@GDScript", "@GlobalScope"]:
            return  # We don't expect these contexts to have parity.

        class_def = state.classes[class_name]
        if class_def.class_group == "variant" and class_def.name != "Object":
            return  # Variant types are replaced with native types in C#, we don't expect parity.

        self.hit_count += 1

        if class_name not in self.hit_map:
            self.hit_map[class_name] = []

        self.hit_map[class_name].append((context, error))


# Entry point for the RST generator.
def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("path", nargs="+", help="A path to an XML file or a directory containing XML files to parse.")
    parser.add_argument("--filter", default="", help="The filepath pattern for XML files to filter.")
    parser.add_argument("--lang", "-l", default="en", help="Language to use for section headings.")
    parser.add_argument(
        "--color",
        action="store_true",
        help="If passed, force colored output even if stdout is not a TTY (useful for continuous integration).",
    )
    group = parser.add_mutually_exclusive_group()
    group.add_argument("--output", "-o", default=".", help="The directory to save output .rst files in.")
    group.add_argument(
        "--dry-run",
        action="store_true",
        help="If passed, no output will be generated and XML files are only checked for errors.",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="If passed, enables verbose printing.",
    )
    args = parser.parse_args()

    toggle_color(args.color)

    # Retrieve heading translations for the given language.
    if not args.dry_run and args.lang != "en":
        lang_file = os.path.join(
            os.path.dirname(os.path.realpath(__file__)), "..", "translations", "{}.po".format(args.lang)
        )
        if os.path.exists(lang_file):
            try:
                import polib  # type: ignore
            except ImportError:
                print("Base template strings localization requires `polib`.")
                exit(1)

            pofile = polib.pofile(lang_file)
            for entry in pofile.translated_entries():
                if entry.msgid in BASE_STRINGS:
                    strings_l10n[entry.msgid] = entry.msgstr
        else:
            print(f'No PO file at "{lang_file}" for language "{args.lang}".')

    print("Checking for errors in the XML class reference...")

    file_list: List[str] = []

    for path in args.path:
        # Cut off trailing slashes so os.path.basename doesn't choke.
        if path.endswith("/") or path.endswith("\\"):
            path = path[:-1]

        if os.path.basename(path) in ["modules", "platform"]:
            for subdir, dirs, _ in os.walk(path):
                if "doc_classes" in dirs:
                    doc_dir = os.path.join(subdir, "doc_classes")
                    class_file_names = (f for f in os.listdir(doc_dir) if f.endswith(".xml"))
                    file_list += (os.path.join(doc_dir, f) for f in class_file_names)

        elif os.path.isdir(path):
            file_list += (os.path.join(path, f) for f in os.listdir(path) if f.endswith(".xml"))

        elif os.path.isfile(path):
            if not path.endswith(".xml"):
                print(f'Got non-.xml file "{path}" in input, skipping.')
                continue

            file_list.append(path)

    classes: Dict[str, Tuple[ET.Element, str]] = {}
    state = State()

    for cur_file in file_list:
        try:
            tree = ET.parse(cur_file)
        except ET.ParseError as e:
            print_error(f"{cur_file}: Parse error while reading the file: {e}", state)
            continue
        doc = tree.getroot()

        name = doc.attrib["name"]
        if name in classes:
            print_error(f'{cur_file}: Duplicate class "{name}".', state)
            continue

        classes[name] = (doc, cur_file)

    for name, data in classes.items():
        try:
            state.parse_class(data[0], data[1])
        except Exception as e:
            print_error(f"{name}.xml: Exception while parsing class: {e}", state)

    state.sort_classes()

    pattern = re.compile(args.filter)

    # Create the output folder recursively if it doesn't already exist.
    os.makedirs(args.output, exist_ok=True)

    print("Generating the RST class reference...")

    grouped_classes: Dict[str, List[str]] = {}

    for class_name, class_def in state.classes.items():
        if args.filter and not pattern.search(class_def.filepath):
            continue
        state.current_class = class_name

        class_def.update_class_group(state)
        make_rst_class(class_def, state, args.dry_run, args.output)

        if class_def.class_group not in grouped_classes:
            grouped_classes[class_def.class_group] = []
        grouped_classes[class_def.class_group].append(class_name)

        if class_def.editor_class:
            if "editor" not in grouped_classes:
                grouped_classes["editor"] = []
            grouped_classes["editor"].append(class_name)

    print("")
    print("Generating the index file...")

    make_rst_index(grouped_classes, args.dry_run, args.output)

    print("")

    # Print out checks.

    if state.script_language_parity_check.hit_count > 0:
        if not args.verbose:
            print(
                f"{Ansi.YELLOW}{state.script_language_parity_check.hit_count} code samples failed parity check. Use --verbose to get more information.{Ansi.RESET}"
            )
        else:
            print(
                f"{Ansi.YELLOW}{state.script_language_parity_check.hit_count} code samples failed parity check:{Ansi.RESET}"
            )

            for class_name in state.script_language_parity_check.hit_map.keys():
                class_hits = state.script_language_parity_check.hit_map[class_name]
                print(f'{Ansi.YELLOW}- {len(class_hits)} hits in class "{class_name}"{Ansi.RESET}')

                for context, error in class_hits:
                    print(f"  - {error} in {format_context_name(context)}")
        print("")

    # Print out warnings and errors, or lack thereof, and exit with an appropriate code.

    if state.num_warnings >= 2:
        print(
            f"{Ansi.YELLOW}{state.num_warnings} warnings were found in the class reference XML. Please check the messages above.{Ansi.RESET}"
        )
    elif state.num_warnings == 1:
        print(
            f"{Ansi.YELLOW}1 warning was found in the class reference XML. Please check the messages above.{Ansi.RESET}"
        )

    if state.num_errors >= 2:
        print(
            f"{Ansi.RED}{state.num_errors} errors were found in the class reference XML. Please check the messages above.{Ansi.RESET}"
        )
    elif state.num_errors == 1:
        print(f"{Ansi.RED}1 error was found in the class reference XML. Please check the messages above.{Ansi.RESET}")

    if state.num_warnings == 0 and state.num_errors == 0:
        print(f"{Ansi.GREEN}No warnings or errors found in the class reference XML.{Ansi.RESET}")
        if not args.dry_run:
            print(f"Wrote reStructuredText files for each class to: {args.output}")
    else:
        exit(1)


# Common helpers.


def print_error(error: str, state: State) -> None:
    print(f"{Ansi.RED}{Ansi.BOLD}ERROR:{Ansi.REGULAR} {error}{Ansi.RESET}")
    state.num_errors += 1


def print_warning(warning: str, state: State) -> None:
    print(f"{Ansi.YELLOW}{Ansi.BOLD}WARNING:{Ansi.REGULAR} {warning}{Ansi.RESET}")
    state.num_warnings += 1


def translate(string: str) -> str:
    """Translate a string based on translations sourced from `doc/translations/*.po`
    for a language if defined via the --lang command line argument.
    Returns the original string if no translation exists.
    """
    return strings_l10n.get(string, string)


def get_git_branch() -> str:
    if hasattr(version, "docs") and version.docs != "latest":
        return version.docs

    return "master"


# Generator methods.


def make_rst_class(class_def: ClassDef, state: State, dry_run: bool, output_dir: str) -> None:
    class_name = class_def.name
    with open(
        os.devnull if dry_run else os.path.join(output_dir, f"class_{sanitize_class_name(class_name, True)}.rst"),
        "w",
        encoding="utf-8",
        newline="\n",
    ) as f:
        # Remove the "Edit on Github" button from the online docs page.
        f.write(":github_url: hide\n\n")

        # Add keywords metadata.
        if class_def.keywords is not None and class_def.keywords != "":
            f.write(f".. meta::\n\t:keywords: {class_def.keywords}\n\n")

        # Warn contributors not to edit this file directly.
        # Also provide links to the source files for reference.

        git_branch = get_git_branch()
        source_xml_path = os.path.relpath(class_def.filepath, root_directory).replace("\\", "/")
        source_github_url = f"https://github.com/godotengine/godot/tree/{git_branch}/{source_xml_path}"
        generator_github_url = f"https://github.com/godotengine/godot/tree/{git_branch}/doc/tools/make_rst.py"

        f.write(".. DO NOT EDIT THIS FILE!!!\n")
        f.write(".. Generated automatically from Godot engine sources.\n")
        f.write(f".. Generator: {generator_github_url}.\n")
        f.write(f".. XML source: {source_github_url}.\n\n")

        # Document reference id and header.
        f.write(f".. _class_{sanitize_class_name(class_name)}:\n\n")
        f.write(make_heading(class_name, "=", False))

        f.write(make_deprecated_experimental(class_def, state))

        ### INHERITANCE TREE ###

        # Ascendants
        if class_def.inherits:
            inherits = class_def.inherits.strip()
            f.write(f'**{translate("Inherits:")}** ')
            first = True
            while inherits is not None:
                if not first:
                    f.write(" **<** ")
                else:
                    first = False

                f.write(make_type(inherits, state))

                if inherits not in state.classes:
                    break  # Parent unknown.

                inode = state.classes[inherits].inherits
                if inode:
                    inherits = inode.strip()
                else:
                    break
            f.write("\n\n")

        # Descendants
        inherited: List[str] = []
        for c in state.classes.values():
            if c.inherits and c.inherits.strip() == class_name:
                inherited.append(c.name)

        if len(inherited):
            f.write(f'**{translate("Inherited By:")}** ')
            for i, child in enumerate(inherited):
                if i > 0:
                    f.write(", ")
                f.write(make_type(child, state))
            f.write("\n\n")

        ### INTRODUCTION ###

        has_description = False

        # Brief description
        if class_def.brief_description is not None and class_def.brief_description.strip() != "":
            has_description = True

            f.write(f"{format_text_block(class_def.brief_description.strip(), class_def, state)}\n\n")

        # Class description
        if class_def.description is not None and class_def.description.strip() != "":
            has_description = True

            f.write(".. rst-class:: classref-introduction-group\n\n")
            f.write(make_heading("Description", "-"))

            f.write(f"{format_text_block(class_def.description.strip(), class_def, state)}\n\n")

        if not has_description:
            f.write(".. container:: contribute\n\n\t")
            f.write(
                translate(
                    "There is currently no description for this class. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!"
                )
                + "\n\n"
            )

        if class_def.name in CLASSES_WITH_CSHARP_DIFFERENCES:
            f.write(".. note::\n\n\t")
            f.write(
                translate(
                    "There are notable differences when using this API with C#. See :ref:`doc_c_sharp_differences` for more information."
                )
                + "\n\n"
            )

        # Online tutorials
        if len(class_def.tutorials) > 0:
            f.write(".. rst-class:: classref-introduction-group\n\n")
            f.write(make_heading("Tutorials", "-"))

            for url, title in class_def.tutorials:
                f.write(f"- {make_link(url, title)}\n\n")

        ### REFERENCE TABLES ###

        # Reused container for reference tables.
        ml: List[Tuple[Optional[str], ...]] = []

        # Properties reference table
        if len(class_def.properties) > 0:
            f.write(".. rst-class:: classref-reftable-group\n\n")
            f.write(make_heading("Properties", "-"))

            ml = []
            for property_def in class_def.properties.values():
                type_rst = property_def.type_name.to_rst(state)
                default = property_def.default_value
                if default is not None and property_def.overrides:
                    ref = f":ref:`{property_def.overrides}<class_{sanitize_class_name(property_def.overrides)}_property_{property_def.name}>`"
                    # Not using translate() for now as it breaks table formatting.
                    ml.append((type_rst, property_def.name, f"{default} (overrides {ref})"))
                else:
                    ref = f":ref:`{property_def.name}<class_{sanitize_class_name(class_name)}_property_{property_def.name}>`"
                    ml.append((type_rst, ref, default))

            format_table(f, ml, True)

        # Constructors, Methods, Operators reference tables
        if len(class_def.constructors) > 0:
            f.write(".. rst-class:: classref-reftable-group\n\n")
            f.write(make_heading("Constructors", "-"))

            ml = []
            for method_list in class_def.constructors.values():
                for m in method_list:
                    ml.append(make_method_signature(class_def, m, "constructor", state))

            format_table(f, ml)

        if len(class_def.methods) > 0:
            f.write(".. rst-class:: classref-reftable-group\n\n")
            f.write(make_heading("Methods", "-"))

            ml = []
            for method_list in class_def.methods.values():
                for m in method_list:
                    ml.append(make_method_signature(class_def, m, "method", state))

            format_table(f, ml)

        if len(class_def.operators) > 0:
            f.write(".. rst-class:: classref-reftable-group\n\n")
            f.write(make_heading("Operators", "-"))

            ml = []
            for method_list in class_def.operators.values():
                for m in method_list:
                    ml.append(make_method_signature(class_def, m, "operator", state))

            format_table(f, ml)

        # Theme properties reference table
        if len(class_def.theme_items) > 0:
            f.write(".. rst-class:: classref-reftable-group\n\n")
            f.write(make_heading("Theme Properties", "-"))

            ml = []
            for theme_item_def in class_def.theme_items.values():
                ref = f":ref:`{theme_item_def.name}<class_{sanitize_class_name(class_name)}_theme_{theme_item_def.data_name}_{theme_item_def.name}>`"
                ml.append((theme_item_def.type_name.to_rst(state), ref, theme_item_def.default_value))

            format_table(f, ml, True)

        ### DETAILED DESCRIPTIONS ###

        # Signal descriptions
        if len(class_def.signals) > 0:
            f.write(make_separator(True))
            f.write(".. rst-class:: classref-descriptions-group\n\n")
            f.write(make_heading("Signals", "-"))

            index = 0

            for signal in class_def.signals.values():
                if index != 0:
                    f.write(make_separator())

                # Create signal signature and anchor point.

                signal_anchor = f"class_{sanitize_class_name(class_name)}_signal_{signal.name}"
                f.write(f".. _{signal_anchor}:\n\n")
                self_link = f":ref:`🔗<{signal_anchor}>`"
                f.write(".. rst-class:: classref-signal\n\n")

                _, signature = make_method_signature(class_def, signal, "", state)
                f.write(f"{signature} {self_link}\n\n")

                # Add signal description, or a call to action if it's missing.

                f.write(make_deprecated_experimental(signal, state))

                if signal.description is not None and signal.description.strip() != "":
                    f.write(f"{format_text_block(signal.description.strip(), signal, state)}\n\n")
                elif signal.deprecated is None and signal.experimental is None:
                    f.write(".. container:: contribute\n\n\t")
                    f.write(
                        translate(
                            "There is currently no description for this signal. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!"
                        )
                        + "\n\n"
                    )

                index += 1

        # Enumeration descriptions
        if len(class_def.enums) > 0:
            f.write(make_separator(True))
            f.write(".. rst-class:: classref-descriptions-group\n\n")
            f.write(make_heading("Enumerations", "-"))

            index = 0

            for e in class_def.enums.values():
                if index != 0:
                    f.write(make_separator())

                # Create enumeration signature and anchor point.

                enum_anchor = f"enum_{sanitize_class_name(class_name)}_{e.name}"
                f.write(f".. _{enum_anchor}:\n\n")
                self_link = f":ref:`🔗<{enum_anchor}>`"
                f.write(".. rst-class:: classref-enumeration\n\n")

                if e.is_bitfield:
                    f.write(f"flags **{e.name}**: {self_link}\n\n")
                else:
                    f.write(f"enum **{e.name}**: {self_link}\n\n")

                for value in e.values.values():
                    # Also create signature and anchor point for each enum constant.

                    f.write(f".. _class_{sanitize_class_name(class_name)}_constant_{value.name}:\n\n")
                    f.write(".. rst-class:: classref-enumeration-constant\n\n")

                    f.write(f"{e.type_name.to_rst(state)} **{value.name}** = ``{value.value}``\n\n")

                    # Add enum constant description.

                    f.write(make_deprecated_experimental(value, state))

                    if value.text is not None and value.text.strip() != "":
                        f.write(f"{format_text_block(value.text.strip(), value, state)}")
                    elif value.deprecated is None and value.experimental is None:
                        f.write(".. container:: contribute\n\n\t")
                        f.write(
                            translate(
                                "There is currently no description for this enum. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!"
                            )
                            + "\n\n"
                        )

                    f.write("\n\n")

                index += 1

        # Constant descriptions
        if len(class_def.constants) > 0:
            f.write(make_separator(True))
            f.write(".. rst-class:: classref-descriptions-group\n\n")
            f.write(make_heading("Constants", "-"))

            for constant in class_def.constants.values():
                # Create constant signature and anchor point.

                constant_anchor = f"class_{sanitize_class_name(class_name)}_constant_{constant.name}"
                f.write(f".. _{constant_anchor}:\n\n")
                self_link = f":ref:`🔗<{constant_anchor}>`"
                f.write(".. rst-class:: classref-constant\n\n")

                f.write(f"**{constant.name}** = ``{constant.value}`` {self_link}\n\n")

                # Add constant description.

                f.write(make_deprecated_experimental(constant, state))

                if constant.text is not None and constant.text.strip() != "":
                    f.write(f"{format_text_block(constant.text.strip(), constant, state)}")
                elif constant.deprecated is None and constant.experimental is None:
                    f.write(".. container:: contribute\n\n\t")
                    f.write(
                        translate(
                            "There is currently no description for this constant. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!"
                        )
                        + "\n\n"
                    )

                f.write("\n\n")

        # Annotation descriptions
        if len(class_def.annotations) > 0:
            f.write(make_separator(True))
            f.write(make_heading("Annotations", "-"))

            index = 0

            for method_list in class_def.annotations.values():  # type: ignore
                for i, m in enumerate(method_list):
                    if index != 0:
                        f.write(make_separator())

                    # Create annotation signature and anchor point.

                    self_link = ""
                    if i == 0:
                        annotation_anchor = f"class_{sanitize_class_name(class_name)}_annotation_{m.name}"
                        f.write(f".. _{annotation_anchor}:\n\n")
                        self_link = f" :ref:`🔗<{annotation_anchor}>`"

                    f.write(".. rst-class:: classref-annotation\n\n")

                    _, signature = make_method_signature(class_def, m, "", state)
                    f.write(f"{signature}{self_link}\n\n")

                    # Add annotation description, or a call to action if it's missing.

                    if m.description is not None and m.description.strip() != "":
                        f.write(f"{format_text_block(m.description.strip(), m, state)}\n\n")
                    else:
                        f.write(".. container:: contribute\n\n\t")
                        f.write(
                            translate(
                                "There is currently no description for this annotation. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!"
                            )
                            + "\n\n"
                        )

                    index += 1

        # Property descriptions
        if any(not p.overrides for p in class_def.properties.values()) > 0:
            f.write(make_separator(True))
            f.write(".. rst-class:: classref-descriptions-group\n\n")
            f.write(make_heading("Property Descriptions", "-"))

            index = 0

            for property_def in class_def.properties.values():
                if property_def.overrides:
                    continue

                if index != 0:
                    f.write(make_separator())

                # Create property signature and anchor point.

                property_anchor = f"class_{sanitize_class_name(class_name)}_property_{property_def.name}"
                f.write(f".. _{property_anchor}:\n\n")
                self_link = f":ref:`🔗<{property_anchor}>`"
                f.write(".. rst-class:: classref-property\n\n")

                property_default = ""
                if property_def.default_value is not None:
                    property_default = f" = {property_def.default_value}"
                f.write(
                    f"{property_def.type_name.to_rst(state)} **{property_def.name}**{property_default} {self_link}\n\n"
                )

                # Create property setter and getter records.

                property_setget = ""

                if property_def.setter is not None and not property_def.setter.startswith("_"):
                    property_setter = make_setter_signature(class_def, property_def, state)
                    property_setget += f"- {property_setter}\n"

                if property_def.getter is not None and not property_def.getter.startswith("_"):
                    property_getter = make_getter_signature(class_def, property_def, state)
                    property_setget += f"- {property_getter}\n"

                if property_setget != "":
                    f.write(".. rst-class:: classref-property-setget\n\n")
                    f.write(property_setget)
                    f.write("\n")

                # Add property description, or a call to action if it's missing.

                f.write(make_deprecated_experimental(property_def, state))

                if property_def.text is not None and property_def.text.strip() != "":
                    f.write(f"{format_text_block(property_def.text.strip(), property_def, state)}\n\n")
                elif property_def.deprecated is None and property_def.experimental is None:
                    f.write(".. container:: contribute\n\n\t")
                    f.write(
                        translate(
                            "There is currently no description for this property. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!"
                        )
                        + "\n\n"
                    )

                # Add copy note to built-in properties returning `Packed*Array`.
                if property_def.type_name.type_name in PACKED_ARRAY_TYPES:
                    copy_note = f"[b]Note:[/b] The returned array is [i]copied[/i] and any changes to it will not update the original property value. See [{property_def.type_name.type_name}] for more details."
                    f.write(f"{format_text_block(copy_note, property_def, state)}\n\n")

                index += 1

        # Constructor, Method, Operator descriptions
        if len(class_def.constructors) > 0:
            f.write(make_separator(True))
            f.write(".. rst-class:: classref-descriptions-group\n\n")
            f.write(make_heading("Constructor Descriptions", "-"))

            index = 0

            for method_list in class_def.constructors.values():
                for i, m in enumerate(method_list):
                    if index != 0:
                        f.write(make_separator())

                    # Create constructor signature and anchor point.

                    self_link = ""
                    if i == 0:
                        constructor_anchor = f"class_{sanitize_class_name(class_name)}_constructor_{m.name}"
                        f.write(f".. _{constructor_anchor}:\n\n")
                        self_link = f" :ref:`🔗<{constructor_anchor}>`"

                    f.write(".. rst-class:: classref-constructor\n\n")

                    ret_type, signature = make_method_signature(class_def, m, "", state)
                    f.write(f"{ret_type} {signature}{self_link}\n\n")

                    # Add constructor description, or a call to action if it's missing.

                    f.write(make_deprecated_experimental(m, state))

                    if m.description is not None and m.description.strip() != "":
                        f.write(f"{format_text_block(m.description.strip(), m, state)}\n\n")
                    elif m.deprecated is None and m.experimental is None:
                        f.write(".. container:: contribute\n\n\t")
                        f.write(
                            translate(
                                "There is currently no description for this constructor. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!"
                            )
                            + "\n\n"
                        )

                    index += 1

        if len(class_def.methods) > 0:
            f.write(make_separator(True))
            f.write(".. rst-class:: classref-descriptions-group\n\n")
            f.write(make_heading("Method Descriptions", "-"))

            index = 0

            for method_list in class_def.methods.values():
                for i, m in enumerate(method_list):
                    if index != 0:
                        f.write(make_separator())

                    # Create method signature and anchor point.

                    self_link = ""

                    if i == 0:
                        method_qualifier = ""
                        if m.name.startswith("_"):
                            method_qualifier = "private_"
                        method_anchor = f"class_{sanitize_class_name(class_name)}_{method_qualifier}method_{m.name}"
                        f.write(f".. _{method_anchor}:\n\n")
                        self_link = f" :ref:`🔗<{method_anchor}>`"

                    f.write(".. rst-class:: classref-method\n\n")

                    ret_type, signature = make_method_signature(class_def, m, "", state)

                    f.write(f"{ret_type} {signature}{self_link}\n\n")

                    # Add method description, or a call to action if it's missing.

                    f.write(make_deprecated_experimental(m, state))

                    if m.description is not None and m.description.strip() != "":
                        f.write(f"{format_text_block(m.description.strip(), m, state)}\n\n")
                    elif m.deprecated is None and m.experimental is None:
                        f.write(".. container:: contribute\n\n\t")
                        f.write(
                            translate(
                                "There is currently no description for this method. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!"
                            )
                            + "\n\n"
                        )

                    index += 1

        if len(class_def.operators) > 0:
            f.write(make_separator(True))
            f.write(".. rst-class:: classref-descriptions-group\n\n")
            f.write(make_heading("Operator Descriptions", "-"))

            index = 0

            for method_list in class_def.operators.values():
                for i, m in enumerate(method_list):
                    if index != 0:
                        f.write(make_separator())

                    # Create operator signature and anchor point.

                    operator_anchor = (
                        f"class_{sanitize_class_name(class_name)}_operator_{sanitize_operator_name(m.name, state)}"
                    )
                    for parameter in m.parameters:
                        operator_anchor += f"_{parameter.type_name.type_name}"
                    f.write(f".. _{operator_anchor}:\n\n")
                    self_link = f":ref:`🔗<{operator_anchor}>`"

                    f.write(".. rst-class:: classref-operator\n\n")

                    ret_type, signature = make_method_signature(class_def, m, "", state)
                    f.write(f"{ret_type} {signature} {self_link}\n\n")

                    # Add operator description, or a call to action if it's missing.

                    f.write(make_deprecated_experimental(m, state))

                    if m.description is not None and m.description.strip() != "":
                        f.write(f"{format_text_block(m.description.strip(), m, state)}\n\n")
                    elif m.deprecated is None and m.experimental is None:
                        f.write(".. container:: contribute\n\n\t")
                        f.write(
                            translate(
                                "There is currently no description for this operator. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!"
                            )
                            + "\n\n"
                        )

                    index += 1

        # Theme property descriptions
        if len(class_def.theme_items) > 0:
            f.write(make_separator(True))
            f.write(".. rst-class:: classref-descriptions-group\n\n")
            f.write(make_heading("Theme Property Descriptions", "-"))

            index = 0

            for theme_item_def in class_def.theme_items.values():
                if index != 0:
                    f.write(make_separator())

                # Create theme property signature and anchor point.

                theme_item_anchor = (
                    f"class_{sanitize_class_name(class_name)}_theme_{theme_item_def.data_name}_{theme_item_def.name}"
                )
                f.write(f".. _{theme_item_anchor}:\n\n")
                self_link = f":ref:`🔗<{theme_item_anchor}>`"
                f.write(".. rst-class:: classref-themeproperty\n\n")

                theme_item_default = ""
                if theme_item_def.default_value is not None:
                    theme_item_default = f" = {theme_item_def.default_value}"
                f.write(
                    f"{theme_item_def.type_name.to_rst(state)} **{theme_item_def.name}**{theme_item_default} {self_link}\n\n"
                )

                # Add theme property description, or a call to action if it's missing.

                f.write(make_deprecated_experimental(theme_item_def, state))

                if theme_item_def.text is not None and theme_item_def.text.strip() != "":
                    f.write(f"{format_text_block(theme_item_def.text.strip(), theme_item_def, state)}\n\n")
                elif theme_item_def.deprecated is None and theme_item_def.experimental is None:
                    f.write(".. container:: contribute\n\n\t")
                    f.write(
                        translate(
                            "There is currently no description for this theme property. Please help us by :ref:`contributing one <doc_updating_the_class_reference>`!"
                        )
                        + "\n\n"
                    )

                index += 1

        f.write(make_footer())


def make_type(klass: str, state: State) -> str:
    if klass.find("*") != -1:  # Pointer, ignore
        return f"``{klass}``"

    def resolve_type(link_type: str) -> str:
        if link_type in state.classes:
            return f":ref:`{link_type}<class_{sanitize_class_name(link_type)}>`"
        else:
            print_error(f'{state.current_class}.xml: Unresolved type "{link_type}".', state)
            return f"``{link_type}``"

    if klass.endswith("[]"):  # Typed array, strip [] to link to contained type.
        return f":ref:`Array<class_Array>`\\[{resolve_type(klass[:-len('[]')])}\\]"

    if klass.startswith("Dictionary["):  # Typed dictionary, split elements to link contained types.
        parts = klass[len("Dictionary[") : -len("]")].partition(", ")
        key = parts[0]
        value = parts[2]
        return f":ref:`Dictionary<class_Dictionary>`\\[{resolve_type(key)}, {resolve_type(value)}\\]"

    return resolve_type(klass)


def make_enum(t: str, is_bitfield: bool, state: State) -> str:
    p = t.rfind(".")
    if p >= 0:
        c = t[0:p]
        e = t[p + 1 :]
        # Variant enums live in GlobalScope but still use periods.
        if c == "Variant":
            c = "@GlobalScope"
            e = "Variant." + e
    else:
        c = state.current_class
        e = t
        if c in state.classes and e not in state.classes[c].enums:
            c = "@GlobalScope"

    if c in state.classes and e in state.classes[c].enums:
        if is_bitfield:
            if not state.classes[c].enums[e].is_bitfield:
                print_error(f'{state.current_class}.xml: Enum "{t}" is not bitfield.', state)
            return f"|bitfield|\\[:ref:`{e}<enum_{sanitize_class_name(c)}_{e}>`\\]"
        else:
            return f":ref:`{e}<enum_{sanitize_class_name(c)}_{e}>`"

    print_error(f'{state.current_class}.xml: Unresolved enum "{t}".', state)

    return t


def make_method_signature(
    class_def: ClassDef, definition: Union[AnnotationDef, MethodDef, SignalDef], ref_type: str, state: State
) -> Tuple[str, str]:
    ret_type = ""

    if isinstance(definition, MethodDef):
        ret_type = definition.return_type.to_rst(state)

    qualifiers = None
    if isinstance(definition, (MethodDef, AnnotationDef)):
        qualifiers = definition.qualifiers

    out = ""
    if isinstance(definition, MethodDef) and ref_type != "":
        if ref_type == "operator":
            op_name = definition.name.replace("<", "\\<")  # So operator "<" gets correctly displayed.
            out += f":ref:`{op_name}<class_{sanitize_class_name(class_def.name)}_{ref_type}_{sanitize_operator_name(definition.name, state)}"
            for parameter in definition.parameters:
                out += f"_{parameter.type_name.type_name}"
            out += ">`"
        elif ref_type == "method":
            ref_type_qualifier = ""
            if definition.name.startswith("_"):
                ref_type_qualifier = "private_"
            out += f":ref:`{definition.name}<class_{sanitize_class_name(class_def.name)}_{ref_type_qualifier}{ref_type}_{definition.name}>`"
        else:
            out += f":ref:`{definition.name}<class_{sanitize_class_name(class_def.name)}_{ref_type}_{definition.name}>`"
    else:
        out += f"**{definition.name}**"

    out += "\\ ("
    for i, arg in enumerate(definition.parameters):
        if i > 0:
            out += ", "
        else:
            out += "\\ "

        out += f"{arg.name}\\: {arg.type_name.to_rst(state)}"

        if arg.default_value is not None:
            out += f" = {arg.default_value}"

    if qualifiers is not None and "vararg" in qualifiers:
        if len(definition.parameters) > 0:
            out += ", ..."
        else:
            out += "\\ ..."

    out += "\\ )"

    if qualifiers is not None:
        # Use substitutions for abbreviations. This is used to display tooltips on hover.
        # See `make_footer()` for descriptions.
        for qualifier in qualifiers.split():
            out += f" |{qualifier}|"

    return ret_type, out


def make_setter_signature(class_def: ClassDef, property_def: PropertyDef, state: State) -> str:
    if property_def.setter is None:
        return ""

    # If setter is a method available as a method definition, we use that.
    if property_def.setter in class_def.methods:
        setter = class_def.methods[property_def.setter][0]
    # Otherwise we fake it with the information we have available.
    else:
        setter_params: List[ParameterDef] = []
        setter_params.append(ParameterDef("value", property_def.type_name, None))
        setter = MethodDef(property_def.setter, TypeName("void"), setter_params, None, None)

    ret_type, signature = make_method_signature(class_def, setter, "", state)
    return f"{ret_type} {signature}"


def make_getter_signature(class_def: ClassDef, property_def: PropertyDef, state: State) -> str:
    if property_def.getter is None:
        return ""

    # If getter is a method available as a method definition, we use that.
    if property_def.getter in class_def.methods:
        getter = class_def.methods[property_def.getter][0]
    # Otherwise we fake it with the information we have available.
    else:
        getter_params: List[ParameterDef] = []
        getter = MethodDef(property_def.getter, property_def.type_name, getter_params, None, None)

    ret_type, signature = make_method_signature(class_def, getter, "", state)
    return f"{ret_type} {signature}"


def make_deprecated_experimental(item: DefinitionBase, state: State) -> str:
    result = ""

    if item.deprecated is not None:
        deprecated_prefix = translate("Deprecated:")
        if item.deprecated.strip() == "":
            default_message = translate(f"This {item.definition_name} may be changed or removed in future versions.")
            result += f"**{deprecated_prefix}** {default_message}\n\n"
        else:
            result += f"**{deprecated_prefix}** {format_text_block(item.deprecated.strip(), item, state)}\n\n"

    if item.experimental is not None:
        experimental_prefix = translate("Experimental:")
        if item.experimental.strip() == "":
            default_message = translate(f"This {item.definition_name} may be changed or removed in future versions.")
            result += f"**{experimental_prefix}** {default_message}\n\n"
        else:
            result += f"**{experimental_prefix}** {format_text_block(item.experimental.strip(), item, state)}\n\n"

    return result


def make_heading(title: str, underline: str, l10n: bool = True) -> str:
    if l10n:
        new_title = translate(title)
        if new_title != title:
            title = new_title
            underline *= 2  # Double length to handle wide chars.
    return f"{title}\n{(underline * len(title))}\n\n"


def make_footer() -> str:
    # Generate reusable abbreviation substitutions.
    # This way, we avoid bloating the generated rST with duplicate abbreviations.
    virtual_msg = translate("This method should typically be overridden by the user to have any effect.")
    const_msg = translate("This method has no side effects. It doesn't modify any of the instance's member variables.")
    vararg_msg = translate("This method accepts any number of arguments after the ones described here.")
    constructor_msg = translate("This method is used to construct a type.")
    static_msg = translate(
        "This method doesn't need an instance to be called, so it can be called directly using the class name."
    )
    operator_msg = translate("This method describes a valid operator to use with this type as left-hand operand.")
    bitfield_msg = translate("This value is an integer composed as a bitmask of the following flags.")
    void_msg = translate("No return value.")

    return (
        f".. |virtual| replace:: :abbr:`virtual ({virtual_msg})`\n"
        f".. |const| replace:: :abbr:`const ({const_msg})`\n"
        f".. |vararg| replace:: :abbr:`vararg ({vararg_msg})`\n"
        f".. |constructor| replace:: :abbr:`constructor ({constructor_msg})`\n"
        f".. |static| replace:: :abbr:`static ({static_msg})`\n"
        f".. |operator| replace:: :abbr:`operator ({operator_msg})`\n"
        f".. |bitfield| replace:: :abbr:`BitField ({bitfield_msg})`\n"
        f".. |void| replace:: :abbr:`void ({void_msg})`\n"
    )


def make_separator(section_level: bool = False) -> str:
    separator_class = "item"
    if section_level:
        separator_class = "section"

    return f".. rst-class:: classref-{separator_class}-separator\n\n----\n\n"


def make_link(url: str, title: str) -> str:
    match = GODOT_DOCS_PATTERN.search(url)
    if match:
        groups = match.groups()
        if match.lastindex == 2:
            # Doc reference with fragment identifier: emit direct link to section with reference to page, for example:
            # `#calling-javascript-from-script in Exporting For Web`
            # Or use the title if provided.
            if title != "":
                return f"`{title} <../{groups[0]}.html{groups[1]}>`__"
            return f"`{groups[1]} <../{groups[0]}.html{groups[1]}>`__ in :doc:`../{groups[0]}`"
        elif match.lastindex == 1:
            # Doc reference, for example:
            # `Math`
            if title != "":
                return f":doc:`{title} <../{groups[0]}>`"
            return f":doc:`../{groups[0]}`"

    # External link, for example:
    # `http://enet.bespin.org/usergroup0.html`
    if title != "":
        return f"`{title} <{url}>`__"
    return f"`{url} <{url}>`__"


def make_rst_index(grouped_classes: Dict[str, List[str]], dry_run: bool, output_dir: str) -> None:
    with open(
        os.devnull if dry_run else os.path.join(output_dir, "index.rst"), "w", encoding="utf-8", newline="\n"
    ) as f:
        # Remove the "Edit on Github" button from the online docs page, and disallow user-contributed notes
        # on the index page. User-contributed notes are allowed on individual class pages.
        f.write(":github_url: hide\n:allow_comments: False\n\n")

        # Warn contributors not to edit this file directly.
        # Also provide links to the source files for reference.

        git_branch = get_git_branch()
        generator_github_url = f"https://github.com/godotengine/godot/tree/{git_branch}/doc/tools/make_rst.py"

        f.write(".. DO NOT EDIT THIS FILE!!!\n")
        f.write(".. Generated automatically from Godot engine sources.\n")
        f.write(f".. Generator: {generator_github_url}.\n\n")

        f.write(".. _doc_class_reference:\n\n")

        f.write(make_heading("All classes", "="))

        for group_name in CLASS_GROUPS:
            if group_name in grouped_classes:
                f.write(make_heading(CLASS_GROUPS[group_name], "="))

                f.write(".. toctree::\n")
                f.write("    :maxdepth: 1\n")
                f.write(f"    :name: toc-class-ref-{group_name}s\n")
                f.write("\n")

                if group_name in CLASS_GROUPS_BASE:
                    f.write(f"    class_{sanitize_class_name(CLASS_GROUPS_BASE[group_name], True)}\n")

                for class_name in grouped_classes[group_name]:
                    if group_name in CLASS_GROUPS_BASE and sanitize_class_name(
                        CLASS_GROUPS_BASE[group_name], True
                    ) == sanitize_class_name(class_name, True):
                        continue

                    f.write(f"    class_{sanitize_class_name(class_name, True)}\n")

                f.write("\n")


# Formatting helpers.


RESERVED_FORMATTING_TAGS = ["i", "b", "u", "lb", "rb", "code", "kbd", "center", "url", "br"]
RESERVED_LAYOUT_TAGS = ["codeblocks"]
RESERVED_CODEBLOCK_TAGS = ["codeblock", "gdscript", "csharp"]
RESERVED_CROSSLINK_TAGS = [
    "method",
    "constructor",
    "operator",
    "member",
    "signal",
    "constant",
    "enum",
    "annotation",
    "theme_item",
    "param",
]


def is_in_tagset(tag_text: str, tagset: List[str]) -> bool:
    for tag in tagset:
        # Complete match.
        if tag_text == tag:
            return True
        # Tag with arguments.
        if tag_text.startswith(tag + " "):
            return True
        # Tag with arguments, special case for [url], [color], and [font].
        if tag_text.startswith(tag + "="):
            return True

    return False


def get_tag_and_args(tag_text: str) -> TagState:
    tag_name = tag_text
    arguments: str = ""

    delim_pos = -1

    space_pos = tag_text.find(" ")
    if space_pos >= 0:
        delim_pos = space_pos

    # Special case for [url], [color], and [font].
    assign_pos = tag_text.find("=")
    if assign_pos >= 0 and (delim_pos < 0 or assign_pos < delim_pos):
        delim_pos = assign_pos

    if delim_pos >= 0:
        tag_name = tag_text[:delim_pos]
        arguments = tag_text[delim_pos + 1 :].strip()

    closing = False
    if tag_name.startswith("/"):
        tag_name = tag_name[1:]
        closing = True

    return TagState(tag_text, tag_name, arguments, closing)


def parse_link_target(link_target: str, state: State, context_name: str) -> List[str]:
    if link_target.find(".") != -1:
        return link_target.split(".")
    else:
        return [state.current_class, link_target]


def format_text_block(
    text: str,
    context: DefinitionBase,
    state: State,
) -> str:
    # Linebreak + tabs in the XML should become two line breaks unless in a "codeblock"
    pos = 0
    while True:
        pos = text.find("\n", pos)
        if pos == -1:
            break

        pre_text = text[:pos]
        indent_level = 0
        while pos + 1 < len(text) and text[pos + 1] == "\t":
            pos += 1
            indent_level += 1
        post_text = text[pos + 1 :]

        # Handle codeblocks
        if (
            post_text.startswith("[codeblock]")
            or post_text.startswith("[codeblock ")
            or post_text.startswith("[gdscript]")
            or post_text.startswith("[gdscript ")
            or post_text.startswith("[csharp]")
            or post_text.startswith("[csharp ")
        ):
            tag_text = post_text[1:].split("]", 1)[0]
            tag_state = get_tag_and_args(tag_text)
            result = format_codeblock(tag_state, post_text, indent_level, state)
            if result is None:
                return ""
            text = f"{pre_text}{result[0]}"
            pos += result[1] - indent_level

        # Handle normal text
        else:
            text = f"{pre_text}\n\n{post_text}"
            pos += 2 - indent_level

    next_brac_pos = text.find("[")
    text = escape_rst(text, next_brac_pos)

    context_name = format_context_name(context)

    # Handle [tags]
    inside_code = False
    inside_code_tag = ""
    inside_code_tabs = False
    ignore_code_warnings = False
    code_warning_if_intended_string = "If this is intended, use [code skip-lint]...[/code]."

    has_codeblocks_gdscript = False
    has_codeblocks_csharp = False

    pos = 0
    tag_depth = 0
    while True:
        pos = text.find("[", pos)
        if pos == -1:
            break

        endq_pos = text.find("]", pos + 1)
        if endq_pos == -1:
            break

        pre_text = text[:pos]
        post_text = text[endq_pos + 1 :]
        tag_text = text[pos + 1 : endq_pos]

        escape_pre = False
        escape_post = False

        # Tag is a reference to a class.
        if tag_text in state.classes and not inside_code:
            if tag_text == state.current_class:
                # Don't create a link to the same class, format it as strong emphasis.
                tag_text = f"**{tag_text}**"
            else:
                tag_text = make_type(tag_text, state)
            escape_pre = True
            escape_post = True

        # Tag is a cross-reference or a formatting directive.
        else:
            tag_state = get_tag_and_args(tag_text)

            # Anything identified as a tag inside of a code block is valid,
            # unless it's a matching closing tag.
            if inside_code:
                # Exiting codeblocks and inline code tags.

                if tag_state.closing and tag_state.name == inside_code_tag:
                    if is_in_tagset(tag_state.name, RESERVED_CODEBLOCK_TAGS):
                        tag_text = ""
                        tag_depth -= 1
                        inside_code = False
                        ignore_code_warnings = False
                        # Strip newline if the tag was alone on one
                        if pre_text[-1] == "\n":
                            pre_text = pre_text[:-1]

                    elif is_in_tagset(tag_state.name, ["code"]):
                        tag_text = "``"
                        tag_depth -= 1
                        inside_code = False
                        ignore_code_warnings = False
                        escape_post = True

                else:
                    if not ignore_code_warnings and tag_state.closing:
                        print_warning(
                            f'{state.current_class}.xml: Found a code string that looks like a closing tag "[{tag_state.raw}]" in {context_name}. {code_warning_if_intended_string}',
                            state,
                        )

                    tag_text = f"[{tag_text}]"

            # Entering codeblocks and inline code tags.

            elif tag_state.name == "codeblocks":
                if tag_state.closing:
                    if not has_codeblocks_gdscript or not has_codeblocks_csharp:
                        state.script_language_parity_check.add_hit(
                            state.current_class,
                            context,
                            "Only one script language sample found in [codeblocks]",
                            state,
                        )

                    has_codeblocks_gdscript = False
                    has_codeblocks_csharp = False

                    tag_depth -= 1
                    tag_text = ""
                    inside_code_tabs = False
                else:
                    tag_depth += 1
                    tag_text = "\n.. tabs::"
                    inside_code_tabs = True

            elif is_in_tagset(tag_state.name, RESERVED_CODEBLOCK_TAGS):
                tag_depth += 1

                if tag_state.name == "gdscript":
                    if not inside_code_tabs:
                        print_error(
                            f"{state.current_class}.xml: GDScript code block is used outside of [codeblocks] in {context_name}.",
                            state,
                        )
                    else:
                        has_codeblocks_gdscript = True
                    tag_text = "\n .. code-tab:: gdscript\n"
                elif tag_state.name == "csharp":
                    if not inside_code_tabs:
                        print_error(
                            f"{state.current_class}.xml: C# code block is used outside of [codeblocks] in {context_name}.",
                            state,
                        )
                    else:
                        has_codeblocks_csharp = True
                    tag_text = "\n .. code-tab:: csharp\n"
                else:
                    state.script_language_parity_check.add_hit(
                        state.current_class,
                        context,
                        "Code sample is formatted with [codeblock] where [codeblocks] should be used",
                        state,
                    )

                    if "lang=text" in tag_state.arguments.split(" "):
                        tag_text = "\n.. code:: text\n"
                    else:
                        tag_text = "\n::\n"

                inside_code = True
                inside_code_tag = tag_state.name
                ignore_code_warnings = "skip-lint" in tag_state.arguments.split(" ")

            elif is_in_tagset(tag_state.name, ["code"]):
                tag_text = "``"
                tag_depth += 1

                inside_code = True
                inside_code_tag = "code"
                ignore_code_warnings = "skip-lint" in tag_state.arguments.split(" ")
                escape_pre = True

                if not ignore_code_warnings:
                    endcode_pos = text.find("[/code]", endq_pos + 1)
                    if endcode_pos == -1:
                        print_error(
                            f"{state.current_class}.xml: Tag depth mismatch for [code]: no closing [/code] in {context_name}.",
                            state,
                        )
                        break

                    inside_code_text = text[endq_pos + 1 : endcode_pos]
                    if inside_code_text.endswith("()"):
                        # It's formatted like a call for some reason, may still be a mistake.
                        inside_code_text = inside_code_text[:-2]

                    if inside_code_text in state.classes:
                        print_warning(
                            f'{state.current_class}.xml: Found a code string "{inside_code_text}" that matches one of the known classes in {context_name}. {code_warning_if_intended_string}',
                            state,
                        )

                    target_class_name, target_name, *rest = parse_link_target(inside_code_text, state, context_name)
                    if len(rest) == 0 and target_class_name in state.classes:
                        class_def = state.classes[target_class_name]

                        if target_name in class_def.methods:
                            print_warning(
                                f'{state.current_class}.xml: Found a code string "{inside_code_text}" that matches the {target_class_name}.{target_name} method in {context_name}. {code_warning_if_intended_string}',
                                state,
                            )

                        elif target_name in class_def.constructors:
                            print_warning(
                                f'{state.current_class}.xml: Found a code string "{inside_code_text}" that matches the {target_class_name}.{target_name} constructor in {context_name}. {code_warning_if_intended_string}',
                                state,
                            )

                        elif target_name in class_def.operators:
                            print_warning(
                                f'{state.current_class}.xml: Found a code string "{inside_code_text}" that matches the {target_class_name}.{target_name} operator in {context_name}. {code_warning_if_intended_string}',
                                state,
                            )

                        elif target_name in class_def.properties:
                            print_warning(
                                f'{state.current_class}.xml: Found a code string "{inside_code_text}" that matches the {target_class_name}.{target_name} member in {context_name}. {code_warning_if_intended_string}',
                                state,
                            )

                        elif target_name in class_def.signals:
                            print_warning(
                                f'{state.current_class}.xml: Found a code string "{inside_code_text}" that matches the {target_class_name}.{target_name} signal in {context_name}. {code_warning_if_intended_string}',
                                state,
                            )

                        elif target_name in class_def.annotations:
                            print_warning(
                                f'{state.current_class}.xml: Found a code string "{inside_code_text}" that matches the {target_class_name}.{target_name} annotation in {context_name}. {code_warning_if_intended_string}',
                                state,
                            )

                        elif target_name in class_def.theme_items:
                            print_warning(
                                f'{state.current_class}.xml: Found a code string "{inside_code_text}" that matches the {target_class_name}.{target_name} theme property in {context_name}. {code_warning_if_intended_string}',
                                state,
                            )

                        elif target_name in class_def.constants:
                            print_warning(
                                f'{state.current_class}.xml: Found a code string "{inside_code_text}" that matches the {target_class_name}.{target_name} constant in {context_name}. {code_warning_if_intended_string}',
                                state,
                            )

                        else:
                            for enum in class_def.enums.values():
                                if target_name in enum.values:
                                    print_warning(
                                        f'{state.current_class}.xml: Found a code string "{inside_code_text}" that matches the {target_class_name}.{target_name} enum value in {context_name}. {code_warning_if_intended_string}',
                                        state,
                                    )
                                    break

                    valid_param_context = isinstance(context, (MethodDef, SignalDef, AnnotationDef))
                    if valid_param_context:
                        context_params: List[ParameterDef] = context.parameters  # type: ignore
                        for param_def in context_params:
                            if param_def.name == inside_code_text:
                                print_warning(
                                    f'{state.current_class}.xml: Found a code string "{inside_code_text}" that matches one of the parameters in {context_name}. {code_warning_if_intended_string}',
                                    state,
                                )
                                break

            # Cross-references to items in this or other class documentation pages.
            elif is_in_tagset(tag_state.name, RESERVED_CROSSLINK_TAGS):
                link_target: str = tag_state.arguments

                if link_target == "":
                    print_error(
                        f'{state.current_class}.xml: Empty cross-reference link "[{tag_state.raw}]" in {context_name}.',
                        state,
                    )
                    tag_text = ""
                else:
                    if (
                        tag_state.name == "method"
                        or tag_state.name == "constructor"
                        or tag_state.name == "operator"
                        or tag_state.name == "member"
                        or tag_state.name == "signal"
                        or tag_state.name == "annotation"
                        or tag_state.name == "theme_item"
                        or tag_state.name == "constant"
                    ):
                        target_class_name, target_name, *rest = parse_link_target(link_target, state, context_name)
                        if len(rest) > 0:
                            print_error(
                                f'{state.current_class}.xml: Bad reference "{link_target}" in {context_name}.',
                                state,
                            )

                        # Default to the tag command name. This works by default for most tags,
                        # but method, member, and theme_item have special cases.
                        ref_type = "_{}".format(tag_state.name)

                        if target_class_name in state.classes:
                            class_def = state.classes[target_class_name]

                            if tag_state.name == "method":
                                if target_name.startswith("_"):
                                    ref_type = "_private_method"

                                if target_name not in class_def.methods:
                                    print_error(
                                        f'{state.current_class}.xml: Unresolved method reference "{link_target}" in {context_name}.',
                                        state,
                                    )

                            elif tag_state.name == "constructor" and target_name not in class_def.constructors:
                                print_error(
                                    f'{state.current_class}.xml: Unresolved constructor reference "{link_target}" in {context_name}.',
                                    state,
                                )

                            elif tag_state.name == "operator" and target_name not in class_def.operators:
                                print_error(
                                    f'{state.current_class}.xml: Unresolved operator reference "{link_target}" in {context_name}.',
                                    state,
                                )

                            elif tag_state.name == "member":
                                ref_type = "_property"

                                if target_name not in class_def.properties:
                                    print_error(
                                        f'{state.current_class}.xml: Unresolved member reference "{link_target}" in {context_name}.',
                                        state,
                                    )

                            elif tag_state.name == "signal" and target_name not in class_def.signals:
                                print_error(
                                    f'{state.current_class}.xml: Unresolved signal reference "{link_target}" in {context_name}.',
                                    state,
                                )

                            elif tag_state.name == "annotation" and target_name not in class_def.annotations:
                                print_error(
                                    f'{state.current_class}.xml: Unresolved annotation reference "{link_target}" in {context_name}.',
                                    state,
                                )

                            elif tag_state.name == "theme_item":
                                if target_name not in class_def.theme_items:
                                    print_error(
                                        f'{state.current_class}.xml: Unresolved theme property reference "{link_target}" in {context_name}.',
                                        state,
                                    )
                                else:
                                    # Needs theme data type to be properly linked, which we cannot get without a class.
                                    name = class_def.theme_items[target_name].data_name
                                    ref_type = f"_theme_{name}"

                            elif tag_state.name == "constant":
                                found = False

                                # Search in the current class
                                search_class_defs = [class_def]

                                if link_target.find(".") == -1:
                                    # Also search in @GlobalScope as a last resort if no class was specified
                                    search_class_defs.append(state.classes["@GlobalScope"])

                                for search_class_def in search_class_defs:
                                    if target_name in search_class_def.constants:
                                        target_class_name = search_class_def.name
                                        found = True

                                    else:
                                        for enum in search_class_def.enums.values():
                                            if target_name in enum.values:
                                                target_class_name = search_class_def.name
                                                found = True
                                                break

                                if not found:
                                    print_error(
                                        f'{state.current_class}.xml: Unresolved constant reference "{link_target}" in {context_name}.',
                                        state,
                                    )

                        else:
                            print_error(
                                f'{state.current_class}.xml: Unresolved type reference "{target_class_name}" in method reference "{link_target}" in {context_name}.',
                                state,
                            )

                        repl_text = target_name
                        if target_class_name != state.current_class:
                            repl_text = f"{target_class_name}.{target_name}"
                        tag_text = f":ref:`{repl_text}<class_{sanitize_class_name(target_class_name)}{ref_type}_{target_name}>`"
                        escape_pre = True
                        escape_post = True

                    elif tag_state.name == "enum":
                        tag_text = make_enum(link_target, False, state)
                        escape_pre = True
                        escape_post = True

                    elif tag_state.name == "param":
                        valid_param_context = isinstance(context, (MethodDef, SignalDef, AnnotationDef))
                        if not valid_param_context:
                            print_error(
                                f'{state.current_class}.xml: Argument reference "{link_target}" used outside of method, signal, or annotation context in {context_name}.',
                                state,
                            )
                        else:
                            context_params: List[ParameterDef] = context.parameters  # type: ignore
                            found = False
                            for param_def in context_params:
                                if param_def.name == link_target:
                                    found = True
                                    break
                            if not found:
                                print_error(
                                    f'{state.current_class}.xml: Unresolved argument reference "{link_target}" in {context_name}.',
                                    state,
                                )

                        tag_text = f"``{link_target}``"
                        escape_pre = True
                        escape_post = True

            # Formatting directives.

            elif is_in_tagset(tag_state.name, ["url"]):
                url_target = tag_state.arguments

                if url_target == "":
                    print_error(
                        f'{state.current_class}.xml: Misformatted [url] tag "[{tag_state.raw}]" in {context_name}.',
                        state,
                    )
                else:
                    # Unlike other tags, URLs are handled in full here, as we need to extract
                    # the optional link title to use `make_link`.
                    endurl_pos = text.find("[/url]", endq_pos + 1)
                    if endurl_pos == -1:
                        print_error(
                            f"{state.current_class}.xml: Tag depth mismatch for [url]: no closing [/url] in {context_name}.",
                            state,
                        )
                        break
                    link_title = text[endq_pos + 1 : endurl_pos]
                    tag_text = make_link(url_target, link_title)

                    pre_text = text[:pos]
                    post_text = text[endurl_pos + 6 :]

                    if pre_text and pre_text[-1] not in MARKUP_ALLOWED_PRECEDENT:
                        pre_text += "\\ "
                    if post_text and post_text[0] not in MARKUP_ALLOWED_SUBSEQUENT:
                        post_text = "\\ " + post_text

                    text = pre_text + tag_text + post_text
                    pos = len(pre_text) + len(tag_text)
                    continue

            elif tag_state.name == "br":
                # Make a new paragraph instead of a linebreak, rst is not so linebreak friendly
                tag_text = "\n\n"
                # Strip potential leading spaces
                while post_text[0] == " ":
                    post_text = post_text[1:]

            elif tag_state.name == "center":
                if tag_state.closing:
                    tag_depth -= 1
                else:
                    tag_depth += 1
                tag_text = ""

            elif tag_state.name == "i":
                if tag_state.closing:
                    tag_depth -= 1
                    escape_post = True
                else:
                    tag_depth += 1
                    escape_pre = True
                tag_text = "*"

            elif tag_state.name == "b":
                if tag_state.closing:
                    tag_depth -= 1
                    escape_post = True
                else:
                    tag_depth += 1
                    escape_pre = True
                tag_text = "**"

            elif tag_state.name == "u":
                if tag_state.closing:
                    tag_depth -= 1
                    escape_post = True
                else:
                    tag_depth += 1
                    escape_pre = True
                tag_text = ""

            elif tag_state.name == "lb":
                tag_text = "\\["

            elif tag_state.name == "rb":
                tag_text = "\\]"

            elif tag_state.name == "kbd":
                tag_text = "`"
                if tag_state.closing:
                    tag_depth -= 1
                    escape_post = True
                else:
                    tag_text = ":kbd:" + tag_text
                    tag_depth += 1
                    escape_pre = True

            # Invalid syntax.
            else:
                if tag_state.closing:
                    print_error(
                        f'{state.current_class}.xml: Unrecognized closing tag "[{tag_state.raw}]" in {context_name}.',
                        state,
                    )

                    tag_text = f"[{tag_text}]"
                else:
                    print_error(
                        f'{state.current_class}.xml: Unrecognized opening tag "[{tag_state.raw}]" in {context_name}.',
                        state,
                    )

                    tag_text = f"``{tag_text}``"
                    escape_pre = True
                    escape_post = True

        # Properly escape things like `[Node]s`
        if escape_pre and pre_text and pre_text[-1] not in MARKUP_ALLOWED_PRECEDENT:
            pre_text += "\\ "
        if escape_post and post_text and post_text[0] not in MARKUP_ALLOWED_SUBSEQUENT:
            post_text = "\\ " + post_text

        next_brac_pos = post_text.find("[", 0)
        iter_pos = 0
        while not inside_code:
            iter_pos = post_text.find("*", iter_pos, next_brac_pos)
            if iter_pos == -1:
                break
            post_text = f"{post_text[:iter_pos]}\\*{post_text[iter_pos + 1 :]}"
            iter_pos += 2

        iter_pos = 0
        while not inside_code:
            iter_pos = post_text.find("_", iter_pos, next_brac_pos)
            if iter_pos == -1:
                break
            if not post_text[iter_pos + 1].isalnum():  # don't escape within a snake_case word
                post_text = f"{post_text[:iter_pos]}\\_{post_text[iter_pos + 1 :]}"
                iter_pos += 2
            else:
                iter_pos += 1

        text = pre_text + tag_text + post_text
        pos = len(pre_text) + len(tag_text)

    if tag_depth > 0:
        print_error(
            f"{state.current_class}.xml: Tag depth mismatch: too many (or too few) open/close tags in {context_name}.",
            state,
        )

    return text


def format_context_name(context: Union[DefinitionBase, None]) -> str:
    context_name: str = "unknown context"
    if context is not None:
        context_name = f'{context.definition_name} "{context.name}" description'

    return context_name


def escape_rst(text: str, until_pos: int = -1) -> str:
    # Escape \ character, otherwise it ends up as an escape character in rst
    pos = 0
    while True:
        pos = text.find("\\", pos, until_pos)
        if pos == -1:
            break
        text = f"{text[:pos]}\\\\{text[pos + 1 :]}"
        pos += 2

    # Escape * character to avoid interpreting it as emphasis
    pos = 0
    while True:
        pos = text.find("*", pos, until_pos)
        if pos == -1:
            break
        text = f"{text[:pos]}\\*{text[pos + 1 :]}"
        pos += 2

    # Escape _ character at the end of a word to avoid interpreting it as an inline hyperlink
    pos = 0
    while True:
        pos = text.find("_", pos, until_pos)
        if pos == -1:
            break
        if not text[pos + 1].isalnum():  # don't escape within a snake_case word
            text = f"{text[:pos]}\\_{text[pos + 1 :]}"
            pos += 2
        else:
            pos += 1

    return text


def format_codeblock(
    tag_state: TagState, post_text: str, indent_level: int, state: State
) -> Union[Tuple[str, int], None]:
    end_pos = post_text.find("[/" + tag_state.name + "]")
    if end_pos == -1:
        print_error(
            f"{state.current_class}.xml: Tag depth mismatch for [{tag_state.name}]: no closing [/{tag_state.name}].",
            state,
        )
        return None

    opening_formatted = tag_state.name
    if len(tag_state.arguments) > 0:
        opening_formatted += " " + tag_state.arguments

    code_text = post_text[len(f"[{opening_formatted}]") : end_pos]
    post_text = post_text[end_pos:]

    # Remove extraneous tabs
    code_pos = 0
    while True:
        code_pos = code_text.find("\n", code_pos)
        if code_pos == -1:
            break

        to_skip = 0
        while code_pos + to_skip + 1 < len(code_text) and code_text[code_pos + to_skip + 1] == "\t":
            to_skip += 1

        if to_skip > indent_level:
            print_error(
                f"{state.current_class}.xml: Four spaces should be used for indentation within [{tag_state.name}].",
                state,
            )

        if len(code_text[code_pos + to_skip + 1 :]) == 0:
            code_text = f"{code_text[:code_pos]}\n"
            code_pos += 1
        else:
            code_text = f"{code_text[:code_pos]}\n    {code_text[code_pos + to_skip + 1 :]}"
            code_pos += 5 - to_skip
    return (f"\n[{opening_formatted}]{code_text}{post_text}", len(f"\n[{opening_formatted}]{code_text}"))


def format_table(f: TextIO, data: List[Tuple[Optional[str], ...]], remove_empty_columns: bool = False) -> None:
    if len(data) == 0:
        return

    f.write(".. table::\n")
    f.write("   :widths: auto\n\n")

    # Calculate the width of each column first, we will use this information
    # to properly format RST-style tables.
    column_sizes = [0] * len(data[0])
    for row in data:
        for i, text in enumerate(row):
            text_length = len(text or "")
            if text_length > column_sizes[i]:
                column_sizes[i] = text_length

    # Each table row is wrapped in two separators, consecutive rows share the same separator.
    # All separators, or rather borders, have the same shape and content. We compose it once,
    # then reuse it.

    sep = ""
    for size in column_sizes:
        if size == 0 and remove_empty_columns:
            continue
        sep += "+" + "-" * (size + 2)  # Content of each cell is padded by 1 on each side.
    sep += "+\n"

    # Draw the first separator.
    f.write(f"   {sep}")

    # Draw each row and close it with a separator.
    for row in data:
        row_text = "|"
        for i, text in enumerate(row):
            if column_sizes[i] == 0 and remove_empty_columns:
                continue
            row_text += f' {(text or "").ljust(column_sizes[i])} |'
        row_text += "\n"

        f.write(f"   {row_text}")
        f.write(f"   {sep}")

    f.write("\n")


def sanitize_class_name(dirty_name: str, is_file_name=False) -> str:
    if is_file_name:
        return dirty_name.lower().replace('"', "").replace("/", "--")
    else:
        return dirty_name.replace('"', "").replace("/", "_").replace(".", "_")


def sanitize_operator_name(dirty_name: str, state: State) -> str:
    clear_name = dirty_name.replace("operator ", "")

    if clear_name == "!=":
        clear_name = "neq"
    elif clear_name == "==":
        clear_name = "eq"

    elif clear_name == "<":
        clear_name = "lt"
    elif clear_name == "<=":
        clear_name = "lte"
    elif clear_name == ">":
        clear_name = "gt"
    elif clear_name == ">=":
        clear_name = "gte"

    elif clear_name == "+":
        clear_name = "sum"
    elif clear_name == "-":
        clear_name = "dif"
    elif clear_name == "*":
        clear_name = "mul"
    elif clear_name == "/":
        clear_name = "div"
    elif clear_name == "%":
        clear_name = "mod"
    elif clear_name == "**":
        clear_name = "pow"

    elif clear_name == "unary+":
        clear_name = "unplus"
    elif clear_name == "unary-":
        clear_name = "unminus"

    elif clear_name == "<<":
        clear_name = "bwsl"
    elif clear_name == ">>":
        clear_name = "bwsr"
    elif clear_name == "&":
        clear_name = "bwand"
    elif clear_name == "|":
        clear_name = "bwor"
    elif clear_name == "^":
        clear_name = "bwxor"
    elif clear_name == "~":
        clear_name = "bwnot"

    elif clear_name == "[]":
        clear_name = "idx"

    else:
        clear_name = "xxx"
        print_error(f'Unsupported operator type "{dirty_name}", please add the missing rule.', state)

    return clear_name


if __name__ == "__main__":
    main()
