/**
 * This file is part of Lua binding for Godot Engine.
 *
 */
#include "luascript/luascript_language.h"
#ifdef GODOT_3_X
#include "core/os/dir_access.h"
#include "core/translation.h"
#else
#include "core/io/dir_access.h"
#include "core/string/translation.h"
#endif
#include "core/io/json.h"

static void _get_lua_var(String &ptype, String *pvalue) {
	if (ptype == "int") {
		if (pvalue && pvalue->length() == 0)
			*pvalue = "0";
		ptype = "number";
	} else if (ptype == "float") {
		if (pvalue && pvalue->length() == 0)
			*pvalue = "0.0";
		ptype = "number";
	} else if (ptype == "bool") {
		if (pvalue && pvalue->length() == 0)
			*pvalue = "false";
		ptype = "boolean";
	} else if (ptype == "String") {
		if (pvalue && pvalue->length() == 0)
			*pvalue = "\"\"";
		ptype = "string";
#ifndef GODOT_3_X
	} else if (ptype == "StringName") {
		if (pvalue) {
			*pvalue = pvalue->length() == 0 ? "\"\"" : pvalue->substr(1);
		}
		ptype = "string";
#endif
	} else if (ptype == "Variant") {
		if (pvalue && pvalue->length() == 0)
			*pvalue = "nil";
		ptype = "any";
	} else if (ptype == "Array" || ptype == "Dictionary") {
		if (pvalue) {
			if (pvalue->length() < 5) {
				*pvalue = "{}";
			} else {
				if (pvalue->get(0) == '[')
					pvalue->set(0, '{');
				if (pvalue->get(pvalue->length() - 1) == ']')
					pvalue->set(pvalue->length() - 1, '}');
				*pvalue = "[[" + *pvalue + "]]";
			}
		}
		ptype = "table";
	} else if (ptype.ends_with("[]")) {
		if (pvalue)
			*pvalue = "{}";
	} else {
		if (pvalue && pvalue->length() == 0)
			*pvalue = "{}";
	}
}

static void _parse_dtr(String &str) {
	str = DTR(str).replace("[code]", "`").replace("[/code]", "`");
	str = str.replace("[b]", "**").replace("[/b]", "**");
	str = str.replace("[i]", "*").replace("[/i]", "*");
	str = str.replace("[codeblock]", "```").replace("[/codeblock]", "```");
	str = str.replace("[gdscript]", "```gdscript").replace("[/gdscript]", "```");
	str = str.replace("[csharp]", "```csharp").replace("[/csharp]", "```");
	str = str.replace("\n", "<br>");
}

static void _write_dtr(FileAccessRef &f, String str, int indent = 0) {
	_parse_dtr(str);

	auto lines = str.split("<br>", true);
	if (lines.size()) {
		if (indent == 0)
			f->store_line("---");
		for (size_t i = 0; i < lines.size(); i++) {
			if (indent > 0)
				f->store_string(String("    ").repeat(indent));
			f->store_line("--- " + lines[i] + "  ");
		}
	}
}

static void _write_constant(FileAccessRef &f, String &enumeration, String &cname, DocData::ConstantDoc &k, const char *str) {
	String value = str == nullptr ? k.value : String(str);
	if (k.enumeration != String()) {
		String enum_name = k.enumeration.replace(".", "");
		bool new_enum = false;
		if (enumeration == String()) {
			enumeration = enum_name;
			new_enum = true;
		} else if (enumeration != enum_name) {
			f->store_line("}");
			enumeration = enum_name;
			new_enum = true;
		}
		if (new_enum) {
			f->store_8('\n');
			f->store_line("---@enum " + enumeration + ": number");
			f->store_line(cname + enumeration + " = {");
		}
		_write_dtr(f, k.description, 1);
		f->store_line("    " + k.name + " = " + value + ",");
	} else {
		if (enumeration != String()) {
			enumeration = String();
			f->store_line("}");
		}
		f->store_8('\n');
		_write_dtr(f, k.description);
		if (value.is_valid_float()) {
			f->store_line(cname + k.name + " = " + value);
		} else {
			f->store_line(cname + k.name + " = '" + value + "'");
		}
	}
}

