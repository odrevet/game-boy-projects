//AUTOGENERATED FILE FROM png2asset
#ifndef METASPRITE_level_H
#define METASPRITE_level_H

#include <stdint.h>
#include <gbdk/platform.h>
#include <gbdk/metasprites.h>

#define level_TILE_ORIGIN 0
#define level_TILE_W 8
#define level_TILE_H 8
#define level_WIDTH 2400
#define level_HEIGHT 144
#define level_TILE_COUNT 41
#define level_PALETTE_COUNT 1
#define level_COLORS_PER_PALETTE 4
#define level_TOTAL_COLORS 4
#define level_MAP_ATTRIBUTES 0

BANKREF_EXTERN(level)

extern const palette_color_t level_palettes[4];
extern const uint8_t level_tiles[656];

extern const unsigned char level_map[5400];

#endif
