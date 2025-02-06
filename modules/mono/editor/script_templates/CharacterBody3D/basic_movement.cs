// meta-description: Classic movement for gravity games (FPS, TPS, ...)

using _BINDINGS_NAMESPACE_;
using System;

public partial class _CLASS_ : _BASE_
{
    // The [Export] attribute allows a variable to be shown and modified from the inspector.
    [Export]
	public float speed = 5.0f;

    [Export]
	public float accel = 5.0f;

    [Export]
	public float jumpSpeed= 4.5f;

	public override void _PhysicsProcess(double delta)
	{
		Vector3 velocity = Velocity;

		// Add the gravity.
		if (!IsOnFloor())
		{
			velocity += GetGravity() * (float)delta;
		}

		// Get the vertical velocity.
		Vector3 verticalVelocity = velocity.Project(UpDirection);

		// Get the horizontal velocity.
		Vector3 horizontalVelocity = velocity - verticalVelocity;

		// Handle Jump.
		if (Input.IsActionJustPressed("ui_accept") && IsOnFloor())
		{
			verticalVelocity = UpDirection * jumpSpeed;
		}

		// As good practice, you should replace UI actions with custom gameplay actions.
		Vector2 inputVector = Input.GetVector("ui_left", "ui_right", "ui_up", "ui_down", 0.15f);

		// Calculate the intended direction in 3D space.
		Vector3 inputDirection = Transform.Basis.Orthonormalized() * new Vector3(inputVector.X, 0, inputVector.Y);

		// Calculate the target horizontal velocity.
		Vector3 targetHorizontalVelocity = inputDirection * speed;

		// Move the current horizontal velocity towards the target horizontal velocity.
		horizontalVelocity = horizontalVelocity.MoveToward(targetHorizontalVelocity, accel * (float)delta);

		// Compose the final velocity.
		velocity = horizontalVelocity + verticalVelocity;

		Velocity = velocity;
		MoveAndSlide();
	}
}
