
local CMP_EPSILON = 0.00001
local Vector3, get = { __type = CONST.TYPE_VECTOR3, __name = 'Vector3' }, {}
local function SGN(n) return n < 0 and 1 or -1 end

Vector3.__index = function (t, k)
    local var = rawget(get, k)
    if var then return var(t) end

    return rawget(Vector3, k)
end
Vector3.__newindex = function (t, n, k) end

Vector3.__call = function(t, x, y, z)
    local t = type(x)
    if t == "table" or t == "userdata" then
        x, y, z = x.x, x.y, x.z
    end
    return setmetatable({x = x or 0, y = y or 0, z = z or 0}, Vector3)
end

Vector3.new = Vector3.__call

---
--- Enumerated value for the X axis. Returned by [method max_axis] and [method min_axis].
Vector3.AXIS_X = 0

---
--- Enumerated value for the Y axis. Returned by [method max_axis] and [method min_axis].
Vector3.AXIS_Y = 1

---
--- Enumerated value for the Z axis. Returned by [method max_axis] and [method min_axis].
Vector3.AXIS_Z = 2

---
--- Zero vector, a vector with all components set to `0`.
get.ZERO = function (t) return Vector3:new( 0, 0, 0 ) end

---
--- One vector, a vector with all components set to `1`.
get.ONE = function (t) return Vector3:new( 1, 1, 1 ) end

---
--- Infinity vector, a vector with all components set to [constant @GDScript.INF].
-- Vector3.INF = Vector3:new( inf, inf, inf )

---
--- Left unit vector. Represents the local direction of left, and the global direction of west.
get.LEFT = function (t) return Vector3:new( -1, 0, 0 ) end

---
--- Right unit vector. Represents the local direction of right, and the global direction of east.
get.RIGHT = function (t) return Vector3:new( 1, 0, 0 ) end

---
--- Up unit vector.
get.UP = function (t) return Vector3:new( 0, 1, 0 ) end

---
--- Down unit vector.
get.DOWN = function (t) return Vector3:new( 0, -1, 0 ) end

---
--- Forward unit vector. Represents the local direction of forward, and the global direction of north.
get.FORWARD = function (t) return Vector3:new( 0, 0, -1 ) end

---
--- Back unit vector. Represents the local direction of back, and the global direction of south.
get.BACK = function (t) return Vector3:new( 0, 0, 1 ) end


function Vector3:abs()
    return Vector3:new(math.abs(self.x), math.abs(self.y))
end

function Vector3:angle_to(to)
    return math.atan(self:cross(to).length(), self:dot(to))
end

function Vector3:bounce(n)
    return -self:reflect(n)
end

function Vector3:ceil()
    return Vector3:new(math.ceil(self.x), math.ceil(self.y), math.ceil(self.z))
end

function Vector3:cross(b)
    local x = self.y * b.z - self.z * b.y
	local y = self.z * b.x - self.x * b.z
	local z = self.x * b.y - self.y * b.x
	return Vector3:new(x, y, z)
end

