def can_build(env, platform):
    env.module_add_dependencies("gdscript", ["jsonrpc", "websocket"], True)
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "@GDScript",
        "@GDTrait",
        "GDScript",
        "GDTrait",
        "GDScriptSyntaxHighlighter",
    ]


def get_doc_path():
    return "doc_classes"
