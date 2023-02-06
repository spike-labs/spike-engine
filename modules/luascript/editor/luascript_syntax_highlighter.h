/**
 * This file is part of Lua binding for Godot Engine.
 *
 */

#pragma once

#include "scene/gui/text_edit.h"
#include "spike_define.h"

#ifdef GODOT_3_X
class LuaScriptSyntaxHighlighter : public SyntaxHighlighter {
#else
#include "editor/plugins/script_editor_plugin.h"

class LuaScriptSyntaxHighlighter : public EditorStandardSyntaxHighlighter {
	GDCLASS(LuaScriptSyntaxHighlighter, EditorStandardSyntaxHighlighter)
#endif

public:
	LuaScriptSyntaxHighlighter();
	~LuaScriptSyntaxHighlighter();

	static SyntaxHighlighter *create();

	virtual void _update_cache() override;
#ifdef GODOT_3_X
	virtual Map<int, TextEdit::HighlighterInfo> _get_line_syntax_highlighting(int p_line) override;
#else
	virtual Dictionary _get_line_syntax_highlighting_impl(int p_line) override;
#endif

	virtual String _get_name() const override;
	virtual PackedStringArray _get_supported_languages() const override;
	virtual Ref<EditorSyntaxHighlighter> _create() const override;

private:
	Color completion_background_color;
	Color completion_selected_color;
	Color completion_existing_color;
	Color completion_font_color;
	Color caret_color;
	Color caret_background_color;
	Color line_number_color;
	Color font_color;
	Color font_selected_color;
	Color keyword_color;
	Color number_color;
	Color function_color;
	Color member_variable_color;
	Color selection_color;
	Color mark_color;
	Color breakpoint_color;
	Color code_folding_color;
	Color current_line_color;
	Color line_length_guideline_color;
	Color brace_mismatch_color;
	Color word_highlighted_color;
	Color search_result_color;
	Color search_result_border_color;
	Color symbol_color;
	Color background_color;
};
