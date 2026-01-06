#include "Game.h"
#include <iostream>
#include <algorithm>
#include <random>

// Game Constants
constexpr float BOMB_TIMER = 5.0f;
constexpr float EXPLOSION_TIMER = 0.5f;
constexpr int BOMB_RANGE = 2; 

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
    
    // Reset players to spawn positions
    for (auto& [id, player] : players) {
        player.alive = true;
        if (id == 1) { player.x = 0; player.y = 0; }
        else if (id == 2) { player.x = GRID_WIDTH - 1; player.y = 0; }
        else if (id == 3) { player.x = 0; player.y = GRID_HEIGHT - 1; }
        else if (id == 4) { player.x = GRID_WIDTH - 1; player.y = GRID_HEIGHT - 1; }
    }
}

void Game::addPlayer(uint32_t id) {
    if (players.count(id)) return;
    Player p;
    p.id = id;
    p.alive = true;
    
    // Default spawns
    if (id == 1) { p.x = 0; p.y = 0; }
    else if (id == 2) { p.x = GRID_WIDTH - 1; p.y = 0; }
    else if (id == 3) { p.x = 0; p.y = GRID_HEIGHT - 1; }
    else if (id == 4) { p.x = GRID_WIDTH - 1; p.y = GRID_HEIGHT - 1; }
    else { p.x = 0; p.y = 0; } // Fallback

    players[id] = p;
    std::cout << "Player " << id << " added at " << p.x << "," << p.y << std::endl;
}

void Game::removePlayer(uint32_t id) {
    players.erase(id);
}

void Game::startGame() {
    if (running) return;
    reset(); // Reset map and positions
    running = true;
    std::cout << "Game Started!" << std::endl;
}

bool Game::isWalkable(int x, int y) const {
    if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT) return false;
    int idx = y * GRID_WIDTH + x;
    
    // Check static grid
    if (grid[idx] != TileState::Empty && grid[idx] != TileState::Explosion) {
        return false; 
    }

    // Check bombs (they are obstacles)
    for (const auto& b : bombs) {
        if (b.x == x && b.y == y) return false;
    }

    // Note: Players can walk through each other in this simple version, 
    // or we can make them solid. Let's make them walk through for simplicity/fewer stuck bugs.
    
    return true;
}

void Game::generateMap() {
    std::fill(grid.begin(), grid.end(), TileState::Empty);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 100);

    for (int y = 0; y < GRID_HEIGHT; ++y) {
        for (int x = 0; x < GRID_WIDTH; ++x) {
            // Indestructible walls (Odd x Odd)
            if (x % 2 != 0 && y % 2 != 0) {
                grid[y * GRID_WIDTH + x] = TileState::BlockedIndestructible;
            } else {
                // Destructible walls random chance
                // Avoid spawn zones
                bool safeZone = false;
                if ((x < 3 && y < 3) || // TL
                    (x > GRID_WIDTH - 4 && y < 3) || // TR
                    (x < 3 && y > GRID_HEIGHT - 4) || // BL
                    (x > GRID_WIDTH - 4 && y > GRID_HEIGHT - 4)) { // BR
                    safeZone = true;
                }

                if (!safeZone && dis(gen) < 60) { // 60% chance for wall
                     grid[y * GRID_WIDTH + x] = TileState::BlockedDestructible;
                }
            }
        }
    }
}

void Game::processInput(uint32_t playerId, int8_t dx, int8_t dy, bool placeBomb) {
    if (!running) return;
    if (players.find(playerId) == players.end()) return;
    
    Player& p = players[playerId];
    if (!p.alive) return;

    // Movement
    if (dx != 0 || dy != 0) {
        // Enforce single step
        int nx = p.x;
        int ny = p.y;
        
        if (dx > 0) nx++;
        else if (dx < 0) nx--;
        
        if (dy > 0) ny++;
        else if (dy < 0) ny--;
        
        // Very basic movement: only one axis at a time or diagonal?
        // Let's prioritize one axis if both pressed? Or allow diagonal? 
        // Grid based movement usually disallows diagonal if it clips corners.
        // Let's just try to move to target.
        
        // Actually, let's process X then Y for sliding? 
        // Or just atomic move.
        // The packet has dx, dy.
        
        // Simplification: Just check if target cell is walkable.
        // To prevent instant teleporting across map, input should be normalized by client, 
        // but here we trust dx/dy is -1, 0, 1.
        
        if (nx != p.x || ny != p.y) {
             if (isWalkable(nx, ny)) {
                 p.x = nx;
                 p.y = ny;
             } else if (nx != p.x && isWalkable(nx, p.y)) {
                 // Try X only
                 p.x = nx;
             } else if (ny != p.y && isWalkable(p.x, ny)) {
                 // Try Y only
                 p.y = ny;
             }
        }
    }

    // Bomb
    if (placeBomb) {
        spawnBomb(p.x, p.y, p.id);
    }
}

