/**
 * spike_log.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "core/string/print_string.h"
#include "core/variant/variant.h"

#ifdef TOOLS_ENABLED
#define ED_MSG(fmt, ...) EditorNode::get_singleton()->get_log()->add_message(vformat(String(fmt), __VA_ARGS__), EditorLog::MSG_TYPE_EDITOR)
#else
#define ED_MSG(fmt, ...)
#endif

#ifdef DEBUG_ENABLED
#define VLog(fmt, ...) print_verbose(vformat(String(fmt), __VA_ARGS__))
#define DLog(fmt, ...) print_line(vformat(String(fmt), __VA_ARGS__))
#else
#define VLog(fmt, ...)
#define DLog(fmt, ...)
#endif

#define WLog(fmt, ...) print_line(vformat(String("[%s:%d]") + fmt, __FILE__, __LINE__, __VA_ARGS__))
#define ELog(fmt, ...) print_error(vformat(String("[%s:%d]") + fmt, __FILE__, __LINE__, __VA_ARGS__))
