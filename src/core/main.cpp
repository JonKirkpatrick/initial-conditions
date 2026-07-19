#include <SFML/Graphics.hpp>
           
#include "core/GameEngine.h"
           
int main()
{
    GameEngine g("assets.txt");
    g.run();
}