static void _write_method(FileAccessRef &f, DocData::ClassDoc &c, DocData::MethodDoc &m, List<String> &reserved_worlds, bool is_global) {
	for (int j = 0; j < m.arguments.size(); j++) {
		auto a = m.arguments[j];
		if (reserved_worlds.find(a.name))
			a.name = "_" + a.name;
		String ptype = a.type;
		_get_lua_var(ptype, nullptr);
		f->store_line("---@param " + a.name + " " + ptype);
	}
	if (m.return_type != String()) {
		String ptype = m.return_type;
		_get_lua_var(ptype, nullptr);
		f->store_line("---@return " + ptype);
	}
	if (!is_global) {
		if (m.name != c.name) {
			if (reserved_worlds.find(m.name)) {
				f->store_string(c.name + "[\"" + m.name + "\"] = function(self");
				if (m.arguments.size() > 0)
					f->store_string(", ");
			} else {
				f->store_string("function " + c.name + ":" + m.name + "(");
			}
		} else {
			f->store_string("function " + c.name + ":new(");
		}
	} else {
		f->store_string("function godot." + m.name + "(");
	}
	for (int j = 0; j < m.arguments.size(); j++) {
		auto a = m.arguments[j];
		if (reserved_worlds.find(a.name))
			a.name = "_" + a.name;
		if (j > 0)
			f->store_string(", ");
		f->store_string(a.name);
	}
	f->store_line(") end");
}

