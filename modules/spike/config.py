def can_build(env, platform):
    return True


def configure(env):
	env.add_module_version_string("spike")

def get_doc_classes():
	return ["EditorUtils", "ExternalTranslationServer"]

def get_doc_path():
	return "doc_classes"
