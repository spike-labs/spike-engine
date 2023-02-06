/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#include "luascript_syntax_highlighter.h"
#include "../constants.h"

LuaScriptSyntaxHighlighter::LuaScriptSyntaxHighlighter() {
}

LuaScriptSyntaxHighlighter::~LuaScriptSyntaxHighlighter() {
}

SyntaxHighlighter *LuaScriptSyntaxHighlighter::create() {
	return memnew(LuaScriptSyntaxHighlighter);
}

void LuaScriptSyntaxHighlighter::_update_cache() {
	EditorStandardSyntaxHighlighter::_update_cache();
}

#ifdef GODOT_3_X
Map<int, TextEdit::HighlighterInfo> LuaScriptSyntaxHighlighter::_get_line_syntax_highlighting(int p_line) {
	const String &line = text_editor->get_line(p_line);

	Map<int, TextEdit::HighlighterInfo> lsh_map;

	return lsh_map;
}

#else
Dictionary LuaScriptSyntaxHighlighter::_get_line_syntax_highlighting_impl(int p_line) {
	return EditorStandardSyntaxHighlighter::_get_line_syntax_highlighting_impl(p_line);
}
#endif

String LuaScriptSyntaxHighlighter::_get_name() const {
	return LUA_NAME;
}

PackedStringArray LuaScriptSyntaxHighlighter::_get_supported_languages() const {
	PackedStringArray supported_languages;
	supported_languages.push_back(LUA_NAME);
	return supported_languages;
}

Ref<EditorSyntaxHighlighter> LuaScriptSyntaxHighlighter::LuaScriptSyntaxHighlighter::_create() const {
	Ref<LuaScriptSyntaxHighlighter> syntax_highlighter;
	syntax_highlighter.instantiate();
	return syntax_highlighter;
}