#ifdef GODOT_3_X
extern void luascript_save_emmylua_doc(DocData &doc, String &path) {
	LuaScriptLanguage::save_emmylua_doc(doc, path);
}
void LuaScriptLanguage::save_emmylua_doc(DocData &doc, String &path)
#else
extern void luascript_save_emmylua_doc(DocTools &doc, String &path) {
	LuaScriptLanguage::save_emmylua_doc(doc, path);
}
void LuaScriptLanguage::save_emmylua_doc(DocTools &doc, String &path)
#endif
{
	List<String> reserved_worlds;
	_get_reserved_words(&reserved_worlds);

	String save_path = PATH_JOIN(path, "bin/doc/gdlua-" + TranslationServer::get_singleton()->get_locale());
	auto da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(save_path);
#ifdef GODOT_3_X
	memdelete(da);

	auto iterator = doc.get_class_list_iterator();
	for (auto E = iterator; E; ITER_NEXT(E)) {
		DocData::ClassDoc &c = ITER_GET(E);
#else
	for (KeyValue<String, DocData::ClassDoc> E : doc.class_list) {
		DocData::ClassDoc &c = E.value;
#endif

		if (c.name == "int" || c.name == "float" || c.name == "bool" || c.name == "String" || c.name == "@GDScript" || c.name == "Array" || c.name == "Dictionary")
			continue;

		Error err;
		String save_file = PATH_JOIN(save_path, c.name + ".lua");
		FileAccessRef f = FileAccess::open(save_file, FileAccess::WRITE, &err);

		ERR_CONTINUE_MSG(err != OK, "Can't write doc file: " + save_file + ".");

		f->store_line("--- **" + c.name + "**");
		_write_dtr(f, c.brief_description);
		_write_dtr(f, c.description);
		if (c.tutorials.size()) {
			f->store_line("---");
			f->store_line("--- **Online Tutorials**");
			for (int i = 0; i < c.tutorials.size(); i++) {
				auto tutorial = c.tutorials.get(i);
				auto tutorial_link = DTR(tutorial.link);
				if (IS_EMPTY(tutorial.title)) {
					f->store_line("--- * [" + tutorial_link + "](" + tutorial_link + ")");
				} else {
					f->store_line("--- * [" + DTR(tutorial.title) + "](" + tutorial_link + ")");
				}
			}
		}

		bool is_global = c.name == "@GlobalScope";
		if (!is_global) {
			f->store_line("---");
			if (IS_EMPTY(c.inherits)) {
				f->store_line("---@class " + c.name);
			} else {
				f->store_line("---@class " + c.name + " : " + c.inherits);
			}
			if (c.properties.size()) {
				c.properties.sort();
				f->store_line(c.name + " = {");
				for (int i = 0; i < c.properties.size(); i++) {
					auto p = c.properties[i];
					f->store_8('\n');
					_write_dtr(f, p.description, 1);
					String ptype = p.type;
					String pvalue = p.default_value;
					_get_lua_var(ptype, &pvalue);
					f->store_line("    ---@type " + ptype);
					if (reserved_worlds.find(p.name) || p.name.find("/") >= 0) {
						f->store_line("    [\"" + p.name + "\"] = " + pvalue + ",");
					} else {
						f->store_line("    " + p.name + " = " + pvalue + ",");
					}
					//String pdesc = p.description;
					//_parse_dtr(pdesc);
					//f->store_line("---@field " + p.name + " " + ptype + " @" + pdesc);
				}
				f->store_line("}");
			} else {
				f->store_line(c.name + " = {}");
			}
			if (!IS_EMPTY(c.inherits)) {
				f->store_8('\n');
				f->store_line("---@return " + c.name);
				f->store_line("function " + c.name + ":new() end");
			}
		}

		if (c.constants.size()) {
			f->store_8('\n');
			f->store_line("--- Constants ---");
			String enumeration = String();
			for (int i = 0; i < c.constants.size(); i++) {
				auto k = c.constants[i];
				String cname;
				if (is_global) {
					k.enumeration = "godot";
				} else {
					k.enumeration = "";
					cname = c.name + ".";
				}
				if (k.is_value_valid) {
					_write_constant(f, enumeration, cname, k, nullptr);
				} else {
					_write_constant(f, enumeration, cname, k, "[[platform-dependent]]");
				}
			}
			if (enumeration != String()) {
				f->store_line("}");
			}
		}

		if (is_global && c.properties.size()) {
			c.properties.sort();
			f->store_8('\n');
			f->store_line("--- Properties ---");

			for (int i = 0; i < c.properties.size(); i++) {
				// String additional_attributes;
				// if (c.properties[i].enumeration != String()) {
				// 	additional_attributes += " enum=\"" + c.properties[i].enumeration + "\"";
				// }
				// if (c.properties[i].default_value != String()) {
				// 	additional_attributes += " default=\"" + c.properties[i].default_value.xml_escape(true) + "\"";
				// }

				auto p = c.properties[i];
				f->store_8('\n');
				_write_dtr(f, p.description);
				String ptype = p.type;
				String pname = p.name;
				String pvalue = p.default_value;
				_get_lua_var(ptype, &pvalue);
				f->store_line("---@type " + ptype);
				f->store_line(p.name + " = " + pvalue);
			}
		}

		if (c.signals.size()) {
			c.signals.sort();
			f->store_8('\n');
			f->store_line("--- Signals ---");
			for (int i = 0; i < c.signals.size(); i++) {
				auto m = c.signals[i];
				f->store_8('\n');
				if (IS_EMPTY(m.qualifiers)) {
					f->store_line("--- **signal**  ");
				} else {
					f->store_line("--- **signal " + m.qualifiers + "**  ");
				}
				_write_dtr(f, m.description);
				_write_method(f, c, m, reserved_worlds, false);
			}
		}

		if (c.methods.size()) {
			c.methods.sort();
			f->store_8('\n');
			f->store_line("--- Methods ---");
			for (int i = 0; i < c.methods.size(); i++) {
				auto m = c.methods[i];
				f->store_8('\n');
				if (!IS_EMPTY(m.qualifiers)) {
					f->store_line("--- **" + m.qualifiers + "**  ");
				}
				_write_dtr(f, m.description);
				_write_method(f, c, m, reserved_worlds, is_global);
			}
		}
	}
}
