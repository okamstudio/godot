#!/usr/bin/env python
# -*- coding: utf-8 -*-

import codecs
import sys
import xml.etree.ElementTree as ET

input_list = []

for arg in sys.argv[1:]:
    input_list.append(arg)

if len(input_list) < 1:
    print 'usage: makerst.py <classes.xml>'
    sys.exit(0)


def validate_tag(elem, tag):
    if elem.tag != tag:
        print "Tag mismatch, expected '" + tag + "', got " + elem.tag
        sys.exit(255)


class_names = []
classes = {}


def ul_string(str, ul):
    str += "\n"
    for i in range(len(str) - 1):
        str += ul
    str += "\n"
    return str


def make_class_list(class_list, columns):

    f = codecs.open('class_list.rst', 'wb', 'utf-8')
    prev = 0
    col_max = len(class_list) / columns + 1
    print ('col max is ', col_max)
    col_count = 0
    row_count = 0
    last_initial = ''
    fit_columns = []

    for n in range(0, columns):
        fit_columns += [[]]

    indexers = []
    last_initial = ''

    idx = 0
    for n in class_list:
        col = idx / col_max
        if col >= columns:
            col = columns - 1
        fit_columns[col] += [n]
        idx += 1
        if n[:1] != last_initial:
            indexers += [n]
        last_initial = n[:1]

    row_max = 0
    f.write("\n")

    for n in range(0, columns):
        if len(fit_columns[n]) > row_max:
            row_max = len(fit_columns[n])

    f.write("| ")
    for n in range(0, columns):
        f.write(" | |")

    f.write("\n")
    f.write("+")
    for n in range(0, columns):
        f.write("--+-------+")
    f.write("\n")

    for r in range(0, row_max):
        s = '+ '
        for c in range(0, columns):
            if r >= len(fit_columns[c]):
                continue

            classname = fit_columns[c][r]
            initial = classname[0]
            if classname in indexers:
                s += '**' + initial + '** | '
            else:
                s += ' | '

            s += '[' + classname + '](class_' + classname.lower() + ') | '

        s += '\n'
        f.write(s)

    for n in range(0, columns):
        f.write("--+-------+")
    f.write("\n")
    f.close()


def rstize_text(text, cclass):

    # Linebreak + tabs in the XML should become two line breaks unless in a "codeblock"
    pos = 0
    while True:
        pos = text.find('\n', pos)
        if pos == -1:
            break

        pre_text = text[:pos]
        while text[pos + 1] == '\t':
            pos += 1
        post_text = text[pos + 1:]

        # Handle codeblocks
        if post_text.startswith("[codeblock]"):
            end_pos = post_text.find("[/codeblock]")
            if end_pos == -1:
                sys.exit("ERROR! [codeblock] without a closing tag!")

            code_text = post_text[len("[codeblock]"):end_pos]
            post_text = post_text[end_pos:]

            # Remove extraneous tabs
            code_pos = 0
            while True:
                code_pos = code_text.find('\n', code_pos)
                if code_pos == -1:
                    break

                to_skip = 0
                while code_pos + to_skip + 1 < len(code_text) and code_text[code_pos + to_skip + 1] == '\t':
                    to_skip += 1

                if len(code_text[code_pos + to_skip + 1:]) == 0:
                    code_text = code_text[:code_pos] + "\n"
                    code_pos += 1
                else:
                    code_text = code_text[:code_pos] + "\n    " + code_text[code_pos + to_skip + 1:]
                    code_pos += 5 - to_skip

            text = pre_text + "\n[codeblock]" + code_text + post_text
            pos += len("\n[codeblock]" + code_text)

        # Handle normal text
        else:
            text = pre_text + "\n\n" + post_text
            pos += 2

    # Escape * character to avoid interpreting it as emphasis
    pos = 0
    while True:
        pos = text.find('*', pos)
        if pos == -1:
            break
        text = text[:pos] + "\*" + text[pos + 1:]
        pos += 2

    # Escape _ character at the end of a word to avoid interpreting it as an inline hyperlink
    pos = 0
    while True:
        pos = text.find('_', pos)
        if pos == -1:
            break
        if not text[pos + 1].isalnum():  # don't escape within a snake_case word
            text = text[:pos] + "\_" + text[pos + 1:]
            pos += 2
        else:
            pos += 1

    # Handle [tags]
    inside_code = False
    pos = 0
    while True:
        pos = text.find('[', pos)
        if pos == -1:
            break

        endq_pos = text.find(']', pos + 1)
        if endq_pos == -1:
            break

        pre_text = text[:pos]
        post_text = text[endq_pos + 1:]
        tag_text = text[pos + 1:endq_pos]

        if tag_text in class_names:
            tag_text = make_type(tag_text)
        else:  # command
            cmd = tag_text
            space_pos = tag_text.find(' ')
            if cmd == '/codeblock':
                tag_text = ''
                inside_code = False
                # Strip newline if the tag was alone on one
                if pre_text[-1] == '\n':
                    pre_text = pre_text[:-1]
            elif cmd == '/code':
                tag_text = '``'
                inside_code = False
            elif inside_code:
                tag_text = '[' + tag_text + ']'
            elif cmd.find('html') == 0:
                cmd = tag_text[:space_pos]
                param = tag_text[space_pos + 1:]
                tag_text = param
            elif cmd.find('method') == 0:
                cmd = tag_text[:space_pos]
                param = tag_text[space_pos + 1:]

                if param.find('.') != -1:
                    (class_param, method_param) = param.split('.')
                    tag_text = ':ref:`' + class_param + '.' + method_param + '<class_' + class_param + '_' + method_param + '>`'
                else:
                    tag_text = ':ref:`' + param + '<class_' + cclass + "_" + param + '>`'
            elif cmd.find('image=') == 0:
                tag_text = ""  # '![](' + cmd[6:] + ')'
            elif cmd.find('url=') == 0:
                tag_text = ':ref:`' + cmd[4:] + '<' + cmd[4:] + ">`"
            elif cmd == '/url':
                tag_text = ')'
            elif cmd == 'center':
                tag_text = ''
            elif cmd == '/center':
                tag_text = ''
            elif cmd == 'codeblock':
                tag_text = '\n::\n'
                inside_code = True
            elif cmd == 'br':
                # Make a new paragraph instead of a linebreak, rst is not so linebreak friendly
                tag_text = '\n\n'
                # Strip potential leading spaces
                while post_text[0] == ' ':
                    post_text = post_text[1:]
            elif cmd == 'i' or cmd == '/i':
                tag_text = '*'
            elif cmd == 'b' or cmd == '/b':
                tag_text = '**'
            elif cmd == 'u' or cmd == '/u':
                tag_text = ''
            elif cmd == 'code':
                tag_text = '``'
                inside_code = True
            else:
                tag_text = make_type(tag_text)

        text = pre_text + tag_text + post_text
        pos = len(pre_text) + len(tag_text)

 # tnode = ET.SubElement(parent,"div")
 # tnode.text=text

    return text


