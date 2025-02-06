def can_build(env, platform):
    return env["platform"] in ["ios", "macos"]


def configure(env):
    if env["platform"] in ["ios", "macos"]:
        env.Append(LINKFLAGS=["-ObjC"])
        env.Append(CPPPATH=["#core"])
        # env.Append(LINKFLAGS=["-lStoreKit.macos." + env["arch"]])
