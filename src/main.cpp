#include "renderer.h"

#include <cstdlib>

int main()
{
    Renderer renderer;
    if (!renderer.init()) {
        return EXIT_FAILURE;
    }
    renderer.run();
    return EXIT_SUCCESS;
}