// meta-description: Classic movement for gravity games (platformer, ...)

using _BINDINGS_NAMESPACE_;
using System;

public partial class _CLASS_ : _BASE_
{
    // The [Export] attribute allows a variable to be shown and modified from the inspector.
    [Export]
	public float speed = 300.0f;

    [Export]
	public float accel = 300.0f;

    [Export]
	public float jumpSpeed = -400.0f;

	public override void _PhysicsProcess(double delta)
	{
		Vector2 velocity = Velocity;

		// Handle gravity.
		if (!IsOnFloor())
		{
			velocity += GetGravity() * (float)delta;
		}

		// Get the vertical velocity.
		Vector2 verticalVelocity = velocity.Project(UpDirection);

		// Get the horizontal velocity.
		Vector2 horizontalVelocity = velocity - verticalVelocity;

		// Handle Jump.
		if (Input.IsActionJustPressed("ui_accept") && IsOnFloor())
		{
			verticalVelocity = UpDirection * jumpSpeed;
		}

		// As good practice, you should replace UI actions with custom gameplay actions.
		float inputAxis = Input.GetAxis("ui_left", "ui_right");

		// Calculate the intended direction in 2D plane.
		Vector2 inputDirection = new Vector2(inputAxis, 0).Rotated(Rotation);

		// Calculate the target horizontal velocity.
		Vector2 targetHorizontalVelocity = inputDirection * speed;

		// Move the current horizontal velocity towards the target horizontal velocity.
		horizontalVelocity = horizontalVelocity.MoveToward(targetHorizontalVelocity, accel * (float)delta);

		// Compose the final velocity.
		velocity = horizontalVelocity + verticalVelocity;

		Velocity = velocity;
		MoveAndSlide();
	}
}
