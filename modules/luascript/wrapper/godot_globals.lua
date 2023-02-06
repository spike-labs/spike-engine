
local getmetatable, rawget = getmetatable, rawget
local CONST = CONST

function load_res(path)
    return ResourceLoader:load(path)
end

function async(f, ...)
    local co = coroutine.create(f)
    local flag, msg = coroutine.resume(co, ...)
    if flag == false then
        printerr(msg)
    end
    return co
end

function tool(t)
    if t then t.__tool = true end
end

local _class_mt = {}
local _signal_mt = { __name = "Signal" }
local _prop_mt = {}

_signal_mt.__index = _signal_mt
function _signal_mt.__call(self, ud, ...)
    ud:emit_signal(self.name, ...)
end

function _signal_mt:connect(...)
    return self.ud:connect(self.name, ...)
end

function _signal_mt:disconnect(...)
    return self.ud:disconnect(self.name, ...)
end

function _signal_mt:is_connected(...)
    return self.ud:is_connected(self.name, ...)
end

function signal(...)
    return setmetatable({...}, _signal_mt)
end

---@param t number
---@param n string
---@return number
local function _get_export_type(t, n)
    if t == nil then
        if ClassDB:class_exists(n) then
            if ClassDB:is_parent_class(n, "Node") then
                t = CONST.TYPE_NODE_PATH
            elseif ClassDB:is_parent_class(n, "Resource") then
                t = CONST.TYPE_OBJECT
            end
        end
    end
    return t
end

local function _get_def_value(type)
    return new_variant(type)
end

local function _gdtype_to_prop(__type, __name)
    local t = _get_export_type(__type, __name)
    if t then
        local prop = { _get_def_value(t), type = t }
        if t == CONST.TYPE_OBJECT then
            prop.hint = CONST.PROPERTY_HINT_RESOURCE_TYPE
            prop.hint_string = __name
        end
        return prop
    end
end

local function _get_array_hint_string(t)
    if type(t) == "table" and getmetatable(t) == nil then
        local k, v = next(t)
        if k == 1 then
            return tostring(CONST.TYPE_ARRAY) .. ":" .. _get_array_hint_string(v)
        else
            return tostring(CONST.TYPE_DICTIONARY)
        end
    end
    return tostring(typeof(t)) .. ":"
end

local function define(t)
    local v, hint, hint_string = t[1], t[2], t[3]
    if hint and t.hint == nil then t.hint, t[2] = hint, nil end
    if hint_string and t.hint_string == nil then t.hint_string, t[3] = hint_string, nil end
    return t
end

function property(t, ...)
    local tb
    local ltype = type(t)
    if ltype == "table" then
        local __type, __name = rawget(t, "__type"), rawget(t, "__name")
        if __name then
            ---= `t`是类型定义
            tb = _gdtype_to_prop(__type, __name)
        else
            local mt = getmetatable(t)
            if mt == nil then
                if t._ then
                    t._ = nil
                    tb = define(t)
                else
                    local k, v = next(t)
                    tb = { t, type = k == 1 and CONST.TYPE_ARRAY or CONST.TYPE_DICTIONARY, }
                    tb.hint = CONST.PROPERTY_HINT_TYPE_STRING
                    tb.hint_string = _get_array_hint_string(v)
                end
            else
                __type, __name = rawget(mt, "__type"), rawget(mt, "__name")
                if __name then
                    tb = _gdtype_to_prop(mt.__type, mt.__name)
                    if tb then
                        tb[1] = t
                    end
                end
            end
        end
    end

    if tb == nil then
        tb = { t }
    end

    if tb.type == CONST.TYPE_ARRAY then
        tb.hint = CONST.PROPERTY_HINT_TYPE_STRING
        local Args = {...}
        if #Args > 0 then
            tb.hint_string = ""
            local elem, hint, hint_string
            for i, v in ipairs(Args) do
                if rawequal(v, Array) then
                    tb.hint_string = tb.hint_string .. ":"
                else
                    elem = v
                    hint, hint_string = Args[i + 1], Args[i + 2]
                    break
                end
            end
            if elem then
                local __type, __name = rawget(elem, "__type"), rawget(elem, "__name")
                if __name then
                    __type = _get_export_type(__type, __name)
                    if __type then
                        tb.hint_string = tb.hint_string .. __type
                        if __type == CONST.TYPE_OBJECT then
                            hint = CONST.PROPERTY_HINT_RESOURCE_TYPE
                            hint_string = __name or "Resource"
                        end
                        if hint then
                            tb.hint_string = tb.hint_string .. "/" .. hint
                        end
                        tb.hint_string = tb.hint_string .. ":" .. (hint_string or "")
                    end
                end
            end
        end
    elseif tb.hint == nil then
        tb.hint, tb.hint_string = select(1, ...)
    end
    if tb.uasge == nil then tb.usage = 0 end

    return setmetatable(tb, _prop_mt)
end

function export(t, ...)
    t = property(t, ...)
    t.usage = (t.uasge or 0) | CONST.PROPERTY_USAGE_DEFAULT | CONST.PROPERTY_USAGE_SCRIPT_VARIABLE
    return t
end

local function track_member_define_order(t, k, v)
    local vtype, value = type(v), v
    if vtype == "function" then
        --- { name = <function name>, arg1, arg2, ... }
        local func, i = { name = k }, 1
        while true do
            local n = debug.getlocal(v, i)
            if n == nil then break end
            func[#func + 1] = { n, 0 }
            i = i + 1
        end
        table.insert(t[".func_list"], func)
        rawset(t, k, value)
    else
        local mt
        if vtype == "table" then mt = getmetatable(v) end

        --- {
        ---     value, name = <string>, type = <type>, usage = <usage>, hint = <int>, hint_string = <string>, rest_mode = <RPCMode>
        ---     get = <function|string>,
        ---     set = <function|string>,
        --- }
        if rawequal(mt, _prop_mt) then
            value = v[1]
            v.name = k
            table.insert(t[".prop_list"], v)
        elseif rawequal(mt, _signal_mt) then
            --- name = { arg1, arg2, ... }
            value = v --function (self, ...) self:emit_signal(k, ...) end
            v.name = k
            t.__signals[k] = v
            rawset(t, k, value)
        elseif mt == _class_mt then
            rawset(t, k, value)
        else
            table.insert(t[".prop_list"], { v, name = k, usage = 0 })
        end
    end

    --rawset(t, k, value)
end

_class_mt.__newindex = track_member_define_order

function class(extends, class_name, class_icon)
    local mt
    if type(extends) == "table" then
        mt = extends
        extends = rawget(extends, "__name") or extends.resource_path
    end

    local define = {
        [".func_list"] = {}, [".prop_list"] = {},
        __class = class_name,
        __icon = class_icon,
        __extends = extends,
        __signals = {},
        __tool = false,

        ["!inherits_gdo"] = true,
        ["!metatable"] = mt,
    }

    return setmetatable(define, _class_mt)
end
