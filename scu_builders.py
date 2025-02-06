"""Functions used to generate scu build source files during build time"""

import glob
import math
import os
from pathlib import Path

from methods import print_error

base_folder_path = str(Path(__file__).parent) + "/"
base_folder_only = os.path.basename(os.path.normpath(base_folder_path))
_verbose = False  # Set manually for debug prints
_scu_folders = set()
_max_includes_per_scu = 1024


def clear_out_stale_files(output_folder, extension, fresh_files):
    output_folder = os.path.abspath(output_folder)
    # print("clear_out_stale_files from folder: " + output_folder)

    if not os.path.isdir(output_folder):
        # folder does not exist or has not been created yet,
        # no files to clearout. (this is not an error)
        return

    for file in glob.glob(output_folder + "/*." + extension):
        file = Path(file)
        if file not in fresh_files:
            # print("removed stale file: " + str(file))
            os.remove(file)


def folder_not_found(folder):
    abs_folder = base_folder_path + folder + "/"
    return not os.path.isdir(abs_folder)


def find_files_in_folder(folder, sub_folder, include_list, extension, sought_exceptions, found_exceptions):
    abs_folder = base_folder_path + folder + "/" + sub_folder

    if not os.path.isdir(abs_folder):
        print_error(f'SCU: "{abs_folder}" not found.')
        return include_list, found_exceptions

    os.chdir(abs_folder)

    sub_folder_slashed = ""
    if sub_folder != "":
        sub_folder_slashed = sub_folder + "/"

    for file in glob.glob("*." + extension):
        simple_name = Path(file).stem

        if file.endswith(".gen.cpp"):
            continue

        li = '#include "' + folder + "/" + sub_folder_slashed + file + '"'

        if simple_name not in sought_exceptions:
            include_list.append(li)
        else:
            found_exceptions.append(li)

    return include_list, found_exceptions


def write_output_file(file_count, include_list, start_line, end_line, output_folder, output_filename_prefix, extension):
    output_folder = os.path.abspath(output_folder)

    if not os.path.isdir(output_folder):
        # create
        os.mkdir(output_folder)
        if not os.path.isdir(output_folder):
            print_error(f'SCU: "{output_folder}" could not be created.')
            return
        if _verbose:
            print("SCU: Creating folder: %s" % output_folder)

    file_text = ""

    for i in range(start_line, end_line):
        if i < len(include_list):
            line = include_list[i]
            li = line + "\n"
            file_text += li

    num_string = ""
    if file_count > 0:
        num_string = "_" + str(file_count)

    short_filename = output_filename_prefix + num_string + ".gen." + extension
    output_filename = output_folder + "/" + short_filename
    output_path = Path(output_filename)

    if not output_path.exists() or output_path.read_text() != file_text:
        if _verbose:
            print("SCU: Generating: %s" % short_filename)
        output_path.write_text(file_text, encoding="utf8")
    elif _verbose:
        print("SCU: Generation not needed for: " + short_filename)

    return output_path


def write_exception_output_file(file_count, exception_string, output_folder, output_filename_prefix, extension):
    output_folder = os.path.abspath(output_folder)
    if not os.path.isdir(output_folder):
        print_error(f"SCU: {output_folder} does not exist.")
        return

    file_text = exception_string + "\n"

    num_string = ""
    if file_count > 0:
        num_string = "_" + str(file_count)

    short_filename = output_filename_prefix + "_exception" + num_string + ".gen." + extension
    output_filename = output_folder + "/" + short_filename

    output_path = Path(output_filename)

    if not output_path.exists() or output_path.read_text() != file_text:
        if _verbose:
            print("SCU: Generating: " + short_filename)
        output_path.write_text(file_text, encoding="utf8")
    elif _verbose:
        print("SCU: Generation not needed for: " + short_filename)

    return output_path


def find_section_name(sub_folder):
    # Construct a useful name for the section from the path for debug logging
    section_path = os.path.abspath(base_folder_path + sub_folder) + "/"

    folders = []
    folder = ""

    for i in range(8):
        folder = os.path.dirname(section_path)
        folder = os.path.basename(folder)
        if folder == base_folder_only:
            break
        folders.append(folder)
        section_path += "../"
        section_path = os.path.abspath(section_path) + "/"

    section_name = ""
    for n in range(len(folders)):
        section_name += folders[len(folders) - n - 1]
        if n != (len(folders) - 1):
            section_name += "_"

    return section_name


