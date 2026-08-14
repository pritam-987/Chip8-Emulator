#include <SDL2/SDL_audio.h>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_log.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <stdio.h> 
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <wchar.h>
#include <time.h>
#include <time.h>


typedef struct{
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_AudioSpec want, have;
    SDL_AudioDeviceID dev;
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
    bool pixel_outlines;
    uint32_t insts_per_sec;
    uint32_t sq_wave_freq;
    uint32_t audio_sample_rate;
    int32_t volume; 
    float color_lerp_rate;
}config_t ;

typedef struct{
    uint16_t opcode;
    uint16_t NNN;
    uint8_t NN;
    uint8_t N;
    uint8_t X;
    uint8_t Y;
}instructions_t;

typedef struct {
    emu_state_t state;
    uint8_t ram[4096]; 
    bool display[64*32];
    uint32_t pixel_colors[64*32];
    uint16_t stack[12];
    uint16_t *stack_pt;
    uint8_t V[16];
    uint16_t I;
    uint16_t PC;
    uint8_t delay_timer;
    uint8_t sound_timer;
    bool keypad[16];
    bool key_waiting;
    const char *rom_name;
    instructions_t inst;
    bool draw;
} chip8_t;

bool init_chip8(chip8_t *chip8,const config_t config, const char rom_name[]);

uint32_t color_lerp(const uint32_t start_color, uint32_t end_color, float t){
    const uint8_t s_r = (start_color >> 24) & 0xFF;
    const uint8_t s_g = (start_color >> 16) & 0xFF;
    const uint8_t s_b = (start_color >> 8) & 0xFF;
    const uint8_t s_a = (start_color >> 0) & 0xFF;

    const uint8_t e_r = (end_color >> 24) & 0xFF;
    const uint8_t e_g = (end_color >> 16) & 0xFF;
    const uint8_t e_b = (end_color >> 8) & 0xFF;
    const uint8_t e_a = (end_color >> 0) & 0xFF;

    const uint8_t ret_r = ((1 - t)*s_r) + (t*e_r);
    const uint8_t ret_g = ((1 - t)*s_g) + (t*e_g);
    const uint8_t ret_b = ((1 - t)*s_b) + (t*e_b);
    const uint8_t ret_a = ((1 - t)*s_a) + (t*e_a);

    return (ret_r << 24) | (ret_g << 16) | (ret_b << 8) | ret_a;
}

void audio_callback(void *userdata, uint8_t *stream, int len){
    config_t *config = (config_t *)userdata; 

    int16_t *audio_data = (int16_t *)stream;
    static uint32_t runnin_sample_index = 0;
    const int32_t sq_wave_period =  config->audio_sample_rate / config->sq_wave_freq;
    const int32_t half_sq_wave_period = sq_wave_period / 2;

    for (int i = 0; i < len /2; i++){
         audio_data[i] = ((runnin_sample_index++ / half_sq_wave_period) % 2) ? config->volume : -config-> volume; 
    }
}

bool init_sdl(sdl_t *sdl,  config_t *config){
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0){
        SDL_Log("Could not init SDL %s\n", SDL_GetError());
        return false;
    }  
    sdl->window = SDL_CreateWindow("chip8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,config->window_width * config->scale_factor, config->window_height * config->scale_factor, SDL_WINDOW_SHOWN);

    if (!sdl->window){
        SDL_Log("Could not start window %s\n", SDL_GetError());
        return false;
    }
    
    sdl->renderer  = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_SOFTWARE);
    if(!sdl->renderer){
        SDL_Log("could not create renderer %s\n", SDL_GetError());
        return false;
    }

    //Audio
    sdl->want = (SDL_AudioSpec){
        .freq = 44100,
        .format = AUDIO_S16LSB,
        .channels = 1,
        .samples = 4096,
        .callback = audio_callback,
        .userdata = config,
    };
    sdl->dev = SDL_OpenAudioDevice(NULL, 0, &sdl->want, &sdl->have, 0);

    if (sdl->dev == 0){
        SDL_Log("Could not get and Audio Device %s\n", SDL_GetError());
        return false;
    }

    if ((sdl->want.channels != sdl->have.channels) || (sdl->want.format != sdl->have.format)){
        SDL_Log("could not get desired auido spec\n");
        return false;
    } 

    return true;
}

