def can_build(env, platform):
    if env.editor_build:  # Encoder dependencies
        env.module_add_dependencies("basis_universal", ["jpg", "tinyexr"])
    return True


def configure(env):
    pass
