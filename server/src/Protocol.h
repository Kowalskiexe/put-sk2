#pragma once
#include <cstdint>

constexpr int GRID_WIDTH = 15;
constexpr int GRID_HEIGHT = 15;
constexpr int GRID_SIZE = GRID_WIDTH * GRID_HEIGHT;
constexpr int SERVER_TCP_PORT = 5000;
constexpr int SERVER_UDP_PORT = 5001;
constexpr int MAX_PLAYERS = 4;

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

enum class PacketType : uint8_t {
    ConnectResponse = 1,
    StartGame = 2,
    Input = 3,
    WorldState = 4,
    Join = 5,
    Restart = 6
};

enum class InputCommand : uint8_t {
    MoveNorth = 1,
    MoveSouth = 2,
    MoveWest = 3,
    MoveEast = 4,
    PlaceBomb = 5
};

#pragma pack(push, 1)

// TCP: Client Join
struct JoinPacket {
    PacketType type;
    uint16_t listeningPort;
};

// TCP: Connect Response
struct ConnectResponsePacket {
    PacketType type;
    uint8_t playerId; // 1-4
};

// UDP: Client Input
struct InputPacket {
    PacketType type;
    uint8_t playerId;
    InputCommand cmd;
};

// UDP: Client Start request
struct StartPacket {
    PacketType type;
    uint8_t playerId; // requester
};

// UDP: Client Restart request 
struct RestartPacket {
    PacketType type;
    uint8_t playerId; // requester
};

// UDP: Server World State
struct WorldStatePacket {
    PacketType type;
    TileState grid[GRID_SIZE];
    uint8_t winnerId; // 0 if no winner, 1-4 player, 5 draw
    uint8_t gameRunning;
};

#pragma pack(pop)
