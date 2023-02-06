/**
 * register_types.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "register_types.h"
#include INC_CLASS_DB
#include "spike_file_access.h"
#include "pck_loader.h"
#include "resource_loader_extension.h"

class MODFileAcess : public SpikeModule {
public:
	static void core(bool do_init) {
		if (do_init) {
            GDREGISTER_CLASS(PckLoader);
            GDREGISTER_CLASS(ResourceLoaderExtension);
            GDREGISTER_CLASS(FileAccessVirtual);
		}
	}
};

IMPL_SPIKE_MODULE(file_access, MODFileAcess)