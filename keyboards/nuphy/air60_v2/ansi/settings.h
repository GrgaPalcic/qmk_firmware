
/*
 * Shared side-light limits used by both config sanitization and RGB logic.
 * Keep these aligned with rgb_table.h.
 */
#define SIDE_BRIGHT_MAX 6
#define SIDE_SPEED_MAX 4
#define SIDE_COLOUR_MAX 10

__attribute__((weak)) void user_config_override(void) {}
__attribute__((weak)) void game_config_override(void) {}
