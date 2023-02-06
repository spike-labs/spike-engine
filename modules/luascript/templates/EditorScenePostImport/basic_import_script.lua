--#Basic import script template
---@class _CLASS_ : EditorScenePostImport
local _CLASS_ = class(EditorScenePostImport)
tool(_CLASS_)

---Called by the editor when a scene has this script set as the import script in the import tab.
function _CLASS_:_post_import(scene)
	-- Modify the contents of the scene upon import.
	return scene -- Return the modified root node when you're done.
end

return _CLASS_
