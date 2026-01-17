using Godot;
using System;
using System.Collections.Generic;

public partial class GameManager : Node2D
{
	private int _playerId;
	private StreamPeerTcp _tcp = new StreamPeerTcp();
	private bool _joined = false;
	private int _clientPort;
	private UdpServer _udp = null;
	private readonly Dictionary<InputCommand, Key> _keyMap = new Dictionary<InputCommand, Key> {
			{ InputCommand.MoveNorth, Key.W },
			{ InputCommand.MoveSouth, Key.S },
			{ InputCommand.MoveWest, Key.A },
			{ InputCommand.MoveEast, Key.D },
			{ InputCommand.PlaceBomb, Key.Space }
		};
	private Dictionary<InputCommand, bool> _lastKeyState = new Dictionary<InputCommand, bool>();
	private PacketPeerUdp _serverUdpPeer;
	private GameBoard _gameBoard;
	[Export] public Button startButton;
	[Export] public CanvasLayer startScreen;
	[Export] public CanvasLayer gameScreen;
	[Export] public CanvasLayer endScreen;
	
	public override void _Ready()
	{
		startScreen.Visible = true;
		gameScreen.Visible = false;
		endScreen.Visible = false;
		_gameBoard = GetNode<GameBoard>("GameBoard");
		foreach (var k in _keyMap.Keys) _lastKeyState[k] = false;
	}

	public override void _Process(double delta)
	{
		HandleTcp();
		HandleUdp();
		handleKeyboard();
	}

	private void HandleTcp() {
		if (_tcp == null)
			return;

		if (_tcp.GetStatus() != StreamPeerTcp.Status.None)
			_tcp.Poll();

		if (_tcp.GetStatus() == StreamPeerTcp.Status.Connected) {
			if (!_joined)
				SendJoinPacket();

			if (_tcp.GetAvailableBytes() > 0)
			{
				var data = _tcp.GetData(_tcp.GetAvailableBytes());
				var error = (Error)(int)data[0];
				
				if (error == Error.Ok)
				{
					var payload = data[1].As<byte[]>();
					var msg = BlittableBuffer.FromBytes<ConnectResponsePacket>(payload);
					_playerId = msg.playerId;
					GD.Print($"Registered as player {_playerId}");
					startScreen.Visible = false;
					SetPlayerText();
					gameScreen.Visible = true;
				}
			}
		}
	}

	private void HandleUdp() {
		if (_udp == null)
			return;

		_udp.Poll();
		if (_udp.IsConnectionAvailable())
		{
			_serverUdpPeer = _udp.TakeConnection();
			GD.Print($"Accepted Peer: {_serverUdpPeer.GetPacketIP()}:{_serverUdpPeer.GetPacketPort()}");
		}

		if (_serverUdpPeer != null) {
			byte[] packet = _serverUdpPeer.GetPacket();
			if (packet.Length > 0) {
				var worldState = BlittableBuffer.FromBytes<WorldStatePacket>(packet);
				SyncWorldState(worldState);
			}
		}
	}

	void handleKeyboard() {
		foreach (var (cmd, key) in _keyMap)
		{
			bool pressed = Input.IsKeyPressed(key);
			bool last = _lastKeyState[cmd];
			if (pressed && !last) {
				SendInput(cmd);
			}
			_lastKeyState[cmd] = pressed;
		}
	}
	
	private unsafe void SyncWorldState(WorldStatePacket packet)  {
		_gameBoard.SyncTileMap(packet);
		startButton.Visible = packet.gameRunning == 0;

		var winningLabel = GetNode<Label>("EndCanvasLayer/WinningPlayerLabel");

		if (packet.gameRunning == 0 && packet.winnerId != 0) {
			endScreen.Visible = true;
			gameScreen.Visible = false;

			if (packet.winnerId == 5) {
				winningLabel.Text = "DRAW!";
			} else if (packet.winnerId >= 1 && packet.winnerId <= 4) {
				winningLabel.Text = $"PLAYER {packet.winnerId} WON!!!";
			} else {
				winningLabel.Text = "GAME OVER";
			}
		} else {
			endScreen.Visible = false;
			gameScreen.Visible = true;
		}
	}