void final_cleanup(const sdl_t sdl){
    SDL_DestroyRenderer(sdl.renderer);
    SDL_DestroyWindow(sdl.window);
    SDL_CloseAudioDevice(sdl.dev);
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
        .pixel_outlines = true,
        .insts_per_sec = 500,
        .sq_wave_freq = 440,
        .audio_sample_rate = 44100,
        .volume = 3000,
        .color_lerp_rate = 0.7,
    
    };

    //overrides from the argcs
    for (int i = 1; i < argc; i++)
    {
        (void)argv[i];

        if (strncmp(argv[i], "--scale-factor", strlen("--scale-factor")) == 0){
            i++;
            config->scale_factor = (uint32_t)strtol(argv[i], NULL, 10);
        } 
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

void update_screen(const sdl_t sdl,const config_t config,  chip8_t *chip8){
    SDL_Rect rect= {.x = 0, .y = 0, .w = config.scale_factor, .h =config.scale_factor};

    // const uint8_t bg_r2 = (config.bg >> 24) & 0xFF;
    // const uint8_t bg_g2 = (config.bg >> 16) & 0xFF;
    // const uint8_t bg_b2 = (config.bg >> 8) & 0xFF;
    // const uint8_t bg_a2 = (config.bg >> 0) & 0xFF;
    // SDL_SetRenderDrawColor(sdl.renderer, bg_r2, bg_g2, bg_b2, bg_a2);
    // SDL_RenderClear(sdl.renderer);


    const uint8_t bg_r = (config.bg >> 24) & 0xFF;
    const uint8_t bg_g = (config.bg >> 16) & 0xFF;
    const uint8_t bg_b = (config.bg >> 8) & 0xFF;
    const uint8_t bg_a = (config.bg >> 0) & 0xFF;

    for (uint32_t i = 0; i < sizeof chip8->display; i++){
        rect.x = (i % config.window_width) * config.scale_factor;
        rect.y = (i / config.window_width) * config.scale_factor;
        if (chip8->display[i]){

            if (chip8->pixel_colors[i] != config.fg){
                chip8->pixel_colors[i] = color_lerp(chip8->pixel_colors[i], config.fg, config.color_lerp_rate); 
            }
            const uint8_t r = (chip8->pixel_colors[i] >> 24) & 0xFF;
            const uint8_t g = (chip8->pixel_colors[i] >> 16) & 0xFF;
            const uint8_t b = (chip8->pixel_colors[i] >> 8) & 0xFF;
            const uint8_t a = (chip8->pixel_colors[i] >> 0) & 0xFF;

            SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
            SDL_RenderFillRect(sdl.renderer, &rect);

        if (config.pixel_outlines){
            SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b,bg_a);
            SDL_RenderDrawRect(sdl.renderer, &rect);

            }

        }else {
            if (chip8->pixel_colors[i] != config.bg){
                chip8->pixel_colors[i] = color_lerp(chip8->pixel_colors[i], config.bg, config.color_lerp_rate); 
            }
            const uint8_t r = (chip8->pixel_colors[i] >> 24) & 0xFF;
            const uint8_t g = (chip8->pixel_colors[i] >> 16) & 0xFF;
            const uint8_t b = (chip8->pixel_colors[i] >> 8) & 0xFF;
            const uint8_t a = (chip8->pixel_colors[i] >> 0) & 0xFF;

            SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        
        }
    }
    SDL_RenderPresent(sdl.renderer);
}

