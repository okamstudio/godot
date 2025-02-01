import zlib


def run(target, source, env):
    src = str(source[0])
    dst = str(target[0])
    with open(src, "rb") as f, open(dst, "w", encoding="utf-8", newline="\n") as g:
        buf = f.read()
        decomp_size = len(buf)

        # Use maximum zlib compression level to further reduce file size
        # (at the cost of initial build times).
        buf = zlib.compress(buf, zlib.Z_BEST_COMPRESSION)

        g.write(
            """/* THIS FILE IS GENERATED DO NOT EDIT */
#pragma once

#ifdef TOOLS_ENABLED

#include "core/io/compression.h"
#include "core/io/file_access.h"
#include "core/string/ustring.h"

"""
        )

        g.write("static const int _gdextension_interface_data_compressed_size = " + str(len(buf)) + ";\n")
        g.write("static const int _gdextension_interface_data_uncompressed_size = " + str(decomp_size) + ";\n")
        g.write("static const unsigned char _gdextension_interface_data_compressed[] = {\n")
        for i in range(len(buf)):
            g.write("\t" + str(buf[i]) + ",\n")
        g.write("};\n")

        g.write(
            """
class GDExtensionInterfaceDump {
    public:
        static void generate_gdextension_interface_file(const String &p_path) {
            Ref<FileAccess> fa = FileAccess::open(p_path, FileAccess::WRITE);
            ERR_FAIL_COND_MSG(fa.is_null(), vformat("Cannot open file '%s' for writing.", p_path));
            Vector<uint8_t> data;
            data.resize(_gdextension_interface_data_uncompressed_size);
            int ret = Compression::decompress(data.ptrw(), _gdextension_interface_data_uncompressed_size, _gdextension_interface_data_compressed, _gdextension_interface_data_compressed_size, Compression::MODE_DEFLATE);
            ERR_FAIL_COND_MSG(ret == -1, "Compressed file is corrupt.");
            fa->store_buffer(data.ptr(), data.size());
        };
};

#endif // TOOLS_ENABLED
"""
        )
