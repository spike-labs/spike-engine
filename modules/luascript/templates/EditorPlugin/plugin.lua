--#Basic plugin template
---@class _CLASS_ : EditorPlugin
local _CLASS_ = class(EditorPlugin)
tool(_CLASS_)

function _CLASS_:_enter_tree()
	-- Initialization of the plugin goes here.
end

function _CLASS_:_exit_tree()
	-- Clean-up of the plugin goes here.
end

return _CLASS_
