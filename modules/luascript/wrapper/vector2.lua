
local CMP_EPSILON = 0.00001
local Vector2, get = { __type = CONST.TYPE_VECTOR2, __name = 'Vector2' }, {}
local function SGN(n) return n < 0 and 1 or -1 end

Vector2.__index = function (t, k)
    local var = rawget(get, k)
    if var then return var(t) end

    return rawget(Vector2, k)
end
Vector2.__newindex = function (t, n, k) end

Vector2.__call = function(t, x, y)
    local t = type(x)
    if t == "table" or t == "userdata" then
        x, y = x.x, x.y
    end
    return setmetatable({x = x or 0, y = y or 0}, Vector2)
end

Vector2.new = Vector2.__call

---
--- Enumerated value for the X axis.
Vector2.AXIS_X = 0

---
--- Enumerated value for the Y axis.
Vector2.AXIS_Y = 1

---
--- Zero vector, a vector with all components set to `0`.
get.ZERO = function (t) return Vector2:new( 0, 0 ) end

---
--- One vector, a vector with all components set to `1`.
get.ONE =  function (t) return Vector2:new( 1, 1 ) end

---
--- Infinity vector, a vector with all components set to [constant @GDScript.INF].
-- Vector2.INF = Vector2:new( inf, inf )

---
--- Left unit vector. Represents the direction of left.
get.LEFT = function (t) return  Vector2:new( -1, 0 ) end

---
--- Right unit vector. Represents the direction of right.
get.RIGHT = function (t) return  Vector2:new( 1, 0 ) end

---
--- Up unit vector. Y is down in 2D, so this vector points -Y.
get.UP = function (t) return  Vector2:new( 0, -1 ) end

---
--- Down unit vector. Y is down in 2D, so this vector points +Y.
get.DOWN = function (t) return  Vector2:new( 0, 1 ) end

function Vector2:abs()
    return Vector2:new(math.abs(self.x), math.abs(self.y))
end

function Vector2:angle()
    return math.atan(self.y, self.x)
end

function Vector2:length()
    return math.sqrt(self.x * self.x + self.y * self.y)
end

function Vector2:length_squared()
    return self.x * self.x + self.y * self.y
end

function Vector2:normalize()
    local l = self.x * self.x + self.y * self.y
    if l ~= 0 then
        l = math.sqrt(l);
        self.x = self.x / l
        self.y = self.y / l
    end
end

function Vector2:normalized()
    local v = Vector2:new(self.x, self.y)
    v:normalize()
    return v
end

function Vector2:is_normalized()
    return math.abs(self:length_squared() - 1) < 0.001
end

function Vector2:distance_to(t)
    return math.sqrt(self:distance_squared_to(t))
end

function Vector2:distance_squared_to(t)
    return (self.x - t.x) * (self.x - t.x) + (self.y - t.y) * (self.y - t.y)
end

function Vector2:angle_to(t)
    return math.atan(self:cross(t), self:dot(t))
end

function Vector2:angle_to_point(t)
    return math.atan(self.y - t.y, self.x - t.x)
end

function Vector2:dot(t)
    return self.x * t.x + self.y * t.y
end

function Vector2:cross(t)
    return self.x * t.y - self.y * t.x
end

function Vector2:sign()
    return Vector2:new(SGN(self.x), SGN(self.y))
end

function Vector2:floor()
    return Vector2:new(math.floor(self.x), math.floor(self.y))
end

function Vector2:ceil()
    return Vector2:new(math.ceil(self.x), math.ceil(self.y))
end

function Vector2:round()
    return Vector2:new(math.floor(self.x + 0.5), math.floor(self.y + 0.5))
end

function Vector2:set_rotation(radians)
    self.x = math.cos(radians)
    self.y = math.sin(radians)
end

function Vector2:rotated(radians)
    local v = Vector2:new()
    v:set_rotation(self:angle() + radians)
    v = v * self:length()
    return v;
end

function Vector2:posmod(mod)
    -- TODO
    return Vector2:new(self.x, self.y) --Vector2(Math::fposmod(x, p_mod), Math::fposmod(y, p_mod));
end

function Vector2:posmodv(mod)
    -- TODO
    return Vector2:new(self.x, self.y) -- Vector2(Math::fposmod(x, p_modv.x), Math::fposmod(y, p_modv.y));
end

function Vector2:project(t)
    return t * (self:dot(t) / t:length_squared())
end

function Vector2:snapped(p_by)
    -- TODO
    return Vector2:new(self.x, self.y)
end

function Vector2:limit_length(len)
    local l = self:length()
    local v = Vector2:new(self.x, self.y)
    if (l > 0 and len < l) then
        v = v / l * len;
    end

    return v
end
Vector2.clamped = Vector2.limit_length

function Vector2:cubic_interpolate(p_b, p_pre_a, p_post_b, p_weight)
    local p0 = p_pre_a;
    local p1 = Vector2:new(self.x, self.y)
    local p2 = p_b
    local p3 = p_post_b

    local t = p_weight;
    local t2 = t * t;
    local t3 = t2 * t;

    local out = ((p1 * 2) + (-p0 + p2) * t +
                    (p0 * 2 - p1 * 5 + p2 * 4 - p3) * t2 +
                    (-p0 + p1 * 3 - p2 * 3 + p3) * t3) * 0.5
    return out;
end

function Vector2:move_toward(p_to, p_delta)
    local v = Vector2:new(self.x, self.y)
    local vd = p_to - v
    local len = vd:length()
    if len <= p_delta or len < CMP_EPSILON then
        return p_to
    else
        return v + vd / len * p_delta
    end
end

function Vector2:slide(p_normal)
    return self - p_normal * self:dot(p_normal)
end

function Vector2:bounce(normal)
    return -self:reflect(p_normal)
end

function Vector2:reflect(normal)
    return normal * self:dot(normal) * 2 - self
end

function Vector2:is_equal_approx(t)
    if t.x and t.y then
        return  math.abs(self.x - t.x) < CMP_EPSILON and math.abs(self.y - t.y) < CMP_EPSILON
    end
    return false
end

Vector2.__tostring = function(self)
    if self.x and self.y then
        return string.format('(%f,%f)', self.x, self.y)
    else
        return "{Vector2}"
    end
end

-- a / d(number)
Vector2.__div = function(va, d)
    return setmetatable({x = va.x / d, y = va.y / d}, Vector2)
end

-- a * d(number)
-- a * b(Vector2)
Vector2.__mul = function(a, d)
    if type(d) == 'number' then
        return setmetatable({x = a.x * d, y = a.y * d}, Vector2)
    else
        return setmetatable({x = a * d.x, y = a * d.y}, Vector2)
    end
end

-- a + b
Vector2.__add = function(a, b)
    return setmetatable({x = a.x + b.x, y = a.y + b.y}, Vector2)
end

-- a - b
Vector2.__sub = function(a, b)
    return setmetatable({x = a.x - b.x, y = a.y - b.y}, Vector2)
end

-- -a
Vector2.__unm = function(v)
    return setmetatable({x = -v.x, y = -v.y}, Vector2)
end

-- a == b
Vector2.__eq = function(a,b)
    return a:is_equal_approx(b)
end

-- get.magnitude 		= Vector2.Magnitude
-- get.normalized 		= Vector2.Normalize
-- get.sqrMagnitude 	= Vector2.SqrMagnitude

setmetatable(Vector2, Vector2)
return Vector2
