import glob
import os
import re
import shutil
import stat
import subprocess
import sys
from typing import Any, Callable

git_tag = "v4.3.3"

include_dirs = [
    "common/tasking",
    "kernels/bvh",
    "kernels/builders",
    "common/sys",
    "kernels",
    "kernels/common",
    "common/math",
    "common/algorithms",
    "common/lexers",
    "common/simd",
    "common/simd/arm",
    "common/simd/wasm",
    "include/embree4",
    "kernels/subdiv",
    "kernels/geometry",
]

cpp_files = [
    "common/sys/sysinfo.cpp",
    "common/sys/alloc.cpp",
    "common/sys/estring.cpp",
    "common/sys/filename.cpp",
    "common/sys/library.cpp",
    "common/sys/thread.cpp",
    "common/sys/regression.cpp",
    "common/sys/mutex.cpp",
    "common/sys/condition.cpp",
    "common/sys/barrier.cpp",
    "common/math/constants.cpp",
    "common/simd/sse.cpp",
    "common/lexers/stringstream.cpp",
    "common/lexers/tokenstream.cpp",
    "common/tasking/taskschedulerinternal.cpp",
    "kernels/common/device.cpp",
    "kernels/common/stat.cpp",
    "kernels/common/acceln.cpp",
    "kernels/common/accelset.cpp",
    "kernels/common/state.cpp",
    "kernels/common/rtcore.cpp",
    "kernels/common/rtcore_builder.cpp",
    "kernels/common/scene.cpp",
    "kernels/common/scene_verify.cpp",
    "kernels/common/alloc.cpp",
    "kernels/common/geometry.cpp",
    "kernels/common/scene_triangle_mesh.cpp",
    "kernels/geometry/primitive4.cpp",
    "kernels/builders/primrefgen.cpp",
    "kernels/bvh/bvh.cpp",
    "kernels/bvh/bvh_statistics.cpp",
    "kernels/bvh/bvh4_factory.cpp",
    "kernels/bvh/bvh8_factory.cpp",
    "kernels/bvh/bvh_collider.cpp",
    "kernels/bvh/bvh_rotate.cpp",
    "kernels/bvh/bvh_refit.cpp",
    "kernels/bvh/bvh_builder.cpp",
    "kernels/bvh/bvh_builder_morton.cpp",
    "kernels/bvh/bvh_builder_sah.cpp",
    "kernels/bvh/bvh_builder_sah_spatial.cpp",
    "kernels/bvh/bvh_builder_sah_mb.cpp",
    "kernels/bvh/bvh_builder_twolevel.cpp",
    "kernels/bvh/bvh_intersector1.cpp",
    "kernels/bvh/bvh_intersector1_bvh4.cpp",
    "kernels/bvh/bvh_intersector_hybrid4_bvh4.cpp",
    "kernels/bvh/bvh_intersector_hybrid.cpp",
]

config_files = [
    "kernels/config.h.in",
    "kernels/rtcore_config.h.in",
]

license_file = "LICENSE.txt"

os.chdir(f"{os.path.dirname(__file__)}/../../thirdparty")

dir_name = "embree"
if os.path.exists(dir_name):
    shutil.rmtree(dir_name)

# In case something went wrong and embree-tmp stayed on the system.
if os.path.exists("embree-tmp"):
    shutil.rmtree("embree-tmp")

subprocess.run(["git", "clone", "https://github.com/embree/embree.git", "embree-tmp"])
os.chdir("embree-tmp")
subprocess.run(["git", "checkout", git_tag])

commit_hash = str(subprocess.check_output(["git", "rev-parse", "HEAD"], universal_newlines=True)).strip()


def on_rm_error(function: Callable[..., Any], path: str, excinfo: Exception) -> None:
    """
    Error handler for `shutil.rmtree()`.

    If the error is due to read-only files,
    it will change the file permissions and retry.
    """
    os.chmod(path, stat.S_IWRITE)
    os.unlink(path)


# We remove the .git directory because it contains
# a lot of read-only files that are problematic on Windows.
if sys.version_info >= (3, 12):
    shutil.rmtree(".git", onexc=on_rm_error)
else:
    shutil.rmtree(".git", onerror=on_rm_error)  # type: ignore

all_files = set(cpp_files)

for config_file in config_files:
    all_files.add(config_file)

all_files.add(license_file)

dest_dir = os.path.join("..", dir_name)
for include_dir in include_dirs:
    headers = glob.iglob(os.path.join(include_dir, "*.h"))
    all_files.update(headers)

for f in all_files:
    d = os.path.join(dest_dir, os.path.dirname(f))
    if not os.path.exists(d):
        os.makedirs(d)
    shutil.copy2(f, d)

with open(os.path.join(dest_dir, "kernels/hash.h"), "w", encoding="utf-8", newline="\n") as hash_file:
    hash_file.write(
        f"""// Copyright 2009-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#define RTC_HASH "{commit_hash}"
"""
    )

for config_file in config_files:
    os.rename(os.path.join(dest_dir, config_file), os.path.join(dest_dir, config_file[:-3]))

with open("CMakeLists.txt", "r", encoding="utf-8") as cmake_file:
    cmake_content = cmake_file.read()
    major_version = int(re.compile(r"EMBREE_VERSION_MAJOR\s(\d+)").findall(cmake_content)[0])
    minor_version = int(re.compile(r"EMBREE_VERSION_MINOR\s(\d+)").findall(cmake_content)[0])
    patch_version = int(re.compile(r"EMBREE_VERSION_PATCH\s(\d+)").findall(cmake_content)[0])

shutil.move(os.path.join(dest_dir, "kernels/rtcore_config.h"), os.path.join(dest_dir, ("include/embree4/")))

with open(
    os.path.join(dest_dir, "include/embree4/rtcore_config.h"), "r+", encoding="utf-8", newline="\n"
) as rtcore_config:
    lines = rtcore_config.readlines()
    rtcore_config.seek(0)
    for i, line in enumerate(lines):
        if line.startswith("#define RTC_VERSION_MAJOR"):
            lines[i : i + 5] = [
                f"#define RTC_VERSION_MAJOR {major_version}\n",
                f"#define RTC_VERSION_MINOR {minor_version}\n",
                f"#define RTC_VERSION_PATCH {patch_version}\n",
                f"#define RTC_VERSION {major_version}{minor_version:02d}{patch_version:02d}\n",
                f'#define RTC_VERSION_STRING "{major_version}.{minor_version}.{patch_version}"\n',
            ]
            break
    rtcore_config.writelines(lines)
    rtcore_config.truncate()

os.chdir("..")
shutil.rmtree("embree-tmp")

subprocess.run(["git", "restore", "embree/patches"])

for patch in os.listdir("embree/patches"):
    subprocess.run(["git", "apply", f"embree/patches/{patch}"])
