def can_build(env, platform):
    return env.editor_build


def configure(env):
    pass


def get_doc_classes():
    return [
            "Annotation", "MemberAnnotation", "BaseResourceAnnotation", "AnnotationExportPlugin",
            "EnumElementAnnotation", "EnumAnnotation", "VariableAnnotation", "ParameterAnnotation",
            "PropertyAnnotation", "ConstantAnnotation", "MethodAnnotation", "SignalAnnotation",
            "NodeAnnotation", "SceneAnnotation", "ResourceAnnotation", "ClassAnnotation",
            "ModuleAnnotation", "EditorPropertyAnnotation",
        ]


def get_doc_path():
    return "doc_classes"
