#pragma once
#include <vector>
#include <map>
#include <chrono>
#include "Protocol.h"

struct Player {
    uint8_t id;
    int x;
    int y;
    bool alive;
};

struct Bomb {
    int x;
    int y;
    float ttl;
    uint8_t ownerId;
};

struct Explosion {
    int x;
    int y;
    float ttl;
};

class Game {
public:
    Game();
    void reset();
    void addPlayer(uint8_t id);
    void removePlayer(uint8_t id);
    void startGame();
    void update(float dt);
    void processInput(uint8_t playerId, InputCommand cmd);
    
    bool isRunning() const { return running; }
    uint8_t getWinner() const { return winnerId; }
    const std::vector<TileState>& getGrid() const { return grid; }

    void fillStatePacket(WorldStatePacket& packet);

private:
    void generateMap();
    bool isWalkable(int x, int y) const;
    void spawnBomb(int x, int y, uint8_t ownerId);
    void triggerExplosion(const Bomb& bomb);
    void clearExplosion(int x, int y);
    void checkPlayerDeath();

    void updateBombs(float delta);
    void updateExplosions(float delta);

    std::vector<TileState> grid;    

    std::map<uint8_t, Player> players;
    std::vector<Bomb> bombs;
    std::vector<Explosion> explosions;

    bool running;
    uint8_t winnerId;
};
