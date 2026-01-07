#include "Game.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <array>
#include <utility>

constexpr float BOMB_TTL = 3.0f;
constexpr float EXPLOSION_TTL = 0.5f;
constexpr int WALL_CHANCE_PERCENT = 60;
constexpr int BOMB_RANGE = 2; 
constexpr int BOMB_LIMIT= 1; 

constexpr std::array<std::pair<int,int>, 4> SPAWN_POINTS = {{
    {0, 0},
    {GRID_WIDTH - 1, 0},
    {0, GRID_HEIGHT - 1},
    {GRID_WIDTH - 1, GRID_HEIGHT - 1}
}};

Game::Game() : running(false), winnerId(0) {
    grid.resize(GRID_SIZE, TileState::Empty);
    generateMap();
}

void Game::reset() {
    running = false;
    winnerId = 0;
    bombs.clear();
    explosions.clear();
    generateMap();
    
    for (auto& [id, player] : players) {
        player.alive = true;
        auto sp = SPAWN_POINTS[id - 1];
        player.x = sp.first;
        player.y = sp.second;
    }
}

void Game::addPlayer(uint8_t id) {
    if (players.count(id)) return;
    Player p;
    p.id = id;
    p.alive = true;
    auto sp = SPAWN_POINTS[id - 1];
    p.x = sp.first;
    p.y = sp.second;

    players[id] = p;
    std::cout << "Player " << int(id) << " added at " << p.x << "," << p.y << std::endl;
}

void Game::removePlayer(uint8_t id) {
    players.erase(id);
}

void Game::startGame() {
    if (running) return;

    if (players.size() < 2) {
        std::cout << "Cannot start game: need at least 2 players (have " << players.size() << ")" << std::endl;
        return;
    }
    reset();
    running = true;
    std::cout << "Game Started!" << std::endl;
}

bool Game::isWalkable(int x, int y) const {
    if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT) return false;
    int idx = y * GRID_WIDTH + x;
    
    if (grid[idx] != TileState::Empty && grid[idx] != TileState::Explosion) {
        return false; 
    }

    for (const auto& b : bombs) {
        if (b.x == x && b.y == y) return false;
    }

    for (const auto& kv : players) {
        const Player& pl = kv.second;
        if (pl.alive && pl.x == x && pl.y == y) return false;
    }

    return true;
}

void Game::generateMap() {
    std::fill(grid.begin(), grid.end(), TileState::Empty);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 100);

    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            // indestructible walls (odd x odd)
            if (x % 2 != 0 && y % 2 != 0) {
                grid[y * GRID_WIDTH + x] = TileState::BlockedIndestructible;
            } else {
                // destructible walls random chance
                bool spawnZone = (
                    (x < 3 && y < 3) || // top left
                    (x > GRID_WIDTH - 4 && y < 3) || // top right
                    (x < 3 && y > GRID_HEIGHT - 4) || // bottom left
                    (x > GRID_WIDTH - 4 && y > GRID_HEIGHT - 4) // bottom right
               );
                if (!spawnZone && dis(gen) < WALL_CHANCE_PERCENT) {
                     grid[y * GRID_WIDTH + x] = TileState::BlockedDestructible;
                }
            }
        }
    }
}

void Game::processInput(uint8_t playerId, InputCommand cmd) {
    if (!running) return;
    if (players.find(playerId) == players.end()) return;
    
    Player& p = players[playerId];
    if (!p.alive) return;

    int nx = p.x;
    int ny = p.y;

    switch (cmd) {
        case InputCommand::MoveNorth: ny--; break;
        case InputCommand::MoveSouth: ny++; break;
        case InputCommand::MoveWest: nx--; break;
        case InputCommand::MoveEast: nx++; break;
        case InputCommand::PlaceBomb: 
            spawnBomb(p.x, p.y, p.id);
            return;
        default: return;
    }

    if (nx != p.x || ny != p.y) {
        if (isWalkable(nx, ny)) {
            p.x = nx;
            p.y = ny;
        }
    }
}

void Game::spawnBomb(int x, int y, uint8_t ownerId) {
    // check if bomb already exists there
    for (const auto& b : bombs) {
        if (b.x == x && b.y == y) return;
    }
    
    int playerBombs = 0;
    for (const auto& b : bombs) if (b.ownerId == ownerId) playerBombs++;

    if (playerBombs < BOMB_LIMIT) {
        bombs.push_back({x, y, BOMB_TTL, ownerId});
    }
}

