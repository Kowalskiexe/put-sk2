using Godot;
using System;
using System.Runtime.InteropServices;


public static class BlittableBuffer
{
	public static byte[] ToBytes<T>(T data) where T : unmanaged
	{
		int size = Marshal.SizeOf<T>();
		byte[] buffer = new byte[size];

		// pin the struct so the GC can't move it
		var handle = GCHandle.Alloc(data, GCHandleType.Pinned);
		try
		{
			// copy directly from the pinned struct to the byte array
			Marshal.Copy(handle.AddrOfPinnedObject(), buffer, 0, size);
		}
		finally
		{
			handle.Free();
		}

		return buffer;
	}


	public static T FromBytes<T>(byte[] buffer) where T : unmanaged
	{
		int size = Marshal.SizeOf<T>();
		if (buffer.Length != size)
			throw new ArgumentException($"Byte array length ({buffer.Length}) does not match expected size ({size})");

		// pin the array so we can get a stable pointer to its data
		var handle = GCHandle.Alloc(buffer, GCHandleType.Pinned);
		try
		{
			return Marshal.PtrToStructure<T>(handle.AddrOfPinnedObject());
		}
		finally
		{
			handle.Free();
		}
	}
}

public enum TileState: byte {
	Empty = 0,
	BlockedIndestructible = 1,
	BlockedDestructible = 2,
	Player1 = 3,
	Player2 = 4,
	Player3 = 5,
	Player4 = 6,
	Bomb = 7,
	Explosion = 8
};

public static class Extensions
{
	public static Vector2I ToVec(this TileState tileState) =>
		new Vector2I((int)tileState % 4, (int)tileState / 4); // TileSet is 4 by 4
}

public enum PacketType: byte {
	ConnectResponse = 1,
	StartGame = 2,
	Input = 3,
	WorldState = 4,
	Join = 5,
	Restart = 6
};

public enum InputCommand: byte {
	MoveNorth = 1,
	MoveSouth = 2,
	MoveWest = 3,
	MoveEast = 4,
	PlaceBomb = 5
};

[StructLayout(LayoutKind.Sequential, Pack = 1)] // set zero padding
public struct JoinPacket {
	public PacketType type;
	public ushort listeningPort;
};

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct ConnectResponsePacket {
	public PacketType type;
	public byte playerId; // 1-4
};

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct InputPacket {
	public PacketType type;
	public byte playerId;
	public InputCommand cmd;
};

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct RestartPacket {
	public PacketType type;
	public byte playerId;
};

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct StartPacket {
	public PacketType type;
	public byte playerId;
};

[StructLayout(LayoutKind.Sequential, Pack = 1)]
public unsafe struct WorldStatePacket {
	public PacketType type;
	public fixed byte grid[15 * 15];
	public byte winnerId; // 0 if no winner, 1-4 player, 5 draw
	public byte gameRunning;
	
	// NOTE: x first
	public TileState GetTile(int x, int y)
	{
		fixed (byte* ptr = grid)
		{
			return (TileState)ptr[y * 15 + x];
		}
	}
};