function Vector3:cubic_interpolate(b, pre_a, post_b, weight)
    local p0 = Vector3:new(pre_a.x, pre_a.y, pre_a.z)
	local p1 = Vector3:new(self.x, self.y, self.z)
	local p2 = Vector3:new(b.x, b.y, b.z)
	local p3 = Vector3:new(post_b.x, post_b.y, post_b.z)

	local t = weight;
	local t2 = t * t;
	local t3 = t2 * t;

	return ((p1 * 2) +
			(-p0 + p2) * t +
			(2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 +
			(-p0 + 3 * p1 - 3 * p2 + p3) * t3) * 0.5
end

function Vector3:direction_to(b)
    return (b - self).normalized()
end

function Vector3:distance_squared_to(b)
    return (self.x - b.x) * (self.x - b.x) + (self.y - b.y) * (self.y - b.y) + (self.z - b.z) * (self.z - b.z)
end

function Vector3:distance_to(b)
    return math.sqrt(self:distance_squared_to(b))
end

function Vector3:dot(b)
    return self.x * b.x + self.y * b.y + self.z * b.z
end

function Vector3:floor()
    return Vector3:new(math.floor(self.x), math.floor(self.y), math.floor(self.z))
end

function Vector3:inverse()
    return Vector3:new(1 / self.x, 1 / self.y, 1 / self.z)
end

function Vector3:is_equal_approx(v)
    if v.x and v.y and v.z then
        return math.abs(self.x - v.x) < CMP_EPSILON
            and math.abs(self.y - v.y) < CMP_EPSILON
            and math.abs(self.z - v.z) < CMP_EPSILON
    end
    return false
end

function Vector3:is_normalized()
    return math.abs(self:length_squared() - 1) < 0.001
end

function Vector3:length()
    return math.sqrt(self:length_squared())
end

function Vector3:length_squared()
    return self.x * self.x + self.y * self.y + self.z * self.z
end

function Vector3:limit_length(length)
    local l = self:length()
    local v = Vector3:new(self.x, self.y)
    if (l > 0 and length < l) then
        v = v / l * length
    end

    return v
end

function Vector3:linear_interpolate(to, weight)
    return Vector3:new(
        self.x + (weight * (to.x - self.x)),
        self.y + (weight * (to.y - self.y)),
        self.z + (weight * (to.z - self.z)))
end

function Vector3:max_axis()
    return math.max(self.x, self.y, self.z)
end

function Vector3:min_axis()
    return math.min(self.x, self.y, self.z)
end

function Vector3:move_toward(to, delta)
    local v = Vector3:new(self.x, self.y, self.z)
    local vd = to - v
    local len = vd:length()
    if len <= delta or len < CMP_EPSILON then
        return to
    else
        return v + vd / len * delta
    end
end

function Vector3:normalize()
    local l = self:length_squared()
    if l ~= 0 then
        l = math.sqrt(l);
        self.x = self.x / l
        self.y = self.y / l
        self.z = self.z / l
    end
end

function Vector3:normalized()
    local v = Vector3:new(self.x, self.y, self.z)
    v:normalize()
    return v
end

function Vector3:outer(b)
    local row0 = Vector3(self.x * b.x, self.x * b.y, self.x * b.z);
	local row1 = Vector3(self.y * b.x, self.y * b.y, self.y * b.z);
	local row2 = Vector3(self.z * b.x, self.z * b.y, self.z * b.z);

	return Basis(row0, row1, row2)
end

function Vector3:posmod(mod)
end

function Vector3:posmodv(modv)
end

function Vector3:project(b)
    return b * (self:dot(b) / b:length_squared())
end

function Vector3:reflect(n)
    return n * self:dot(n) * 2 - self
end

function Vector3:rotated(axis, rad)
    local ret = Vector3:new(self.x, self.y, self.z)
    return Basis(axis, rad).xform(ret)
end

function Vector3:round()
    return Vector3:new(math.floor(self.x + 0.5), math.floor(self.y + 0.5), math.floor(self.z + 0.5))
end

function Vector3:sign()
    return Vector3:new(SGN(self.x), SGN(self.y), SGN(self.z))
end

function Vector3:signed_angle_to(to, axis)
    local cross_to = self:cross(to)
	local unsigned_angle = math.atan(cross_to.length(), self:dot(to))
	local sign = cross_to:dot(axis)
	return (sign < 0) and -unsigned_angle or unsigned_angle
end

function Vector3:slerp(to, weight)
    local theta = self:angle_to(to)
	return self:rotated(self:cross(to).normalized(), theta * weight)
end

function Vector3:slide(n)
    return self - n * self:dot(n)
end

function Vector3:snapped(by)
end

function Vector3:to_diagonal_matrix()
    return Basis(self.x, 0, 0, 0, self.y, 0, 0, 0, self.z)
end

Vector3.__tostring = function(self)
    if self.x and self.y and self.z then
        return string.format('(%f,%f,%f)', self.x, self.y, self.z)
    else
        return "{Vector3}"
    end
end

-- a / d(number)
Vector3.__div = function(va, d)
    return Vector3:new(va.x / d, va.y / d, va.z / d)
end

-- a * d(number)
-- a * b(Vector3)
Vector3.__mul = function(a, d)
    if type(d) == 'number' then
        return Vector3:new(a.x * d, a.y * d, a.z * d)
    else
        return Vector3:new(a.x * d.x, a.y * d.y, a.z * d.z)
    end
end

-- a + b
Vector3.__add = function(a, b)
    return Vector3:new(a.x + b.x, a.y + b.y, a.z + b.z)
end

-- a - b
Vector3.__sub = function(a, b)
    return Vector3:new(a.x - b.x, a.y - b.y, a.z - b.z)
end

-- -a
Vector3.__unm = function(v)
    return Vector3:new(-v.x, -v.y, -v.z)
end

-- a == b
Vector3.__eq = function(a,b)
    return a:is_equal_approx(b)
end

-- get.magnitude 		= Vector3.Magnitude
-- get.normalized 		= Vector3.Normalize
-- get.sqrMagnitude 	= Vector3.SqrMagnitude

setmetatable(Vector3, Vector3)
return Vector3
