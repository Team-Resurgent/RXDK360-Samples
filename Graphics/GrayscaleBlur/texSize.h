
// this is the number of pixels along the Y axis after which the tiling pattern repeats
static const INT    VERT_TILE_REPEAT = 16;

static const INT    HORZ_8_TO_32_TILE_REPEAT = 32;

static const INT    HORZ_32_TO_8_TILE_REPEAT = 128;

// shared register index
#define REG_TEX_SIZES_0   0
#define REG_TEX_SIZES_1   1


#define PASTE0( a, b )  a ## b
#define PASTE( a, b )   PASTE0( a, b )
