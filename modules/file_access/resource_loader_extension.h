/* resource_loader_extension.h */

#ifndef RESOURCE_LOADER_EXTENSION_H
#define RESOURCE_LOADER_EXTENSION_H

#include "core/object/ref_counted.h"

class ResourceLoaderExtension : public RefCounted {
    GDCLASS(ResourceLoaderExtension, RefCounted);

protected:
    static void _bind_methods();

public:
    static Ref<Resource> load(const String &p_path);

    ResourceLoaderExtension() {}
	virtual ~ResourceLoaderExtension() {}
};
#endif