#ifndef ENEMY_H
#define ENEMY_H

#include <gb/gb.h>
#include <gb/metasprites.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "camera.h"

#define ENEMY_MAX 4

typedef struct enemy_t {
  uint16_t x;
  uint16_t y;
  uint16_t vel_x;
  uint16_t vel_y;
} enemy_t;

extern uint8_t enemy_count;
extern enemy_t enemies[ENEMY_MAX];

void enemy_new(uint16_t x, uint16_t y);
void enemy_update();
void enemy_draw();

#endif