void handle_input(chip8_t *chip8, config_t *config){
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

                    case SDLK_SPACE:
                        if (chip8->state == RUNNING ){
                            chip8->state = PAUSED;
                            puts("====PAUSED====");
                        }
                        else{
                            chip8->state = RUNNING;
                        }
                        return;

                    case SDLK_EQUALS:
                        init_chip8(chip8, *config, chip8->rom_name);
                        break;

                    case SDLK_j:
                        if (config->color_lerp_rate > 0.1) config->color_lerp_rate -= 0.1;
                        break;

                    case SDLK_k:
                        if (config->color_lerp_rate < 0.1) config->color_lerp_rate += 0.1;
                        break;

                    case SDLK_o:
                        if (config->volume > 0) config->volume -= 500;
                        break;

                    case SDLK_p:
                        if (config->volume < INT16_MAX) config->volume += 500;
                        break;

                    case SDLK_1: chip8->keypad[0x1] = true; break;
                    case SDLK_2: chip8->keypad[0x2] = true; break;
                    case SDLK_3: chip8->keypad[0x3] = true; break;
                    case SDLK_4: chip8->keypad[0xC] = true; break;

                    case SDLK_q: chip8->keypad[0x4] = true; break;
                    case SDLK_w: chip8->keypad[0x5] = true; break;
                    case SDLK_e: chip8->keypad[0x6] = true; break;
                    case SDLK_r: chip8->keypad[0xD] = true; break;

                    case SDLK_a: chip8->keypad[0x7] = true; break;
                    case SDLK_s: chip8->keypad[0x8] = true; break;
                    case SDLK_d: chip8->keypad[0x9] = true; break;
                    case SDLK_f: chip8->keypad[0xE] = true; break;

                    case SDLK_z: chip8->keypad[0xA] = true; break;
                    case SDLK_x: chip8->keypad[0x0] = true; break;
                    case SDLK_c: chip8->keypad[0xB] = true; break;
                    case SDLK_v: chip8->keypad[0xF] = true; break;
                    default: break;
                }
                break;

            case SDL_KEYUP:
                switch (event.key.keysym.sym){
                    case SDLK_1: chip8->keypad[0x1] = false; break;
                    case SDLK_2: chip8->keypad[0x2] = false; break;
                    case SDLK_3: chip8->keypad[0x3] = false; break;
                    case SDLK_4: chip8->keypad[0xC] = false; break;

                    case SDLK_q: chip8->keypad[0x4] = false; break;
                    case SDLK_w: chip8->keypad[0x5] = false; break;
                    case SDLK_e: chip8->keypad[0x6] = false; break;
                    case SDLK_r: chip8->keypad[0xD] = false; break;

                    case SDLK_a: chip8->keypad[0x7] = false; break;
                    case SDLK_s: chip8->keypad[0x8] = false; break;
                    case SDLK_d: chip8->keypad[0x9] = false; break;
                    case SDLK_f: chip8->keypad[0xE] = false; break;

                    case SDLK_z: chip8->keypad[0xA] = false; break;
                    case SDLK_x: chip8->keypad[0x0] = false; break;
                    case SDLK_c: chip8->keypad[0xB] = false; break;
                    case SDLK_v: chip8->keypad[0xF] = false; break;

                    default: break;
                }
                break;
            }
        }

    }



