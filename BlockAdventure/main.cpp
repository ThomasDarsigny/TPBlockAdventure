#include <SFML/Window.hpp>
#include <iostream>
#include "engine.h"

int main()
{
    Engine engine;
    engine.SetMaxFps(144);
    engine.Start("BlockAdventure", 800, 600, false);
}
