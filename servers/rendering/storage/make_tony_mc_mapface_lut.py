import zlib


def run(target, source, env):
    with open(str(target[0]), "w", encoding="utf-8", newline="\n") as file:
        file.write('// This file is generated at build time by the "{}" script.\n\n'.format(source[0].name))

        file.write("#ifndef TONY_MC_MAPFACE_LUT_H\n")
        file.write("#define TONY_MC_MAPFACE_LUT_H\n\n")

        file.write("static const int TONY_MC_MAPFACE_LUT_DIMENSIONS = 48; // 48x48x48\n")
        file.write("static const int TONY_MC_MAPFACE_LUT_DECOMPRESSED_SIZE = 48 * 48 * 48 * 4 * sizeof(uint8_t);\n")
        file.write("static const int TONY_MC_MAPFACE_LUT_COMPRESSED_SIZE = 313165 * sizeof(uint8_t);\n\n")

        file.write("// Tony McMapface LUT by Tomasz Stachowiak (https://github.com/h3r2tic/tony-mc-mapface)\n")
        file.write("// RGBE5999 (E5B9G9R9_UFLOAT_PACK32), DEFLATE compressed\n")

        file.write("static const uint8_t TONY_MC_MAPFACE_LUT[] = {")
        with open(source[1].path, mode="rb") as binary:
            buffer = binary.read()
            buffer = buffer[148:]  # skip .dds header
            buffer = zlib.compress(buffer, zlib.Z_BEST_COMPRESSION)

            file.write("0x{:02x}".format(buffer[0]))
            for byte in range(1, len(buffer)):
                file.write(",0x{:02x}".format(buffer[byte]))
        file.write("};\n\n")

        file.write("#endif // TONY_MC_MAPFACE_LUT_H\n")
