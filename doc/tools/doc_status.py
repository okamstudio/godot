#!/usr/bin/env python3

import fnmatch
import math
import os
import re
import sys
import xml.etree.ElementTree as ET
from typing import Dict, List, Set

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../"))

from methods import COLOR_SUPPORTED, Ansi, toggle_color

################################################################################
#                                    Config                                    #
################################################################################

flags = {
    "c": COLOR_SUPPORTED,
    "b": False,
    "g": False,
    "s": False,
    "u": False,
    "h": False,
    "p": False,
    "o": True,
    "i": False,
    "a": True,
    "e": False,
}
flag_descriptions = {
    "c": "Toggle colors when outputting.",
    "b": "Toggle showing only not fully described classes.",
    "g": "Toggle showing only completed classes.",
    "s": "Toggle showing comments about the status.",
    "u": "Toggle URLs to docs.",
    "h": "Show help and exit.",
    "p": "Toggle showing percentage as well as counts.",
    "o": "Toggle overall column.",
    "i": "Toggle collapse of class items columns.",
    "a": "Toggle showing all items.",
    "e": "Toggle hiding empty items.",
}
long_flags = {
    "colors": "c",
    "use-colors": "c",
    "bad": "b",
    "only-bad": "b",
    "good": "g",
    "only-good": "g",
    "comments": "s",
    "status": "s",
    "urls": "u",
    "gen-url": "u",
    "help": "h",
    "percent": "p",
    "use-percentages": "p",
    "overall": "o",
    "use-overall": "o",
    "items": "i",
    "collapse": "i",
    "all": "a",
    "empty": "e",
}
table_columns = [
    "name",
    "brief_description",
    "description",
    "methods",
    "constants",
    "members",
    "theme_items",
    "signals",
    "operators",
    "constructors",
]
table_column_names = [
    "Name",
    "Brief Desc.",
    "Desc.",
    "Methods",
    "Constants",
    "Members",
    "Theme Items",
    "Signals",
    "Operators",
    "Constructors",
]
colors = {
    "name": [Ansi.CYAN],  # cyan
    "part_big_problem": [Ansi.RED, Ansi.UNDERLINE],  # underline, red
    "part_problem": [Ansi.RED],  # red
    "part_mostly_good": [Ansi.YELLOW],  # yellow
    "part_good": [Ansi.GREEN],  # green
    "url": [Ansi.BLUE, Ansi.UNDERLINE],  # underline, blue
    "section": [Ansi.BOLD, Ansi.UNDERLINE],  # bold, underline
    "state_off": [Ansi.CYAN],  # cyan
    "state_on": [Ansi.BOLD, Ansi.MAGENTA],  # bold, magenta/plum
    "bold": [Ansi.BOLD],  # bold
}
overall_progress_description_weight = 10


################################################################################
#                                    Utils                                     #
################################################################################


def validate_tag(elem: ET.Element, tag: str) -> None:
    if elem.tag != tag:
        print('Tag mismatch, expected "' + tag + '", got ' + elem.tag)
        sys.exit(255)


def color(color: str, string: str) -> str:
    color_format = "".join([str(x) for x in colors[color]])
    return f"{color_format}{string}{Ansi.RESET}"


ansi_escape = re.compile(r"\x1b[^m]*m")


def nonescape_len(s: str) -> int:
    return len(ansi_escape.sub("", s))


################################################################################
#                                   Classes                                    #
################################################################################


class ClassStatusProgress:
    def __init__(self, described: int = 0, total: int = 0):
        self.described: int = described
        self.total: int = total

    def __add__(self, other: "ClassStatusProgress"):
        return ClassStatusProgress(self.described + other.described, self.total + other.total)

    def increment(self, described: bool):
        if described:
            self.described += 1
        self.total += 1

    def is_ok(self):
        return self.described >= self.total

    def to_configured_colored_string(self):
        if flags["p"]:
            return self.to_colored_string("{percent}% ({has}/{total})", "{pad_percent}{pad_described}{s}{pad_total}")
        else:
            return self.to_colored_string()

    def to_colored_string(self, format: str = "{has}/{total}", pad_format: str = "{pad_described}{s}{pad_total}"):
        ratio = float(self.described) / float(self.total) if self.total != 0 else 1
        percent = int(round(100 * ratio))
        s = format.format(has=str(self.described), total=str(self.total), percent=str(percent))
        if self.described >= self.total:
            s = color("part_good", s)
        elif self.described >= self.total / 4 * 3:
            s = color("part_mostly_good", s)
        elif self.described > 0:
            s = color("part_problem", s)
        else:
            s = color("part_big_problem", s)
        pad_size = max(len(str(self.described)), len(str(self.total)))
        pad_described = "".ljust(pad_size - len(str(self.described)))
        pad_percent = "".ljust(3 - len(str(percent)))
        pad_total = "".ljust(pad_size - len(str(self.total)))
        return pad_format.format(pad_described=pad_described, pad_total=pad_total, pad_percent=pad_percent, s=s)


