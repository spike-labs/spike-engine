/**
 * sstring.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include <core/string/ustring.h>

#ifdef TOOLS_ENABLED
#define CONTEXT_SPIKE "Spike"
#define CONTEXT_OVERRIDE "Override"
String STTR(const String &p_text, const String &p_context = "");
String STTRN(const String &p_text, const String &p_text_plural, int p_n, const String &p_context = "");
String SDTR(const String &p_text, const String &p_context = "");
String SDTRN(const String &p_text, const String &p_text_plural, int p_n, const String &p_context = "");
#endif