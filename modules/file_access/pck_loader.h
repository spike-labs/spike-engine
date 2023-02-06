/**
 * pck_loader.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */

#ifndef PCK_LOADER_H
#define PCK_LOADER_H

#include "core/object/ref_counted.h"

class PckLoader : public RefCounted {
    GDCLASS(PckLoader, RefCounted);

protected:
    static void _bind_methods();

public:
    static bool load_pack_with_key(const String &p_pack, const String &p_key, bool p_replace_files, int p_offset);
    static bool load_pack(const String &p_pack, bool p_replace_files, int p_offset);

    PckLoader() {}
	virtual ~PckLoader() {}
};
#endif