void Game::spawnBomb(int x, int y, uint32_t ownerId) {
    // Check if bomb already exists there
    for (const auto& b : bombs) {
        if (b.x == x && b.y == y) return;
    }
    
    // Check max bombs? (Infinite for now as per spec "no powerups" implying default limitation or none)
    // Spec says nothing about limits, but usually it's 1. 
    // Let's count user's bombs.
    int userBombs = 0;
    for(const auto& b : bombs) if(b.ownerId == ownerId) userBombs++;

    if (userBombs < 1) { // Default 1 bomb limit
        bombs.push_back({x, y, BOMB_TIMER, ownerId});
    }
}

void Game::update(float dt) {
    if (!running) return;

    // Update Bombs
    for (auto it = bombs.begin(); it != bombs.end(); ) {
        it->timer -= dt;
        if (it->timer <= 0) {
            triggerExplosion(*it);
            it = bombs.erase(it);
        } else {
            ++it;
        }
    }

    // Update Explosions
    for (auto it = explosions.begin(); it != explosions.end(); ) {
        it->timer -= dt;
        if (it->timer <= 0) {
            it = explosions.erase(it);
        } else {
            // Kill players in explosion
            for (auto& [id, p] : players) {
                if (p.alive && p.x == it->x && p.y == it->y) {
                    p.alive = false;
                    std::cout << "Player " << id << " died!" << std::endl;
                }
            }
            ++it;
        }
    }
    
    checkPlayerDeath();
}

void Game::triggerExplosion(const Bomb& bomb) {
    auto addExplosion = [&](int x, int y) {
        if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT) return false; // Stop ray
        int idx = y * GRID_WIDTH + x;
        
        if (grid[idx] == TileState::BlockedIndestructible) return false; // Stop ray
        
        explosions.push_back({x, y, EXPLOSION_TIMER});
        
        if (grid[idx] == TileState::BlockedDestructible) {
            grid[idx] = TileState::Empty; // Destroy wall
            return false; // Stop ray after destroying wall
        }
        return true; // Continue ray
    };

    // Center
    addExplosion(bomb.x, bomb.y);

    // 4 Directions
    int range = BOMB_RANGE;
    // Right
    for (int i = 1; i <= range; ++i) if (!addExplosion(bomb.x + i, bomb.y)) break;
    // Left
    for (int i = 1; i <= range; ++i) if (!addExplosion(bomb.x - i, bomb.y)) break;
    // Down
    for (int i = 1; i <= range; ++i) if (!addExplosion(bomb.x, bomb.y + i)) break;
    // Up
    for (int i = 1; i <= range; ++i) if (!addExplosion(bomb.x, bomb.y - i)) break;
}

void Game::checkPlayerDeath() {
    // Check winner
    int aliveCount = 0;
    uint32_t lastAlive = 0;
    for (const auto& [id, p] : players) {
        if (p.alive) {
            aliveCount++;
            lastAlive = id;
        }
    }

    if (aliveCount <= 1 && players.size() > 1) { // If >1 players started, last one wins
         winnerId = lastAlive;
         // running = false; // Keep it running or stop? "Winner is..." implies end.
         // Let's keep sending state for a bit?
         // Spec: "Winner is the last one standing."
    }
}

void Game::fillStatePacket(WorldStatePacket& packet) {
    packet.header.type = PacketType::WorldState;
    packet.gameRunning = running;
    packet.winnerId = winnerId;

    // 1. Copy base grid (Walls)
    for (int i = 0; i < GRID_SIZE; ++i) {
        packet.grid[i] = grid[i];
    }

    // 2. Overlay Bombs
    for (const auto& b : bombs) {
        int idx = b.y * GRID_WIDTH + b.x;
        if (idx >= 0 && idx < GRID_SIZE) packet.grid[idx] = TileState::Bomb;
    }

    // 3. Overlay Explosions
    for (const auto& e : explosions) {
        int idx = e.y * GRID_WIDTH + e.x;
        if (idx >= 0 && idx < GRID_SIZE) packet.grid[idx] = TileState::Explosion;
    }

    // 4. Overlay Players (on top of everything)
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
}
