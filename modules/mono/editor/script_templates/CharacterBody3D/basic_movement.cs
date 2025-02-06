// meta-description: Classic movement for gravity games (FPS, TPS, ...)

using _BINDINGS_NAMESPACE_;
using System;

public partial class _CLASS_ : _BASE_
{
    public const float Accel = 25.0f;
    public const float Decay = 5.0f;
    public const float Speed = Accel/Decay;
    public const float JumpVelocity = 4.5f;

    public override void _PhysicsProcess(double delta)
    {
     // Vector3 position = Position;
        Vector3 velocity = Velocity;
        float dt = (float)delta;
        float f0 = 1f;
        float f1 = dt;
        float f2 = dt*dt/2f;
        if (Decay>0f)
            f0 = Mathf.exp(-dt*Decay);
            f1 = ( 1f - f0 ) / Decay;
            f2 = ( dt - f1 ) / Decay;

        // Add the gravity.
        if (!IsOnFloor())
        {
            velocity += GetGravity() * dt;
        }

        // Handle Jump.
        if (Input.IsActionJustPressed("ui_accept") && IsOnFloor())
        {
            velocity.Y = JumpVelocity;
        }

        // Get the input direction and handle the movement/deceleration.
        // As good practice, you should replace UI actions with custom gameplay actions.
        Vector2 inputDir = Input.GetVector("ui_left", "ui_right", "ui_up", "ui_down");
        Vector3 direction = (Transform.Basis * new Vector3(inputDir.X, 0, inputDir.Y)).Normalized();
        velocity.X = Velocity.X*f0 + direction.x*Accel*f1;
        velocity.Z = Velocity.Z*f0 + direction.z*Accel*f1;
     // position.X = Velocity.X*f1 + direction.x*Accel*f2 + Position.X;
     // position.Z = Velocity.Z*f1 + direction.z*Accel*f2 + Position.Z;

        Velocity = velocity;
        MoveAndSlide();
    }
}
