
local GLO = class("Object")
local __gd_obj
---@type table<number, ActionListDEF>
local _co_map = {}
local _co_count = 0

local _actionlist_maps = {}
---@class ActionListDEF
local ActionListDEF = {}
ActionListDEF.__index = ActionListDEF

---@param gd Object
function ActionListDEF.get(gd, signal)
    local id = gd and gd:get_instance_id() or 0
    local map = _actionlist_maps[id]
    if map == nil then
        map = {}
        _actionlist_maps[id] = map
    end

    local self = map[signal]
    if self == nil then
        self = setmetatable({ id = id, o = gd, s = signal, }, ActionListDEF)
        map[signal] = self
    end
    return self
end

local function _remove_action(self, index)
    table.remove(self, index)
    if self:count() == 0 then
        local map = _actionlist_maps[self.id]
        map[self.s] = nil
        GLO.disconnect(self.o, self.s, __gd_obj, "_godot_lua_callback")
        local _, list = next(map)
        if list == nil then _actionlist_maps[self.id] = nil end
    end
end

function ActionListDEF:__call(...)
    self.pending = {}
    for _, action in ipairs(self) do
        local args = action.obj and { action.obj, ... } or { ... }
        if action.binds then
            table.move(action.binds, 1, #action.binds, #args + 1, args)
        end
        local status, err
        if type(action.fun) == "function" then
            status, err = pcall(action.fun, table.unpack(args))
        else
            status, err = coroutine.resume(action.fun, table.unpack(args))
        end
        if not status then printerr(err) end
    end

    for i = #self, 1, -1 do
        local action = self[i]
        if (action.flag & Object.CONNECT_ONE_SHOT) ~= 0 then
            _remove_action(self, i)
        end
    end

    for _, v in ipairs(self.pending) do
        table.insert(self, v)
    end
    self.pending = nil
end

function ActionListDEF:count()
    return #self + (self.pending and #self.pending or 0)
end

function ActionListDEF:add_action(obj, fun, binds, flag)
    local action = { obj = obj, fun = fun, binds = binds, flag = flag or 0 }
    table.insert(self.pending or self,  action)
    return action
end

function ActionListDEF:find_index(arg)
    local atype, index = type(arg)
    if atype == "table" then
        for i,v in ipairs(self) do if v == arg then index = i break end end
    elseif atype == "number" then
        index = arg
    else
        for i,v in ipairs(self) do if v.fun == arg then index = i break end end
    end
    return index
end

function ActionListDEF:remove_action(arg)
    local index = self:find_index(arg)
    if index then
        if self.pending == nil then
            _remove_action(self, index)
        else
            local action = self[index]
            action.flag = action.flag | (Object.CONNECT_ONE_SHOT or Object.CONNECT_ONESHOT)
        end
    end
end

---@param gd Object
---@param signal string
---@param obj Script
---@param method string|function|thread
---@param binds any[]
---@param flag number
local function connect_action(gd, signal, obj, method, binds, flag)
    local list = ActionListDEF.get(gd, signal)
    local action = list:add_action(obj, method, binds, flag)
    if list:count() == 1 then
        _co_count = _co_count + 1
        _co_map[_co_count] = list
        GLO.connect(gd, signal, __gd_obj, "_godot_lua_callback", { _co_count }, 0)
    end
    return function() list:remove_action(action) end
end

local function execute_action(...)
    local args = {...}
    table.remove(args)

    local key = select(-1, ...)
    local co = _co_map[key]
    local mtype = type(co)
    if mtype == "table" then
        co(table.unpack(args))
    else
        printerr("action `" .. key .. "` is", co, ...)
    end
end
function GLO:_init()
    __gd_obj = self
end

function GLO:_godot_lua_callback(...)
    local status, err = pcall(execute_action, ...)
    if not status then printerr(err) end
end

function await(gd, signal)
    if not coroutine.isyieldable() then
        error("`await` must be call inside a yieldable coroutine.")
    return end

    if type(signal) ~= "string" then
        signal = "completed"
    end

    local otype = type(gd)
    if otype == "thread" then
        return coroutine.yield(signal)
    end

    connect_action(gd, signal, nil, (coroutine.running()), nil, Object.CONNECT_ONE_SHOT or Object.CONNECT_ONESHOT)
    return coroutine.yield(signal)
end

function __godot_object_connect(...)
    local gd, signal, obj, method, binds, flag = select(1, ...)
    local mtype = type(method)
    if mtype == "function" then
        return connect_action(gd, signal, obj, method, binds, flag)
    else
        --return godot_call("connect", ...)
        return GLO.connect(...)
    end
end

function __godot_object_disconnect(...)
    local gd, signal, obj, method = select(1, ...)
    local mtype = type(method)
    if mtype == "function" then
        local list = ActionListDEF.get(gd, signal)
        if list then
            list:remove_action(method)
        end
    else
        --return godot_call("disconnect", ...)
        return GLO.disconnect(...)
    end
end

function __godot_object_is_connected(...)
    local gd, signal, obj, method = select(1, ...)
    local mtype = type(method)
    if mtype == "function" then
        local list = ActionListDEF.get(gd, signal)
        return (list and list:find_index(method)) ~= nil
    else
        --return godot_call("is_connected", ...)
        return GLO.is_connected(...)
    end
end

return GLO
