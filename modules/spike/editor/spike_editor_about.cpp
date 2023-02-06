/**
 * spike_editor_about.cpp
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#include "editor/editor_about.h"
#include "modules/spike_license.gen.h"
#include "scene/gui/box_container.h"
#include "scene/gui/link_button.h"
#include "spike/version.h"
#include "spike_editor_node.h"

#define SPIKE_INTOR STTR("\
Spike Engine is part of Spike ecosystem. It combines multi-chain smart contracts, \
signature machines, Built-in wallets, NFT interoperability specifications \
and other on-chain functions, aiming to build a Web3 native metauniverse engine.\n\n\
The birth of Spike Engine is inseparable from the [url=about godot][color=70bafa]Godot[/color][/url] open source project \
and other [url=open source][color=70bafa]open source software[/color][/url].\
")

extern const char *const VERSION_HASH;
static const String META_TEXT_TO_COPY = "text_to_copy";

#define FILL_ABOUT_TABS(tabs)                                        \
	int __count = about->get_child_count();                          \
	Control *tabs[] = {                                              \
		Object::cast_to<VBoxContainer>(about->get_child(--__count)), \
		Object::cast_to<VBoxContainer>(about->get_child(--__count)), \
		Object::cast_to<VBoxContainer>(about->get_child(--__count)), \
		nullptr                                                      \
	}

void SpikeEditorNode::_init_about_dialog() {
	about->set_title(STTR("About Spike Engine"));

	// Godot About Panel

	VBoxContainer *vbc = Object::cast_to<VBoxContainer>(about->get_child(about->get_child_count() - 1));
	if (vbc) {
		auto lnks = vbc->get_child(0)->find_children("", "LinkButton", true, false);
		if (lnks.size()) {
			String hash = String(VERSION_HASH);
			if (hash.length() != 0) {
				hash = " " + vformat("[%s]", hash.left(9));
			}
			LinkButton* version_lnk = Object::cast_to<LinkButton>(lnks.get(0));
			version_lnk->set_text("Godot Engine v" VERSION_NUMBER "." VERSION_STATUS + hash);
		}

		LinkButton *link_spike = memnew(LinkButton);
		vbc->add_child(link_spike);
		vbc->move_child(link_spike, 0);
		link_spike->set_text(STTR("< Back"));
		link_spike->connect("pressed", callable_mp(this, &SpikeEditorNode::_see_about_content).bind(1));
	}

	// Spike About Panel
	vbc = memnew(VBoxContainer);
	about->add_child(vbc);
	{
		HBoxContainer *hbc = memnew(HBoxContainer);
		vbc->add_child(hbc);
		vbc->move_child(hbc, 0);
		hbc->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		hbc->set_alignment(BoxContainer::ALIGNMENT_CENTER);
		hbc->add_theme_constant_override("separation", 30 * EDSCALE);

		auto spike_logo = memnew(TextureRect);
		spike_logo->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
		hbc->add_child(spike_logo);
		spike_logo->set_texture(about->get_theme_icon(SNAME("SpikeLogo"), SNAME("EditorIcons")));

		VBoxContainer *version_info_vbc = memnew(VBoxContainer);
		hbc->add_child(version_info_vbc);

		Control *v_spacer = memnew(Control);
		version_info_vbc->add_child(v_spacer);

		LinkButton *version_lnk = memnew(LinkButton);
		version_info_vbc->add_child(version_lnk);
		String hash = String(VERSION_SPIKE_HASH);
		if (hash.length() != 0) {
			hash = " " + vformat("[%s]", hash.left(9));
		}
		version_lnk->set_text(String(SPIKE_FULL_NAME) + hash);
		version_lnk->set_meta(META_TEXT_TO_COPY, "v" SPIKE_FULL_BUILD + hash);
		version_lnk->set_underline_mode(LinkButton::UNDERLINE_MODE_ON_HOVER);
		version_lnk->set_tooltip_text(TTR("Click to copy."));
		version_lnk->connect("pressed", callable_mp(this, &SpikeEditorNode::_about_version_lnk_pressed).bind(version_lnk));

		Label *copyright = memnew(Label);
		version_info_vbc->add_child(copyright);
		copyright->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
		copyright->set_text(String::utf8("\xc2\xa9 2022-present ") + STTR("Spike Engine developers") + ".");

		RichTextLabel *spike_intro = memnew(RichTextLabel);
		vbc->add_child(spike_intro);
		spike_intro->set_use_bbcode(true);
		spike_intro->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		spike_intro->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		spike_intro->set_text(SPIKE_INTOR);
		spike_intro->connect("meta_clicked", callable_mp(this, &SpikeEditorNode::_about_meta_clicked));

		// Show Spike Version
		this->version_btn->set_text(SPIKE_FULL_CONFIG);
		this->version_btn->set_meta(META_TEXT_TO_COPY, "v" SPIKE_FULL_BUILD + hash);
	}

	// Spike Licence Penel

	vbc = memnew(VBoxContainer);
	about->add_child(vbc);
	{
		LinkButton *link_spike = memnew(LinkButton);
		vbc->add_child(link_spike);
		link_spike->set_text(STTR("< Back"));
		link_spike->connect("pressed", callable_mp(this, &SpikeEditorNode::_see_about_content).bind(1));

		HSplitContainer *split = memnew(HSplitContainer);
		vbc->add_child(split);
		split->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		split->set_split_offset(200 * EDSCALE);

		ItemList *list = memnew(ItemList);
		split->add_child(list);
		list->connect("item_selected", callable_mp(this, &SpikeEditorNode::_select_license_item));
		const SpikeLicenseInfo *license = SPIKE_LICENSE;
		while (license->name) {
			list->add_item(license->name);
			license++;
		}

		rtl_spike_license = memnew(RichTextLabel);
		split->add_child(rtl_spike_license);
		rtl_spike_license->set_use_bbcode(true);
		rtl_spike_license->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		rtl_spike_license->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		rtl_spike_license->connect("meta_clicked", callable_mp(this, &SpikeEditorNode::_about_meta_clicked));
	}

	about->connect("about_to_popup", callable_mp(this, &SpikeEditorNode::_about_will_popup));
}

void SpikeEditorNode::_see_about_content(int p_index) {
	FILL_ABOUT_TABS(tabs);
	Control **ptr = tabs;
	for (int i = 0; *ptr; ptr++, i++) {
		(*ptr)->set_visible(p_index == i);
	}
}

void SpikeEditorNode::_about_will_popup() {
	FILL_ABOUT_TABS(tabs);
	Control **ptr = tabs;
	for (int i = 0; *ptr; ptr++, i++) {
		if (i == 1) {
			(*ptr)->set_visible(true);
		} else {
			(*ptr)->call_deferred("set_visible", false);
		}
	}
}

void SpikeEditorNode::_about_meta_clicked(Variant p_meta) {
	if (p_meta.get_type() == Variant::STRING) {
		String meta = p_meta;
		if (meta == "about godot") {
			_see_about_content(2);
		} else if (meta == "open source") {
			_see_about_content(0);
		} else if (meta.begins_with("http")) {
			OS::get_singleton()->shell_open(meta);
		}
	}
}

void SpikeEditorNode::_about_version_lnk_pressed(const LinkButton *p_btn) {
	DisplayServer::get_singleton()->clipboard_set(p_btn->get_meta(META_TEXT_TO_COPY));
}

void SpikeEditorNode::_select_license_item(int p_index) {
	const SpikeLicenseInfo *info = SPIKE_LICENSE + p_index;
	rtl_spike_license->set_text(vformat("[url=%s]%s[/url]\n\n%s", info->homepage, info->homepage, info->license));
}
