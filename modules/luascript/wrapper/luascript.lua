
local LuaScript = {}

local rawget, rawset, getmetatable = rawget, rawset, getmetatable

local function _find_meta_field(mt, k)
    while mt and not rawget(mt, "__native") do
        local v = rawget(mt, k)
        if v ~= nil then return v end
        mt = getmetatable(mt)
    end
end

local function _collect_properties(proplist, list)
    if type(proplist) ~= "table" then return end

    for i, p in ipairs(proplist) do
        local vtype = p.type
        if vtype == nil then
            vtype = p[1] ~= nil and typeof(p[1]) or CONST.TYPE_OBJECT
        end
        table.insert(list, {
            name = p.name, type = vtype, usage = p.usage or CONST.PROPERTY_USAGE_DEFAULT,
            hint = p.hint or 0, hint_string = p.hint_string,
        })
    end
end

local function _get_custom_properties(script, self)
    local prop_cache = rawget(script, ".prop_cache")
    if prop_cache == nil then
        local fun_get = _find_meta_field(script, "_get_property_list")
        if fun_get then
            prop_cache = fun_get(self or script)
        else
            prop_cache = false
        end
        rawset(script, ".prop_cache", prop_cache)
    end
    return prop_cache
end

local function collect_script_property(script, self, list)
    _collect_properties(rawget(script, ".prop_list"), list)
    _collect_properties(_get_custom_properties(script, self), list)
end

local function collect_script_method(self, list)
    local funclist = rawget(self, ".func_list")
    if funclist == nil then return end

    for i, f in ipairs(funclist) do
        table.insert(list, f)
    end
end

local function _find_property(proplist, name)
    if type(proplist) ~= "table" then return end

    for i, p in ipairs(proplist) do
        if p.name == name then
            return p
        end
    end
end

local function find_prop_data(script, self, name)
    local p = _find_property(rawget(script, ".prop_list"), name)
    if p == nil then
        p = _find_property(_get_custom_properties(script, self), name)
    end
    return p
end

function LuaScript.get_script_property_list(script, self)
    local list = {}
    if script then
        collect_script_property(script, self, list)
    end
    return list
end

function LuaScript.get_script_method_list(script)
    local list = {}
    if script then
        collect_script_method(script, list)
    end
    return list
end

-- ============================================================================

local function get_prop_info(mt, self, k)
    while mt and not rawget(mt, "__native") do
        local info = find_prop_data(mt, self, k)
        if info then return info end
        mt = getmetatable(mt)
    end
end

function LuaScript.set_instance_property(t, k, v)
    if t then
        local mt = getmetatable(t)
        local info = get_prop_info(mt, t, k)
        if info then
            local props = rawget(t, ".props")
            if props == nil then
                props = {}
                rawset(t, ".props", props)
            end

            local stype = type(info.set)
            if stype == "function" then
                info.set(t, v)
            elseif stype == "string" then
                local setfun = _find_meta_field(t, info.set)
                if setfun then setfun(t, v) end
            end
            props[k] = v
            return true
        end

        local _set = _find_meta_field(mt, "_set")
        if _set and _set(t, k, v) then
            return true
        end
    end
end

--- Find member of script (P: function > signal > property)
---
--- 1. Look up the metatable fields, starting with itself and ending with the native class. (Sometimes starting from itself is a repetition).
--- Only functions and signals are recorded directly in the metatable, as both are **read-only** members.
---
--- 2. Search from properties's definition。
---@param flags number @(1=property, 2=function, 3=property&function)
---@return any, boolean
local function _internal_get_member(script, instance, k, flags)
    local mt = instance or script
    local member = _find_meta_field(mt, k)
    if member ~= nil then return member end

    if (flags & 1) ~= 0 then
        local prop = get_prop_info(script, instance, k)
        if prop then
            if prop[1] == nil and prop.type then
                prop[1] = new_variant(prop.type)
            end
        end
        return prop, true
    end
end

function LuaScript.get_script_member(script, field, flags)
    local member, is_prop = _internal_get_member(script, nil, field, flags)
    local exists, value = member ~= nil, member
    if exists and is_prop then
        value = member[1]
    end
    return exists, value
end

function LuaScript.get_instance_member(instance, field, flags)
    local script = getmetatable(instance)
    local member, is_prop = _internal_get_member(script, instance, field, flags)
    local exists, value = member ~= nil, member
    if exists then
        if is_prop then
            local gtype = type(member.get)
            if gtype == "function" then
                member.get(instance)
            elseif gtype == "string" then
                local getfun = _find_meta_field(instance, member.get)
                if getfun then getfun(instance) end
            end
            local props = rawget(instance, ".props")
            if props == nil then
                props = {}
                rawset(instance, ".props", props)
            end

            value = props[field]
            if value == nil then
                value = member[1]
                if type(value) == "table" then
                    value = {}
                    for k, v in pairs(member[1]) do value[k] = v end
                    props[field] = setmetatable(value, getmetatable(member[1]))
                end
            end
        else
            if type(member) == "table" then
                local mt = getmetatable(member)
                if mt and rawget(mt, "__name") == "Signal" then
                    --- Make a temporary signal object
                    value = setmetatable({ ud = instance, name = field, }, mt)
                end
            end
        end
    elseif (flags & 1) ~= 0 then
        local _get = _find_meta_field(script, "_get")
        if _get then
            local v = _get(instance, field)
            exists, value = v ~= nil, v
        end
    end

    return exists, value
end

local function rep_udata(o, t)
    local keys = {}
    for k, _ in pairs(o) do
        if k ~= "__udata" then
            keys[k] = true
        end
    end

    for k, v in pairs(t) do
        if k ~= "__udata" then
            if type(v) == "table" then
                local name_path = rawget(v, "__name")
                if name_path and name_path:find("::") then
                    local ov = rawget(o, k)
                    if ov then
                        rep_udata(ov, v)
                        keys[k] = nil
                    end
                end
            end
            if keys[k] then
                rawset(o, k, v)
                keys[k] = nil
            end
        end
    end

    for k, _ in pairs(keys) do
        rawset(o, k, nil)
    end
end

-- Sync all fileds of `t` to `o`, recursively
function __reassign_script(o, t)
    if o then
        rep_udata(o, t)
    end
end


return LuaScript
