#include <jo/jo.h>

#include "game.h"

void jo_main(void)
{
    jo_core_init(JO_COLOR_Black);
    if (!game_init())
    {
        jo_core_run();
        return;
    }
    jo_core_add_callback(game_loop);
    jo_core_run();
}
