#include <gb/gb.h>
#include <gb/metasprites.h>
#include <gbdk/incbin.h>
#include <gbdk/platform.h>
#include <gbdk/rledecompress.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graphics/enemies.h"
#include "graphics/mario.h"

#include "enemy.h"
#include "global.h"
#include "graphics/text.h"
#include "hUGEDriver.h"
#include "sound.h"
#include "text.h"

#include "../res/level_1_1.h"
INCBIN(level_map_bin_rle, "res/level_1_1_map.bin.rle")
INCBIN_EXTERN(level_map_bin_rle)

INCBIN(level_tiles_bin, "res/level_1_1_tiles.bin")
INCBIN_EXTERN(level_tiles_bin)

#define DEVICE_SCREEN_PX_WIDTH_HALF DEVICE_SCREEN_PX_WIDTH / 2
#define LEVEL_HEIGHT 16
#define MARGIN_TOP 2
#define MARGIN_TOP_PX 2 * TILE_SIZE
#define DEVICE_SPRITE_OFFSET_Y 2

// Tilesets offsets
#define TEXT_TILESET_START 0
#define LEVEL_TILESET_START text_TILE_COUNT - 3

uint8_t coldata[LEVEL_HEIGHT]; // buffer of one columns
uint8_t datapos = 0;

uint16_t camera_x = 0;
uint16_t camera_x_mask = 0;
uint16_t camera_x_previous = 0;

const uint8_t window_location = WINDOW_Y + WINDOW_HEIGHT_TILE * TILE_SIZE;

uint8_t coins;
uint16_t score;
bool redraw;
uint8_t joy;
uint16_t time;
uint8_t lives;
uint8_t level_index;

// player coords and movements
uint16_t player_x;
uint16_t player_y;
uint16_t player_x_next;
uint16_t player_y_next;
uint16_t player_draw_x;
uint16_t player_draw_y;
uint16_t player_draw_x_next;
uint16_t player_draw_y_next;
int8_t vel_x;
int8_t vel_y;
bool is_jumping = FALSE;
bool display_jump_frame;
bool display_slide_frame;
bool touch_ground = FALSE;
uint8_t current_jump = 0;
int8_t player_max_speed = PLAYER_MAX_SPEED_WALK;
uint8_t player_current_frame = 0;
uint8_t frame_counter = 0;
bool mario_flip;

uint16_t scroll_modulo = 0;
uint16_t previous_scroll_modulo = TILE_SIZE;

enum tileset_index {
  TILE_EMPTY = LEVEL_TILESET_START + 0x01,
  TILE_UNBREAKABLE = LEVEL_TILESET_START + 0x0B,
  TILE_COIN = LEVEL_TILESET_START + 0x13,
  BREAKABLE_BLOCK = LEVEL_TILESET_START + 0x14,
  PIPE_TOP_LEFT = LEVEL_TILESET_START + 0x17,
  PIPE_TOP_RIGHT = LEVEL_TILESET_START + 0x18,
  PIPE_CENTER_LEFT = LEVEL_TILESET_START + 0x19,
  PIPE_CENTER_RIGHT = LEVEL_TILESET_START + 0x1A,
  TILE_FLOOR = LEVEL_TILESET_START + 0x22,
  TILE_INTEROGATION_BLOCK = LEVEL_TILESET_START + 0x0A,
  TILE_EMPTIED = LEVEL_TILESET_START + 0X1E
};

// music
extern const hUGESong_t cognition;

inline bool is_solid(int x, int y) {
  const uint8_t tile =
      get_bkg_tile_xy((x / TILE_SIZE) % DEVICE_SCREEN_BUFFER_WIDTH,
                      y / TILE_SIZE - DEVICE_SPRITE_OFFSET_Y);
  return tile == TILE_FLOOR || tile == TILE_INTEROGATION_BLOCK ||
         tile == BREAKABLE_BLOCK || tile == TILE_UNBREAKABLE ||
         tile == PIPE_TOP_LEFT || tile == PIPE_TOP_RIGHT ||
         tile == PIPE_CENTER_LEFT || tile == PIPE_CENTER_RIGHT;
}

void update_frame_counter() {
  frame_counter++;
  if (frame_counter == LOOP_PER_ANIMATION_FRAME) {
    frame_counter = 0;
    player_current_frame = (player_current_frame % 3) + 1;
  }
}

