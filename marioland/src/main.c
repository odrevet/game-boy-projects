#include <gb/gb.h>
#include <gb/metasprites.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graphics/World1Tileset.h"
#include "graphics/mario.h"
#include "maps/world1area1.h"
#include "maps/world1area2.h"

#include "camera.h"
#include "hUGEDriver.h"
#include "sound.h"
#include "text.h"

#define TILE_SIZE 8
#define OFFSET_X 1
#define OFFSET_Y 2
#define SCREEN_HEIGHT 18
#define SCREEN_WIDTH 20
#define WINDOW_HEIGHT 2
#define WINDOW_WIDTH SCREEN_WIDTH
#define WINDOW_SIZE WINDOW_WIDTH *WINDOW_HEIGHT
#define WINDOW_X 7
#define WINDOW_Y 0

#define TIME_INITIAL_VALUE 10000
#define MARIO_SPEED_WALK 1
#define MARIO_SPEED_RUN 2
#define JUMP_MAX 20
#define LOOP_PER_ANIMATION_FRAME 5
short mario_current_frame = 0;
short frame_counter = 0;
bool is_jumping = FALSE;
bool touch_ground = FALSE;
short current_jump = 0;
short mario_speed = 0;

bool redraw;

uint8_t joy;

// map
const unsigned char *map;
const unsigned char *map_attributes;
short map_width;
short map_height;
short map_tile_count;

// music
// extern const hUGESong_t fish_n_chips;
extern const hUGESong_t cognition;

void update_frame_counter() {
  frame_counter++;
  if (frame_counter == LOOP_PER_ANIMATION_FRAME) {
    frame_counter = 0;
    mario_current_frame = (mario_current_frame % 3) + 1;
  }
}

void init_map() {
  map_pos_x = map_pos_y = 0;
  old_map_pos_x = old_map_pos_y = 255;
  set_bkg_submap(map_pos_x, map_pos_y, 20, 18, map, map_width);

  camera_x = camera_y = 0;
  old_camera_x = camera_x;
  old_camera_y = camera_y;

  redraw = FALSE;

  SCX_REG = camera_x;
  SCY_REG = camera_y;
}

void load_area1() {
  map = world1area1_map;
  map_attributes = world1area1_map_attributes;
  map_width = world1area1_WIDTH;
  map_height = world1area1_HEIGHT;
  map_tile_count = world1area1_TILE_COUNT;
}

void load_area2() {
  map = world1area2_map;
  map_attributes = world1area2_map_attributes;
  map_width = world1area2_WIDTH;
  map_height = world1area2_HEIGHT;
  map_tile_count = world1area2_TILE_COUNT;
}

void interupt() {
  if (LYC_REG == WINDOW_Y) {
    SHOW_WIN;
    LYC_REG = WINDOW_Y + WINDOW_HEIGHT * TILE_SIZE;
  } else if (LYC_REG == WINDOW_Y + WINDOW_HEIGHT * TILE_SIZE) {
    HIDE_WIN;
    LYC_REG = WINDOW_Y;
  }
}

// TODO use solid map when available
bool is_solid(int x, int y) {
  return map[map_width * (y / TILE_SIZE - 1) + (x / TILE_SIZE - OFFSET_X)] !=
         15;
}

