script_call = """ScriptInstance *_script_instance = ((Object *)(this))->get_script_instance();\\
		if (_script_instance) {\\
			Callable::CallError ce;\\
			$CALLSIARGS\\
			$CALLSIBEGIN_script_instance->callp(_gdvirtual_##$VARNAME##_sn, $CALLSIARGPASS, ce);\\
			if (ce.error == Callable::CallError::CALL_OK) {\\
				$CALLSIRET\\
				return true;\\
			}\\
		}"""

script_has_method = """ScriptInstance *_script_instance = ((Object *)(this))->get_script_instance();\\
		if (_script_instance && _script_instance->has_method(_gdvirtual_##$VARNAME##_sn)) {\\
			return true;\\
		}"""

proto = """#define GDVIRTUAL$VER($ALIAS $RET m_name $ARG)\\
	StringName _gdvirtual_##$VARNAME##_sn = #m_name;\\
	mutable bool _gdvirtual_##$VARNAME##_initialized = false;\\
	mutable void *_gdvirtual_##$VARNAME = nullptr;\\
	_FORCE_INLINE_ bool _gdvirtual_##$VARNAME##_call($CALLARGS) $CONST {\\
		$SCRIPTCALL\\
		if (unlikely(_get_extension() && !_gdvirtual_##$VARNAME##_initialized)) {\\
			MethodInfo mi = _gdvirtual_##$VARNAME##_get_method_info();\\
			uint32_t hash = mi.get_compatibility_hash();\\
			_gdvirtual_##$VARNAME = nullptr;\\
			if (_get_extension()->get_virtual_call_data2 && _get_extension()->call_virtual_with_data) {\\
				_gdvirtual_##$VARNAME = _get_extension()->get_virtual_call_data2(_get_extension()->class_userdata, &_gdvirtual_##$VARNAME##_sn, hash);\\
			} else if (_get_extension()->get_virtual2) {\\
				_gdvirtual_##$VARNAME = (void *)_get_extension()->get_virtual2(_get_extension()->class_userdata, &_gdvirtual_##$VARNAME##_sn, hash);\\
			}\\
			_GDVIRTUAL_GET_DEPRECATED(_gdvirtual_##$VARNAME, _gdvirtual_##$VARNAME##_sn, $COMPAT)\\
			_GDVIRTUAL_TRACK(_gdvirtual_##$VARNAME, _gdvirtual_##$VARNAME##_initialized);\\
			_gdvirtual_##$VARNAME##_initialized = true;\\
		}\\
		if (_gdvirtual_##$VARNAME) {\\
			$CALLPTRARGS\\
			$CALLPTRRETDEF\\
			if (_get_extension()->call_virtual_with_data) {\\
				_get_extension()->call_virtual_with_data(_get_extension_instance(), &_gdvirtual_##$VARNAME##_sn, _gdvirtual_##$VARNAME, $CALLPTRARGPASS, $CALLPTRRETPASS);\\
				$CALLPTRRET\\
			} else {\\
				((GDExtensionClassCallVirtual)_gdvirtual_##$VARNAME)(_get_extension_instance(), $CALLPTRARGPASS, $CALLPTRRETPASS);\\
				$CALLPTRRET\\
			}\\
			return true;\\
		}\\
		$REQCHECK\\
		$RVOID\\
		return false;\\
	}\\
	_FORCE_INLINE_ bool _gdvirtual_##$VARNAME##_overridden() const {\\
		$SCRIPTHASMETHOD\\
		if (unlikely(_get_extension() && !_gdvirtual_##$VARNAME##_initialized)) {\\
			MethodInfo mi = _gdvirtual_##$VARNAME##_get_method_info();\\
			uint32_t hash = mi.get_compatibility_hash();\\
			_gdvirtual_##$VARNAME = nullptr;\\
			if (_get_extension()->get_virtual_call_data2 && _get_extension()->call_virtual_with_data) {\\
				_gdvirtual_##$VARNAME = _get_extension()->get_virtual_call_data2(_get_extension()->class_userdata, &_gdvirtual_##$VARNAME##_sn, hash);\\
			} else if (_get_extension()->get_virtual2) {\\
				_gdvirtual_##$VARNAME = (void *)_get_extension()->get_virtual2(_get_extension()->class_userdata, &_gdvirtual_##$VARNAME##_sn, hash);\\
			}\\
			_GDVIRTUAL_GET_DEPRECATED(_gdvirtual_##$VARNAME, _gdvirtual_##$VARNAME##_sn, $COMPAT)\\
			_GDVIRTUAL_TRACK(_gdvirtual_##$VARNAME, _gdvirtual_##$VARNAME##_initialized);\\
			_gdvirtual_##$VARNAME##_initialized = true;\\
		}\\
		if (_gdvirtual_##$VARNAME) {\\
			return true;\\
		}\\
		return false;\\
	}\\
	_FORCE_INLINE_ static MethodInfo _gdvirtual_##$VARNAME##_get_method_info() {\\
		MethodInfo method_info;\\
		method_info.name = #m_name;\\
		method_info.flags = $METHOD_FLAGS;\\
		$FILL_METHOD_INFO\\
		return method_info;\\
	}

"""