//Init chip8 machine
bool init_chip8(chip8_t *chip8, const config_t config, const char rom_name[]){
    const uint32_t entry_point = 0x200;
    const uint8_t font[] = {
        
        0xF0, 0x90, 0x90, 0x90, 0xF0,   // 0   
        0x20, 0x60, 0x20, 0x20, 0x70,   // 1  
        0xF0, 0x10, 0xF0, 0x80, 0xF0,   // 2 
        0xF0, 0x10, 0xF0, 0x10, 0xF0,   // 3
        0x90, 0x90, 0xF0, 0x10, 0x10,   // 4    
        0xF0, 0x80, 0xF0, 0x10, 0xF0,   // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0,   // 6
        0xF0, 0x10, 0x20, 0x40, 0x40,   // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0,   // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0,   // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90,   // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0,   // B
        0xF0, 0x80, 0x80, 0x80, 0xF0,   // C
        0xE0, 0x90, 0x90, 0x90, 0xE0,   // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0,   // E
        0xF0, 0x80, 0xF0, 0x80, 0x80,   // F
    };
    // initialize entire chip8 machine
    memset(chip8, 0, sizeof(chip8_t));

    //load font
    memcpy(&chip8->ram[0], font, sizeof(font));

    //load rom to memory
    FILE *rom = fopen(rom_name, "rb");
    if (!rom){
        SDL_Log("rom file %s is invalid or does not exist\n", rom_name);
        return false;
    }
    fseek(rom, 0, SEEK_END);
    long rom_size = ftell(rom);
    long max_size = sizeof(chip8->ram) - entry_point;
    rewind(rom);

    if (rom_size > max_size ){
        SDL_Log("rom file %s is too big Rom size: %ld, max size: %ld \n", rom_name, rom_size, max_size);
        return false;
        
    }

    if (fread(&chip8->ram[entry_point], rom_size, 1, rom) != 1){
        SDL_Log("could not read rom file %s\n", rom_name);
        return false;

    } 
    fclose(rom);
    
    //set defaults
    chip8->state = RUNNING;
    chip8->PC = entry_point;
    chip8->rom_name = rom_name;
    chip8->stack_pt = &chip8->stack[0];
    memset(&chip8->pixel_colors[0], config.bg, sizeof(chip8->pixel_colors));

    return true;
}

