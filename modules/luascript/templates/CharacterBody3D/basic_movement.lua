--#Classic movement for gravity games (FPS, TPS, ...)
---@class _CLASS_ : _BASE_
local _CLASS_ = class(_BASE_)

local SPEED = 5.0
local JUMP_VELOCITY = 4.5

---Get the gravity from the project settings to be synced with RigidBody nodes.
_CLASS_.gravity = ProjectSettings:get_setting("physics/3d/default_gravity")

function _CLASS_:_physics_process(delta)
	-- Add the gravity.
	if not self:is_on_floor() then
		self.velocity.y = self.velocity.y -  self.gravity * delta
	end

	-- Handle Jump.
	if Input:is_action_just_pressed("ui_accept") and self:is_on_floor() then
		self.velocity.y = JUMP_VELOCITY
	end

	-- Get the input direction and handle the movement/deceleration.
	-- As good practice, you should replace UI actions with custom gameplay actions.
	local input_dir = Input:get_vector("ui_left", "ui_right", "ui_up", "ui_down")
	local direction = (self.transform.basis * Vector3(input_dir.x, 0, input_dir.y)):normalized()
	if direction ~= Vector3.ZERO then
		velocity.x = direction.x * SPEED
		velocity.z = direction.z * SPEED
	else
		velocity.x = self:move_toward(velocity.x, 0, SPEED)
		velocity.z = self:move_toward(velocity.z, 0, SPEED)
	end

	self:move_and_slide()
end

return _CLASS_