inline bool is_coin(int x, int y) {
  return get_bkg_tile_xy((x / TILE_SIZE) % DEVICE_SCREEN_BUFFER_WIDTH,
                         y / TILE_SIZE) == TILE_COIN;
}

void hud_update_coins() {
  char coins_str[3];

  if (coins < 10) {
    coins_str[0] = '0'; // Add leading zero
    itoa(coins, coins_str + 1, 10);
  } else {
    itoa(coins, coins_str, 10);
  }

  text_print_string_win(9, 1, coins_str);
}

void hud_update_score() {
  char score_str[4];
  itoa(score, score_str, 10);
  text_print_string_win(0, 1, score_str);
}

inline void on_get_coin(int x_right, int y_bottom) {
  set_bkg_tile_xy(x_right / TILE_SIZE, y_bottom / TILE_SIZE, TILE_EMPTY);

  sound_play_bump(); // TODO play sound coin

  coins++;
  score += 100;

  if (coins == 100) {
    lives++;
    coins = 0;
  }

  hud_update_coins();
  hud_update_score();
}

void player_draw() {
  uint16_t player_draw_x_camera_offset = player_draw_x - camera_x + TILE_SIZE;
  metasprite_t *mario_metasprite = mario_metasprites[player_current_frame];
  if (mario_flip) {
    move_metasprite_vflip(mario_metasprite, 0, 0, player_draw_x_camera_offset,
                          player_draw_y + DEVICE_SPRITE_PX_OFFSET_Y -
                              TILE_SIZE);
  } else {
    move_metasprite(mario_metasprite, 0, 0, player_draw_x_camera_offset,
                    player_draw_y + DEVICE_SPRITE_PX_OFFSET_Y - TILE_SIZE);
  }
}

void interruptLCD() {
  while (STAT_REG & 3)
    ;
  HIDE_WIN;
}

void interruptVBL() { SHOW_WIN; }

