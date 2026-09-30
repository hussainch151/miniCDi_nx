#include <SDL2/SDL.h>
#include <switch.h>


int main(int argc, char* argv[]) {

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER);

    SDL_Quit();
    return 0;
}