	public void _on_play_again_button_pressed() {
		GD.Print("Play again");
		var restartPacket = new RestartPacket {
			type = PacketType.Restart,
			playerId = (byte)_playerId
		};
		if (_serverUdpPeer == null) {
			GD.PrintErr("UDP peer not available; cannot send restart");
			return;
		}
		var restartPayload = BlittableBuffer.ToBytes(restartPacket);
		var rerr = _serverUdpPeer.PutPacket(restartPayload);
		if (rerr != Error.Ok) GD.PrintErr($"UDP restart send failed: {rerr}");
	}
	
	public void _on_start_button_pressed() {
		GD.Print("Starting game");
		var startPacket = new StartPacket {
			type = PacketType.StartGame,
			playerId = (byte)_playerId
		};
		if (_serverUdpPeer == null) {
			GD.PrintErr("UDP peer not available; cannot send start");
			return;
		}
		var startPayload = BlittableBuffer.ToBytes(startPacket);
		var serr = _serverUdpPeer.PutPacket(startPayload);
		if (serr != Error.Ok) GD.PrintErr($"UDP start send failed: {serr}");
	}
	
	private void SetPlayerText() {
		var playerLabel = GetNode<Label>("GameCanvasLayer/PlayerLabel");
		playerLabel.Text = $"Player {_playerId}";
	}
	
	public Error Connect(string serverAddress, int serverPort, int clientPort)
	{
		// clean up previous connection if present, e.g. hanging due to wrong port
		if (_tcp != null && _tcp.GetStatus() != StreamPeerTcp.Status.None)
		{
			GD.Print("Disconnecting existing TCP connection");
			_tcp.DisconnectFromHost();
		}
		_tcp = new StreamPeerTcp();
		_udp = new UdpServer();

		_joined = false;
		_serverUdpPeer = null;
		_clientPort = clientPort;

		var tcpErr = _tcp.ConnectToHost(serverAddress, serverPort);
		if (tcpErr != Error.Ok)
		{
			GD.PrintErr($"Could not connect: {tcpErr}");
			return tcpErr;
		}
		
		var lerr = _udp.Listen((ushort)clientPort);
		if (lerr != Error.Ok) {
			GD.PrintErr($"Could not listen UDP on port {clientPort}: {lerr}");
			return lerr;
		}
		return Error.Ok;
	}
	
	private void SendJoinPacket() {
		var joinPacket = new JoinPacket {
			type = PacketType.Join,
			listeningPort = (ushort)_clientPort
		};
		SendPacket(BlittableBuffer.ToBytes(joinPacket));
		_joined = true;
	}
	
	
	public void SendPacket(byte[] payload)
	{
		if (_tcp.GetStatus() != StreamPeerTcp.Status.Connected)
		{
			GD.PrintErr("Not connected!");
			return;
		}
		
		var err = _tcp.PutData(payload);
		
		if (err != Error.Ok)
			GD.PrintErr($"Send failed: {err}");
			else
				GD.Print($"Sent TCP ({payload.Length} bytes): {System.BitConverter.ToString(payload)}");
	}

	private void SendInput(InputCommand cmd)
	{
		if (_playerId == 0) return; // not registered yet

		var packet = new InputPacket {
			type = PacketType.Input,
			playerId = (byte)_playerId,
			cmd = cmd
		};
		var payload = BlittableBuffer.ToBytes(packet);

		if (_serverUdpPeer == null) {
			GD.PrintErr("UDP peer not available; dropping input");
			return;
		}

		var err = _serverUdpPeer.PutPacket(payload);
		if (err != Error.Ok) {
			GD.PrintErr($"UDP send failed: {err}");
		} else {
			GD.Print($"Sent UDP input: {cmd}");
		}
	}
	
	public override void _ExitTree()
	{
		if (_tcp != null)
		{
			_tcp.DisconnectFromHost();
		}
	}
}