def generate_version(argcount, const=False, returns=False, required=False, compat=False):
    s = proto
    if compat:
        s = s.replace("$SCRIPTCALL", "")
        s = s.replace("$SCRIPTHASMETHOD", "")
    else:
        s = s.replace("$SCRIPTCALL", script_call)
        s = s.replace("$SCRIPTHASMETHOD", script_has_method)

    sproto = str(argcount)
    method_info = ""
    method_flags = "METHOD_FLAG_VIRTUAL"
    if returns:
        sproto += "R"
        s = s.replace("$RET", "m_ret,")
        s = s.replace("$RVOID", "(void)r_ret;")  # If required, may lead to uninitialized errors
        s = s.replace("$CALLPTRRETDEF", "PtrToArg<m_ret>::EncodeT ret;")
        method_info += "method_info.return_val = GetTypeInfo<m_ret>::get_class_info();\\\n"
        method_info += "\t\tmethod_info.return_val_metadata = GetTypeInfo<m_ret>::METADATA;"
    else:
        s = s.replace("$RET ", "")
        s = s.replace("\t\t$RVOID\\\n", "")
        s = s.replace("\t\t\t$CALLPTRRETDEF\\\n", "")

    if const:
        sproto += "C"
        method_flags += " | METHOD_FLAG_CONST"
        s = s.replace("$CONST", "const")
    else:
        s = s.replace("$CONST ", "")

    if required:
        sproto += "_REQUIRED"
        method_flags += " | METHOD_FLAG_VIRTUAL_REQUIRED"
        s = s.replace(
            "$REQCHECK",
            'ERR_PRINT_ONCE("Required virtual method " + get_class() + "::" + #m_name + " must be overridden before calling.");',
        )
    else:
        s = s.replace("\t\t$REQCHECK\\\n", "")

    if compat:
        sproto += "_COMPAT"
        s = s.replace("$COMPAT", "true")
        s = s.replace("$ALIAS", "m_alias,")
        s = s.replace("$VARNAME", "m_alias")
    else:
        s = s.replace("$COMPAT", "false")
        s = s.replace("$ALIAS ", "")
        s = s.replace("$VARNAME", "m_name")

    s = s.replace("$METHOD_FLAGS", method_flags)
    s = s.replace("$VER", sproto)
    argtext = ""
    callargtext = ""
    callsiargs = ""
    callsiargptrs = ""
    callptrargsptr = ""
    if argcount > 0:
        argtext += ", "
        callsiargs = f"Variant vargs[{argcount}] = {{ "
        callsiargptrs = f"\t\t\tconst Variant *vargptrs[{argcount}] = {{ "
        callptrargsptr = f"\t\t\tGDExtensionConstTypePtr argptrs[{argcount}] = {{ "
    callptrargs = ""
    for i in range(argcount):
        if i > 0:
            argtext += ", "
            callargtext += ", "
            callsiargs += ", "
            callsiargptrs += ", "
            callptrargs += "\t\t\t"
            callptrargsptr += ", "
        argtext += f"m_type{i + 1}"
        callargtext += f"m_type{i + 1} arg{i + 1}"
        callsiargs += f"arg{i + 1}"
        callsiargptrs += f"&vargs[{i}]"
        callptrargs += (
            f"PtrToArg<m_type{i + 1}>::EncodeT argval{i + 1} = (PtrToArg<m_type{i + 1}>::EncodeT)arg{i + 1};\\\n"
        )
        callptrargsptr += f"&argval{i + 1}"
        if method_info:
            method_info += "\\\n\t\t"
        method_info += f"method_info.arguments.push_back(GetTypeInfo<m_type{i + 1}>::get_class_info());\\\n"
        method_info += f"\t\tmethod_info.arguments_metadata.push_back(GetTypeInfo<m_type{i + 1}>::METADATA);"

    if argcount:
        callsiargs += " };\\\n"
        callsiargptrs += " };"
        s = s.replace("$CALLSIARGS", callsiargs + callsiargptrs)
        s = s.replace("$CALLSIARGPASS", f"(const Variant **)vargptrs, {argcount}")
        callptrargsptr += " };"
        s = s.replace("$CALLPTRARGS", callptrargs + callptrargsptr)
        s = s.replace("$CALLPTRARGPASS", "reinterpret_cast<GDExtensionConstTypePtr *>(argptrs)")
    else:
        s = s.replace("\t\t\t$CALLSIARGS\\\n", "")
        s = s.replace("$CALLSIARGPASS", "nullptr, 0")
        s = s.replace("\t\t\t$CALLPTRARGS\\\n", "")
        s = s.replace("$CALLPTRARGPASS", "nullptr")

    if returns:
        if argcount > 0:
            callargtext += ", "
        callargtext += "m_ret &r_ret"
        s = s.replace("$CALLSIBEGIN", "Variant ret = ")
        s = s.replace("$CALLSIRET", "r_ret = VariantCaster<m_ret>::cast(ret);")
        s = s.replace("$CALLPTRRETPASS", "&ret")
        s = s.replace("$CALLPTRRET", "r_ret = (m_ret)ret;")
    else:
        s = s.replace("$CALLSIBEGIN", "")
        s = s.replace("\t\t\t\t$CALLSIRET\\\n", "")
        s = s.replace("$CALLPTRRETPASS", "nullptr")
        s = s.replace("\t\t\t\t$CALLPTRRET\\\n", "")

    s = s.replace(" $ARG", argtext)
    s = s.replace("$CALLARGS", callargtext)
    if method_info:
        s = s.replace("$FILL_METHOD_INFO", method_info)
    else:
        s = s.replace("\t\t$FILL_METHOD_INFO\\\n", method_info)

    return s


