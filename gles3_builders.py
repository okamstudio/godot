"""Functions used to generate source files during build time"""

import os.path
from typing import Optional

from methods import print_error, to_raw_cstring


class GLES3HeaderStruct:
    def __init__(self):
        self.vertex_lines = []
        self.fragment_lines = []
        self.uniforms = []
        self.fbos = []
        self.texunits = []
        self.texunit_names = []
        self.ubos = []
        self.ubo_names = []
        self.feedbacks = []

        self.vertex_included_files = []
        self.fragment_included_files = []

        self.reading = ""
        self.line_offset = 0
        self.vertex_offset = 0
        self.fragment_offset = 0
        self.variant_defines = []
        self.variant_names = []
        self.specialization_names = []
        self.specialization_values = []


def include_file_in_gles3_header(filename: str, header_data: GLES3HeaderStruct, depth: int):
    with open(filename, "r", encoding="utf-8") as fs:
        line = fs.readline()

        while line:
            if line.find("=") != -1 and header_data.reading == "":
                # Mode
                eqpos = line.find("=")
                defname = line[:eqpos].strip().upper()
                define = line[eqpos + 1 :].strip()
                header_data.variant_names.append(defname)
                header_data.variant_defines.append(define)
                line = fs.readline()
                header_data.line_offset += 1
                header_data.vertex_offset = header_data.line_offset
                continue

            if line.find("=") != -1 and header_data.reading == "specializations":
                # Specialization
                eqpos = line.find("=")
                specname = line[:eqpos].strip()
                specvalue = line[eqpos + 1 :]
                header_data.specialization_names.append(specname)
                header_data.specialization_values.append(specvalue)
                line = fs.readline()
                header_data.line_offset += 1
                header_data.vertex_offset = header_data.line_offset
                continue

            if line.find("#[modes]") != -1:
                # Nothing really, just skip
                line = fs.readline()
                header_data.line_offset += 1
                header_data.vertex_offset = header_data.line_offset
                continue

            if line.find("#[specializations]") != -1:
                header_data.reading = "specializations"
                line = fs.readline()
                header_data.line_offset += 1
                header_data.vertex_offset = header_data.line_offset
                continue

            if line.find("#[vertex]") != -1:
                header_data.reading = "vertex"
                line = fs.readline()
                header_data.line_offset += 1
                header_data.vertex_offset = header_data.line_offset
                continue

            if line.find("#[fragment]") != -1:
                header_data.reading = "fragment"
                line = fs.readline()
                header_data.line_offset += 1
                header_data.fragment_offset = header_data.line_offset
                continue

            while line.find("#include ") != -1:
                includeline = line.replace("#include ", "").strip()[1:-1]

                included_file = os.path.relpath(os.path.dirname(filename) + "/" + includeline)
                if included_file not in header_data.vertex_included_files and header_data.reading == "vertex":
                    header_data.vertex_included_files += [included_file]
                    if include_file_in_gles3_header(included_file, header_data, depth + 1) is None:
                        print_error(f'In file "{filename}": #include "{includeline}" could not be found!"')
                elif included_file not in header_data.fragment_included_files and header_data.reading == "fragment":
                    header_data.fragment_included_files += [included_file]
                    if include_file_in_gles3_header(included_file, header_data, depth + 1) is None:
                        print_error(f'In file "{filename}": #include "{includeline}" could not be found!"')

                line = fs.readline()

            if line.find("uniform") != -1 and line.lower().find("texunit:") != -1:
                # texture unit
                texunitstr = line[line.find(":") + 1 :].strip()
                if texunitstr == "auto":
                    texunit = "-1"
                else:
                    texunit = str(int(texunitstr))
                uline = line[: line.lower().find("//")]
                uline = uline.replace("uniform", "")
                uline = uline.replace("highp", "")
                uline = uline.replace(";", "")
                lines = uline.split(",")
                for x in lines:
                    x = x.strip()
                    x = x[x.rfind(" ") + 1 :]
                    if x.find("[") != -1:
                        # unfiorm array
                        x = x[: x.find("[")]

                    if x not in header_data.texunit_names:
                        header_data.texunits += [(x, texunit)]
                        header_data.texunit_names += [x]

            elif line.find("uniform") != -1 and line.lower().find("ubo:") != -1:
                # uniform buffer object
                ubostr = line[line.find(":") + 1 :].strip()
                ubo = str(int(ubostr))
                uline = line[: line.lower().find("//")]
                uline = uline[uline.find("uniform") + len("uniform") :]
                uline = uline.replace("highp", "")
                uline = uline.replace(";", "")
                uline = uline.replace("{", "").strip()
                lines = uline.split(",")
                for x in lines:
                    x = x.strip()
                    x = x[x.rfind(" ") + 1 :]
                    if x.find("[") != -1:
                        # unfiorm array
                        x = x[: x.find("[")]

                    if x not in header_data.ubo_names:
                        header_data.ubos += [(x, ubo)]
                        header_data.ubo_names += [x]

            elif line.find("uniform") != -1 and line.find("{") == -1 and line.find(";") != -1:
                uline = line.replace("uniform", "")
                uline = uline.replace(";", "")
                lines = uline.split(",")
                for x in lines:
                    x = x.strip()
                    x = x[x.rfind(" ") + 1 :]
                    if x.find("[") != -1:
                        # unfiorm array
                        x = x[: x.find("[")]

                    if x not in header_data.uniforms:
                        header_data.uniforms += [x]

            if (line.strip().find("out ") == 0 or line.strip().find("flat ") == 0) and line.find("tfb:") != -1:
                uline = line.replace("flat ", "")
                uline = uline.replace("out ", "")
                uline = uline.replace("highp ", "")
                uline = uline.replace(";", "")
                uline = uline[uline.find(" ") :].strip()

                if uline.find("//") != -1:
                    name, bind = uline.split("//")
                    if bind.find("tfb:") != -1:
                        name = name.strip()
                        bind = bind.replace("tfb:", "").strip()
                        header_data.feedbacks += [(name, bind)]

            line = line.replace("\r", "")
            line = line.replace("\n", "")

            if header_data.reading == "vertex":
                header_data.vertex_lines += [line]
            if header_data.reading == "fragment":
                header_data.fragment_lines += [line]

            line = fs.readline()
            header_data.line_offset += 1

    return header_data


