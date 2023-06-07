/**
 * modular_resource_picker.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "modular_resource_picker.h"
#include "core/object/method_bind.h"

void ModularResourcePicker::_update_resource_preview(const String &p_path, const Ref<Texture2D> &p_preview, const Ref<Texture2D> &p_small_preview, ObjectID p_obj) {
}

void ModularResourcePicker::_bind_methods() {
	String mdname = "_update_resource_preview";
	MethodBind *bind = create_method_bind(&ModularResourcePicker::_update_resource_preview);
	bind->set_name(mdname);
	auto type = ClassDB::classes.getptr(get_class_static());
	if (type) {
#ifdef DEBUG_METHODS_ENABLED
		type->method_order.push_back(mdname);
#endif
		type->method_map[mdname] = bind;
	}
}