#ifdef DEBUG
void print_debug_info(chip8_t *chip8){
    printf("Adress: 0x%04X, Opcode: 0x%04X Desc:\n", chip8->PC-2, chip8->inst.opcode);
    switch((chip8->inst.opcode >> 12) & 0x0F){
        case 0x00:
            if (chip8->inst.NN == 0xE0){
                printf("clear screen\n");

            }else if (chip8->inst.NN == 0xEE){
                printf("Return from subroutine to address 0x%04X\n", *(chip8->stack_pt - 1));
                
            }else {
                printf("Unimplemented Opcode.\n");

            }
            break;

        case 0x01:
            printf("Jump to adress NNN (0x%04X)\n", chip8->inst.NNN);
            break;

        case 0x02:
            printf("Call subroutine at NNN (0x%04X)\n", chip8->inst.NNN);
            break;

        case 0x03:
            printf("Check if V%X (0x%02X) == NN (0x%02X), skip next instruction if true\n", chip8->inst.X, chip8->V[chip8->inst.NN], chip8->inst.NN);
            break;

        case 0x04:
            printf("Check if V%X (0x%02X) != NN (0x%02X), skip next instruction if true\n", chip8->inst.X, chip8->V[chip8->inst.NN], chip8->inst.NN);
            break;

        case 0x05:
            printf("Check if V%X (0x%02X) == V%X (0x%02X), skip next instruction if true\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.X]);
            break;

        case 0x06:
            printf("Set register V%X to NN 0x%02X\n",
            chip8->inst.X , chip8->inst.NN);
            break;
        case 0x07:
            printf("Set register V%X (0x%02X)+= NN (0x%02X), result: 0x%02X\n",
            chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NN, chip8->V[chip8->inst.X] + chip8->inst.NN );
            break;

        case 0x08:
            switch (chip8->inst.N) {
            case 0:
                printf("Set register V%X = V%X (0x%02X) \n",
                chip8->inst.X , chip8->inst.Y, chip8->V[chip8->inst.Y]);
                break;
            case 1:
                printf("Set register V%X (0x%02X)|= V%X (0x%02X); Result: 0x%02X \n",
                chip8->inst.X, chip8->V[chip8->inst.X],
                chip8->inst.Y, chip8->V[chip8->inst.Y],
                chip8->V[chip8->inst.X] | chip8->V[chip8->inst.Y]);
                break;
            case 2:
                printf("Set register V%X (0x%02X)&= V%X (0x%02X); Result: 0x%02X \n",
                chip8->inst.X, chip8->V[chip8->inst.X],
                chip8->inst.Y, chip8->V[chip8->inst.Y],
                chip8->V[chip8->inst.X] & chip8->V[chip8->inst.Y]);
                break;
            case 3:
                printf("Set register V%X (0x%02X)^= V%X (0x%02X); Result: 0x%02X \n",
                chip8->inst.X, chip8->V[chip8->inst.X],
                chip8->inst.Y, chip8->V[chip8->inst.Y],
                chip8->V[chip8->inst.X] ^ chip8->V[chip8->inst.Y]);
                break;
            case 4:
                printf("Set register V%X (0x%02X)+= V%X (0x%02X), VF = 1 if carry; Result: 0x%02X, VF = %X\n",
                chip8->inst.X, chip8->V[chip8->inst.X],
                chip8->inst.Y, chip8->V[chip8->inst.Y],
                chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y],
                ((uint16_t) chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y] > 255)
                );
                break;
                    
            case 5:
                printf("Set register V%X (0x%02X) -= V%X (0x%02X), VF = 1 if carry; Result: 0x%02X, VF = %X\n",
                chip8->inst.X, chip8->V[chip8->inst.X],
                chip8->inst.Y, chip8->V[chip8->inst.Y],
                chip8->V[chip8->inst.X] - chip8->V[chip8->inst.Y],
                ( chip8->V[chip8->inst.Y] <= chip8->V[chip8->inst.X]));
                break;
            case 6:
                printf("Set register V%X (0x%02X) >>= 1, VF = shifted off bit (%X); Result: 0x%02X\n",
                chip8->inst.X, chip8->V[chip8->inst.X],
                chip8->V[chip8->inst.X] & 1,
                chip8->V[chip8->inst.X] >> 1);
                break;
            case 7:
                printf("Set register V%X = V%X (0x%02X) - V%X (0x%02X), VF = 1 if no borrow; Result: 0x%02X, VF = %X\n",
                chip8->inst.X, chip8->inst.Y, chip8->V[chip8->inst.Y],
                chip8->inst.X, chip8->V[chip8->inst.X],
                chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X],
                ( chip8->V[chip8->inst.X] <= chip8->V[chip8->inst.Y]));
                break;
            case 0xE:
                printf("Set register V%X (0x%02X) <<= 1, VF = shifted off bit (%X); Result: 0x%02X\n",
                chip8->inst.X, chip8->V[chip8->inst.X],
                (chip8->V[chip8->inst.X] & 0x80) >> 7,
                chip8->V[chip8->inst.X] << 1);
                break;
                
            default:
                break;
            }
            break;

        case 0x09:
            printf("Check if V%X (0x%02X) != V%X (0x%02X), skip next instruction if true\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.X]);
            break;

        case 0x0A:
            printf("Set I to NNN (0x%04X)\n", chip8->inst.NNN);
            break;

        case 0x0B:
            printf("Set PC to V%X (0x%02X) + NNN (0x%04X); Result PC = 0x%04X\n",
             chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.NNN, chip8->V[chip8->inst.X] + chip8->inst.NNN);
            break;

        case 0x0C:
            printf("Set V%X = rand() %% 256 & NN (0x%02X)\n",
            chip8->inst.X, chip8->inst.NN);
            break;
        case 0x0D:
            printf("Draw N (%u) height sprite at V%X (0x%02X), V%X (0x%02X)  from memory loc I (0x%04X), Set VF = 1 if any pixels are turned off\n", chip8
                   ->inst.N, chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, chip8->V[chip8->inst.Y], chip8->I);
            break;

        case 0x0E:
            if (chip8->inst.NN == 0x9E){
                printf("Skip next instruction if key in V%X (0x%02X) is pressed; keypad value: %d\n",
                       chip8->inst.X, chip8->V[chip8->inst.X], chip8->keypad[chip8->V[chip8->inst.X]]);
            }
            else if (chip8->inst.NN == 0xA1){
                printf("Skip next instruction if key in V%X (0x%02X) is not pressed; keypad value: %d\n",
                       chip8->inst.X, chip8->V[chip8->inst.X], chip8->keypad[chip8->V[chip8->inst.X]]);

            }
            break;

        case 0x0F:
            switch (chip8->inst.NN) {
                case 0x0A:
                    printf("Await until a key is pressed; Store key in V%X\n", chip8->inst.X);
                    break;
                case 0x1E:
                    printf("I (0x%04X) += V%X (0x%02X) Result: (I): 0x%04X\n", chip8->I, chip8->inst.X, chip8->V[chip8->inst.X], chip8->I + chip8->V[chip8->inst.X]);
                    break;
                case 0x07:
                    printf("Set V%X = delay timer value (0x%02X)\n", 
                    chip8->inst.X, chip8->delay_timer);
                    break;
                case 0x15:
                    printf("Set delay timer value = V%X (0x%02X)\n", chip8->inst.X, chip8->V[chip8->inst.X]);
                    break;

                case 0x18:
                    printf("Set sound timer value = V%X (0x%02X)\n", chip8->inst.X, chip8->V[chip8->inst.X]);
                    break;

                case 0x29:
                    printf("Set I to location in memory for character in V%X (0x%02X). Result(VX*5) = (0x%02X)\n",chip8->inst.X, chip8->V[chip8->inst.X], chip8->V[chip8->inst.X] * 5);
                    break;

                case 0x33:
                    printf("Store BCD represntation of V%X (0x%02X) at memory from I (0x%04X)\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->I);
                    break;
                
                case 0x55:
                    printf("Register dump V0-V%X inclusive (0x%02X) at memory from I (0x%04X)\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->I);
                    break;

                case 0x65:
                    printf("Register load V0-V%X inclusive (0x%02X) at memory from I (0x%04X)\n", chip8->inst.X, chip8->V[chip8->inst.X], chip8->I);
                    break;
                    
            }
            break;
        default:
            printf("Unimplemented Opcode.\n");
            break;

    }


}

#endif

void emulate_instructions(chip8_t *chip8, const config_t config){
    chip8->inst.opcode = (chip8->ram[chip8->PC] << 8) | chip8-> ram[chip8->PC +1];
    chip8->PC += 2;

    //fill out inst format
    chip8 -> inst.NNN = chip8->inst.opcode & 0x0FFF;
    chip8 -> inst.NN = chip8->inst.opcode & 0x0FF;
    chip8 -> inst.N = chip8->inst.opcode & 0x0F;
    chip8 -> inst.X = (chip8->inst.opcode >> 8) & 0x0F;
    chip8 -> inst.Y = (chip8->inst.opcode >> 4) & 0x0F; 

#ifdef DEBUG
    print_debug_info(chip8);
#endif


    //Emulate
    switch((chip8->inst.opcode >> 12) & 0x0F){
        case 0x00:
            if (chip8->inst.NN == 0xE0){
                memset(&chip8->display[0], false, sizeof(chip8->display));
                chip8->draw = true;

            }else if (chip8->inst.NN == 0xEE){
                chip8->PC = *--chip8->stack_pt;
                
            }else {
            
            }
            break;

        case 0x01:
            chip8->PC = chip8->inst.NNN;
            break;

        case 0x02:
            *chip8->stack_pt++ = chip8->PC;
            chip8->PC = chip8->inst.NNN;
            break;

        case 0x03:
            if (chip8->V[chip8->inst.X] == chip8->inst.NN){
                chip8->PC += 2;
            }
            break;

        case 0x04:
            if (chip8->V[chip8->inst.X] != chip8->inst.NN){
                chip8->PC += 2;
            }
            break;

        case 0x05:
            if (chip8->inst.N !=0) break;
            if (chip8->V[chip8->inst.X] == chip8->V[chip8->inst.Y]){
                chip8->PC += 2;
            }
            break;

        case 0x06:
            chip8->V[chip8->inst.X] = chip8->inst.NN;
            break;

        case 0x07:
            chip8->V[chip8->inst.X] += chip8->inst.NN;
            break;

        case 0x08:
            switch (chip8->inst.N) {
            case 0:
                chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y];
                break;
            case 1:
                chip8->V[chip8->inst.X] |= chip8->V[chip8->inst.Y];
                break;
            case 2:
                chip8->V[chip8->inst.X] &= chip8->V[chip8->inst.Y];
                break;
            case 3:
                chip8->V[chip8->inst.X] ^= chip8->V[chip8->inst.Y];
                break;
            case 4:
                {
                    uint16_t sum = chip8->V[chip8->inst.X] + chip8->V[chip8->inst.Y];
                    chip8->V[chip8->inst.X] = sum & 0xFF;
                    chip8->V[0xF] = (sum > 0xFF);
                }
                break;
                    
            case 5:
                {
                    uint8_t flag = (chip8->V[chip8->inst.Y]) <= (chip8->V[chip8->inst.X]);
                    chip8->V[chip8->inst.X] -= chip8->V[chip8->inst.Y];
                    chip8->V[0xF] = flag;
                }
                break;
            case 6:
                {
                    uint8_t flag = chip8->V[chip8->inst.X] & 1;
                    chip8->V[chip8->inst.X] >>= 1;
                    chip8->V[0xF] = flag;
                }
                break;
            case 7:
                {
                    uint8_t flag = (chip8->V[chip8->inst.Y]) >= chip8->V[chip8->inst.X];
                    chip8->V[chip8->inst.X] = chip8->V[chip8->inst.Y] - chip8->V[chip8->inst.X];
                    chip8->V[0xF] = flag;
                }
                break;
            case 0xE:
                {
                    uint8_t flag = (chip8->V[chip8->inst.X] & 0x80) >> 7;
                    chip8->V[chip8->inst.X] <<= 1;
                    chip8->V[0xF] = flag;
                }
                break;
                
            default:
                break;
            }
            break;

        case 0x09:
            //if (chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y])
            //    chip8->PC += 2;
            //break;
            if (chip8->V[chip8->inst.X] != chip8->V[chip8->inst.Y]){
                chip8->PC += 2;
            }
            break;
            

        case 0x0A:
            chip8 -> I = chip8->inst.NNN;
            break;

        case 0x0B:
            chip8->PC = chip8->V[chip8->inst.X] + chip8->inst.NNN;
            break;

        case 0x0C:
            chip8->V[chip8->inst.X] = (rand() % 256) & chip8->inst.NN;
            break;

        case 0x0D:
            uint8_t X_cord = chip8->V[chip8->inst.X] % config.window_width;
            uint8_t Y_cord = chip8->V[chip8->inst.Y] % config.window_height;
            const uint8_t orig_X = X_cord;
            chip8->V[0xF] = 0;

            for (uint8_t i = 0; i < chip8->inst.N; i++){
                const uint8_t sprite_data = chip8->ram[chip8->I + i];
                X_cord = orig_X;

                for (int j = 7; j >= 0; j--){
                    bool *pixel = &chip8->display[Y_cord * config.window_width + X_cord];
                    const bool sprite_bit =sprite_data & (1 << j); 
                    if (sprite_bit && *pixel)
                    {
                        chip8->V[0XF] = 1;

                    }
                    *pixel ^= sprite_bit;

                    if (++X_cord >= config.window_width) break;
                }
                if (++Y_cord >= config.window_height) break;

            }
            chip8->draw = true;
            break;
        case 0x0E:
            if (chip8->inst.NN == 0x9E){
                if (chip8->keypad[chip8->V[chip8->inst.X]])
                    chip8->PC += 2;
            }
            else if (chip8->inst.NN == 0xA1){
                if (!chip8->keypad[chip8->V[chip8->inst.X]])
                    chip8->PC += 2;

            }
            break;
        case 0x0F:
            switch (chip8->inst.NN) {
                case 0x0A:
                    chip8->PC -= 2;
                    if (chip8->key_waiting) {
                        if (!chip8->keypad[chip8->V[chip8->inst.X]]) {
                            chip8->key_waiting = false;
                            chip8->PC += 2;
                        }
                    } else {
                        for (uint8_t i = 0; i < sizeof chip8->keypad; i++){
                            if (chip8->keypad[i]) {
                                chip8->V[chip8->inst.X] = i;
                                chip8->key_waiting = true;
                                break;
                            }
                        }
                    }
                    break;
                case 0x1E:
                    chip8->I += chip8->V[chip8->inst.X];
                    break;
                    
                case 0x07:
                    chip8->V[chip8->inst.X] = chip8->delay_timer;
                    break;

                case 0x15:
                     chip8->delay_timer = chip8->V[chip8->inst.X];            
                    break;

                case 0x18:
                     chip8->sound_timer = chip8->V[chip8->inst.X];            
                    break;

                case 0x29:
                    chip8->I = chip8->V[chip8->inst.X] * 5;
                    break;

                case 0x33:
                    uint8_t bcd = chip8->V[chip8->inst.X];
                    chip8->ram[chip8->I+2] = bcd % 10;
                    bcd /= 10;
                    chip8->ram[chip8->I+1] = bcd % 10;
                    bcd /= 10;
                    chip8->ram[chip8->I] = bcd;
                    break;

                case 0x55:
                    for (uint8_t i = 0; i <= chip8->inst.X; i++){
                        chip8->ram[chip8->I + i] = chip8->V[i];
                    }
                    chip8->I += chip8->inst.X;
                    break;

                case 0x65:
                    for (uint8_t i = 0; i <= chip8->inst.X; i++){
                        chip8->V[i] = chip8->ram[chip8->I + i];
                    }
                    chip8->I += chip8->inst.X;
                    break;
            default:
                break;
            }

            break;
    }

}

