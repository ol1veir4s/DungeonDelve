// This file is part of DungeonDelveC.
// Copyright (C) 2024 - 2025 Guilherme Oliveira Santos
//
// DungeonDelveC is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "network.h"
#include "stdlib.h"
#include "entity/player.h"

char* GetLocalIPAddress(int serverSocket) {
    static char ip[16];
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    socklen_t addrlen;  // added temporary variable
    
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        strcpy(ip, "Unknown");
        return ip;
    }
    
    // Look for the first non-loopback IPv4 address
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
            
        family = ifa->ifa_addr->sa_family;
        
        if (family == AF_INET) {
            addrlen = sizeof(struct sockaddr_in);  // set length before call
            s = getsockname(serverSocket, (struct sockaddr*)ifa->ifa_addr, &addrlen);
            if (inet_ntop(AF_INET, &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr, 
                    ip, sizeof(ip)) != NULL) {
                if (strcmp(ip, "127.0.0.1") != 0) {
                    break;
                }
            }
        }
    }
    
    freeifaddrs(ifaddr);
    return ip;
}

void setupServer(int* serverSocket, int clientSockets[], Player* clientPlayers[], int* numClients) {
    *serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (*serverSocket < 0) {
        perror("Erro ao criar o socket do servidor");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    if (setsockopt(*serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("Erro no setsockopt");
        exit(EXIT_FAILURE);
    }
    // This is already correct in your code
    struct sockaddr_in serverAddr = { 
        .sin_family = AF_INET, 
        .sin_addr.s_addr = INADDR_ANY,  // This binds to all interfaces
        .sin_port = htons(PORT) 
    };
    if (bind(*serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Bind do servidor falhou");
        exit(EXIT_FAILURE);
    }
    if (listen(*serverSocket, MAX_CLIENTS) < 0) {
        perror("Erro no listen do servidor");
        exit(EXIT_FAILURE);
    }
    fcntl(*serverSocket, F_SETFL, O_NONBLOCK);
    printf("Servidor iniciado. Aguardando conexões na porta %d...\n", PORT);
    *numClients = 0;
    memset(clientSockets, 0, sizeof(int) * MAX_CLIENTS);
    memset(clientPlayers, 0, sizeof(Player*) * MAX_CLIENTS);
}

void renderServerScreen(int numClients, int serverSocket) {
    char* localIP = GetLocalIPAddress(serverSocket);
    BeginDrawing();
    ClearBackground(BLACK);
    DrawText(TextFormat("Server IP: %s\nWaiting for connections.\nNumber of connected players:\n%d/%d", 
                        localIP, numClients, MAX_CLIENTS), 
            (SCREEN_WIDTH/2)/2, (SCREEN_WIDTH/2)/2, 40, WHITE);
    EndDrawing();
}

void setupClient(int* sock, Player** serverPlayer, MapNode* tileMap, int* myID, const char* serverIP) {
    *sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*sock < 0) {
        perror("Erro ao criar o socket do cliente");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in servAddr = { .sin_family = AF_INET, .sin_port = htons(PORT) };
    if (inet_pton(AF_INET, serverIP, &servAddr.sin_addr) <= 0) {
        printf("Endereço inválido: %s\n", serverIP);
        exit(EXIT_FAILURE);
    }
    if (connect(*sock, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
        perror("Falha na conexão do cliente");
        exit(EXIT_FAILURE);
    }
    fcntl(*sock, F_SETFL, O_NONBLOCK);
    printf("Cliente conectado ao servidor!\n");
    *serverPlayer = InitPlayer(tileMap);
    int bytes = recv(*sock, myID, sizeof(*myID), 0);
    if (bytes <= 0) {
        perror("Erro ao receber ID do servidor");
        exit(EXIT_FAILURE);
    }
    printf("Recebido ID: %d\n", *myID);
}

void handleServerNetwork(int serverSocket, int clientSockets[], Player* clientPlayers[], int* numClients, Player* localPlayer, MapNode* tileMap) {
    static int nextClientID = 1; // Server is 0, clients start from 1
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    int newSock = accept(serverSocket, (struct sockaddr*)&clientAddr, &addrLen);
    
    // Handle new client connection
    if (newSock > 0 && *numClients < MAX_CLIENTS) {
        fcntl(newSock, F_SETFL, O_NONBLOCK);
        clientSockets[*numClients] = newSock;
        clientPlayers[*numClients] = InitPlayer(tileMap);
        int clientID = nextClientID++;
        
        // Send ID to new client
        send(newSock, &clientID, sizeof(clientID), 0);
        
        // Broadcast new player notification to all connected clients
        ServerNotification notification = {
            .messageType = 1,
            .newPlayerID = clientID
        };
        
        for (int i = 0; i < *numClients; i++) {
            if (send(clientSockets[i], &notification, sizeof(notification), 0) < 0) {
                perror("Erro ao enviar notificação de novo jogador");
            }
        }
        
        (*numClients)++;
        printf("Novo cliente conectado. ID: %d, Total: %d\n", clientID, *numClients);
    }
    
    // Receive updates from all clients
    for (int i = 0; i < *numClients; i++) {
        PlayerUpdate clientUpdate;
        int bytes = recv(clientSockets[i], &clientUpdate, sizeof(clientUpdate), 0);
        if (bytes > 0) {
            // Update all client player properties
            clientPlayers[i]->entity.position.x = clientUpdate.posX;
            clientPlayers[i]->entity.position.y = clientUpdate.posY;
            clientPlayers[i]->current_animation = clientUpdate.current_animation;
            clientPlayers[i]->entity.health = clientUpdate.health;
            clientPlayers[i]->entity.stamina = clientUpdate.stamina;
            clientPlayers[i]->entity.mana = clientUpdate.mana;
            clientPlayers[i]->entity.isAlive = clientUpdate.isAlive;
            clientPlayers[i]->entity.isAttacking = clientUpdate.isAttacking;
            clientPlayers[i]->entity.isMoving = clientUpdate.isMoving;
        }
    }
    
    // Create GameState with all player information
    GameState state = { .numPlayers = 1 + *numClients };
    
    // Add server player
    state.players[0].playerID = 0;
    state.players[0].posX = localPlayer->entity.position.x;
    state.players[0].posY = localPlayer->entity.position.y;
    state.players[0].current_animation = localPlayer->current_animation;
    state.players[0].health = localPlayer->entity.health;
    state.players[0].stamina = localPlayer->entity.stamina;
    state.players[0].mana = localPlayer->entity.mana;
    state.players[0].isAlive = localPlayer->entity.isAlive;
    state.players[0].isAttacking = localPlayer->entity.isAttacking;
    state.players[0].isMoving = localPlayer->entity.isMoving;
    
    // Add client players
    for (int i = 0; i < *numClients; i++) {
        state.players[i + 1].playerID = i + 1;
        state.players[i + 1].posX = clientPlayers[i]->entity.position.x;
        state.players[i + 1].posY = clientPlayers[i]->entity.position.y;
        state.players[i + 1].current_animation = clientPlayers[i]->current_animation;
        state.players[i + 1].health = clientPlayers[i]->entity.health;
        state.players[i + 1].stamina = clientPlayers[i]->entity.stamina;
        state.players[i + 1].mana = clientPlayers[i]->entity.mana;
        state.players[i + 1].isAlive = clientPlayers[i]->entity.isAlive;
        state.players[i + 1].isAttacking = clientPlayers[i]->entity.isAttacking;
        state.players[i + 1].isMoving = clientPlayers[i]->entity.isMoving;
    }
    
    // Prepare server notification for game state update
    ServerNotification notification = {
        .messageType = 0, // Regular game state update
        .newPlayerID = -1 // Not applicable for regular updates
    };
    
    // Send the notification and game state to all clients
    for (int i = 0; i < *numClients; i++) {
        // First send notification type
        if (send(clientSockets[i], &notification, sizeof(notification), 0) < 0) {
            perror("Erro ao enviar tipo de notificação");
            continue;
        }
        
        // Then send game state
        if (send(clientSockets[i], &state, sizeof(state), 0) < 0) {
            perror("Erro ao enviar GameState para um cliente");
        }
    }
}


void handleClientNetwork(int sock, Player* localPlayer, Player* allPlayers[], int myID, MapNode* tileMap) {
    // Send local player update to server
    PlayerUpdate update = {
        .playerID = myID,
        .posX = localPlayer->entity.position.x,
        .posY = localPlayer->entity.position.y,
        .current_animation = localPlayer->current_animation,
        .health = localPlayer->entity.health,
        .stamina = localPlayer->entity.stamina,
        .mana = localPlayer->entity.mana,
        .isAlive = localPlayer->entity.isAlive,
        .isAttacking = localPlayer->entity.isAttacking,
        .isMoving = localPlayer->entity.isMoving
    };
    
    if (send(sock, &update, sizeof(update), 0) < 0) {
        perror("Erro ao enviar dados do cliente");
    }
    
    // Receive server notification first
    ServerNotification notification;
    int notifBytes = recv(sock, &notification, sizeof(notification), 0);
    
    if (notifBytes > 0) {
        if (notification.messageType == 1) {
            // New player joined notification
            int newPlayerID = notification.newPlayerID;
            printf("Novo jogador conectado! ID: %d\n", newPlayerID);
            
            // Initialize the new player
            if (allPlayers[newPlayerID] == NULL) {
                allPlayers[newPlayerID] = InitPlayer(tileMap);
                // Print confirmation of successful initialization
                printf("Player %d initialized successfully\n", newPlayerID);
            }
            
            // Wait for the next game state update
            return;
        }
        
        // Regular game state update
        GameState state;
        int bytes = recv(sock, &state, sizeof(state), 0);
        
        if (bytes > 0) {
            for (int j = 0; j < state.numPlayers; j++) {
                int pid = state.players[j].playerID;
                if (pid == myID) continue; // Skip updating local player
                
                // Ensure the player is initialized before updating
                if (allPlayers[pid] == NULL) {
                    allPlayers[pid] = InitPlayer(tileMap);
                    printf("Player %d initialized during state update\n", pid);
                }
                
                // Update all player properties
                allPlayers[pid]->entity.position.x = state.players[j].posX;
                allPlayers[pid]->entity.position.y = state.players[j].posY;
                allPlayers[pid]->current_animation = state.players[j].current_animation;
                allPlayers[pid]->entity.health = state.players[j].health;
                allPlayers[pid]->entity.stamina = state.players[j].stamina;
                allPlayers[pid]->entity.mana = state.players[j].mana;
                allPlayers[pid]->entity.isAlive = state.players[j].isAlive;
                allPlayers[pid]->entity.isAttacking = state.players[j].isAttacking;
                allPlayers[pid]->entity.isMoving = state.players[j].isMoving;
            }
        }
    }
}

