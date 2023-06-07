/**
 * register_types.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "register_types.h"
#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "filesystem_server/file_provider.h"
#include "filesystem_server/filesystem_server.h"
#include "filesystem_server/providers/file_provider_pack.h"
#include "filesystem_server/providers/file_provider_remap.h"
#include "spike_define.h"

#ifdef TOOLS_ENABLED
#include "editor/project_scanner.h"
#endif

static FileSystemServer *filesystem_server = nullptr;

class FileSystemServerModule : public SpikeModule {
public:
	static void core(bool do_init) {
		if (do_init) {
			filesystem_server = memnew(FileSystemServer);
			GDREGISTER_ABSTRACT_CLASS(FileProvider);
			GDREGISTER_CLASS(FileProviderPack);
			GDREGISTER_CLASS(FileProviderRemap);
			GDREGISTER_CLASS(FileSystemServer);
			GDREGISTER_CLASS(ProjectEnvironment);

#ifdef TOOLS_ENABLED
			GDREGISTER_CLASS(ProjectScanner);
#endif

			Engine::get_singleton()->add_singleton(Engine::Singleton("FileSystemServer", FileSystemServer::get_singleton()));
		} else {
			if (filesystem_server) {
				memdelete(filesystem_server);
				filesystem_server = nullptr;
			}
		}
	}

	static void servers(bool do_init) {
		if (do_init) {
		}
	}

	static void scene(bool do_init) {
		if (do_init) {
		}
	}

#ifdef TOOLS_ENABLED
	static void editor(bool do_init) {
		if (do_init) {
		}
	}
#endif
};

IMPL_SPIKE_MODULE(filesystem_server, FileSystemServerModule)