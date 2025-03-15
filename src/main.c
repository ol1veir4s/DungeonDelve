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

#include "defs.h"
#include "structs.h"
#include "menu.h"
#include "entity/player.h"
#include "render/render.h"
#include "utils/utils.h"
#include "events/events.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 12345
#define MAX_CLIENTS 10
#define SERVER_ID 0
// Updated PlayerUpdate to include playerID and current_animation
typedef struct {
    int playerID;          // Unique identifier for each player
    float posX;            // X position
    float posY;            // Y position
    int current_animation; // Current animation state
} PlayerUpdate;

typedef struct {
    int numPlayers;
    PlayerUpdate players[MAX_CLIENTS + 1];
} GameState;

void FreeMemory(MenuData* MapInfo);

void FreeMemory(MenuData* MapInfo);
MapNode* GenerateNewMap(int map_size);
void initializeBasics();
void setupGame(MenuData* mapInfo, GameVariables* gameVar, MapNode** tileMap, Player** localPlayer, Camera2D* camera, Music* backgroundMusic);
void setupServer(int* serverSocket, int clientSockets[], Player* clientPlayers[], int* numClients);
void setupClient(int* sock, Player** serverPlayer, MapNode* tileMap, int* myID);
void updateGame(GameVariables* gameVar, Player* localPlayer, MapNode* tileMap, Camera2D* camera, MenuData* mapInfo);
void handleServerNetwork(int serverSocket, int clientSockets[], Player* clientPlayers[], int* numClients, Player* localPlayer, MapNode* tileMap);
void handleClientNetwork(int sock, Player* localPlayer, Player* allPlayers[], int myID, MapNode* tileMap);
void renderGame(MapNode* tileMap, Player* localPlayer, Player* allPlayers[], int myID, Camera2D camera, MenuData* mapInfo, int);
int handlePause();
void freeResources(MenuData* mapInfo, MapNode* tileMap, Player* localPlayer, Player* allPlayers[], int myID, int numClients, Music backgroundMusic, int serverSocket, int clientSockets[]);
void UpdateGameVariables(GameVariables* game_variables);

void renderServerScreen(int numClients) {
    BeginDrawing();
    ClearBackground(BLACK);
    DrawText(TextFormat("Waiting for connections.\nNumber of connected players:\n%d/%d", numClients, MAX_CLIENTS), (SCREEN_WIDTH/2)/2, (SCREEN_WIDTH/2)/2, 40, WHITE);
    EndDrawing();
}

int main(void) {
    while (true) {
        initializeBasics();

        MenuData* mapInfo = menu_screen();
        if (mapInfo == NULL) {
            CloseAudioDevice();
            CloseWindow();
            break;
        }
        LoadingWindow();

        // Game state variables
        GameVariables gameVar = { .update = UpdateGameVariables };
        MapNode* tileMap;
        Player* localPlayer;
        Camera2D camera;
        Music backgroundMusic;
        int serverSocket = -1;
        int clientSockets[MAX_CLIENTS] = {0};
        Player* clientPlayers[MAX_CLIENTS] = {0};
        int numClients = 0;
        Player* allPlayers[MAX_CLIENTS + 1] = {0}; // Array to hold all players on client side
        int myID = -1;

        setupGame(mapInfo, &gameVar, &tileMap, &localPlayer, &camera, &backgroundMusic);

        if (mapInfo->isServer){
            localPlayer->entity.isAlive = false;
            localPlayer->entity.health = -1;

        }

        if (mapInfo->isServer) {
            myID = SERVER_ID; // Server has ID 0
            allPlayers[SERVER_ID] = localPlayer; // Server's local player at index 0
            setupServer(&serverSocket, clientSockets, clientPlayers, &numClients);
        } else if (mapInfo->isClient) {
            int sock;
            Player* serverPlayer;
            setupClient(&sock, &serverPlayer, tileMap, &myID);
            mapInfo->sock = sock;
            allPlayers[0] = serverPlayer; // Server player at index 0
        }

        // Game loop
        while (!WindowShouldClose()) {
            UpdateMusicStream(backgroundMusic);
            updateGame(&gameVar, localPlayer, tileMap, &camera, mapInfo);
            if (mapInfo->isServer) {
                handleServerNetwork(serverSocket, clientSockets, clientPlayers, &numClients, localPlayer, tileMap);
            } else if (mapInfo->isClient) {
                handleClientNetwork(mapInfo->sock, localPlayer, allPlayers, myID, tileMap);
            }
            
            if (!mapInfo->isServer) {
                renderGame(tileMap, localPlayer, allPlayers, myID, camera, mapInfo, numClients);                
            } else {
                renderServerScreen(numClients);
            }

            int pauseAction = handlePause();
            if (pauseAction == 1 || pauseAction == 2) {
                freeResources(mapInfo, tileMap, localPlayer, allPlayers, myID, numClients, backgroundMusic, serverSocket, clientSockets);
                if (pauseAction == 2) {
                    return 0;
                }
                break;
            }
        }

        freeResources(mapInfo, tileMap, localPlayer, allPlayers, myID, numClients, backgroundMusic, serverSocket, clientSockets);
    }
    return 0;
}

