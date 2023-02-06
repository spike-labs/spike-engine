
local Rect2, get = { __type = CONST.TYPE_RECT2, __name = 'Rect2' }, {}

Rect2.__index = function (t, k)
    local var = rawget(get, k)
    if var then return var(t) end

    return rawget(Rect2, k)
end
Rect2.__newindex = function (t, n, k) end

Rect2.__call = function(t, x, y, w, h)
    local pos, siz
    if type(x) == "table" and type(y) == "table" and x.__name == "Vector2" and y.__name == "Vector2" then
        pos, siz = Vector2(x.x, x.y), Vector2(y.x, y.y)
    else
        pos = Vector2(x, y)
        siz = Vector2(w, h)
    end
    return setmetatable({position = pos, size = siz}, Rect2)
end

Rect2.new = Rect2.__call

function Rect2:abs()
    local position, size = self.position, self.size
    return Rect2(Vector2(position.x + math.min(size.x, 0), position.y + math.min(size.y, 0)), size:abs());
end

function Rect2:clip(b)
    local rt = Rect2:new(b.position, b.size)

    if not self:intersects(rt) then
        return Rect2()
    end

    rt.position.x = math.max(b.position.x, b.x)
    rt.position.y = math.max(b.position.y, b.y)

    local b_end = b.position + b.size
    local s_end = self.position + self.size

    rt.size.x = math.min(b_end.x, s_end.x) - rt.position.x
    rt.size.y = math.min(b_end.y, s_end.y) - rt.position.y

    return rt;
end

function Rect2:encloses(b)
    local position, size = self.position, self.size
    return (b.position.x >= position.x) and (b.position.y >= position.y)
        and ((b.position.x + b.size.x) <= (position.x + size.x))
        and ((b.position.y + b.size.y) <= (position.y + size.y))
end

function Rect2:expand(to)
    local rt = Rect2:new(self.position, self.size)
    local b = rt.position;
    local e = rt.position + rt.size;

    if to.x < b.x then b.x = to.x end
    if to.y < b.y then b.y = to.y end
    if to.x > e.x then e.x = to.x end
    if to.y > e.y then e.y = to.y end

    rt.position = b
    rt.size = e - b
    return rt
end

function Rect2:get_area()
    return self.size.x * self.size.y
end

function Rect2:get_center()
    return self.position + (self.size * 0.5)
end

function Rect2:grow(by)
    local rt = Rect2:new(self.position, self.size)
    rt.position.x = rt.position.x - by;
    rt.position.y = rt.position.y - by;
	rt.size.x = rt.size.x + by * 2;
	rt.size.y = rt.size.y + by * 2;
    return rt
end

function Rect2:grow_individual(left, top, right, bottom)
    local rt = Rect2:new(self.position, self.size)
    rt.position.x = rt.position.x - left;
    rt.position.y = rt.position.y - top;
    rt.size.x = rt.size.x + right;
    rt.size.y = rt.size.y + bottom;

    return rt;
end

function Rect2:grow_margin(margin, by)
    if not by then by = 0 end
	local g = self:grow_individual(
        (0 == margin) and by or 0,
        (1 == margin) and by or 0,
        (2 == margin) and by or 0,
        (3 == margin) and by or 0)
	return g;
end

function Rect2:has_no_area()
    return self.size.x <= 0 or self.size.y <= 0
end

function Rect2:has_point(point)
    local position, size = self.position, self.size
    return point.x >= position.x and point.y >= position.y
        and point.x < (position.x + size.x)
        and point.y < (position.y + size.y)
end

function Rect2:intersects(b, include_borders)
    local position, size = self.position, self.size
    if include_borders then
        if position.x > (b.position.x + b.size.x) then return false end
        if (position.x + size.x) < b.position.x then return false end
        if position.y > (b.position.y + b.size.y) then return false end
        if (position.y + size.y) < b.position.y then return false end
    else
        if position.x >= (b.position.x + b.size.x) then return false end
        if (position.x + size.x) <= b.position.x then return false end
        if position.y >= (b.position.y + b.size.y) then return false end
        if (position.y + size.y) <= b.position.y then return false end
    end

    return true;
end

function Rect2:is_equal_approx(rect)
    if rect.position and rect.size then
        return self.position:is_equal_approx(rect.position) and self.size:is_equal_approx(rect.size)
    end
    return false
end

function Rect2:merge(b)
    local position, size = self.position, self.size
    local rt = Rect2:new()

    rt.position.x = math.min(b.position.x, position.x)
    rt.position.y = math.min(b.position.y, position.y)

    rt.size.x = math.max(b.position.x + b.size.x, position.x + size.x)
    rt.size.y = math.max(b.position.y + b.size.y, position.y + size.y)

    rt.size = rt.size - rt.position

    return rt
end

Rect2.__tostring = function(self)
    if self.position and self.size then
        return string.format('(%s,%s)', self.position, self.size)
    else
        return "{Rect2}"
    end
end

get["end"] = function (self)
    return self.position + self.size
end

setmetatable(Rect2, Rect2)
return Rect2
