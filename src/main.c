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
#include "network.h"



void initializeBasics();
void setupGame(MenuData* mapInfo, GameVariables* gameVar, MapNode** tileMap, Player** localPlayer, Camera2D* camera, Music* backgroundMusic);
void updateGame(GameVariables* gameVar, Player* localPlayer, MapNode* tileMap, Camera2D* camera, MenuData* mapInfo);



void renderGame(MapNode* tileMap, Player* localPlayer, Player* allPlayers[], int myID, Camera2D camera, MenuData* mapInfo, int);
int handlePause();
void freeResources(MenuData* mapInfo, MapNode* tileMap, Player* localPlayer, Player* allPlayers[], int myID, int numClients, Music backgroundMusic, int serverSocket, int clientSockets[]);
void UpdateGameVariables(GameVariables* game_variables);

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
            setupClient(&sock, &serverPlayer, tileMap, &myID,  mapInfo->serverIP);
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
                renderServerScreen(numClients, mapInfo->sock);
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