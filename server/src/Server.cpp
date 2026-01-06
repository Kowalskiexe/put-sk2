#include "Server.h"
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <arpa/inet.h>

Server::Server(Game& game_inst) : game(game_inst), tcpListenSock(-1), udpSock(-1), nextPlayerId(1) {
    std::memset(udpClients, 0, sizeof(udpClients));
    std::memset(udpClientKnown, 0, sizeof(udpClientKnown));
}

Server::~Server() {
    if (tcpListenSock != -1) close(tcpListenSock);
    if (udpSock != -1) close(udpSock);
    for (auto const& [sock, id] : tcpClients) {
        close(sock);
    }
}

bool Server::init() {
    // 1. Setup TCP
    tcpListenSock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpListenSock < 0) {
        perror("TCP socket failed");
        return false;
    }

    int opt = 1;
    setsockopt(tcpListenSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Non-blocking TCP listener
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

    // 2. Setup UDP
    udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSock < 0) {
        perror("UDP socket failed");
        return false;
    }
    
    setsockopt(udpSock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Non-blocking UDP
    flags = fcntl(udpSock, F_GETFL, 0);
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
    // 1. Accept new TCP connections
    handleNewConnection();

    // 2. Read TCP data (Start Game command)
    // We iterate a copy of keys or handle safely because we might remove closed sockets?
    // Actually, simple vector of fds or map is fine for this scale.
    for (auto it = tcpClients.begin(); it != tcpClients.end();) {
        handleTcpData(it->first);
        // If socket closed inside handle, we need to be careful. 
        // For simplicity, handleTcpData handles mostly reads. 
        // We'll check validity or use a separate list if it gets complex. 
        // Here, we just assume they stay open or we detect close later.
        ++it;
    }

    // 3. Read UDP inputs
    handleUdpData();

    // 4. Send World State
    broadcastState();
}

void Server::handleNewConnection() {
    sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);
    int clientSock = accept(tcpListenSock, (struct sockaddr*)&clientAddr, &len);
    
    if (clientSock >= 0) {
        // Make non-blocking
        int flags = fcntl(clientSock, F_GETFL, 0);
        fcntl(clientSock, F_SETFL, flags | O_NONBLOCK);

        if (nextPlayerId <= MAX_PLAYERS) {
            uint32_t pid = nextPlayerId++;
            tcpClients[clientSock] = pid;
            game.addPlayer(pid);
            
            std::cout << "Player " << pid << " connected from " << inet_ntoa(clientAddr.sin_addr) << std::endl;

            // Send ID assignment
            ConnectResponsePacket resp;
            resp.header.type = PacketType::ConnectResponse;
            resp.playerId = pid;
            send(clientSock, &resp, sizeof(resp), 0);
        } else {
            std::cout << "Connection rejected: Server full" << std::endl;
            close(clientSock);
        }
    }
}

void Server::handleTcpData(int clientSock) {
    PacketHeader header;
    int bytes = recv(clientSock, &header, sizeof(header), MSG_PEEK); // Peek first
    
    if (bytes > 0) {
        if (header.type == PacketType::StartGame) {
            // Consume
            recv(clientSock, &header, sizeof(header), 0);
            std::cout << "Received START_GAME command" << std::endl;
            game.startGame();
        } else {
            // Consume unknown
            char trash[128];
            recv(clientSock, trash, sizeof(trash), 0);
        }
    } else if (bytes == 0) {
        // Disconnected?
        // We handle disconnect logic if needed, but for now we keep the slot? 
        // Or remove player. Simple game: player disconnects -> restart server mostly.
    }
}

void Server::handleUdpData() {
    InputPacket packet;
    sockaddr_in senderAddr;
    socklen_t len = sizeof(senderAddr);
    
    // Process all pending UDP packets
    while (true) {
        int bytes = recvfrom(udpSock, &packet, sizeof(packet), 0, (struct sockaddr*)&senderAddr, &len);
        if (bytes < 0) break; // No more data (EAGAIN/EWOULDBLOCK) or error
        
        if (bytes >= (int)sizeof(InputPacket) && packet.header.type == PacketType::Input) {
            uint32_t pid = packet.playerId;
            if (pid >= 1 && pid <= MAX_PLAYERS) {
                // Update known UDP address for this player
                udpClients[pid] = senderAddr;
                udpClientKnown[pid] = true;
                
                // Process Input
                game.processInput(pid, packet.dx, packet.dy, packet.placeBomb);
            }
        }
    }
}

void Server::broadcastState() {
    WorldStatePacket packet;
    game.fillStatePacket(packet);
    
    for (int i = 1; i <= MAX_PLAYERS; ++i) {
        if (udpClientKnown[i]) {
            sendto(udpSock, &packet, sizeof(packet), 0, (struct sockaddr*)&udpClients[i], sizeof(udpClients[i]));
        }
    }
}
