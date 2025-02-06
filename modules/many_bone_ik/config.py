def can_build(env, platform):
    return not env["disable_3d"]


def configure(env):
    pass


def get_doc_classes():
    return [
        "ManyBoneIK3D",
        "IKBone3D",
        "IKEffector3D",
        "IKBoneSegment3D",
        "IKEffectorTemplate3D",
        "IKKusudama3D",
        "IKRay3D",
        "IKNode3D",
        "IKLimitCone3D",
    ]


def get_doc_path():
    return "doc_classes"
