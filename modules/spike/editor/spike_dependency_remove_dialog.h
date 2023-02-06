/**
 * spike_dependency_remove_dialog.h
 *
 * This file is part of Spike engine, a modification and extension of Godot.
 *
 */
#ifndef SPIKE_DEPENDENCY_EDITOR_H
#define SPIKE_DEPENDENCY_EDITOR_H

#include "editor/dependency_editor.h"

class SpikeDependencyRemoveDialog : public DependencyRemoveDialog {
	GDCLASS(SpikeDependencyRemoveDialog, DependencyRemoveDialog);

private:
protected:
	static void _bind_methods();
	void ok_pressed() override;

public:
	SpikeDependencyRemoveDialog() {}

	void remove_file(const String &file);
};

#endif