def make_type(t):
    global class_names
    if t in class_names:
        return ':ref:`' + t + '<class_' + t.lower() + '>`'
    return t


def make_method(
        f,
        name,
        m,
        declare,
        cname,
        event=False,
        pp=None
):

    if (declare or pp == None):
        t = '- '
    else:
        t = ""

    ret_type = 'void'
    args = list(m)
    mdata = {}
    mdata['argidx'] = []
    for a in args:
        if a.tag == 'return':
            idx = -1
        elif a.tag == 'argument':
            idx = int(a.attrib['index'])
        else:
            continue

        mdata['argidx'].append(idx)
        mdata[idx] = a

    if not event:
        if -1 in mdata['argidx']:
            t += make_type(mdata[-1].attrib['type'])
        else:
            t += 'void'
        t += ' '

    if declare or pp == None:

        # span.attrib["class"]="funcdecl"
        # a=ET.SubElement(span,"a")
        # a.attrib["name"]=name+"_"+m.attrib["name"]
        # a.text=name+"::"+m.attrib["name"]

        s = ' **' + m.attrib['name'] + '** '
    else:
        s = ':ref:`' + m.attrib['name'] + '<class_' + cname + "_" + m.attrib['name'] + '>` '

    s += ' **(**'
    argfound = False
    for a in mdata['argidx']:
        arg = mdata[a]
        if a < 0:
            continue
        if a > 0:
            s += ', '
        else:
            s += ' '

        s += make_type(arg.attrib['type'])
        if 'name' in arg.attrib:
            s += ' ' + arg.attrib['name']
        else:
            s += ' arg' + str(a)

        if 'default' in arg.attrib:
            s += '=' + arg.attrib['default']

        argfound = True

    if argfound:
        s += ' '
    s += ' **)**'

    if 'qualifiers' in m.attrib:
        s += ' ' + m.attrib['qualifiers']

#	f.write(s)
    if (not declare):
        if (pp != None):
            pp.append((t, s))
        else:
            f.write("- " + t + " " + s + "\n")
    else:
        f.write(t + s + "\n")


def make_heading(title, underline):
    return title + '\n' + underline * len(title) + "\n\n"


def make_rst_class(node):

    name = node.attrib['name']

    f = codecs.open("class_" + name.lower() + '.rst', 'wb', 'utf-8')

    # Warn contributors not to edit this file directly
    f.write(".. Generated automatically by doc/tools/makerst.py in Godot's source tree.\n")
    f.write(".. DO NOT EDIT THIS FILE, but the doc/base/classes.xml source instead.\n\n")

    f.write(".. _class_" + name + ":\n\n")
    f.write(make_heading(name, '='))

    if 'inherits' in node.attrib:
        inh = node.attrib['inherits'].strip()