void update_timers(const sdl_t sdl, chip8_t *chip8){
    if (chip8->delay_timer > 0) chip8->delay_timer--;
    
    if (chip8->sound_timer > 0) {
        chip8->sound_timer--;
        SDL_PauseAudioDevice(sdl.dev, 0);
    }
    else{
        SDL_PauseAudioDevice(sdl.dev, 1);
    }
     
}

int main(int argc, char **argv){
    // Defalut messages
    if (argc < 2){
        fprintf(stderr,"Usage: %s <rom_name>\n", argv[0]); 
        exit(EXIT_FAILURE);
    }
    // init emulator config_t
    config_t config = {0};
    if (!set_config_from_args(&config,  argc, argv)) exit(EXIT_FAILURE);

    sdl_t sdl = {0};
    if (!init_sdl(&sdl, &config)){
        exit(EXIT_FAILURE);
    }
    //init chip8 machine
    chip8_t chip8 = {0};
    const char *rom_name = argv[1];
    if (!init_chip8(&chip8, config, rom_name)) exit(EXIT_FAILURE);

    //Init screen clear
    clear_screen(sdl, config);

    //Seed random generator
    srand(time(NULL));

    //Main emulator loop
    while (chip8.state != QUIT){
        //handle input
        handle_input(&chip8, &config);

        if (chip8.state == PAUSED) continue;

        //Get time before instructions
        uint64_t before_frame = SDL_GetPerformanceCounter();

        //Emulate instructions for this emulator "frame" (60hz)
        for (uint32_t i = 0; i < config.insts_per_sec / 60; i++){
        emulate_instructions(&chip8, config);}

        //Get time after instructions
        uint64_t after_frame = SDL_GetPerformanceCounter();

        double time_elapsed = (double)((after_frame - before_frame) * 1000) / SDL_GetPerformanceFrequency(); 

        //Delay for 60hz
        SDL_Delay(16.67f > time_elapsed ? 16.67f - time_elapsed : 0);

        //update_screen(sdl, config, chip8);
        //update window with changes
       if (chip8.draw){
           update_screen(sdl, config, &chip8);
           chip8.draw = false;
       }
        //update delay and sound timers 
        update_timers(sdl, &chip8);

    }
    //Final cleanup
    final_cleanup(sdl);
    
    exit(EXIT_SUCCESS);
}