void initializeBasics() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_TITLE);
    SetTargetFPS(TARGET_FPS);
    InitAudioDevice();
}

void setupGame(MenuData* mapInfo, GameVariables* gameVar, MapNode** tileMap, Player** localPlayer, Camera2D* camera, Music* backgroundMusic) {
    *tileMap = mapInfo->TileMapGraph;
    *localPlayer = InitPlayer(*tileMap);
    *camera = InitPlayerCamera(*localPlayer);
    InitRandomSeed(NULL);
    *backgroundMusic = LoadMusicStream(BACKGROUND_MUSIC);
    PlayMusicStream(*backgroundMusic);


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
    struct sockaddr_in serverAddr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(PORT) };
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

void setupClient(int* sock, Player** serverPlayer, MapNode* tileMap, int* myID) {
    *sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*sock < 0) {
        perror("Erro ao criar o socket do cliente");
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in servAddr = { .sin_family = AF_INET, .sin_port = htons(PORT) };
    if (inet_pton(AF_INET, "127.0.0.1", &servAddr.sin_addr) <= 0) {
        printf("Endereço inválido\n");
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

void updateGame(GameVariables* gameVar, Player* localPlayer, MapNode* tileMap, Camera2D* camera, MenuData* mapInfo) {
    gameVar->update(gameVar);
    uint8_t* collisionType = localPlayer->update(localPlayer, gameVar->delta_time, gameVar->current_frame, tileMap);
    localPlayer->updateCamera(camera, localPlayer, gameVar->delta_time);
    tileMap->updateEnemies(tileMap, gameVar->delta_time, gameVar->current_frame, localPlayer);
    if (*collisionType == STAIR || *collisionType == HOLE) {
        StartPlayerOnNewMap(localPlayer, *collisionType, mapInfo, tileMap);
        *collisionType = NO_COLLISION;
    }
}

void handleServerNetwork(int serverSocket, int clientSockets[], Player* clientPlayers[], int* numClients, Player* localPlayer, MapNode* tileMap) {
    static int nextClientID = 1; // Server is 0, clients start from 1
    struct sockaddr_in clientAddr;
    socklen_t addrLen = sizeof(clientAddr);
    int newSock = accept(serverSocket, (struct sockaddr*)&clientAddr, &addrLen);
    if (newSock > 0 && *numClients < MAX_CLIENTS) {
        fcntl(newSock, F_SETFL, O_NONBLOCK);
        clientSockets[*numClients] = newSock;
        clientPlayers[*numClients] = InitPlayer(tileMap);
        int clientID = nextClientID++;
        send(newSock, &clientID, sizeof(clientID), 0);
        (*numClients)++;
        printf("Novo cliente conectado. ID: %d, Total: %d\n", clientID, *numClients);
    }
    for (int i = 0; i < *numClients; i++) {
        PlayerUpdate clientUpdate;
        int bytes = recv(clientSockets[i], &clientUpdate, sizeof(clientUpdate), 0);
        if (bytes > 0) {
            clientPlayers[i]->entity.position.x = clientUpdate.posX;
            clientPlayers[i]->entity.position.y = clientUpdate.posY;
            clientPlayers[i]->current_animation = clientUpdate.current_animation;
        }
    }
    GameState state = { .numPlayers = 1 + *numClients };
    state.players[0].playerID = 0;
    state.players[0].posX = localPlayer->entity.position.x;
    state.players[0].posY = localPlayer->entity.position.y;
    state.players[0].current_animation = localPlayer->current_animation;
    for (int i = 0; i < *numClients; i++) {
        state.players[i + 1].playerID = i + 1;
        state.players[i + 1].posX = clientPlayers[i]->entity.position.x;
        state.players[i + 1].posY = clientPlayers[i]->entity.position.y;
        state.players[i + 1].current_animation = clientPlayers[i]->current_animation;
    }
    for (int i = 0; i < *numClients; i++) {
        if (send(clientSockets[i], &state, sizeof(state), 0) < 0) {
            perror("Erro ao enviar GameState para um cliente");
        }
    }
}

void handleClientNetwork(int sock, Player* localPlayer, Player* allPlayers[], int myID, MapNode* tileMap) {
    PlayerUpdate update = {
        .playerID = myID,
        .posX = localPlayer->entity.position.x,
        .posY = localPlayer->entity.position.y,
        .current_animation = localPlayer->current_animation
    };
    if (send(sock, &update, sizeof(update), 0) < 0) {
        perror("Erro ao enviar dados do cliente");
    }
    GameState state;
    int bytes = recv(sock, &state, sizeof(state), 0);
    if (bytes > 0) {
        for (int j = 0; j < state.numPlayers; j++) {
            int pid = state.players[j].playerID;
            if (pid == myID) continue; // Skip updating local player
            if (allPlayers[pid] == NULL) {
                allPlayers[pid] = InitPlayer(tileMap);
            }
            allPlayers[pid]->entity.position.x = state.players[j].posX;
            allPlayers[pid]->entity.position.y = state.players[j].posY;
            allPlayers[pid]->current_animation = state.players[j].current_animation;
        }
    }
}


void renderGame(MapNode* tileMap, Player* localPlayer, Player* allPlayers[], int myID, Camera2D camera, MenuData* mapInfo, int num_clients) {
    BeginDrawing();
    ClearBackground(BLACK);
    BeginMode2D(camera);
    tileMap->drawMap(tileMap, camera);
    tileMap->drawEnemies(tileMap);
    int i = 0;
    if (mapInfo->isServer) {
        for (i = 0; i < MAX_CLIENTS; i++) {
            if (allPlayers[i + 1] != NULL) { // Client IDs start from 1
                allPlayers[i + 1]->draw(allPlayers[i + 1]);
            } else {
                break;
            }
        }
    } else { //pid=1 not render server
        for (int pid = 1; pid <= MAX_CLIENTS; pid++) {
            if (pid != myID && allPlayers[pid] != NULL) {
                allPlayers[pid]->draw(allPlayers[pid]);
            }
        }
        localPlayer->draw(localPlayer);
    }

    DrawFog(camera, FOG_RADIUS);
    EndMode2D();
    ShowControls();
    EndDrawing();
}


int handlePause() {
    switch (PauseEvent()) {
        case 1: return 1; // Back to menu
        case 2: return 2; // Exit game
        default: return 0; // Continue
    }
}

void freeResources(MenuData* mapInfo, MapNode* tileMap, Player* localPlayer, Player* allPlayers[], int myID, int numClients, Music backgroundMusic, int serverSocket, int clientSockets[]) {
    UnloadMusicStream(backgroundMusic);
    CloseAudioDevice();
    CloseWindow();
    free(tileMap);
    free(mapInfo);
    free(localPlayer);
    if (mapInfo->isServer) {
        for (int i = 0; i < numClients; i++) {
            free(allPlayers[i + 1]); // Free client players (IDs 1+)
            close(clientSockets[i]);
        }
        close(serverSocket);
    } else if (mapInfo->isClient) {
        close(mapInfo->sock);
        for (int pid = 0; pid <= MAX_CLIENTS; pid++) {
            if (pid != myID && allPlayers[pid] != NULL) {
                free(allPlayers[pid]);
            }
        }
    }
}

void UpdateGameVariables(GameVariables* game_variables) {
    game_variables->delta_time = GetFrameTime();
    game_variables->frame_counter++;
    if (game_variables->frame_counter >= (TARGET_FPS / GLOBAL_FRAME_SPEED)) {
        game_variables->frame_counter = 0;
    }
    if (game_variables->frame_counter == 0) {
        if (game_variables->current_frame >= 3) {
            game_variables->current_frame = 0;
        } else {
            game_variables->current_frame++;
        }
    }
}