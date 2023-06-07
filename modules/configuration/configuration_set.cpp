/**
 * configuration_set.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "configuration_set.h"
#include "configuration_server.h"

void ConfigurationSet::_bind_methods() {
	ClassDB::bind_method(D_METHOD("size"), &ConfigurationSet::size);
	ClassDB::bind_method(D_METHOD("add", "name", "value"), &ConfigurationSet::add);
	ClassDB::bind_method(D_METHOD("remove", "name"), &ConfigurationSet::remove);
	ClassDB::bind_method(D_METHOD("to_dictionary"), &ConfigurationSet::to_dictionary);
}

bool ConfigurationSet::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == "__source") {
		source_path = p_value;
		return true;
	}

	if (is_patch) {
		Ref<ConfigurationSet> source_set = source_res;
		if (source_set.is_valid() && source_set->get(p_name) == p_value) {
			remove(p_name);
			return true;
		}
	}

	int index = _keys.find(p_name);
	if (index == -1) {
		_keys.append(p_name);
		_values.append(p_value);
	} else {
		_values.set(index, p_value);
	}

	return true;
}

bool ConfigurationSet::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == "__source") {
		r_ret = source_path;
		return true;
	}

	if (!is_patch) {
		auto patches = ConfigurationServer::get_patches(get_path());
		if (nullptr != patches && patches->size() > 0) {
			for (int i = patches->size() - 1; i >= 0; i--) {
				Ref<ConfigurationResource> elem = patches->get(i);
				if (elem == this)
					break;
				if (elem.is_valid()) {
					Variant val = elem->get(p_name);
					if (val.get_type() != Variant::NIL) {
						r_ret = val;
						return true;
					}
				}
			}
		}
	}

	int index = _keys.find(p_name);
	if (index >= 0) {
		r_ret = _values.get(index);
		return true;
	}

	if (is_patch) {
		Ref<ConfigurationSet> source_set = source_res;
		if (source_set.is_valid()) {
			return source_set->_get(p_name, r_ret);
		}
	}

	return false;
}

void ConfigurationSet::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::STRING, "__source", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
	for (int i = 0; i < _keys.size(); i++) {
		p_list->push_back(PropertyInfo(_values[i].get_type(), _keys[i], PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR));
	}
}

int ConfigurationSet::get_editable_size() const {
	if (is_patch) {
		Ref<ConfigurationSet> source_set = source_res;
		if (source_set.is_valid()) {
			return source_set->size();
		}
	}
	return size();
}

Vector<StringName> ConfigurationSet::get_editable_keys() const {
	if (is_patch) {
		Ref<ConfigurationSet> source_set = source_res;
		if (source_set.is_valid()) {
			return source_set->keys();
		}
	}
	return keys();
}

Vector<Variant> ConfigurationSet::get_editable_values() const {
	if (is_patch) {
		Ref<ConfigurationSet> source_set = source_res;
		if (source_set.is_valid()) {
			return source_set->values();
		}
	}
	return values();
}

bool ConfigurationSet::has(const String &p_name) const {
	return _keys.has(p_name);
}

Error ConfigurationSet::add(const String &p_name, const Variant &p_value) {
	ERR_FAIL_COND_V(_keys.has(p_name), ERR_ALREADY_EXISTS);
	_keys.append(p_name);
	_values.append(p_value);
	return OK;
}

Error ConfigurationSet::insert(const int &p_index, const String &p_name, const Variant &p_value) {
	ERR_FAIL_COND_V(_keys.has(p_name), ERR_ALREADY_EXISTS);
	_keys.insert(p_index, p_name);
	_values.insert(p_index, p_value);
	return OK;
}

Error ConfigurationSet::remove(const String &p_name) {
	int index = _keys.find(p_name);
	ERR_FAIL_COND_V(index == -1, ERR_DOES_NOT_EXIST);
	_keys.remove_at(index);
	_values.remove_at(index);
	return OK;
}

Dictionary ConfigurationSet::to_dictionary() const {
	Dictionary dictionary;
	for (int i = 0; i < _keys.size(); i++) {
		dictionary[_keys[i]] = _values[i];
	}
	return dictionary;
}

ConfigurationSet::ConfigurationSet() {
}

ConfigurationSet::~ConfigurationSet() {
}