class ClassStatus:
    def __init__(self, name: str = ""):
        self.name: str = name
        self.has_brief_description: bool = True
        self.has_description: bool = True
        self.progresses: Dict[str, ClassStatusProgress] = {
            "methods": ClassStatusProgress(),
            "constants": ClassStatusProgress(),
            "members": ClassStatusProgress(),
            "theme_items": ClassStatusProgress(),
            "signals": ClassStatusProgress(),
            "operators": ClassStatusProgress(),
            "constructors": ClassStatusProgress(),
        }

    def __add__(self, other: "ClassStatus"):
        new_status = ClassStatus()
        new_status.name = self.name
        new_status.has_brief_description = self.has_brief_description and other.has_brief_description
        new_status.has_description = self.has_description and other.has_description
        for k in self.progresses:
            new_status.progresses[k] = self.progresses[k] + other.progresses[k]
        return new_status

    def is_ok(self):
        ok = True
        ok = ok and self.has_brief_description
        ok = ok and self.has_description
        for k in self.progresses:
            ok = ok and self.progresses[k].is_ok()
        return ok

    def is_empty(self):
        sum = 0
        for k in self.progresses:
            if self.progresses[k].is_ok():
                continue
            sum += self.progresses[k].total
        return sum < 1

    def make_output(self) -> Dict[str, str]:
        output: Dict[str, str] = {}
        output["name"] = color("name", self.name)

        ok_string = color("part_good", "OK")
        missing_string = color("part_big_problem", "MISSING")

        output["brief_description"] = ok_string if self.has_brief_description else missing_string
        output["description"] = ok_string if self.has_description else missing_string

        description_progress = ClassStatusProgress(
            (self.has_brief_description + self.has_description) * overall_progress_description_weight,
            2 * overall_progress_description_weight,
        )
        items_progress = ClassStatusProgress()

        for k in ["methods", "constants", "members", "theme_items", "signals", "constructors", "operators"]:
            items_progress += self.progresses[k]
            output[k] = self.progresses[k].to_configured_colored_string()

        output["items"] = items_progress.to_configured_colored_string()

        output["overall"] = (description_progress + items_progress).to_colored_string(
            color("bold", "{percent}%"), "{pad_percent}{s}"
        )

        if self.name.startswith("Total"):
            output["url"] = color("url", "https://docs.godotengine.org/en/latest/classes/")
            if flags["s"]:
                output["comment"] = color("part_good", "ALL OK")
        else:
            output["url"] = color(
                "url", "https://docs.godotengine.org/en/latest/classes/class_{name}.html".format(name=self.name.lower())
            )

            if flags["s"] and not flags["g"] and self.is_ok():
                output["comment"] = color("part_good", "ALL OK")

        return output

    @staticmethod
    def generate_for_class(c: ET.Element):
        status = ClassStatus()
        status.name = c.attrib["name"]

        for tag in list(c):
            len_tag_text = 0 if (tag.text is None) else len(tag.text.strip())

            if tag.tag == "brief_description":
                status.has_brief_description = len_tag_text > 0

            elif tag.tag == "description":
                status.has_description = len_tag_text > 0

            elif tag.tag in ["methods", "signals", "operators", "constructors"]:
                for sub_tag in list(tag):
                    is_deprecated = "deprecated" in sub_tag.attrib
                    is_experimental = "experimental" in sub_tag.attrib
                    descr = sub_tag.find("description")
                    has_descr = (descr is not None) and (descr.text is not None) and len(descr.text.strip()) > 0
                    status.progresses[tag.tag].increment(is_deprecated or is_experimental or has_descr)
            elif tag.tag in ["constants", "members", "theme_items"]:
                for sub_tag in list(tag):
                    if sub_tag.text is not None:
                        is_deprecated = "deprecated" in sub_tag.attrib
                        is_experimental = "experimental" in sub_tag.attrib
                        has_descr = len(sub_tag.text.strip()) > 0
                        status.progresses[tag.tag].increment(is_deprecated or is_experimental or has_descr)

            elif tag.tag in ["tutorials"]:
                pass  # Ignore those tags for now

            else:
                print(tag.tag, tag.attrib)

        return status


################################################################################
#                                  Arguments                                   #
################################################################################

input_file_list: List[str] = []
input_class_list: List[str] = []
merged_file: str = ""

for arg in sys.argv[1:]:
    try:
        if arg.startswith("--"):
            flags[long_flags[arg[2:]]] = not flags[long_flags[arg[2:]]]
        elif arg.startswith("-"):
            for f in arg[1:]:
                flags[f] = not flags[f]
        elif os.path.isdir(arg):
            for f in os.listdir(arg):
                if f.endswith(".xml"):
                    input_file_list.append(os.path.join(arg, f))
        else:
            input_class_list.append(arg)
    except KeyError:
        print("Unknown command line flag: " + arg)
        sys.exit(1)

if flags["i"]:
    for r in ["methods", "constants", "members", "signals", "theme_items"]:
        index = table_columns.index(r)
        del table_column_names[index]
        del table_columns[index]
    table_column_names.append("Items")
    table_columns.append("items")

