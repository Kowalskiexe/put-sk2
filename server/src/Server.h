#pragma once
#include <netinet/in.h>
#include <vector>
#include <map>
#include "Game.h"
#include "Protocol.h"

class Server {
public:
    Server(Game& game);
    ~Server();

    bool init();
    void update();

private:
    void handleNewConnection();
    bool handleTcpData(int clientSock);
    void handleUdpData();
    void broadcastState();

    bool setupTcp();
    bool setupUdp();

    Game& game;
    
    int tcpListenSock;
    int udpSock;
    
    // socket_fd -> playerId
    std::map<int, uint8_t> tcpClients;
    
    // playerId -> network destination
    std::map<uint8_t, sockaddr_in> udpClients;
    
    uint8_t nextPlayerId;
};
