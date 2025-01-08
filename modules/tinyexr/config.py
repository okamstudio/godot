def can_build(env, platform):
    return env["tinyexr_always"] or env.editor_build

def get_opts(platform):
    from SCons.Variables import BoolVariable
    return [
        BoolVariable("tinyexr_always", "Enable tinyexr module in non-editor builds too", False),
    ]

def configure(env):
    pass
