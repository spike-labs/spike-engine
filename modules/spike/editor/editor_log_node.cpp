/**
 * editor_log_node.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "editor_log_node.h"

#include "core/os/keyboard.h"
#include "core/version.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "modules/regex/regex.h"
#include "scene/gui/center_container.h"

void SpikeEditorLogNode::_line_clicked(Variant p_line) {
	if (p_line.get_type() == Variant::STRING) {
		String msg = p_line.operator String();
		Vector<String> file_infos = msg.split(":", false);
		auto debugger = EditorDebuggerNode::get_singleton();
		auto stack_script = ResourceLoader::load(file_infos[0] + ":" + file_infos[1]);
		debugger->emit_signal("goto_script_line", stack_script, file_infos[2].to_int() - 1);
		debugger->emit_signal("set_execution", stack_script, file_infos[2].to_int() - 1);
	}
}

void SpikeEditorLogNode::_add_log_line(LogMessage &p_message, bool p_replace_previous) {
	if (!is_inside_tree()) {
		// The log will be built all at once when it enters the tree and has its theme items.
		return;
	}

	// Only add the message to the log if it passes the filters.
	bool filter_active = type_filter_map[p_message.type]->is_active();
	String search_text = search_box->get_text();
	bool search_match = search_text.is_empty() || p_message.text.findn(search_text) > -1;

	if (!filter_active || !search_match) {
		return;
	}

	if (p_replace_previous) {
		// Remove last line if replacing, as it will be replace by the next added line.
		// Why "- 2"? RichTextLabel is weird. When you add a line with add_newline(), it also adds an element to the list of lines which is null/blank,
		// but it still counts as a line. So if you remove the last line (count - 1) you are actually removing nothing...
		log->remove_paragraph(log->get_paragraph_count() - 2);
	}

	switch (p_message.type) {
		case MSG_TYPE_STD: {
			log->push_color(get_theme_color(SNAME("font_color"), SNAME("Editor")));
		} break;
		case MSG_TYPE_STD_RICH: {
			log->push_color(get_theme_color(SNAME("font_color"), SNAME("Editor")));
		} break;
		case MSG_TYPE_ERROR: {
			log->push_color(theme_cache.error_color);
			Ref<Texture2D> icon = theme_cache.error_icon;
			log->add_image(icon);
			log->add_text(" ");
			tool_button->set_icon(icon);
		} break;
		case MSG_TYPE_WARNING: {
			log->push_color(theme_cache.warning_color);
			Ref<Texture2D> icon = theme_cache.warning_icon;
			log->add_image(icon);
			log->add_text(" ");
			tool_button->set_icon(icon);
		} break;
		case MSG_TYPE_EDITOR: {
			// Distinguish editor messages from messages printed by the project
			log->push_color(theme_cache.message_color);
		} break;
	}

	// If collapsing, add the count of this message in bold at the start of the line.
	if (collapse && p_message.count > 1) {
		log->push_bold();
		log->add_text(vformat("(%s) ", itos(p_message.count)));
		log->pop();
	}

	RegEx re = RegEx();
	re.compile("res://(.*):(\\d+)");
	auto result = re.search(p_message.text);
	if (result != nullptr) {
		auto p_scritp_info = result->get_string(0);
		p_scritp_info = p_scritp_info.replace("\"]:", ":");
		log->push_meta(p_scritp_info);
	}

	if (p_message.type == MSG_TYPE_STD_RICH) {
		log->append_text(p_message.text);
	} else {
		log->add_text(p_message.text);
	}

	// Need to use pop() to exit out of the RichTextLabels current "push" stack.
	// We only "push" in the above switch when message type != STD and RICH, so only pop when that is the case.
	if (p_message.type != MSG_TYPE_STD && p_message.type != MSG_TYPE_STD_RICH) {
		log->pop();
	}

	log->add_newline();
}

void SpikeEditorLogNode::_bind_methods() {
	ClassDB::bind_method("_line_clicked", &SpikeEditorLogNode::_line_clicked);
}

SpikeEditorLogNode::SpikeEditorLogNode(EditorNode *p_editor) {
	log->connect("meta_clicked", callable_mp(this, &SpikeEditorLogNode::_line_clicked));
}

SpikeEditorLogNode::~SpikeEditorLogNode() {
}