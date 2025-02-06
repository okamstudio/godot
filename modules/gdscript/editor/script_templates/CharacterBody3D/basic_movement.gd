# meta-description: Classic movement for gravity games (FPS, TPS, ...)

extends _BASE_

# The @export annotation allows a variable to be shown and modified from the inspector.
@export var speed: float = 5.0
@export var accel: float = 5.0
@export var jump_speed: float = 4.5


func _physics_process(delta: float) -> void:
	# Handle gravity.
	if not is_on_floor():
		velocity += get_gravity() * delta

	# Get the vertical velocity.
	var vertical_velocity: Vector3 = velocity.project(up_direction)

	# Get the horizontal velocity.
	var horizontal_velocity: Vector3 = velocity - vertical_velocity

	# Handle jump.
	if Input.is_action_just_pressed("ui_accept") and is_on_floor():
		vertical_velocity = up_direction * jump_speed

	# As good practice, you should replace UI actions with custom gameplay actions.
	var input_vector: Vector2 = Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down", 0.15)

	# Calculate the intended direction in 3D space.
	var input_direction: Vector3 = transform.basis.orthonormalized() * Vector3(input_vector.x, 0, input_vector.y)

	# Calculate the target horizontal velocity.
	var target_horizontal_velocity: Vector3 = input_direction * speed

	# Move the current horizontal velocity towards the target horizontal velocity.
	horizontal_velocity = horizontal_velocity.move_toward(target_horizontal_velocity, accel * delta)

	# Compose the final velocity.
	velocity = horizontal_velocity + vertical_velocity

	move_and_slide()
