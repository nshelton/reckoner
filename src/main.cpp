#include <iostream>
#include "app/App.h"
#include "screens/MainScreen.h"

int main()
{
    try {
        App app(1280, 800, "Reckoner");
        MainScreen screen;
        app.run(screen);
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