void Game::update(float delta) {
    if (!running) return;

    updateBombs(delta);
    updateExplosions(delta);

    checkPlayerDeath();
}

void Game::updateBombs(float delta) {
    for (auto it = bombs.begin(); it != bombs.end(); ) {
        it->ttl -= delta;
        if (it->ttl <= 0) {
            triggerExplosion(*it);
            it = bombs.erase(it);
        } else {
            ++it;
        }
    }
}

void Game::updateExplosions(float delta) {
    for (auto it = explosions.begin(); it != explosions.end(); ) {
        it->ttl -= delta;
        if (it->ttl <= 0) {
            it = explosions.erase(it);
        } else {
            // kill players in explosion
            for (auto& [id, p] : players) {
                if (p.alive && p.x == it->x && p.y == it->y) {
                    p.alive = false;
                    std::cout << "Player " << int(id) << " died!" << std::endl;
                }
            }
            ++it;
        }
    }
}

void Game::triggerExplosion(const Bomb& bomb) {
    // returns true if the ray should continue, false otherwise
    auto addExplosion = [&](int x, int y) {
        if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT) return false;
        int idx = y * GRID_WIDTH + x;
        
        if (grid[idx] == TileState::BlockedIndestructible) return false;
        
        explosions.push_back({x, y, EXPLOSION_TTL});
        
        // destroy breakable walls and stop
        if (grid[idx] == TileState::BlockedDestructible) {
            grid[idx] = TileState::Empty;
            return false;
        }
        return true;
    };

    addExplosion(bomb.x, bomb.y);
    for (int i = 1; i <= BOMB_RANGE; ++i) if (!addExplosion(bomb.x + i, bomb.y)) break;
    for (int i = 1; i <= BOMB_RANGE; ++i) if (!addExplosion(bomb.x - i, bomb.y)) break;
    for (int i = 1; i <= BOMB_RANGE; ++i) if (!addExplosion(bomb.x, bomb.y + i)) break;
    for (int i = 1; i <= BOMB_RANGE; ++i) if (!addExplosion(bomb.x, bomb.y - i)) break;
}

void Game::checkPlayerDeath() {
    int aliveCount = 0;
    uint8_t lastAlive = 0;
    for (const auto& [id, p] : players) {
        if (p.alive) {
            aliveCount++;
            lastAlive = id;
        }
    }

    if (aliveCount <= 1) {
        if (aliveCount == 0) {
            // DRAW
            winnerId = 5;
            std::cout << "Game ended in a draw." << std::endl;
        } else {
            winnerId = lastAlive;
            std::cout << "Player " << int(winnerId) << " wins the match." << std::endl;
        }
        running = false;
    }
}

void Game::fillStatePacket(WorldStatePacket& packet) {
    packet.type = PacketType::WorldState;
    packet.gameRunning = running;
    packet.winnerId = winnerId;

    // copy base grid
    for (int i = 0; i < GRID_SIZE; ++i) {
        packet.grid[i] = grid[i];
    }

    // overlay explosions
    for (const auto& e : explosions) {
        int idx = e.y * GRID_WIDTH + e.x;
        if (idx >= 0 && idx < GRID_SIZE) packet.grid[idx] = TileState::Explosion;
    }

    // overlay players 
    for (const auto& [id, p] : players) {
        if (!p.alive) continue;
        int idx = p.y * GRID_WIDTH + p.x;
        if (idx >= 0 && idx < GRID_SIZE) {
            if (id == 1) packet.grid[idx] = TileState::Player1;
            else if (id == 2) packet.grid[idx] = TileState::Player2;
            else if (id == 3) packet.grid[idx] = TileState::Player3;
            else if (id == 4) packet.grid[idx] = TileState::Player4;
        }
    }

    // overlay bombs
    for (const auto& b : bombs) {
        int idx = b.y * GRID_WIDTH + b.x;
        if (idx >= 0 && idx < GRID_SIZE) packet.grid[idx] = TileState::Bomb;
    }
    // Bombs are overlaid last so if a player placed a bomb and hasn't moved,
    // only the bomb will be displayed. This is the intended behavior.
}
