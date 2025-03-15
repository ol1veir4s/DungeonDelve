// This file is part of DungeonDelveC.
// Copyright (C) 2024 - 2025 Guilherme Oliveira Santos

// DungeonDelveC is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "events.h"

static const char* pauseOptions[] = {
    "Resume", 
    "Back to Menu",
    "Exit"
};

int PauseEvent(void){ 
    bool show = IsKeyPressed(KEY_ESCAPE) ? true : false;
    if (!show) return 0; // 0: Game is not paused 

    int selectedOption = 0;
    float centerX = GetScreenWidth() / 2;   
    float centerY = GetScreenHeight() / 2;
    Sound changeOptionSound = LoadSound(CHANGE_OPTION_SOUND);
    Sound selectOptionSound = LoadSound(SELECT_OPTION_SOUND);
    SetSoundVolume(changeOptionSound, 0.5f);
    SetSoundVolume(selectOptionSound, 0.5f);

    PlaySound(selectOptionSound);
    while (show) {
        BeginDrawing();

        if (IsKeyPressed(KEY_DOWN)) {
            PlaySound(changeOptionSound);
            selectedOption = (selectedOption + 1) % 3;
        } 
        
        else if (IsKeyPressed(KEY_UP)) {
            PlaySound(changeOptionSound);
            selectedOption = (selectedOption - 1 + 3) % 3;
        }

        else if (IsKeyPressed(KEY_ENTER)) {
            PlaySound(selectOptionSound);

            switch (selectedOption) {
                case 0:
                    return 0; //0: Resume/Game is not paused
                case 1:
                    return 1; //1: Back to menu
                case 2:
                    return 2; //2: Exit
            }
        }   for (int i = 0; i < 3; i++) {
                if (i == selectedOption) 
                    DrawText(pauseOptions[i], centerX - MeasureText(pauseOptions[i], 20) / 2, centerY - 50 + 50 * i, 20, RED);
                else 
                    DrawText(pauseOptions[i], centerX - MeasureText(pauseOptions[i], 20) / 2, centerY - 50 + 50 * i, 20, WHITE);
        }
    
    EndDrawing();
    }

    return -1; // If gets here, something went wrong

}

void LoadingWindow(void) {
    time_t start_time = time(NULL);
    int max_loading_seconds = 2;

    while (!WindowShouldClose() && time(NULL) - start_time < max_loading_seconds) {
        BeginDrawing();
            ClearBackground(BLACK);
            DrawText("Loading...", SCREEN_WIDTH / 2 - 50, SCREEN_HEIGHT / 2, 20, WHITE);
        EndDrawing();
    }
}

void handleDifficultySelection(MenuData* menuData, MenuSounds* menuSounds) {
    PlaySound(menuSounds->selectOptionSound);
    menuData->difficulty = menuData->selectedOption + 1;
    menuData->currentState = MENU_WORLD_SETTINGS;
    menuData->selectedOption = 0;  // Reset selection
}

void handleWorldSettingsSelection(MenuData* menuData, MenuSounds* menuSounds) {
    PlaySound(menuSounds->selectOptionSound);
    switch (menuData->selectedOption) {
        case 0:  // Map Size
            menuData->MapSize = 50 + ((menuData->MapSize - 50 + 50) % (menuData->MapMaxSize - 50 + 50));
            if (menuData->MapSize < 50) menuData->MapSize = 50;
            break;
        case 1:  // Map Seed
            menuData->MapSeed = rand() % menuData->MaxSeed;
            break;
        case 2:  // Back
            menuData->currentState = MENU_DIFFICULTY;
            menuData->selectedOption = 0;
            break;
        case 3:  // Start Game
            startSinglePlayer();
            break;
    }
}

void handleMultiplayerSelection(MenuData* menuData, MenuSounds* menuSounds) {
    PlaySound(menuSounds->selectOptionSound);
    switch (menuData->selectedOption) {
        case 0:  // HOST A SERVER
            menuData->currentState = MENU_HOSTING;
            menuData->selectedOption = 0;
            break;
        case 1:  // CONNECT
            menuData->currentState = MENU_CONNECTING;
            menuData->selectedOption = 0;
            break;
        case 2:  // RETURN
            menuData->currentState = MENU_MAIN;
            menuData->selectedOption = 0;
            break;
        case 3:  // EXIT
            usleep(500000);
            CloseWindow();
            break;
    }
}

void StartPlayerOnNewMap(Player* player, int collisionType, MenuData* MapInfo, MapNode* TileMap){
    player->entity.position = player->entity.spawn_point; // Avoid collision with the new map

    GenerateMap(TileMap);
    LoadingWindow();
    switch (collisionType) {
        case STAIR:
            MapInfo->map_level++;
            break;
        case HOLE:
            MapInfo->map_level = MapInfo->map_level == 0 ? 2 : MapInfo->map_level << 1;
            break;
    }
}


void handleMainMenuSelection(MenuData* menuData, MenuSounds* menuSounds) {
    PlaySound(menuSounds->selectOptionSound);
    switch (menuData->selectedOption) {
        case OPTION_SINGLEPLAYER:
            menuData->currentState = MENU_DIFFICULTY;
            menuData->selectedOption = 0;  // Reset selection
            break;
        case OPTION_MULTIPLAYER:
            menuData->currentState = MENU_MULTIPLAYER;
            menuData->selectedOption = 0;  // Reset selection
            break;
        case OPTION_OPTIONS:
            // Placeholder for future options menu
            break;
        case OPTION_EXIT:
            usleep(500000);
            CloseWindow();
            break;
    }
}

void UpdateOptions(MenuData* menuData, MenuSounds* menuSounds) {
    if (IsKeyPressed(KEY_DOWN)) {
        menuData->selectedOption = (menuData->selectedOption + 1) % MAX_OPTIONS;
        PlaySound(menuSounds->changeOptionSound);
    } else if (IsKeyPressed(KEY_UP)) {
        menuData->selectedOption = (menuData->selectedOption - 1 + MAX_OPTIONS) % MAX_OPTIONS;
        PlaySound(menuSounds->changeOptionSound);
    }

    if (IsKeyPressed(KEY_ENTER)) {
        switch (menuData->currentState) {
            case MENU_MAIN:
                handleMainMenuSelection(menuData, menuSounds);
                break;
            case MENU_DIFFICULTY:
                handleDifficultySelection(menuData, menuSounds);
                break;
            case MENU_WORLD_SETTINGS:
                handleWorldSettingsSelection(menuData, menuSounds);
                break;
            case MENU_MULTIPLAYER:
                handleMultiplayerSelection(menuData, menuSounds);
                break;
        }
    }
}

