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

#ifndef MENU_H
#define MENU_H

#include "defs.h"
#include "map/maps.h"
#include "utils/utils.h"
#include "events/events.h"

#define LOGO_PATH "res/static/background.png"
#define WALL_PATH "res/static/wall.png"
#define FIRE_ANIM_PATH "res/static/fire.gif"
#define RAIN_ANIM_PATH "res/static/rain.gif"
#define PORT 12345
#define GAME_SERVER_IP "127.0.0.1"
#define BACKGROUND_MENU_MUSIC "res/sounds/menu_background.mp3"
#define CHANGE_OPTION_SOUND "res/static/change_option.mp3"
#define SELECT_OPTION_SOUND "res/static/select.mp3"
#define LIGHTNING_SOUND "res/static/lightning.mp3"

#define MAX_OPTIONS 4


typedef enum {
    OPTION_SINGLEPLAYER,
    OPTION_MULTIPLAYER,
    OPTION_OPTIONS,
    OPTION_EXIT
} MenuOption;

MenuData* menu_screen(void);

// Initialization
void InitSounds(void);
void InitData(void);

// Updates
void UpdateRaining(void);
void UpdateFrames(Texture2D, Image, Texture2D, Image);

// Draws
void DrawBackground(Texture2D logo, Texture2D wall, Texture2D texFireAnim, Texture2D rainTexture);
void DrawMainMenu(int selectedOption);
void DrawDifficultyMenu(int selectedOption);
void DrawMultiplayerMenu(int selectedOption);
void DrawWorldSettings(int selectedOption);
void DrawOption(const char *text, Rectangle optionRect, Color color);
void startSinglePlayer(void);

void DrawHostingMenu(int selectedOption);
void DrawConnectMenu(int selectedOption);

int startHosting(void);
int startConnection(void);

#endif