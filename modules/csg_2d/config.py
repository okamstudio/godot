def can_build(env, platform):
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "CSGCapsule2D",
        "CSGCircle2D",
        "CSGCombiner2D",
        "CSGMesh2D",
        "CSGPolygon2D",
        "CSGPrimitive2D",
        "CSGShape2D",
        "CSGRectangle2D",
    ]


def get_doc_path():
    return "doc_classes"
