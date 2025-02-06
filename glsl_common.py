def minify_glsl(line: str):
    """
    Perform basic line-by-line GLSL minification to reduce the compiled binary size.
    """

    # index = line.find("//")
    # if index != -1:
    #     # Strip comments.
    #     line = line[:index]

    line = line.replace(" = ", "=")
    line = line.replace(" += ", "+=")
    line = line.replace(" -= ", "-=")
    line = line.replace(" *= ", "*=")
    line = line.replace(" /= ", "/=")
    line = line.replace(" %= ", "%=")
    line = line.replace(" &= ", "&=")
    line = line.replace(" |= ", "|=")
    line = line.replace(" ^= ", "^=")

    line = line.replace(", ", ",")
    line = line.replace("; ", ";")
    line = line.replace("if (", "if(")
    line = line.replace("} else {", "}else{")
    line = line.replace("} else", "}else")
    line = line.replace("for (", "for(")
    line = line.replace("while (", "while(")
    line = line.replace(") {", "){")
    line = line.replace(" && ", "&&")
    line = line.replace(" || ", "||")

    line = line.replace(" + ", "+")
    line = line.replace(" - ", "-")
    line = line.replace(" * ", "*")
    line = line.replace(" / ", "/")
    line = line.replace(" % ", "%")
    line = line.replace(" & ", "&")
    line = line.replace(" | ", "|")
    line = line.replace(" ^ ", "^")
    line = line.replace(" >> ", ">>")
    line = line.replace(" << ", "<<")

    return line
