//windows only

//Create the widnow and initialize all related stuff
// currently in main.cpp
extern bool GfxWinCreate(game* g);

//update the window
// currently put at the end of game::handle_action in game.cpp
extern void GfxDraw(int destx=0, int desty=0, int centerx=-1, int centery=-1, int width=-1, int height=-1);
GfxDraw();