# "folders" is a list of folders to add all the files from to add to the SCU
# "section (like a module)". The name of the scu file will be derived from the first folder
# (thus e.g. scene/3d becomes scu_scene_3d.gen.cpp)

# "includes_per_scu" limits the number of includes in a single scu file.
# This allows the module to be built in several translation units instead of just 1.
# This will usually be slower to compile but will use less memory per compiler instance, which
# is most relevant in release builds.

# "sought_exceptions" are a list of files (without extension) that contain
# e.g. naming conflicts, and are therefore not suitable for the scu build.
# These will automatically be placed in their own separate scu file,
# which is slow like a normal build, but prevents the naming conflicts.
# Ideally in these situations, the source code should be changed to prevent naming conflicts.


# "extension" will usually be cpp, but can also be set to c (for e.g. third party libraries that use c)
def process_folder(folders, sought_exceptions=[], includes_per_scu=0, extension="cpp"):
    if len(folders) == 0:
        return

    # Construct the filename prefix from the FIRST folder name
    # e.g. "scene_3d"
    out_filename = find_section_name(folders[0])

    found_includes = []
    found_exceptions = []

    main_folder = folders[0]
    abs_main_folder = base_folder_path + main_folder

    # Keep a record of all folders that have been processed for SCU,
    # this enables deciding what to do when we call "add_source_files()"
    global _scu_folders
    _scu_folders.add(main_folder)

    # main folder (first)
    found_includes, found_exceptions = find_files_in_folder(
        main_folder, "", found_includes, extension, sought_exceptions, found_exceptions
    )

    # sub folders
    for d in range(1, len(folders)):
        found_includes, found_exceptions = find_files_in_folder(
            main_folder, folders[d], found_includes, extension, sought_exceptions, found_exceptions
        )

    found_includes = sorted(found_includes)

    # calculate how many lines to write in each file
    total_lines = len(found_includes)

    # adjust number of output files according to whether DEV or release
    num_output_files = 1

    if includes_per_scu == 0:
        includes_per_scu = _max_includes_per_scu
    else:
        if includes_per_scu > _max_includes_per_scu:
            includes_per_scu = _max_includes_per_scu

    num_output_files = max(math.ceil(total_lines / float(includes_per_scu)), 1)

    lines_per_file = math.ceil(total_lines / float(num_output_files))
    lines_per_file = max(lines_per_file, 1)

    start_line = 0

    # These do not vary throughout the loop
    output_folder = abs_main_folder + "/scu/"
    output_filename_prefix = "scu_" + out_filename

    fresh_files = set()

    for file_count in range(0, num_output_files):
        end_line = start_line + lines_per_file

        # special case to cover rounding error in final file
        if file_count == (num_output_files - 1):
            end_line = len(found_includes)

        fresh_file = write_output_file(
            file_count, found_includes, start_line, end_line, output_folder, output_filename_prefix, extension
        )

        fresh_files.add(fresh_file)

        start_line = end_line

    # Write the exceptions each in their own scu gen file,
    # so they can effectively compile in "old style / normal build".
    for exception_count in range(len(found_exceptions)):
        fresh_file = write_exception_output_file(
            exception_count, found_exceptions[exception_count], output_folder, output_filename_prefix, extension
        )

        fresh_files.add(fresh_file)

    # Clear out any stale file (usually we will be overwriting if necessary,
    # but we want to remove any that are pre-existing that will not be
    # overwritten, so as to not compile anything stale).
    clear_out_stale_files(output_folder, extension, fresh_files)


