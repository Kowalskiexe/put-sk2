#pragma once
#include <cstdint>

// Constants
constexpr int GRID_WIDTH = 15;
constexpr int GRID_HEIGHT = 15;
constexpr int GRID_SIZE = GRID_WIDTH * GRID_HEIGHT;
constexpr int SERVER_TCP_PORT = 5000;
constexpr int SERVER_UDP_PORT = 5001;
constexpr int MAX_PLAYERS = 4;

// Tile States
enum class TileState : uint8_t {
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

// Packet Types
enum class PacketType : uint8_t {
    ConnectResponse = 1,
    StartGame = 2,
    Input = 3,
    WorldState = 4
};

#pragma pack(push, 1)

struct PacketHeader {
    PacketType type;
};

// TCP: Connect Response
struct ConnectResponsePacket {
    PacketHeader header;
    uint32_t playerId; // 1-4
};

// UDP: Client Input
struct InputPacket {
    PacketHeader header;
    uint32_t playerId;
    int8_t dx; // -1, 0, 1
    int8_t dy; // -1, 0, 1
    bool placeBomb;
};

// UDP: Server World State
struct WorldStatePacket {
    PacketHeader header;
    TileState grid[GRID_SIZE];
    bool gameRunning;
    uint32_t winnerId; // 0 if no winner yet or draw logic
};

#pragma pack(pop)