void main(void) {
  DISPLAY_ON;
  SHOW_BKG;
  SHOW_WIN;
  SHOW_SPRITES;
  SPRITES_8x16;

  STAT_REG |= 0x40U;
  LYC_REG = WINDOW_Y;
  disable_interrupts();
  add_LCD(interupt);
  set_interrupts(LCD_IFLAG | VBL_IFLAG);
  enable_interrupts();

  sound_init();
  __critical {
    hUGE_init(&cognition);
    // add_VBL(hUGE_dosound);
  };

  // text
  text_load_font();

  // joypad
  int joypad_previous, joypad_current = 0;

  // player
  int player_x = (SCREEN_WIDTH / 2) * TILE_SIZE,
      player_y = (SCREEN_HEIGHT / 2) * TILE_SIZE;
  int player_x_next;
  int player_y_next;

  set_sprite_data(0, mario_TILE_COUNT, mario_tiles);

  short level_index = 0;
  load_area1();
  init_map();

  set_bkg_data(0, World1Tileset_TILE_COUNT, World1Tileset_tiles);

  short score = 0;
  short time = TIME_INITIAL_VALUE;
  short lives = 3;
  short coins = 0;

  char score_str[4] = "0";

  short vel_x = 0;
  short vel_y = 0;

  frame_counter = 0;
  bool mario_flip = FALSE;

  // text
  unsigned char windata[WINDOW_SIZE];
  memset(windata, 15, WINDOW_SIZE);
  set_win_tiles(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, windata);
  move_win(WINDOW_X, WINDOW_Y);

  char hud_line_one[SCREEN_WIDTH + 1] = "MARIOX01  WORLD TIME";

  hUGE_mute_channel(0, HT_CH_MUTE);
  hUGE_mute_channel(1, HT_CH_MUTE);
  hUGE_mute_channel(2, HT_CH_PLAY);
  hUGE_mute_channel(3, HT_CH_MUTE);

  text_print_string_win(0, 0, hud_line_one);
  text_print_string_win(5, 1, score_str);
  text_print_string_win(7, 1, "CX00");
  text_print_string_win(12, 1, "1-1");
  text_print_string_win(17, 1, "400");

  while (1) {
    // inputs
    joypad_previous = joypad_current;
    joypad_current = joypad();

#if defined(DEBUG)
    if (joypad_current & J_SELECT && !(joypad_previous & J_SELECT)) {
      level_index++;
      if (level_index == 2) {
        level_index = 0;
      }

      if (level_index == 0) {
        load_area1();
      } else {
        load_area2();
      }

      init_map();
    }
#endif

    if (joypad_current & J_RIGHT && player_x / 8 < map_width * 8) {
      vel_x = mario_speed;
      mario_flip = FALSE;
      update_frame_counter();
    }

    if (joypad_current & J_LEFT && player_x > 12) {
      vel_x = -mario_speed;
      mario_flip = TRUE;
      update_frame_counter();
    }

    // on release left pad
    if (!(joypad_current & J_LEFT) && (joypad_previous & J_LEFT)) {
      vel_x = 0;
    }

    // on release right pad
    if (!(joypad_current & J_RIGHT) && (joypad_previous & J_RIGHT)) {
      vel_x = 0;
    }

    if (joypad_current & J_A && !(joypad_previous & J_A) && !is_jumping &&
        touch_ground) {
      is_jumping = TRUE;
      sound_play_jumping();
    }

    // pause
    if (joypad_current & J_START && !(joypad_previous & J_START)) {
      sound_play_jumping();                 // TODO pause sound
      text_print_string_win(0, 0, "PAUSE"); // TODO center
      while (1) {
        joypad_current = joypad();
        // TODO if press start
        if (joypad_current & J_A && !(joypad_previous & J_A)) {
          break;
        }

        wait_vbl_done();
      }
    }

    if (joypad_current & J_B) {
      mario_speed = MARIO_SPEED_RUN;
    } else {
      mario_speed = MARIO_SPEED_WALK;
    }

// print text
#if defined(DEBUG)
    char buffer[WINDOW_SIZE + 1];
    char fmt[] = "X:%d Y:%d\nINDEX:%d TILE:%d";
    int index =
        ((player_y - OFFSET_Y) / 8) * map_width + ((player_x - -OFFSET_X) / 8);
    sprintf(buffer, fmt, (int16_t)player_x - OFFSET_X,
            (int16_t)player_y - OFFSET_Y, (int16_t)index, map[index]);
    text_print_string_win(0, 0, buffer);
#else

#endif

    if (is_solid(player_x, player_y + 1)) {
      touch_ground = TRUE;
    }

    if (is_jumping) {
      mario_current_frame = 4;
      vel_y = -1;
      current_jump++;
      if (current_jump > JUMP_MAX) {
        is_jumping = FALSE;
        current_jump = 0;
      }
    } else {
      // gravity
      vel_y = 1;
    }

    // apply velocity to player coords
    player_x_next = player_x + vel_x;
    player_y_next = player_y + vel_y;

    // move down
    if (vel_y > 0) {
      if (is_solid(player_x, player_y_next)) {
        int index_y = player_y_next / TILE_SIZE;
        player_y = index_y * TILE_SIZE;
        vel_y = 0;
      } else {
        player_y = player_y_next;
      }
    }
    // move up
    else if (vel_y < 0) {
      if (is_solid(player_x, player_y_next - 8)) {
        int index_y = player_y_next / TILE_SIZE;
        player_y = index_y * TILE_SIZE + TILE_SIZE;
        vel_y = 0;
        sound_play_bump();
      } else {
        player_y = player_y_next;
      }
    }

    // move right
    if (vel_x > 0) {
      if (is_solid(player_x_next, player_y - 8)) {
        int index_x = player_x_next / TILE_SIZE;
        player_x = index_x * TILE_SIZE;
        vel_x = 0;
      } else {
        player_x = player_x_next;
      }
    }
    // move left
    else if (vel_x < 0) {
      if (is_solid(player_x_next, player_y - 8)) {
        int index_x = player_x_next / TILE_SIZE;
        player_x = index_x * TILE_SIZE + TILE_SIZE;
        vel_x = 0;
      } else {
        player_x = player_x_next;
      }
    }

    if (mario_flip)
      move_metasprite_vflip(mario_metasprites[mario_current_frame], 0, 0,
                            player_x, player_y);
    else {
      move_metasprite(mario_metasprites[mario_current_frame], 0, 0, player_x,
                      player_y);
    }

    time--;
    if (time == 0) {
      time = TIME_INITIAL_VALUE;
      lives--;
    }

    if (redraw) {
      set_camera(map_height, map_width, map);
      redraw = FALSE;
    }

    hUGE_dosound();
    wait_vbl_done();
  }
}
