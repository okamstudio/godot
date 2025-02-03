def can_build(env, platform):
    return env["minizip"]


def configure(env):
    pass


def get_doc_classes():
    return [
        "ZIPReader",
        "ZIPPacker",
    ]


def get_doc_path():
    return "doc_classes"