if flags["o"] == (not flags["i"]):
    table_column_names.append(color("bold", "Overall"))
    table_columns.append("overall")

if flags["u"]:
    table_column_names.append("Docs URL")
    table_columns.append("url")

toggle_color(flags["c"])

################################################################################
#                                     Help                                     #
################################################################################

if len(input_file_list) < 1 or flags["h"]:
    if not flags["h"]:
        print(color("section", "Invalid usage") + ": Please specify a classes directory")
    print(color("section", "Usage") + ": doc_status.py [flags] <classes_dir> [class names]")
    print("\t< and > signify required parameters, while [ and ] signify optional parameters.")
    print(color("section", "Available flags") + ":")
    possible_synonym_list = list(long_flags)
    possible_synonym_list.sort()
    flag_list = list(flags)
    flag_list.sort()
    for flag in flag_list:
        synonyms = [color("name", "-" + flag)]
        for synonym in possible_synonym_list:
            if long_flags[synonym] == flag:
                synonyms.append(color("name", "--" + synonym))

        print(
            (
                "{synonyms} (Currently "
                + color("state_" + ("on" if flags[flag] else "off"), "{value}")
                + ")\n\t{description}"
            ).format(
                synonyms=", ".join(synonyms),
                value=("on" if flags[flag] else "off"),
                description=flag_descriptions[flag],
            )
        )
    sys.exit(0)


################################################################################
#                               Parse class list                               #
################################################################################

class_names: List[str] = []
classes: Dict[str, ET.Element] = {}

for file in input_file_list:
    tree = ET.parse(file)
    doc = tree.getroot()

    if doc.attrib["name"] in class_names:
        continue
    class_names.append(doc.attrib["name"])
    classes[doc.attrib["name"]] = doc

class_names.sort()

if len(input_class_list) < 1:
    input_class_list = ["*"]

filtered_classes_set: Set[str] = set()
for pattern in input_class_list:
    filtered_classes_set |= set(fnmatch.filter(class_names, pattern))
filtered_classes = list(filtered_classes_set)
filtered_classes.sort()

################################################################################
#                               Make output table                              #
################################################################################

table = [table_column_names]
table_row_chars = "| - "
table_column_chars = "|"

total_status = ClassStatus("Total")

for cn in filtered_classes:
    c = classes[cn]
    validate_tag(c, "class")
    status = ClassStatus.generate_for_class(c)

    total_status = total_status + status

    if (flags["b"] and status.is_ok()) or (flags["g"] and not status.is_ok()) or (not flags["a"]):
        continue

    if flags["e"] and status.is_empty():
        continue

    out = status.make_output()
    row: List[str] = []
    for column in table_columns:
        if column in out:
            row.append(out[column])
        else:
            row.append("")

    if "comment" in out and out["comment"] != "":
        row.append(out["comment"])

    table.append(row)


################################################################################
#                              Print output table                              #
################################################################################

if len(table) == 1 and flags["a"]:
    print(color("part_big_problem", "No classes suitable for printing!"))
    sys.exit(0)

if len(table) > 2 or not flags["a"]:
    total_status.name = "Total = {0}".format(len(table) - 1)
    out = total_status.make_output()
    row = []
    for column in table_columns:
        if column in out:
            row.append(out[column])
        else:
            row.append("")
    table.append(row)

if flags["a"]:
    # Duplicate the headers at the bottom of the table so they can be viewed
    # without having to scroll back to the top.
    table.append(table_column_names)

table_column_sizes: List[int] = []
for row in table:
    for cell_i, cell in enumerate(row):
        if cell_i >= len(table_column_sizes):
            table_column_sizes.append(0)

        table_column_sizes[cell_i] = max(nonescape_len(cell), table_column_sizes[cell_i])

divider_string = table_row_chars[0]
for cell_i in range(len(table[0])):
    divider_string += (
        table_row_chars[1] + table_row_chars[2] * (table_column_sizes[cell_i]) + table_row_chars[1] + table_row_chars[0]
    )

for row_i, row in enumerate(table):
    row_string = table_column_chars
    for cell_i, cell in enumerate(row):
        padding_needed = table_column_sizes[cell_i] - nonescape_len(cell) + 2
        if cell_i == 0:
            row_string += table_row_chars[3] + cell + table_row_chars[3] * (padding_needed - 1)
        else:
            row_string += (
                table_row_chars[3] * int(math.floor(float(padding_needed) / 2))
                + cell
                + table_row_chars[3] * int(math.ceil(float(padding_needed) / 2))
            )
        row_string += table_column_chars

    print(row_string)

    # Account for the possible double header (if the `a` flag is enabled).
    # No need to have a condition for the flag, as this will behave correctly
    # if the flag is disabled.
    if row_i == 0 or row_i == len(table) - 3 or row_i == len(table) - 2:
        print(divider_string)

print(divider_string)

if total_status.is_ok() and not flags["g"]:
    print("All listed classes are " + color("part_good", "OK") + "!")
