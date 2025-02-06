using Godot;
using System;

namespace WasmTest
{
    public partial class TestNode : Node
    {
        public override void _Ready()
        {
            GD.Print("Hello from WebAssembly!");
            
            // Create a simple 2D scene
            var sprite = new Sprite2D();
            sprite.Texture = GD.Load<Texture2D>("icon.png");
            sprite.Position = new Vector2(GetViewport().GetVisibleRect().Size / 2);
            AddChild(sprite);
            
            // Add some animation
            var tween = CreateTween();
            tween.TweenProperty(sprite, "rotation", 2 * Math.PI, 2.0f)
                .SetTrans(Tween.TransitionType.Linear)
                .SetEase(Tween.EaseType.InOut)
                .SetLoops();
        }

        public override void _Process(double delta)
        {
            // Update logic here
        }

        public override void _Input(InputEvent @event)
        {
            if (@event is InputEventMouseButton mouseButton)
            {
                if (mouseButton.ButtonIndex == MouseButton.Left && mouseButton.Pressed)
                {
                    GD.Print($"Mouse clicked at: {mouseButton.Position}");
                }
            }
        }
    }
}
