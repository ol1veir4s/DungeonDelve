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

#include "structs.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

#define MAX_CLIENTS 10
#define SERVER_ID 0
#define PORT 12345

// Updated PlayerUpdate to include playerID and current_animation
typedef struct {
    int playerID;           // Unique identifier for each player
    float posX;             // X position
    float posY;             // Y position
    int current_animation;  // Current animation state
    float health;           // Player health
    float stamina;          // Player stamina
    float mana;             // Player mana
    bool isAlive;           // Player alive status
    bool isAttacking;       // Player attacking status
    bool isMoving;          // Player moving status
} PlayerUpdate;

// Add a new struct for player connection notifications
typedef struct {
    int messageType;        // 0: GameState update, 1: New player joined
    int newPlayerID;        // ID of the newly joined player
} ServerNotification;

// Update the GameState struct
typedef struct {
    int numPlayers;
    PlayerUpdate players[MAX_CLIENTS + 1];
} GameState;

void handleServerNetwork(int serverSocket, int clientSockets[], Player* clientPlayers[], int* numClients, Player* localPlayer, MapNode* tileMap);
void setupServer(int* serverSocket, int clientSockets[], Player* clientPlayers[], int* numClients);
void renderServerScreen(int numClients, int serverSocket);

void handleClientNetwork(int sock, Player* localPlayer, Player* allPlayers[], int myID, MapNode* tileMap);
void setupClient(int* sock, Player** serverPlayer, MapNode* tileMap, int* myID, const char* );