def run(target, source, env):
    max_versions = 12

    txt = """/* THIS FILE IS GENERATED DO NOT EDIT */
#pragma once

#include "core/object/script_instance.h"

#include <utility>

#ifdef TOOLS_ENABLED
#define _GDVIRTUAL_TRACK(m_virtual, m_initialized)\\
	if (_get_extension()->reloadable) {\\
		VirtualMethodTracker *tracker = memnew(VirtualMethodTracker);\\
		tracker->method = (void **)&m_virtual;\\
		tracker->initialized = &m_initialized;\\
		tracker->next = virtual_method_list;\\
		virtual_method_list = tracker;\\
	}
#else
#define _GDVIRTUAL_TRACK(m_virtual, m_initialized)
#endif

#ifndef DISABLE_DEPRECATED
#define _GDVIRTUAL_GET_DEPRECATED(m_virtual, m_name_sn, m_compat)\\
	else if (m_compat || ClassDB::get_virtual_method_compatibility_hashes(get_class_static(), m_name_sn).size() == 0) {\\
		if (_get_extension()->get_virtual_call_data && _get_extension()->call_virtual_with_data) {\\
			m_virtual = _get_extension()->get_virtual_call_data(_get_extension()->class_userdata, &m_name_sn);\\
		} else if (_get_extension()->get_virtual) {\\
			m_virtual = (void *)_get_extension()->get_virtual(_get_extension()->class_userdata, &m_name_sn);\\
		}\\
	}
#else
#define _GDVIRTUAL_GET_DEPRECATED(m_name, m_name_sn, m_compat)
#endif

"""

    for i in range(max_versions + 1):
        txt += f"/* {i} Arguments */\n\n"
        txt += generate_version(i, False, False)
        txt += generate_version(i, False, True)
        txt += generate_version(i, True, False)
        txt += generate_version(i, True, True)
        txt += generate_version(i, False, False, True)
        txt += generate_version(i, False, True, True)
        txt += generate_version(i, True, False, True)
        txt += generate_version(i, True, True, True)
        txt += generate_version(i, False, False, False, True)
        txt += generate_version(i, False, True, False, True)
        txt += generate_version(i, True, False, False, True)
        txt += generate_version(i, True, True, False, True)

    with open(str(target[0]), "w", encoding="utf-8", newline="\n") as f:
        f.write(txt)
