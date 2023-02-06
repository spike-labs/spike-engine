/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#pragma once
#include "spike_define.h"

#ifdef GODOT_3_X
#include "core/ustring.h"
#else
#include "core/string/ustring.h"
#endif

const String EMPTY_STRING = "";
const String SCRIPT_TYPE = "Script";
const String LUA_NAME = "Lua";
const String LUA_TYPE = "LuaScript";
const String LUA_EXTENSION = "lua";
const int LUA_PIL_IDENTATION_SIZE = 2; // Following PiL indenting often uses two spaces
const String LUA_PIL_IDENTATION = "  "; // Keep it in sync with LUA_PIL_IDENTATION_SIZE constant
