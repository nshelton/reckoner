#include "app/Screen.h"
#include "AppModel.h"
#include "screens/MainScreen.h"

extern "C" {

IScreen* create_screen(AppModel* model)
{
    return new MainScreen(model);
}

void destroy_screen(IScreen* screen)
{
    delete screen;
}

} // extern "C"