def generate_scu_files(max_includes_per_scu):
    global _max_includes_per_scu
    _max_includes_per_scu = max_includes_per_scu

    print("SCU: Generating build files... (max includes per SCU: %d)" % _max_includes_per_scu)

    curr_folder = os.path.abspath("./")

    # check we are running from the correct folder
    if folder_not_found("core") or folder_not_found("platform") or folder_not_found("scene"):
        raise RuntimeError("scu_builders.py must be run from the godot folder.")
        return

    process_folder(["core"])
    process_folder(["core/crypto"])
    process_folder(["core/debugger"])
    process_folder(["core/extension"])
    process_folder(["core/input"])
    process_folder(["core/io"])
    process_folder(["core/math"])
    process_folder(["core/object"])
    process_folder(["core/os"])
    process_folder(["core/string"])
    process_folder(["core/variant"], ["variant_utility"])

    process_folder(["drivers/unix"])
    process_folder(["drivers/png"])

    process_folder(["drivers/gles3/effects"])
    process_folder(["drivers/gles3/storage"])

    process_folder(["editor"], ["file_system_dock", "editor_resource_preview"], 32)
    process_folder(["editor/debugger"])
    process_folder(["editor/debugger/debug_adapter"])
    process_folder(["editor/export"])
    process_folder(["editor/gui"])
    process_folder(["editor/themes"])
    process_folder(["editor/project_manager"])
    process_folder(["editor/import"])
    process_folder(["editor/import/3d"])
    process_folder(["editor/plugins"])
    process_folder(["editor/plugins/gizmos"])
    process_folder(["editor/plugins/tiles"])

    process_folder(["platform/android/export"])
    process_folder(["platform/ios/export"])
    process_folder(["platform/linuxbsd/export"])
    process_folder(["platform/macos/export"])
    process_folder(["platform/web/export"])
    process_folder(["platform/windows/export"])

    process_folder(["modules/lightmapper_rd"])
    process_folder(["modules/gltf"])
    process_folder(["modules/gltf/structures"])
    process_folder(["modules/gltf/editor"])
    process_folder(["modules/gltf/extensions"])
    process_folder(["modules/gltf/extensions/physics"])
    process_folder(["modules/navigation"])
    process_folder(["modules/navigation/2d"])
    process_folder(["modules/navigation/3d"])
    process_folder(["modules/webrtc"])
    process_folder(["modules/websocket"])
    process_folder(["modules/gridmap"])
    process_folder(["modules/multiplayer"])
    process_folder(["modules/multiplayer/editor"])
    process_folder(["modules/saveload"])
    process_folder(["modules/saveload/editor"])
    process_folder(["modules/openxr"], ["register_types"])
    process_folder(["modules/openxr/action_map"])
    process_folder(["modules/openxr/editor"])
    process_folder(["modules/openxr/extensions"])
    process_folder(["modules/openxr/scene"])
    process_folder(["modules/godot_physics_2d"])
    process_folder(["modules/godot_physics_3d"])
    process_folder(["modules/godot_physics_3d/joints"])

    process_folder(["modules/csg"])
    process_folder(["modules/gdscript"])
    process_folder(["modules/gdscript/editor"])
    process_folder(["modules/gdscript/language_server"])

    process_folder(["scene/2d"])
    process_folder(["scene/2d/physics"])
    process_folder(["scene/2d/physics/joints"])
    process_folder(["scene/3d"])
    process_folder(["scene/3d/physics"])
    process_folder(["scene/3d/physics/joints"])
    process_folder(["scene/animation"])
    process_folder(["scene/gui"])
    process_folder(["scene/main"])
    process_folder(["scene/theme"])
    process_folder(["scene/resources"])
    process_folder(["scene/resources/2d"])
    process_folder(["scene/resources/2d/skeleton"])
    process_folder(["scene/resources/3d"])

    process_folder(["servers"])
    process_folder(["servers/rendering"])
    process_folder(["servers/rendering/dummy/storage"])
    process_folder(["servers/rendering/storage"])
    process_folder(["servers/rendering/renderer_rd"])
    process_folder(["servers/rendering/renderer_rd/effects"])
    process_folder(["servers/rendering/renderer_rd/environment"])
    process_folder(["servers/rendering/renderer_rd/storage_rd"])
    process_folder(["servers/rendering/renderer_rd/forward_clustered"])
    process_folder(["servers/rendering/renderer_rd/forward_mobile"])
    process_folder(["servers/audio"])
    process_folder(["servers/audio/effects"])
    process_folder(["servers/navigation"])
    process_folder(["servers/xr"])

    # Finally change back the path to the calling folder
    os.chdir(curr_folder)

    if _verbose:
        print("SCU: Processed folders: %s" % sorted(_scu_folders))

    return _scu_folders
