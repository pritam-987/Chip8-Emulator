#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_timer.h>
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

typedef enum{
    QUIT,
    RUNNING,
    PAUSED,
} emu_state_t;

typedef struct{
    uint32_t window_width;
    uint32_t window_height;
    uint32_t fg; 
    uint32_t bg;
    uint32_t scale_factor;
}config_t ;

typedef struct {
    emu_state_t state;
} chip8_t;

bool init_sdl(sdl_t *sdl, const config_t config){
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0){
        SDL_Log("Could not init SDL %s\n", SDL_GetError());
        return false;
    }  
    sdl->window = SDL_CreateWindow("chip8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,config.window_width * config.scale_factor, config.window_height * config.scale_factor, 0);

    if (!sdl->window){
        SDL_Log("Could not start window %s\n", SDL_GetError());
        return false;
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
        .fg = 0xFFFF00FF,
        .bg = 0x00000000,
        .scale_factor = 20,
    };

    //overrides from the argcs
    for (int i = 1; i < argc; i++)
    {
        (void)argv[i];
    }
    return true;

}

void clear_screen(const sdl_t sdl, const config_t config){
    const uint8_t r = (config.bg >> 24) & 0xFF;
    const uint8_t g = (config.bg >> 16) & 0xFF;
    const uint8_t b = (config.bg >> 8) & 0xFF;
    const uint8_t a = (config.bg >> 0) & 0xFF;
    SDL_SetRenderDrawColor(sdl.renderer, r, g,  b, a);
    SDL_RenderClear(sdl.renderer);

}

void update_screen(const sdl_t sdl){
    SDL_RenderPresent(sdl.renderer);
}

void handle_input(chip8_t *chip8){
    SDL_Event event;
    
    while (SDL_PollEvent(&event))
    {
        switch (event.type){
            case SDL_QUIT:
                chip8->state = QUIT; // exit main loop
                return;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    // Exit using ESCAPE
                    case  SDLK_ESCAPE:
                        chip8->state = QUIT;
                        return;
                    default:
                        break;
                }
                break;

            case SDL_KEYUP:
                break;

            default:
                break;
        }
    }

}

//Init chip8 machine
bool init_chip8(chip8_t *chip8){
    chip8->state = RUNNING;
    return true;
}
int main(int argc, char **argv){
    // init
    // init emulator config_t
    config_t config = {0};
    if (!set_config_from_args(&config,  argc, argv)) exit(EXIT_FAILURE);

    sdl_t sdl = {0};
    if (!init_sdl(&sdl, config)){
        exit(EXIT_FAILURE);
    }
    //init chip8 machine
    chip8_t chip8 = {0};
    if (!init_chip8(&chip8)) exit(EXIT_FAILURE);

    //Init screen clear
    clear_screen(sdl, config);

    //Main emulator loop
    while (chip8.state != QUIT){
        //handle input
        handle_input(&chip8);
        //Delay for 60hz
        SDL_Delay(16);
        //update widnow with changes
        update_screen(sdl);

    }
    //Final cleanup
    final_cleanup(sdl);
    
    exit(EXIT_SUCCESS);
}
