#pragma once
#include <vector>
#include <map>
#include <chrono>
#include "Protocol.h"

struct Player {
    uint32_t id;
    int x;
    int y;
    bool alive;
    
    // Simple cooldown to prevent moving too fast if we receive many packets
    // In this simple version we might just process every input, but limiting is better.
    // Actually, client sends state every tick, server processes tick.
    // We will just process inputs as they come or per tick. 
    // Let's stick to processing "latest input" per server tick.
};

struct Bomb {
    int x;
    int y;
    float timer; // Counts down from 5.0f
    uint32_t ownerId;
};

struct Explosion {
    int x;
    int y;
    float timer; // Counts down from 0.5f
};

class Game {
public:
    Game();
    void reset();
    void addPlayer(uint32_t id);
    void removePlayer(uint32_t id);
    void startGame();
    void update(float dt);
    void processInput(uint32_t playerId, int8_t dx, int8_t dy, bool placeBomb);
    
    bool isRunning() const { return running; }
    uint32_t getWinner() const { return winnerId; }
    const std::vector<TileState>& getGrid() const { return grid; }

    // Helper to serialize grid state for network with players overlaid
    void fillStatePacket(WorldStatePacket& packet);

private:
    void generateMap();
    bool isWalkable(int x, int y) const;
    void spawnBomb(int x, int y, uint32_t ownerId);
    void triggerExplosion(const Bomb& bomb);
    void clearExplosion(int x, int y);
    void checkPlayerDeath();

    std::vector<TileState> grid; // The base map (Walls, Destructible walls)
    // We overlay Players, Bombs, Explosions on top for the packet, 
    // but keep them in separate structures for logic.
    
    std::map<uint32_t, Player> players;
    std::vector<Bomb> bombs;
    std::vector<Explosion> explosions;

    bool running;
    uint32_t winnerId;
};
