#include "vk_core/bootstrap/bootstrap.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

using namespace lcf;

int main()
{
    vkc::bs::BootstrapConfig config;
    auto context = vkc::bs::bootstrap(config);
    SDL_Window * w_p;
    
    
    return 0;
}