#include <iostream>
#include <curl/curl.h>
#include "app/App.h"
#include "screens/MainScreen.h"

int main()
{
    // Must init curl globally before any threads use it
    curl_global_init(CURL_GLOBAL_ALL);

    try {
        App app(1920, 1080, "Reckoner");
        MainScreen screen;
        app.run(screen);
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        curl_global_cleanup();
        return 1;
    }

    curl_global_cleanup();
    return 0;
}