#		whle inh in classes[cn]
        f.write('**Inherits:** ')
        first = True
        while(inh in classes):
            if (not first):
                f.write(" **<** ")
            else:
                first = False

            f.write(make_type(inh))
            inode = classes[inh]
            if ('inherits' in inode.attrib):
                inh = inode.attrib['inherits'].strip()
            else:
                inh = None

        f.write("\n\n")

    inherited = []
    for cn in classes:
        c = classes[cn]
        if 'inherits' in c.attrib:
            if (c.attrib['inherits'].strip() == name):
                inherited.append(c.attrib['name'])

    if (len(inherited)):
        f.write('**Inherited By:** ')
        for i in range(len(inherited)):
            if (i > 0):
                f.write(", ")
            f.write(make_type(inherited[i]))
        f.write("\n\n")
    if 'category' in node.attrib:
        f.write('**Category:** ' + node.attrib['category'].strip() + "\n\n")

    f.write(make_heading('Brief Description', '-'))
    briefd = node.find('brief_description')
    if briefd != None:
        f.write(rstize_text(briefd.text.strip(), name) + "\n\n")

    methods = node.find('methods')

    if methods != None and len(list(methods)) > 0:
        f.write(make_heading('Member Functions', '-'))
        ml = []
        for m in list(methods):
            make_method(f, node.attrib['name'], m, False, name, False, ml)
        longest_t = 0
        longest_s = 0
        for s in ml:
            sl = len(s[0])
            if (sl > longest_s):
                longest_s = sl
            tl = len(s[1])
            if (tl > longest_t):
                longest_t = tl

        sep = "+"
        for i in range(longest_s + 2):
            sep += "-"
        sep += "+"
        for i in range(longest_t + 2):
            sep += "-"
        sep += "+\n"
        f.write(sep)
        for s in ml:
            rt = s[0]
            while(len(rt) < longest_s):
                rt += " "
            st = s[1]
            while(len(st) < longest_t):
                st += " "
            f.write("| " + rt + " | " + st + " |\n")
            f.write(sep)
        f.write('\n')

    events = node.find('signals')
    if events != None and len(list(events)) > 0:
        f.write(make_heading('Signals', '-'))
        for m in list(events):
            make_method(f, node.attrib['name'], m, True, name, True)
            d = m.find('description')
            if d == None or d.text.strip() == '':
                continue
            f.write(rstize_text(d.text.strip(), name))
            f.write("\n\n")
            
        f.write('\n')

    members = node.find('members')
    if members != None and len(list(members)) > 0:
        f.write(make_heading('Member Variables', '-'))

        for c in list(members):
            s = '- '
            s += make_type(c.attrib['type']) + ' '
            s += '**' + c.attrib['name'] + '**'
            if c.text.strip() != '':
                s += ' - ' + c.text.strip()
            f.write(s + '\n')
        f.write('\n')

    constants = node.find('constants')
    if constants != None and len(list(constants)) > 0:
        f.write(make_heading('Numeric Constants', '-'))
        for c in list(constants):
            s = '- '
            s += '**' + c.attrib['name'] + '**'
            if 'value' in c.attrib:
                s += ' = **' + c.attrib['value'] + '**'
            if c.text.strip() != '':
                s += ' --- ' + rstize_text(c.text.strip(), name)
            f.write(s + '\n')
        f.write('\n')

    descr = node.find('description')
    if descr != None and descr.text.strip() != '':
        f.write(make_heading('Description', '-'))
        f.write(rstize_text(descr.text.strip(), name) + "\n\n")

    methods = node.find('methods')
    if methods != None and len(list(methods)) > 0:
        f.write(make_heading('Member Function Description', '-'))
        for m in list(methods):
            f.write(".. _class_" + name + "_" + m.attrib['name'] + ":\n\n")
#			f.write(ul_string(m.attrib['name'],"^"))
            #f.write('\n<a name="'+m.attrib['name']+'">' + m.attrib['name'] + '</a>\n------\n')
            make_method(f, node.attrib['name'], m, True, name)
            f.write('\n')
            d = m.find('description')
            if d == None or d.text.strip() == '':
                continue
            f.write(rstize_text(d.text.strip(), name))
            f.write("\n\n")
        f.write('\n')
    f.close()


for file in input_list:
    tree = ET.parse(file)
    doc = tree.getroot()

    if 'version' not in doc.attrib:
        print "Version missing from 'doc'"
        sys.exit(255)

    version = doc.attrib['version']

    for c in list(doc):
        if c.attrib['name'] in class_names:
            continue
        class_names.append(c.attrib['name'])
        classes[c.attrib['name']] = c

class_names.sort()

# Don't make class list for Sphinx, :toctree: handles it
#make_class_list(class_names, 2)

for cn in class_names:
    c = classes[cn]
    make_rst_class(c)
