proto_mod = """
#define MODBIND$VER($RETTYPE m_name$ARG) \\
virtual $RETVAL _##m_name($FUNCARGS) $CONST; \\
_FORCE_INLINE_ virtual $RETVAL m_name($FUNCARGS) $CONST override { \\
    $RETX _##m_name($CALLARGS);\\
}
"""


def generate_mod_version(argcount, const=False, returns=False):
    s = proto_mod
    sproto = str(argcount)
    if returns:
        sproto += "R"
        s = s.replace("$RETTYPE", "m_ret, ")
        s = s.replace("$RETVAL", "m_ret")
        s = s.replace("$RETX", "return")

    else:
        s = s.replace("$RETTYPE", "")
        s = s.replace("$RETVAL", "void")
        s = s.replace("$RETX", "")

    if const:
        sproto += "C"
        s = s.replace("$CONST", "const")
    else:
        s = s.replace("$CONST", "")

    s = s.replace("$VER", sproto)
    argtext = ""
    funcargs = ""
    callargs = ""

    for i in range(argcount):
        if i > 0:
            funcargs += ", "
            callargs += ", "

        argtext += ", m_type" + str(i + 1)
        funcargs += "m_type" + str(i + 1) + " arg" + str(i + 1)
        callargs += "arg" + str(i + 1)

    if argcount:
        s = s.replace("$ARG", argtext)
        s = s.replace("$FUNCARGS", funcargs)
        s = s.replace("$CALLARGS", callargs)
    else:
        s = s.replace("$ARG", "")
        s = s.replace("$FUNCARGS", funcargs)
        s = s.replace("$CALLARGS", callargs)

    return s


proto_ex = """
#define EXBIND$VER($RETTYPE m_name$ARG) \\
GDVIRTUAL$VER_REQUIRED($RETTYPE_##m_name$ARG)\\
virtual $RETVAL m_name($FUNCARGS) $CONST override { \\
    $RETPRE\\
    GDVIRTUAL_CALL(_##m_name$CALLARGS$RETREF);\\
    $RETPOST\\
}
"""


def generate_ex_version(argcount, const=False, returns=False):
    s = proto_ex
    sproto = str(argcount)
    if returns:
        sproto += "R"
        s = s.replace("$RETTYPE", "m_ret, ")
        s = s.replace("$RETVAL", "m_ret")
        s = s.replace("$RETPRE", "m_ret ret; ZeroInitializer<m_ret>::initialize(ret);\\\n")
        s = s.replace("$RETPOST", "return ret;\\\n")

    else:
        s = s.replace("$RETTYPE", "")
        s = s.replace("$RETVAL", "void")
        s = s.replace("$RETPRE", "")
        s = s.replace("$RETPOST", "return;")

    if const:
        sproto += "C"
        s = s.replace("$CONST", "const")
    else:
        s = s.replace("$CONST", "")

    s = s.replace("$VER", sproto)
    argtext = ""
    funcargs = ""
    callargs = ""

    for i in range(argcount):
        if i > 0:
            funcargs += ", "

        argtext += ", m_type" + str(i + 1)
        funcargs += "m_type" + str(i + 1) + " arg" + str(i + 1)
        callargs += ", arg" + str(i + 1)

    if argcount:
        s = s.replace("$ARG", argtext)
        s = s.replace("$FUNCARGS", funcargs)
        s = s.replace("$CALLARGS", callargs)
    else:
        s = s.replace("$ARG", "")
        s = s.replace("$FUNCARGS", funcargs)
        s = s.replace("$CALLARGS", callargs)

    if returns:
        s = s.replace("$RETREF", ", ret")
    else:
        s = s.replace("$RETREF", "")

    return s


def run(target, source, env):
    max_versions = 12

    txt = "#pragma once"

    for i in range(max_versions + 1):
        txt += "\n/* Extension Wrapper " + str(i) + " Arguments */\n"
        txt += generate_ex_version(i, False, False)
        txt += generate_ex_version(i, False, True)
        txt += generate_ex_version(i, True, False)
        txt += generate_ex_version(i, True, True)

    for i in range(max_versions + 1):
        txt += "\n/* Module Wrapper " + str(i) + " Arguments */\n"
        txt += generate_mod_version(i, False, False)
        txt += generate_mod_version(i, False, True)
        txt += generate_mod_version(i, True, False)
        txt += generate_mod_version(i, True, True)

    with open(str(target[0]), "w", encoding="utf-8", newline="\n") as f:
        f.write(txt)
