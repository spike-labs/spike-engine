/**
 * modular_graph.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#pragma once

#include "core/io/resource.h"
#include "modular_data.h"
#include "modular_system/register_types.h"
#include "spike_define.h"

#define MODULAR_GRAPH_VERSION 1
#define LAUNCH_MODULAR_PRE "modular="

#define GET_MOD_INPUTS(ins, node)                           \
	PackedStringArray ins;                                  \
	do {                                                    \
		for (int i = 0; i < node->get_input_count(); i++) { \
			inputs.push_back(node->get_input_name(i));      \
		}                                                   \
	} while (0)

#define GET_MOD_OUTPUTS(outs, node)                          \
	PackedStringArray outs;                                  \
	do {                                                     \
		for (int i = 0; i < node->get_output_count(); i++) { \
			outs.push_back(node->get_output_name(i));        \
		}                                                    \
	} while (0)

typedef String ModularNodeID;

extern String modular_get_relative_path(const String &p_source_path);

class ModularNode : public Resource {
	GDCLASS(ModularNode, Resource);
	friend class ModularGraph;

public:
	enum NodeType {
		MODULAR_OUTPUT_NODE,
		MODULAR_COMPOSITE_NODE,
	};

private:
	ModularNodeID id;
	Ref<RefCounted> res;

protected:
	virtual bool _set(const StringName &p_name, const Variant &p_value);
	virtual bool _get(const StringName &p_name, Variant &r_ret) const;
	virtual void _get_property_list(List<PropertyInfo> *p_list) const;

	static void _bind_methods();

public:
	ModularNodeID get_id() const { return id; }
	Ref<ModularData> get_res() const { return res; }
	virtual int get_type() const = 0;

	virtual int get_output_index(const String &p_name) const { return -1; }
	virtual int get_output_count() const { return 0; }
	virtual const String get_output_name(int p_index) const { return String(); }
	virtual int add_output(const String &p_name) { return -1; }
	virtual void remove_output(const String &p_name) {}

	virtual int get_input_index(const String &p_name) const { return -1; }
	virtual int get_input_count() const { return 0; }
	virtual const String get_input_name(int p_index) const { return String(); }
	virtual int add_input(const String &p_name) { return -1; }
	virtual void remove_input(const String &p_name) {}

	ModularNode(const String &p_id = String(), const Ref<Resource> &p_res = Ref<Resource>()) :
			id(p_id), res(p_res) {}
};

class ModularOutputNode : public ModularNode {
	GDCLASS(ModularOutputNode, ModularNode);

protected:
	String output;

	virtual bool _set(const StringName &p_name, const Variant &p_value) override;
	virtual bool _get(const StringName &p_name, Variant &r_ret) const override;
	virtual void _get_property_list(List<PropertyInfo> *p_list) const override;

public:
	virtual int get_type() const override { return MODULAR_OUTPUT_NODE; }
	virtual int get_output_index(const String &p_name) const override;
	virtual int get_output_count() const override;
	virtual const String get_output_name(int p_index) const override;
	virtual int add_output(const String &p_name) override;
	virtual void remove_output(const String &p_name) override;

	ModularOutputNode(const String &p_id = String(), const Ref<Resource> &p_res = Ref<Resource>()) :
			ModularNode(p_id, p_res){};
};

class ModularCompositeNode : public ModularNode {
	GDCLASS(ModularCompositeNode, ModularNode);

protected:
	Vector<String> in_list;
	Vector<String> out_list;
	String out;
	bool _set(const StringName &p_name, const Variant &p_value) override;
	bool _get(const StringName &p_name, Variant &r_ret) const override;
	void _get_property_list(List<PropertyInfo> *p_list) const override;

public:
	virtual int get_type() const override { return MODULAR_COMPOSITE_NODE; }
	virtual int get_input_index(const String &p_name) const override;
	virtual int get_input_count() const override;
	virtual const String get_input_name(int p_index) const override;
	virtual int add_input(const String &p_name) override;
	virtual void remove_input(const String &p_name) override;

	virtual int get_output_index(const String &p_name) const override;
	virtual int get_output_count() const override;
	virtual const String get_output_name(int p_index) const override;
	virtual int add_output(const String &p_name) override;
	virtual void remove_output(const String &p_name) override;

	ModularCompositeNode(const String &p_id = String(), const Ref<Resource> &p_res = Ref<Resource>()) :
			ModularNode(p_id, p_res){};
};

class ModularGraph : public ModularData {
	GDCLASS(ModularGraph, ModularData);

public:
	struct Connection {
		ModularNodeID from;
		ModularNodeID to;
		String port;
	};

	HashSet<Ref<ModularData>, ModularDataHasher, ModularDataComparator> exported_datas;

protected:
	Vector<Ref<ModularNode>> node_list = Vector<Ref<ModularNode>>();
	Vector<Connection> conn_list;

	PackedVector2Array positions;

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	ModularGraph::Connection _add_connection(ModularNodeID p_from, ModularNodeID p_to, const String &p_name);
	String _export_modular_node(const Ref<ModularNode> &p_node, const String &p_save_path);
	Error _export_composite_node(const Ref<ModularCompositeNode> &p_node, const String &p_save_path, bool p_neasted);

	void _import_button_pressed(Node *p_parent);
	void _import_modular_json(const String &p_path, Node *p_parent);
	void _modular_loaded_to_node(Ref<ModularGraph> p_modular, Node *p_parent);

	void _merge_module_info(Dictionary &p_dic, Ref<ModularNode> p_node);

	Dictionary _get_connection(int p_index);
	void _add_connection_bind(ModularNodeID p_from, ModularNodeID p_to, const String p_name);

	static void _bind_methods();

public:
	Ref<ModularNode> find_node(ModularNodeID p_id) const;
	Ref<ModularNode> find_main_node() const;
	int get_node_count() const { return node_list.size(); }
	Ref<ModularNode> get_node(int p_index) const;
	void add_node(const Ref<ModularNode> &p_node);
	void remove_node(ModularNodeID p_id);
	void change_node_id(Ref<ModularNode> p_node, ModularNodeID p_id);

	int get_connection_count() const { return conn_list.size(); }
	ModularGraph::Connection get_connection(int p_index) const;
	ModularGraph::Connection add_connection(ModularNodeID p_from, ModularNodeID p_to, const String &p_name);
	void remove_connection(ModularNodeID p_from, ModularNodeID p_to);

	void set_node_pos(ModularNodeID p_id, Vector2 p_pos);
	Vector2 find_node_pos(ModularNodeID p_id);
	Vector2 get_node_pos(int p_index);
	void move_all_nodes(const Vector2 &p_offset);

	Error run_modular(const Ref<ModularNode> &p_node);
	Error export_modular(const Ref<ModularNode> &p_node, const String &p_save_path);

	virtual void validation() override;
	virtual String get_module_name() const override;
	virtual void edit_data(Object *p_context) override;
	virtual String export_data() override;
	virtual Dictionary get_module_info() override;
	virtual void inspect_modular(Node *p_parent) override;

	void reimport_from_uri(const String &p_uri, const Callable &p_callback);

#ifdef TOOLS_ENABLED
	Dictionary generate_running_conf(Ref<ModularNode> p_node);
	static void apply_running_conf(const Dictionary &p_conf, const String &p_main_node);
	void init_packed_file_providers();

private:
	bool inited = false;
#endif
};

class ModularMenuNode : public Node {
	GDCLASS(ModularMenuNode, Node);

public:
	virtual bool is_available(Node *p_node) { return true; };
	virtual String get_menu_name() = 0;
};