/**
 * engine_utils.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "engine_utils.h"

#include "spike/version.h"

void EngineUtils::_bind_methods() {
	ClassDB::bind_static_method(get_class_static(), D_METHOD("get_version_info"), &EngineUtils::get_version_info);
}

Dictionary EngineUtils::get_version_info() {
	Dictionary dict;
	dict["major"] = VERSION_SPIKE_MAJOR;
	dict["minor"] = VERSION_SPIKE_MINOR;
	dict["patch"] = VERSION_SPIKE_PATCH;
	dict["hex"] = VERSION_SPIKE_HEX;
	dict["status"] = VERSION_SPIKE_STATUS;
	dict["build"] = VERSION_BUILD;
	dict["year"] = VERSION_YEAR;

	String hash = String(VERSION_SPIKE_HASH);
	dict["hash"] = hash.is_empty() ? String("unknown") : hash;

	String stringver = String(dict["major"]) + "." + String(dict["minor"]);
	if (VERSION_SPIKE_PATCH != 0) {
		stringver += "." + itos(VERSION_SPIKE_PATCH);
	}
	stringver += "-" + String(dict["status"]) + " (" + String(dict["build"]) + ")";
	dict["string"] = stringver;

	return dict;
}
