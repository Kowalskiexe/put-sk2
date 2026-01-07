#include "Server.h"
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <arpa/inet.h>
#include <errno.h>
#include <cstring>

Server::Server(Game& game) : game(game), tcpListenSock(-1), udpSock(-1), nextPlayerId(1) {}

Server::~Server() {
    if (tcpListenSock != -1) close(tcpListenSock);
    if (udpSock != -1) close(udpSock);
    for (auto const& [sock, id] : tcpClients) {
        close(sock);
    }
}

// returns true on success
bool Server::init() {
    return setupTcp() && setupUdp();
}

bool Server::setupTcp() {
    tcpListenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpListenSock < 0) {
        perror("TCP socket failed");
        return false;
    }

    int opt = 1;
    setsockopt(tcpListenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    int flags = fcntl(tcpListenSock, F_GETFL, 0);
    fcntl(tcpListenSock, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in tcpAddr;
    tcpAddr.sin_family = AF_INET;
    tcpAddr.sin_addr.s_addr = INADDR_ANY;
    tcpAddr.sin_port = htons(SERVER_TCP_PORT);

    if (bind(tcpListenSock, (struct sockaddr*)&tcpAddr, sizeof(tcpAddr)) < 0) {
        perror("TCP bind failed");
        return false;
    }

    if (listen(tcpListenSock, MAX_PLAYERS) < 0) {
        perror("TCP listen failed");
        return false;
    }

    std::cout << "TCP Listening on port " << SERVER_TCP_PORT << std::endl;
    return true;
}

bool Server::setupUdp() {
    udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSock < 0) {
        perror("UDP socket failed");
        return false;
    }
    
    int opt = 1;
    setsockopt(udpSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    int flags = fcntl(udpSock, F_GETFL, 0);
    fcntl(udpSock, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in udpAddr;
    udpAddr.sin_family = AF_INET;
    udpAddr.sin_addr.s_addr = INADDR_ANY;
    udpAddr.sin_port = htons(SERVER_UDP_PORT);

    if (bind(udpSock, (struct sockaddr*)&udpAddr, sizeof(udpAddr)) < 0) {
        perror("UDP bind failed");
        return false;
    }

    std::cout << "UDP Listening on port " << SERVER_UDP_PORT << std::endl;
    return true;
}

void Server::update() {
    handleNewConnection();

    for (auto it = tcpClients.begin(); it != tcpClients.end();) {
        int sock = it->first;
        bool shouldRemove = handleTcpData(sock);
        if (shouldRemove) {
            uint8_t pid = it->second;
            close(sock);
            it = tcpClients.erase(it);
            std::cout << "Closed TCP connection for socket " << sock << " (player " << int(pid) << ")" << std::endl;
        } else {
            ++it;
        }
    }
    handleUdpData();

    broadcastState();
}

void Server::handleNewConnection() {
    sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);
    int clientSock = accept(tcpListenSock, (struct sockaddr*)&clientAddr, &len);
    
    if (clientSock >= 0) {
        int flags = fcntl(clientSock, F_GETFL, 0);
        fcntl(clientSock, F_SETFL, flags | O_NONBLOCK);

        // We accept the connection but don't assign a player ID yet.
        // ID 0 means "connected but not registered"
        tcpClients[clientSock] = 0;
        std::cout << "Client connected from " << inet_ntoa(clientAddr.sin_addr) << ". Waiting for Join packet..." << std::endl;
    }
}

// returns true if connections should be closed
bool Server::handleTcpData(int clientSock) {
    JoinPacket packet;
    int bytes = recv(clientSock, &packet, sizeof(packet), 0);
    
    if (bytes < 0) {
        // EAGAIN - Resource temporarily unavailable
        // EWOULDBLOCK - Operation would block
        if (errno == EAGAIN || errno == EWOULDBLOCK) return false;
        std::cerr << "Error on socket " << clientSock << ": " << strerror(errno) << std::endl;
        return true;
    }

    if (bytes == 0) {
        std::cout << "Peer closed TCP connection on socket " << clientSock << std::endl;
        return true;
    }

    if (tcpClients[clientSock] == 0 && nextPlayerId <= MAX_PLAYERS) {
        uint8_t pid = nextPlayerId++;
        tcpClients[clientSock] = pid;
        game.addPlayer(pid);
        
        sockaddr_in addr;
        socklen_t len = sizeof(addr);
        getpeername(clientSock, (struct sockaddr*)&addr, &len);
        
        udpClients[pid] = addr;
        udpClients[pid].sin_port = htons(packet.listeningPort);
        
        std::cout << "Player " << pid << " registered from " << inet_ntoa(addr.sin_addr) << ":" << ntohs(udpClients[pid].sin_port) << std::endl;

        ConnectResponsePacket resp;
        resp.type = PacketType::ConnectResponse;
        resp.playerId = pid;
        send(clientSock, &resp, sizeof(resp), 0);
        
        // no TCP communication after joining
        return true;
    }
    return false;
}

void Server::handleUdpData() {
    // must be at last as big as smallest possible input from clients
    uint8_t buffer[256];
    sockaddr_in senderAddr;
    socklen_t len = sizeof(senderAddr);

    // iterate over pending packets
    while (true) {
        int bytes = recvfrom(udpSock, buffer, sizeof(buffer), 0, (struct sockaddr*)&senderAddr, &len);
        if (bytes < 0) break; // EAGAIN/EWOULDBLOCK or error
        if (bytes == 0) continue;

        PacketType *ptype = (PacketType*)buffer;
        switch (*ptype) {
            case PacketType::Input: {
                InputPacket *input = (InputPacket*)buffer;
                uint8_t pid = input->playerId;
                game.processInput(pid, input->cmd);
                break;
            }
            case PacketType::StartGame: {
                StartPacket *sp = (StartPacket*)buffer;
                std::cout << "Received UDP START command from player " << int(sp->playerId) << std::endl;
                game.startGame();
                broadcastState();
                break;
            }
            case PacketType::Restart: {
                RestartPacket *rp = (RestartPacket*)buffer;
                std::cout << "Received UDP RESTART command from player " << int(rp->playerId) << std::endl;
                game.reset();
                game.startGame();
                broadcastState();
                break;
            }
            default:
                break;
        }
    }
}

void Server::broadcastState() {
    WorldStatePacket packet;
    game.fillStatePacket(packet);
    
    for (const auto& kv : udpClients) {
        sendto(udpSock, &packet, sizeof(packet), 0, (struct sockaddr*)&kv.second, sizeof(kv.second));
    }
}