void main(void) {
  DISPLAY_ON;
  SHOW_BKG;
  SHOW_WIN;
  SHOW_SPRITES;
  SPRITES_8x16;

  STAT_REG = 0x40;
  LYC_REG = 0x0F;

  move_bkg(0, -MARGIN_TOP_PX);

  disable_interrupts();
  add_LCD(interruptLCD);
  add_VBL(interruptVBL);
  set_interrupts(VBL_IFLAG | LCD_IFLAG);
  enable_interrupts();

  sound_init();
  __critical {
    hUGE_init(&cognition);
    add_VBL(hUGE_dosound);
  };

  // joypad
  int joypad_previous, joypad_current = 0;

  // player
  player_x = 43 << 4;
  player_y = (16 * TILE_SIZE) << 4;
  player_draw_x = player_x >> 4;
  player_draw_y = player_y >> 4;

  set_sprite_data(SPRITE_START_MARIO, mario_TILE_COUNT, mario_tiles);
  set_sprite_data(SPRITE_START_ENEMIES, enemies_TILE_COUNT, enemies_tiles);

  level_index = 0;

  SCX_REG = 0;
  camera_x = 0;
  camera_x_mask = 0;

  score = 0;
  time = TIME_INITIAL_VALUE;
  lives = 3;
  coins = 0;

  vel_x = 0;
  vel_y = 0;

  display_jump_frame = FALSE;
  display_slide_frame = FALSE;

  frame_counter = 0;
  mario_flip = FALSE;

  // HUD
  // text
  unsigned char windata[WINDOW_SIZE];
  memset(windata, 15, WINDOW_SIZE);
  set_win_tiles(0, 0, WINDOW_WIDTH_TILE, WINDOW_HEIGHT_TILE, windata);
  move_win(WINDOW_X, WINDOW_Y);
  text_print_string_win(0, 0, "MARIOx02  WORLD TIME");
  text_print_string_win(0, 1, "     0  x00 1-1  400");

  // display a coin in the HUD
  set_win_tile_xy(7, 1, TILE_COIN);

  // Set music channels channel
  hUGE_mute_channel(0, HT_CH_PLAY);
  hUGE_mute_channel(1, HT_CH_PLAY);
  hUGE_mute_channel(2, HT_CH_PLAY);
  hUGE_mute_channel(3, HT_CH_PLAY);

  // spawn enemies
  // enemy_new(50, 136, ENEMY_TYPE_GOOMBA);
  // enemy_new(70, 136, ENEMY_TYPE_KOOPA);

  // HUD
  set_bkg_data(TEXT_TILESET_START, text_TILE_COUNT, text_tiles);

  // map
  set_bkg_data(LEVEL_TILESET_START, INCBIN_SIZE(level_tiles_bin) >> 4,
               level_tiles_bin);

  rle_init(level_map_bin_rle);

  for (uint8_t col = 0; col < DEVICE_SCREEN_WIDTH + 1; col++) {
    // decompress a column of tile data
    rle_decompress(coldata, LEVEL_HEIGHT);

    // copy to VRAM
    set_bkg_tiles(col & (DEVICE_SCREEN_BUFFER_WIDTH - 1), 0, 1,
                  DEVICE_SCREEN_HEIGHT, coldata);
  }

  while (1) {
    // inputs
    joypad_previous = joypad_current;
    joypad_current = joypad();

    if (joypad_current & J_RIGHT) {
      if (vel_x < player_max_speed) {
        vel_x += 1;
        mario_flip = FALSE;
        if (vel_x < 0) {
          display_slide_frame = TRUE;
        } else {
          display_slide_frame = FALSE;
        }
      } else if (vel_x > player_max_speed) {
        vel_x -= 1;
      }
    } else if (vel_x > 0) {
      vel_x -= 1;
    }

    if ((joypad_current & J_LEFT)) {
      if (abs(vel_x) < player_max_speed) {
        vel_x -= 1;
        mario_flip = TRUE;
        if (vel_x > 0) {
          display_slide_frame = TRUE;
        } else {
          display_slide_frame = FALSE;
        }
      } else if (abs(vel_x) > player_max_speed) {
        vel_x += 1;
      }
    } else if (vel_x < 0) {
      vel_x += 1;
    }

    if (joypad_current & J_A && !(joypad_previous & J_A) && !is_jumping &&
        touch_ground) {
      is_jumping = TRUE;
      display_jump_frame = TRUE;
      // sound_play_jumping();
    }

    // pause
    /*if (joypad_current & J_START && !(joypad_previous & J_START)) {
      sound_play_jumping();                 // TODO pause sound
      text_print_string_win(0, 0, "PAUSE"); // TODO bottom

      while (1) {
        joypad_current = joypad();
        // TODO if press start
        if (joypad_current & J_A && !(joypad_previous & J_A)) {
          break;
        }

        wait_vbl_done();
      }
    }*/

    if (joypad_current & J_B) {
      player_max_speed = PLAYER_MAX_SPEED_RUN;
    } else {
      player_max_speed = PLAYER_MAX_SPEED_WALK;
    }

    if (is_solid(player_draw_x, player_draw_y + 1)) {
      touch_ground = TRUE;
    }

    if (is_jumping) {
      vel_y = -JUMP_SPEED;
      current_jump++;
      if (current_jump > JUMP_MAX) {
        is_jumping = FALSE;
      }
    } else {
      vel_y = GRAVITY_SPEED;
    }

    int16_t x_right_draw = player_draw_x + MARIO_HALF_WIDTH - 1;
    int16_t x_left_draw = player_draw_x - MARIO_HALF_WIDTH;

    // apply velocity to player coords
    if (vel_y != 0) {
      player_y_next = player_y + vel_y;
      player_draw_y_next = player_y_next >> 4;

      // move down
      if (vel_y > 0) {
        int16_t y_bottom_next = player_draw_y_next;
        if (is_solid(x_left_draw, y_bottom_next) ||
            is_solid(x_right_draw, y_bottom_next)) {
          uint8_t index_y = y_bottom_next / TILE_SIZE;
          player_y = (index_y * TILE_SIZE) << 4;
          touch_ground = TRUE;
          current_jump = 0;
          is_jumping = FALSE;
          display_jump_frame = FALSE;
        } else {
          touch_ground = FALSE;
          player_y = player_y_next;
        }
      }

      // move up
      else if (vel_y < 0) {
        int16_t y_top_next = player_draw_y_next - 6;
        if (is_solid(x_left_draw, y_top_next) ||
            is_solid(x_right_draw, y_top_next)) {
          uint8_t index_y = player_draw_y_next / TILE_SIZE;
          player_y = (index_y * TILE_SIZE + TILE_SIZE) << 4;
          current_jump = 0;
          is_jumping = FALSE;
          sound_play_bump();
        } else {
          player_y = player_y_next;
        }
      }
      player_draw_y = player_y >> 4;
    }

    int16_t y_top_draw = player_draw_y - MARIO_HALF_WIDTH;
    int16_t y_bottom_draw = player_draw_y - 1;

    if (vel_x != 0) {
      player_x_next = player_x + vel_x;
      player_draw_x_next = player_x_next >> 4;

      // move right
      if (vel_x > 0) {
        int16_t x_right_next = player_draw_x_next + MARIO_HALF_WIDTH;
        if (is_solid(x_right_next, y_top_draw) ||
            is_solid(x_right_next, y_bottom_draw)) {
          uint8_t index_x = player_draw_x_next / TILE_SIZE;
          player_x = (index_x * TILE_SIZE + MARIO_HALF_WIDTH) << 4;
        } else {
          player_x = player_x_next;
        }
      }
      // move left
      else if (vel_x < 0) {
        int16_t x_left_next = player_draw_x_next - MARIO_HALF_WIDTH;
        if (is_solid(x_left_next, y_top_draw) ||
            is_solid(x_left_next, y_bottom_draw)) {
          uint8_t index_x = player_draw_x_next / TILE_SIZE;
          player_x = (index_x * TILE_SIZE + TILE_SIZE - MARIO_HALF_WIDTH) << 4;
        } else if (player_draw_x_next > 1) {
          player_x = player_x_next;
        }
      }

      player_draw_x = player_x >> 4;
    }

    // set player frame
    if (display_jump_frame) {
      player_current_frame = 4;
    } else if (vel_x != 0) {
      if (display_slide_frame) {
        player_current_frame = 5;
      } else {
        update_frame_counter();
      }
    } else {
      player_current_frame = 0;
    }

    player_draw();
    // enemy_update();
    // enemy_draw(SPRITE_START_ENEMIES);

    // print DEBUG text
#if defined(DEBUG)
    char buffer[WINDOW_SIZE + 1];
    char fmt[] = "X:%d;XD:%d;MC:%d;\nXV:%d;CX:%d;T:%d;";
    sprintf(buffer, fmt, (int16_t)player_x, (int16_t)player_draw_x,
            player_draw_x - camera_x, vel_x, camera_x,
            get_bkg_tile_xy((player_draw_x / TILE_SIZE) %
                                DEVICE_SCREEN_BUFFER_WIDTH,
                            player_draw_y / TILE_SIZE - 1));
    text_print_string_win(0, 0, buffer);
#endif

    time--;
    if (time == 0) {
      time = TIME_INITIAL_VALUE;
      lives--;
    }

    // check coins

    if (is_coin(x_right_draw, y_bottom_draw - TILE_SIZE)) {
      on_get_coin(x_right_draw, y_bottom_draw - TILE_SIZE);
    }

    if (is_coin(x_right_draw, y_top_draw)) {
      on_get_coin(x_right_draw, y_top_draw);
    }

    if (is_coin(x_left_draw, y_bottom_draw - TILE_SIZE)) {
      on_get_coin(x_left_draw, y_bottom_draw - TILE_SIZE);
    }

    if (is_coin(x_left_draw, y_top_draw)) {
      on_get_coin(x_left_draw, y_top_draw);
    }

    wait_vbl_done();

    // scroll

    if (vel_x > 0 && player_draw_x - camera_x > DEVICE_SCREEN_PX_WIDTH_HALF) {
      camera_x_mask += vel_x;
      camera_x = camera_x_mask >> 4;
      SCX_REG = camera_x;
      scroll_modulo = camera_x + (camera_x % TILE_SIZE);

      if (scroll_modulo >= previous_scroll_modulo) {
        previous_scroll_modulo = scroll_modulo + TILE_SIZE;
        datapos = (SCX_REG >> 3);
        uint8_t map_x_column =
            (datapos + DEVICE_SCREEN_WIDTH) & (DEVICE_SCREEN_BUFFER_WIDTH - 1);

        bool more_data = rle_decompress(coldata, LEVEL_HEIGHT);
        //if (!more_data) {
        //}

        set_bkg_tiles(map_x_column, 0, 1, LEVEL_HEIGHT, coldata);
      }
    }
  }
}