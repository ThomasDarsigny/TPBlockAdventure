#include <cstdlib>
#include <cstring>
#include <iostream>
#include "engine.h"

// Options de ligne de commande (utiles pour tester rapidement une situation) :
//   --shot <s>            capture l'ecran apres <s> secondes puis quitte
//   --tp <x> <y> <z>      demarre a cette position
//   --look <lacet> <site> oriente la camera
//   --time <0..1>         heure du jour (0 = minuit, 0.5 = midi)
//   --creative            demarre en mode creatif, en vol
int main(int argc, char** argv)
{
    Engine engine;
    engine.SetMaxFps(144);

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--shot") == 0 && i + 1 < argc)
            engine.SetAutoScreenshot((float)atof(argv[++i]));
        else if (std::strcmp(argv[i], "--tp") == 0 && i + 3 < argc)
        {
            engine.SetStartPosition((float)atof(argv[i + 1]), (float)atof(argv[i + 2]), (float)atof(argv[i + 3]));
            i += 3;
        }
        else if (std::strcmp(argv[i], "--look") == 0 && i + 2 < argc)
        {
            engine.SetStartLook((float)atof(argv[i + 1]), (float)atof(argv[i + 2]));
            i += 2;
        }
        else if (std::strcmp(argv[i], "--time") == 0 && i + 1 < argc)
            engine.SetStartTime((float)atof(argv[++i]));
        else if (std::strcmp(argv[i], "--creative") == 0)
            engine.SetCreativeStart();
        else if (std::strcmp(argv[i], "--debug") == 0)
            engine.SetDebugOverlay();
        else if (std::strcmp(argv[i], "--map") == 0)
            engine.SetDumpMap();
        else if (std::strcmp(argv[i], "--fstest") == 0 && i + 1 < argc)
            engine.SetFullscreenTest((float)atof(argv[++i]));
        else if (std::strcmp(argv[i], "--fluidtest") == 0)
            engine.SetFluidTest();
        else if (std::strcmp(argv[i], "--crafttest") == 0)
            engine.SetCraftTest();
        else if (std::strcmp(argv[i], "--mobtest") == 0)
            engine.SetMobTest();
    }

    engine.Start("BlockAdventure", 1280, 720, false);
    return 0;
}
