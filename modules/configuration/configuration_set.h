/**
 * configuration_set.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "configuration_resource.h"
#include "spike_define.h"

class ConfigurationSet : public ConfigurationResource {
	GDCLASS(ConfigurationSet, ConfigurationResource);

protected:
	static void _bind_methods();

protected:
	Vector<StringName> _keys;
	Vector<Variant> _values;

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	int get_editable_size() const;
	Vector<StringName> get_editable_keys() const;
	Vector<Variant> get_editable_values() const;

	int size() const { return _keys.size(); }
	Vector<StringName> keys() const { return _keys; }
	Vector<StringName> *keys_ptr() { return &_keys; }
	Vector<Variant> values() const { return _values; }
	Vector<Variant> *values_ptr() { return &_values; }

	bool has(const String &p_name) const;
	Error add(const String &p_name, const Variant &p_value);
	Error insert(const int &p_index, const String &p_name, const Variant &p_value);
	Error remove(const String &p_name);

	Dictionary to_dictionary() const;

	ConfigurationSet();
	~ConfigurationSet();
};