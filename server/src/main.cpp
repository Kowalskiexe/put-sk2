#include <iostream>
#include <thread>
#include <chrono>
#include "Server.h"
#include "Game.h"

int main() {
    std::cout << "Starting Bomberman Server..." << std::endl;

    Game game;
    Server server(game);

    if (!server.init()) {
        std::cerr << "Failed to initialize server" << std::endl;
        return 1;
    }

    std::cout << "Server ready. Waiting for players..." << std::endl;

    using clock = std::chrono::high_resolution_clock;
    auto lastTime = clock::now();
    
    // Target 30 Tick Per Second
    const std::chrono::milliseconds TARGET_FRAME_TIME(33);

    while (true) {
        auto currentTime = clock::now();
        std::chrono::duration<float> dt = currentTime - lastTime;
        lastTime = currentTime;

        // 1. Network Updates (Receive Inputs, Handle Connections)
        server.update();

        // 2. Game Logic (Movement, Bombs, Explosions)
        game.update(dt.count());

        // 3. Sleep to maintain frame rate
        auto frameEndTime = clock::now();
        auto frameDuration = frameEndTime - currentTime;
        
        if (frameDuration < TARGET_FRAME_TIME) {
            std::this_thread::sleep_for(TARGET_FRAME_TIME - frameDuration);
        }
    }

    return 0;
}
