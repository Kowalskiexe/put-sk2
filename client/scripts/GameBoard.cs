using Godot;
using System;

public partial class GameBoard : Node2D
{
	
	private TileMapLayer _tileMap;
	
	// Called when the node enters the scene tree for the first time.
	public override void _Ready()
	{
		_tileMap = GetNode<TileMapLayer>("TileMapLayer");
	}

	// Called every frame. 'delta' is the elapsed time since the previous frame.
	public override void _Process(double delta)
	{

	}
	
	public void SyncTileMap(WorldStatePacket worldState) {
		_tileMap.Clear();
		for (int y = 0; y <= 16; y++) {
			for (int x = 0; x <= 16; x++) {
				if (y == 0 || y == 16 || x == 0 || x == 16) {
					_tileMap.SetCell(new Vector2I(x, y), 0, TileState.BlockedIndestructible.ToVec(), 0);
					continue;
				}
				var tile = worldState.GetTile(x - 1, y - 1);
				_tileMap.SetCell(new Vector2I(x, y), 0, tile.ToVec(), 0);
			}
		}
	}
}
