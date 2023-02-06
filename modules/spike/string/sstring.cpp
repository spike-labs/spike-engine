/**
 * sstring.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "sstring.h"

#ifdef TOOLS_ENABLED
String STTR(const String &p_text, const String &p_context) {
    return TTR(p_text, !p_context.is_empty() ? p_context : CONTEXT_SPIKE);
}
String STTRN(const String &p_text, const String &p_text_plural, int p_n, const String &p_context) {
    return TTRN(p_text, p_text_plural, p_n, !p_context.is_empty() ? p_context : CONTEXT_SPIKE);
}
String SDTR(const String &p_text, const String &p_context) {
    return DTR(p_text, !p_context.is_empty() ? p_context : CONTEXT_SPIKE);
}
String SDTRN(const String &p_text, const String &p_text_plural, int p_n, const String &p_context) {
    return DTRN(p_text, p_text_plural, p_n, !p_context.is_empty() ? p_context : CONTEXT_SPIKE);
}
#endif