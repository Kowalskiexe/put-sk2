#include <iostream>
#include <thread>
#include <chrono>
#include "Server.h"
#include "Game.h"

constexpr int TARGET_PFS = 30;

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
    
    const std::chrono::milliseconds TARGET_FRAME_TIME(1000 / TARGET_PFS);

    while (true) {
        auto currentTime = clock::now();
        std::chrono::duration<float> delta = currentTime - lastTime;
        lastTime = currentTime;

        server.update();
        game.update(delta.count());

        auto frameEndTime = clock::now();
        auto frameDuration = frameEndTime - currentTime;
        
        if (frameDuration < TARGET_FRAME_TIME) {
            std::this_thread::sleep_for(TARGET_FRAME_TIME - frameDuration);
        }
    }

    return 0;
}