def build_gles3_header(
    filename: str,
    include: str,
    class_suffix: str,
    optional_output_filename: Optional[str] = None,
    header_data: Optional[GLES3HeaderStruct] = None,
):
    header_data = header_data or GLES3HeaderStruct()
    include_file_in_gles3_header(filename, header_data, 0)

    if optional_output_filename is None:
        out_file = filename + ".gen.h"
    else:
        out_file = optional_output_filename

    with open(out_file, "w", encoding="utf-8", newline="\n") as fd:
        defspec = 0
        defvariant = ""

        fd.write("/* WARNING, THIS FILE WAS GENERATED, DO NOT EDIT */\n")
        fd.write("#pragma once\n")

        out_file_base = out_file
        out_file_base = out_file_base[out_file_base.rfind("/") + 1 :]
        out_file_base = out_file_base[out_file_base.rfind("\\") + 1 :]

        out_file_class = (
            out_file_base.replace(".glsl.gen.h", "").title().replace("_", "").replace(".", "") + "Shader" + class_suffix
        )
        fd.write("\n\n")
        fd.write('#include "' + include + '"\n\n\n')
        fd.write("class " + out_file_class + " : public Shader" + class_suffix + " {\n\n")

        fd.write("public:\n\n")

        if header_data.uniforms:
            fd.write("\tenum Uniforms {\n")
            for x in header_data.uniforms:
                fd.write("\t\t" + x.upper() + ",\n")
            fd.write("\t};\n\n")

        if header_data.variant_names:
            fd.write("\tenum ShaderVariant {\n")
            for x in header_data.variant_names:
                fd.write("\t\t" + x + ",\n")
            fd.write("\t};\n\n")
        else:
            fd.write("\tenum ShaderVariant { DEFAULT };\n\n")
            defvariant = "=DEFAULT"

        if header_data.specialization_names:
            fd.write("\tenum Specializations {\n")
            counter = 0
            for x in header_data.specialization_names:
                fd.write("\t\t" + x.upper() + "=" + str(1 << counter) + ",\n")
                counter += 1
            fd.write("\t};\n\n")

        for i in range(len(header_data.specialization_names)):
            defval = header_data.specialization_values[i].strip()
            if defval.upper() == "TRUE" or defval == "1":
                defspec |= 1 << i

        fd.write(
            "\t_FORCE_INLINE_ bool version_bind_shader(RID p_version,ShaderVariant p_variant"
            + defvariant
            + ",uint64_t p_specialization="
            + str(defspec)
            + ") { return _version_bind_shader(p_version,p_variant,p_specialization); }\n\n"
        )

        if header_data.uniforms:
            fd.write(
                "\t_FORCE_INLINE_ int version_get_uniform(Uniforms p_uniform,RID p_version,ShaderVariant p_variant"
                + defvariant
                + ",uint64_t p_specialization="
                + str(defspec)
                + ") { return _version_get_uniform(p_uniform,p_version,p_variant,p_specialization); }\n\n"
            )

            fd.write(
                "\t#define _FU if (version_get_uniform(p_uniform,p_version,p_variant,p_specialization)<0) return; \n\n "
            )
            fd.write(
                "\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, float p_value,RID p_version,ShaderVariant p_variant"
                + defvariant
                + ",uint64_t p_specialization="
                + str(defspec)
                + ") { _FU glUniform1f(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),p_value); }\n\n"
            )
            fd.write(
                "\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, double p_value,RID p_version,ShaderVariant p_variant"
                + defvariant
                + ",uint64_t p_specialization="
                + str(defspec)
                + ") { _FU glUniform1f(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),p_value); }\n\n"
            )
            fd.write(
                "\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, uint8_t p_value,RID p_version,ShaderVariant p_variant"
                + defvariant
                + ",uint64_t p_specialization="
                + str(defspec)
                + ") { _FU glUniform1ui(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),p_value); }\n\n"
            )
            fd.write(
                "\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, int8_t p_value,RID p_version,ShaderVariant p_variant"
                + defvariant
                + ",uint64_t p_specialization="
                + str(defspec)
                + ") { _FU glUniform1i(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),p_value); }\n\n"
            )
            fd.write(
                "\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, uint16_t p_value,RID p_version,ShaderVariant p_variant"
                + defvariant
                + ",uint64_t p_specialization="
                + str(defspec)
                + ") { _FU glUniform1ui(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),p_value); }\n\n"
            )
            fd.write(
                "\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, int16_t p_value,RID p_version,ShaderVariant p_variant"
                + defvariant
                + ",uint64_t p_specialization="
                + str(defspec)
                + ") { _FU glUniform1i(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),p_value); }\n\n"
            )
            fd.write(
                "\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, uint32_t p_value,RID p_version,ShaderVariant p_variant"
                + defvariant
                + ",uint64_t p_specialization="
                + str(defspec)
                + ") { _FU glUniform1ui(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),p_value); }\n\n"
            )
            fd.write(
                "\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, int32_t p_value,RID p_version,ShaderVariant p_variant"
                + defvariant
                + ",uint64_t p_specialization="
                + str(defspec)
                + ") { _FU glUniform1i(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),p_value); }\n\n"
            )
            fd.write(
                "\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Color& p_color,RID p_version,ShaderVariant p_variant"
                + defvariant
                + ",uint64_t p_specialization="
                + str(defspec)
                + ") { _FU GLfloat col[4]={p_color.r,p_color.g,p_color.b,p_color.a}; glUniform4fv(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),1,col); }\n\n"
            )
            fd.write(
                "\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Vector2& p_vec2,RID p_version,ShaderVariant p_variant"
                + defvariant
                + ",uint64_t p_specialization="
                + str(defspec)
                + ") { _FU GLfloat vec2[2]={float(p_vec2.x),float(p_vec2.y)}; glUniform2fv(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),1,vec2); }\n\n"
            )
            fd.write(
                "\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Size2i& p_vec2,RID p_version,ShaderVariant p_variant"
                + defvariant
                + ",uint64_t p_specialization="
                + str(defspec)
                + ") { _FU GLint vec2[2]={GLint(p_vec2.x),GLint(p_vec2.y)}; glUniform2iv(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),1,vec2); }\n\n"
            )
            fd.write(
                "\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Vector3& p_vec3,RID p_version,ShaderVariant p_variant"
                + defvariant
                + ",uint64_t p_specialization="
                + str(defspec)
                + ") { _FU GLfloat vec3[3]={float(p_vec3.x),float(p_vec3.y),float(p_vec3.z)}; glUniform3fv(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),1,vec3); }\n\n"
            )
            fd.write(
                "\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Vector4& p_vec4,RID p_version,ShaderVariant p_variant"
                + defvariant
                + ",uint64_t p_specialization="
                + str(defspec)
                + ") { _FU GLfloat vec4[4]={float(p_vec4.x),float(p_vec4.y),float(p_vec4.z),float(p_vec4.w)}; glUniform4fv(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),1,vec4); }\n\n"
            )
            fd.write(
                "\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, float p_a, float p_b,RID p_version,ShaderVariant p_variant"
                + defvariant
                + ",uint64_t p_specialization="
                + str(defspec)
                + ") { _FU glUniform2f(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),p_a,p_b); }\n\n"
            )
            fd.write(
                "\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, float p_a, float p_b, float p_c,RID p_version,ShaderVariant p_variant"
                + defvariant
                + ",uint64_t p_specialization="
                + str(defspec)
                + ") { _FU glUniform3f(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),p_a,p_b,p_c); }\n\n"
            )
            fd.write(
                "\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, float p_a, float p_b, float p_c, float p_d,RID p_version,ShaderVariant p_variant"
                + defvariant
                + ",uint64_t p_specialization="
                + str(defspec)
                + ") { _FU glUniform4f(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),p_a,p_b,p_c,p_d); }\n\n"
            )

            fd.write(
                """\t_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Transform3D& p_transform,RID p_version,ShaderVariant p_variant"""
                + defvariant
                + """,uint64_t p_specialization="""
                + str(defspec)
                + """) {  _FU

                const Transform3D &tr = p_transform;

                GLfloat matrix[16]={ /* build a 16x16 matrix */
                    (GLfloat)tr.basis.rows[0][0],
                    (GLfloat)tr.basis.rows[1][0],
                    (GLfloat)tr.basis.rows[2][0],
                    (GLfloat)0,
                    (GLfloat)tr.basis.rows[0][1],
                    (GLfloat)tr.basis.rows[1][1],
                    (GLfloat)tr.basis.rows[2][1],
                    (GLfloat)0,
                    (GLfloat)tr.basis.rows[0][2],
                    (GLfloat)tr.basis.rows[1][2],
                    (GLfloat)tr.basis.rows[2][2],
                    (GLfloat)0,
                    (GLfloat)tr.origin.x,
                    (GLfloat)tr.origin.y,
                    (GLfloat)tr.origin.z,
                    (GLfloat)1
                };

                        glUniformMatrix4fv(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),1,false,matrix);

            }

            """
            )

            fd.write(
                """_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Transform2D& p_transform,RID p_version,ShaderVariant p_variant"""
                + defvariant
                + """,uint64_t p_specialization="""
                + str(defspec)
                + """) {  _FU

                const Transform2D &tr = p_transform;

            GLfloat matrix[16]={ /* build a 16x16 matrix */
                (GLfloat)tr.columns[0][0],
                (GLfloat)tr.columns[0][1],
                (GLfloat)0,
                (GLfloat)0,
                (GLfloat)tr.columns[1][0],
                (GLfloat)tr.columns[1][1],
                (GLfloat)0,
                (GLfloat)0,
                (GLfloat)0,
                (GLfloat)0,
                (GLfloat)1,
                (GLfloat)0,
                (GLfloat)tr.columns[2][0],
                (GLfloat)tr.columns[2][1],
                (GLfloat)0,
                (GLfloat)1
            };

                glUniformMatrix4fv(version_get_uniform(p_uniform,p_version,p_variant,p_specialization),1,false,matrix);

            }

            """
            )

            fd.write(
                """_FORCE_INLINE_ void version_set_uniform(Uniforms p_uniform, const Projection& p_matrix, RID p_version, ShaderVariant p_variant"""
                + defvariant
                + """,uint64_t p_specialization="""
                + str(defspec)
                + """) {  _FU

                GLfloat matrix[16];

                for (int i = 0; i < 4; i++) {
                    for (int j = 0; j < 4; j++) {
                        matrix[i * 4 + j] = p_matrix.columns[i][j];
                    }
                }

                glUniformMatrix4fv(version_get_uniform(p_uniform, p_version, p_variant, p_specialization), 1, false, matrix);
        }"""
            )

            fd.write("\n\n#undef _FU\n\n\n")

        fd.write("protected:\n\n")

        fd.write("\tvirtual void _init() override {\n\n")

        if header_data.uniforms:
            fd.write("\t\tstatic const char* _uniform_strings[]={\n")
            if header_data.uniforms:
                for x in header_data.uniforms:
                    fd.write('\t\t\t"' + x + '",\n')
            fd.write("\t\t};\n\n")
        else:
            fd.write("\t\tstatic const char **_uniform_strings=nullptr;\n")

        variant_count = 1
        if len(header_data.variant_defines) > 0:
            fd.write("\t\tstatic const char* _variant_defines[]={\n")
            for x in header_data.variant_defines:
                fd.write('\t\t\t"' + x + '",\n')
            fd.write("\t\t};\n\n")
            variant_count = len(header_data.variant_defines)
        else:
            fd.write('\t\tstatic const char **_variant_defines[]={" "};\n')

        if header_data.texunits:
            fd.write("\t\tstatic TexUnitPair _texunit_pairs[]={\n")
            for x in header_data.texunits:
                fd.write('\t\t\t{"' + x[0] + '",' + x[1] + "},\n")
            fd.write("\t\t};\n\n")
        else:
            fd.write("\t\tstatic TexUnitPair *_texunit_pairs=nullptr;\n")

        if header_data.ubos:
            fd.write("\t\tstatic UBOPair _ubo_pairs[]={\n")
            for x in header_data.ubos:
                fd.write('\t\t\t{"' + x[0] + '",' + x[1] + "},\n")
            fd.write("\t\t};\n\n")
        else:
            fd.write("\t\tstatic UBOPair *_ubo_pairs=nullptr;\n")

        specializations_found = []

        if header_data.specialization_names:
            fd.write("\t\tstatic Specialization _spec_pairs[]={\n")
            for i in range(len(header_data.specialization_names)):
                defval = header_data.specialization_values[i].strip()
                if defval.upper() == "TRUE" or defval == "1":
                    defval = "true"
                else:
                    defval = "false"

                fd.write('\t\t\t{"' + header_data.specialization_names[i] + '",' + defval + "},\n")
                specializations_found.append(header_data.specialization_names[i])
            fd.write("\t\t};\n\n")
        else:
            fd.write("\t\tstatic Specialization *_spec_pairs=nullptr;\n")

        feedback_count = 0

        if header_data.feedbacks:
            fd.write("\t\tstatic const Feedback _feedbacks[]={\n")
            for x in header_data.feedbacks:
                name = x[0]
                spec = x[1]
                if spec in specializations_found:
                    fd.write('\t\t\t{"' + name + '",' + str(1 << specializations_found.index(spec)) + "},\n")
                else:
                    fd.write('\t\t\t{"' + name + '",0},\n')

                feedback_count += 1

            fd.write("\t\t};\n\n")
        else:
            fd.write("\t\tstatic const Feedback* _feedbacks=nullptr;\n")

        fd.write("\t\tstatic const char _vertex_code[]={\n")
        fd.write(to_raw_cstring(header_data.vertex_lines))
        fd.write("\n\t\t};\n\n")

        fd.write("\t\tstatic const char _fragment_code[]={\n")
        fd.write(to_raw_cstring(header_data.fragment_lines))
        fd.write("\n\t\t};\n\n")

        fd.write(
            '\t\t_setup(_vertex_code,_fragment_code,"'
            + out_file_class
            + '",'
            + str(len(header_data.uniforms))
            + ",_uniform_strings,"
            + str(len(header_data.ubos))
            + ",_ubo_pairs,"
            + str(feedback_count)
            + ",_feedbacks,"
            + str(len(header_data.texunits))
            + ",_texunit_pairs,"
            + str(len(header_data.specialization_names))
            + ",_spec_pairs,"
            + str(variant_count)
            + ",_variant_defines);\n"
        )

        fd.write("\t}\n\n")

        fd.write("};\n")


def build_gles3_headers(target, source, env):
    env.NoCache(target)
    for x in source:
        build_gles3_header(str(x), include="drivers/gles3/shader_gles3.h", class_suffix="GLES3")
