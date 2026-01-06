#pragma once
#include <netinet/in.h>
#include <vector>
#include <map>
#include "Game.h"
#include "Protocol.h"

// Basic wrapper for POSIX sockets
class Server {
public:
    Server(Game& game_inst);
    ~Server();

    bool init();
    void update(); // Check sockets

private:
    void handleNewConnection();
    void handleTcpData(int clientSock);
    void handleUdpData();
    void broadcastState();

    Game& game;
    
    int tcpListenSock;
    int udpSock;
    
    // We keep TCP sockets open to listen for "Start Game".
    // Map socket_fd -> playerId
    std::map<int, uint32_t> tcpClients;
    
    // Map playerId -> UDP address (learned from incoming UDP packets)
    struct sockaddr_in udpClients[MAX_PLAYERS + 1]; 
    bool udpClientKnown[MAX_PLAYERS + 1];
    
    uint32_t nextPlayerId;
};
