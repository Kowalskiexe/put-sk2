using Godot;
using System;

public partial class JoinButton : Button
{
	private GameManager _manager;
	[Export] public LineEdit ServerAddressLineEdit;
	[Export] public LineEdit ServerPortLineEdit;
	[Export] public LineEdit ClientPortLineEdit;
	
	// Called when the node enters the scene tree for the first time.
	public override void _Ready()
	{
		_manager = GetNode<GameManager>("/root/GameManager");
	}

	// Called every frame. 'delta' is the elapsed time since the previous frame.
	public override void _Process(double delta)
	{
		bool isServerPortValid = int.TryParse(ServerPortLineEdit.Text, out _);
		if (!isServerPortValid) {
			ServerPortLineEdit.AddThemeColorOverride("font_color", new Color(1, 0, 0));
		} else {
			ServerPortLineEdit.RemoveThemeColorOverride("font_color");
		}
		bool isClientPortValid = int.TryParse(ClientPortLineEdit.Text, out _);
		if (!isClientPortValid) {
			ClientPortLineEdit.AddThemeColorOverride("font_color", new Color(1, 0, 0));
		} else {
			ClientPortLineEdit.RemoveThemeColorOverride("font_color");
		}
		Disabled = ServerAddressLineEdit.Text == "" || !isServerPortValid || !isClientPortValid;
	}
	
	public void _on_pressed() {
		int serverPort = int.Parse(ServerPortLineEdit.Text);
		int clientPort = int.Parse(ClientPortLineEdit.Text);
		var err = _manager.Connect(ServerAddressLineEdit.Text, serverPort, clientPort);
		if (err != Error.Ok) {
			GD.PrintErr($"Connect failed with: {err}");
			GD.PrintErr($"Could not connect and no ErrorDialog assigned: {err}");
		}
	}
}
