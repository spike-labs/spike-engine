--#Classic movement for gravity games (platformer, ...)
---@class _CLASS_ : _BASE_
local _CLASS_ = class(_BASE_)

local SPEED = 300.0
local JUMP_VELOCITY = -400.0

---Get the gravity from the project settings to be synced with RigidBody nodes.
_CLASS_.gravity = ProjectSettings:get_setting("physics/2d/default_gravity")

function _CLASS_:_physics_process(delta)
	-- Add the gravity.
	if not self:is_on_floor() then
		self.velocity.y = self.velocity.y + self.gravity * delta
	end

	-- Handle Jump.
	if Input:is_action_just_pressed("ui_accept") and self:is_on_floor() then
		self.velocity.y = JUMP_VELOCITY
	end

	-- Get the input direction and handle the movement/deceleration.
	-- As good practice, you should replace UI actions with custom gameplay actions.
	local direction = Input:get_axis("ui_left", "ui_right")
	if direction ~= Vector2.ZERO then
		velocity.x = direction * SPEED
	else
		velocity.x = move_toward(velocity.x, 0, SPEED)
	end

	self:move_and_slide()
end

return _CLASS_
