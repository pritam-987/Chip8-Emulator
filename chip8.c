#include <SDL2/SDL_error.h>
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_video.h>
#include <stdio.h> 
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct{
    SDL_Window *window;
    SDL_Renderer *renderer;
} sdl_t;

typedef struct{
    uint32_t window_width;
    uint32_t window_height;
}config_t ;

bool init_sdl(sdl_t *sdl, const config_t config){
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0){
        SDL_Log("Could not init SDL %s\n", SDL_GetError());
        return false;
    }  
    sdl->window = SDL_CreateWindow("chip8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,config.window_width, config.window_height, 0);

    if (!sdl->window){
        SDL_Log("Could not start window %s\n", SDL_GetError());
    }
    
    sdl->renderer  = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
    if(!sdl->renderer){
        SDL_Log("could not create renderer %s\n", SDL_GetError());
        return false;
    }

    return true;
}

void final_cleanup(const sdl_t sdl){
    SDL_DestroyRenderer(sdl.renderer);
    SDL_DestroyWindow(sdl.window);
    SDL_Quit();

}
//set up initial config from args
bool set_config_from_args(config_t *config, int argc, char **argv){
    //default
    *config = (config_t){
        .window_width = 64,
        .window_height = 32,
    };

    //overrides from the argcs
    for (int i = 1; i < argc; i++)
    {
        (void)argv[1];
    }
    return true;

}

int main(void){
    // init
    // init emulator config_t
    config_t config = {0};
    if (!set_config_from_args(&config,  argc, argv)) exit(EXIT_FAILURE);

    sdl_t sdl = {0};
    if (!init_sdl(&sdl, config)){
        exit(EXIT_FAILURE);
    }
    //Final cleanup
    final_cleanup(&sdl);
    
    exit(EXIT_SUCCESS);
}
