#if (defined ANDROID)
#include "catacurse.h"
#include "options.h"
#include "output.h"
#include "color.h"
#include "debug.h"
#include <cstdlib>
#include <fstream>
#include <unistd.h>
#include "SDL.h"

//***********************************
//Globals                           *
//***********************************

#define ActualWidth 800
#define ActualHeight 480
#define XOffset 80


WINDOW *mainwin;
SDL_Window* window = NULL;
static SDL_Color windowsPalette[256];
static SDL_Texture *glyph_font = NULL;
static SDL_Texture *ui_bg = NULL;
static SDL_Texture *backbuffer = NULL;
SDL_Renderer* renderer = NULL;
static int textureWidth, textureHeight;
int nativeWidth;
int nativeHeight;
int WindowX;            //X pos of the actual window, not the curses window
int WindowY;            //Y pos of the actual window, not the curses window
int WindowWidth;        //Width of the actual window, not the curses window
int WindowHeight;       //Height of the actual window, not the curses window
int lastchar;          //the last character that was pressed, resets in getch
int inputdelay;         //How long getch will wait for a character to be typed
//WINDOW *_windows;  //Probably need to change this to dynamic at some point
//int WindowCount;        //The number of curses windows currently in use
int fontwidth;          //the width of the font, background is always this size
int fontheight;         //the height of the font, background is always this size
int halfwidth;          //half of the font width, used for centering lines
int halfheight;          //half of the font height, used for centering lines
pairs *colorpairs;   //storage for pair'ed colored, should be dynamic, meh
int echoOn;     //1 = getnstr shows input, 0 = doesn't show. needed for echo()-ncurses compatibility.

#define KEYS "<>1234567890abcdefghijklmnopqrstuvwxyz,./?;':\"[]{}!@#$%^&*()-=_+\\|\t"
#define GRIDSIZE 80
#define GRIDX (ActualWidth/GRIDSIZE)
#define GRIDY (ActualHeight/GRIDSIZE)
#define GRID(x,y)  ((int)((y)*GRIDY)*GRIDX+(int)((x)*GRIDX))
/*
0  1  2  3  4  5  6  7  8  9 
10 11 12 13 14 15 16 17 18 19
20 21 22 ....
...
50 51 52 53 54 55 56 57 58 59

*/

static int gridshow = 0;
static unsigned long gridtime = 0;
static int holdkey = 0;
static unsigned long holdtime = 0;
static bool repeatn = false;

/*
Prev                 Next
0                    cap
1                    esc
2                   return
3                   space
4 5 6 7 8 9 10 11 12 del

*/
static char keys[256] = {KEYS};
#define KEYPAGESIZE 13
#define MAXKEYPAGE (totalkeys/KEYPAGESIZE + ((totalkeys%KEYPAGESIZE)!=0))
static int keypage = 0;
static bool keycaps = false;
int totalkeys = (sizeof(KEYS)-1);


static float debugx = 0.0f;
static float debugy = 0.0f;


static unsigned long lastupdate = 0;
static unsigned long interval = 25;
static bool needupdate = false;

//***********************************
//Non-curses, Window functions      *
//***********************************

void ClearScreen()
{
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
}


bool fexists(const char *filename)
{
  std::ifstream ifile(filename);
  return ifile;
}

//The following 3 methods use mem functions for fast drawing
inline void VertLineDIB(int x, int y, int y2, int thickness, unsigned char color)
{
	SDL_Rect rect;
	rect.x = x+XOffset;
	rect.y = y;
	rect.w = thickness;
	rect.h = y2-y;
    SDL_SetRenderDrawColor(renderer, windowsPalette[color].r,windowsPalette[color].g,windowsPalette[color].b, 255);
    SDL_RenderFillRect(renderer, &rect);
};
inline void HorzLineDIB(int x, int y, int x2, int thickness, unsigned char color)
{
	SDL_Rect rect;
	rect.x = x+XOffset;
	rect.y = y;
	rect.w = x2-x;
	rect.h = thickness;
    SDL_SetRenderDrawColor(renderer, windowsPalette[color].r,windowsPalette[color].g,windowsPalette[color].b, 255);
    SDL_RenderFillRect(renderer, &rect);
};
inline void FillRectDIB(int x, int y, int width, int height, unsigned char color)
{
	SDL_Rect rect;
	rect.x = x+XOffset;
	rect.y = y;
	rect.w = width;
	rect.h = height;
    SDL_SetRenderDrawColor(renderer, windowsPalette[color].r,windowsPalette[color].g,windowsPalette[color].b, 255);
    SDL_RenderFillRect(renderer, &rect);
};

static void OutputChar(char t, int x, int y, unsigned char color)
{
    unsigned char ch = t & 0x7f;
    color &= 0xf;

	SDL_Rect rect;
	rect.x = x+XOffset;
	rect.y = y;
	rect.w = fontwidth;
	rect.h = fontheight;
	SDL_Rect srect;
	srect.x = (ch%16)*fontwidth;
	srect.y = (ch/16)*fontheight;
	srect.w = fontwidth;
	srect.h = fontheight;

    SDL_SetTextureColorMod(glyph_font, windowsPalette[color].r,windowsPalette[color].g,windowsPalette[color].b);
    SDL_RenderCopy(renderer, glyph_font, &srect, &rect);
}

static char btn_key(int i)
{
	char c = keys[(keypage*KEYPAGESIZE+i)%totalkeys];
	if(keycaps && c>='a' && c<='z') c += 'A'-'a';
	return c;
}

static char btn_input(int i)
{
	static SDL_Rect rects[] = {
		{0,80,80,80},
		{0,160,80,80},
		{0,240,80,80},
		{0,320,80,80},
		{0,400,80,80},
		{80,400,80,80},
		{160,400,80,80},
		{240,400,80,80},
		{320,400,80,80},
		{400,400,80,80},
		{480,400,80,80},
		{560,400,80,80},
		{640,400,80,80}
	};

	SDL_Rect rect = rects[i];

	char c = btn_key(i);

	if(i%2==0) SDL_SetRenderDrawColor(renderer, 224, 224, 224, 255);
	else SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

	SDL_RenderFillRect(renderer, &rect);
	
	if(c=='\t')
	{
		OutputChar('T', rect.x-80+36-8, rect.y+32, 3);
		OutputChar('a', rect.x-80+36, rect.y+32, 3);
		OutputChar('b', rect.x-80+36+8, rect.y+32, 3);
	}
	else OutputChar(c, rect.x-80+36, rect.y+32, 3);

	return c;
}

static void btn_refresh()
{
    SDL_SetTextureColorMod(glyph_font, 255,255,255);
	SDL_Rect srect, rect;
	srect.x = 128; srect.y = 0; srect.w = srect.h = 80;
	rect.x = 0; rect.y = 0; rect.w = rect.h = 80;
	SDL_RenderCopy(renderer, glyph_font, &srect, &rect);
	srect.x = 128; srect.y = 80;
	rect.x = 720; rect.y = 0;
	SDL_RenderCopy(renderer, glyph_font, &srect, &rect);
	srect.x = keycaps?208:288; srect.y = 80;
	rect.x = 720; rect.y = 80;
	SDL_RenderCopy(renderer, glyph_font, &srect, &rect);
	srect.x = 288; srect.y = 0;
	rect.x = 720; rect.y = 160;
	SDL_RenderCopy(renderer, glyph_font, &srect, &rect);
	srect.x = 208; srect.y = 0;
	rect.x = 720; rect.y = 240;
	SDL_RenderCopy(renderer, glyph_font, &srect, &rect);
	srect.x = 368; srect.y = 0;
	rect.x = 720; rect.y = 320;
	SDL_RenderCopy(renderer, glyph_font, &srect, &rect);
	srect.x = 368; srect.y = 80;
	rect.x = 720; rect.y = 400;
	SDL_RenderCopy(renderer, glyph_font, &srect, &rect);

	for(int i=0; i<KEYPAGESIZE; i++)
		btn_input(i);
}

#define nextpowerof2(x) pow(2,ceil(log(((double)x))/log(2.0)))
//Registers, creates, and shows the Window!!
bool WinCreate()
{
	SDL_DisplayMode mode;
	int init_flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK;
	const char *var = SDL_getenv("SDL_VIDEO_FULLSCREEN_DISPLAY");
	int vm;

	if(SDL_Init(init_flags) < 0)
	{
		return false;
	}

	//SDL_EnableUNICODE(1);
	//SDL_EnableKeyRepeat(500, 25);

	atexit(SDL_Quit);

    if ( !var ) {
        var = SDL_getenv("SDL_VIDEO_FULLSCREEN_HEAD");
    }
    if ( var ) {
        vm = SDL_atoi(var);
    } else {
        vm = 0;
    }

	// Store the monitor's current resolution before setting the video mode for the first time
	if(SDL_GetDesktopDisplayMode(vm, &mode) == 0)
	{
		nativeWidth = mode.w;
		nativeHeight = mode.h;
	}
	else
	{
		return false;
	}

    //create a full screen window, as big as your native size, NOT curses window size!
	if( NULL==(window = SDL_CreateWindow("Cataclysm-DDA", 0, 0, nativeWidth, nativeHeight, SDL_WINDOW_SHOWN|SDL_WINDOW_FULLSCREEN)))
	{
		return false;
	}
	if( NULL==(renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED)))
	{
		return false;
	}
	
	SDL_RendererInfo info;
	SDL_GetRendererInfo(renderer, &info);

	SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, "linear", SDL_HINT_DEFAULT); // "nearest"
    
	// now create a texture
	textureWidth = ActualWidth;
	textureHeight = ActualHeight;

	int allocTextureWidth = nextpowerof2(textureWidth);
	int allocTextureHeight = nextpowerof2(textureHeight);

	int format = SDL_MasksToPixelFormatEnum (32, 0xFF,0xFF00,0xFF0000,0xFF000000);
	if(NULL==(backbuffer = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_TARGET, allocTextureWidth, allocTextureHeight)))
	{
		return false;
	}
	//SDL hard coded android file path so we have to hack it a bit...
	char font_path[128];
	strcat(getcwd(font_path, 128), "/data/font/bmpfont.bmp");
    SDL_Surface* bmp = SDL_LoadBMP(font_path);
	if(NULL==bmp) return false;
	SDL_Surface* font = SDL_CreateRGBSurface(SDL_SWSURFACE, bmp->w, bmp->h, 32, 0xff, 0xff00, 0xff0000, 0xff000000);
	SDL_BlitSurface(bmp, NULL, font, NULL);
    SDL_FreeSurface(bmp); bmp = NULL;
	if(SDL_MUSTLOCK(font)) SDL_LockSurface(font);
	int *p = (int*)font->pixels;
	int line = font->pitch/4;
	for(int i=0; i<font->h; i++)
	{
		for(int j=0; j<font->w; j++)
		{
			int d = i*line+j;
			if((p[d]&0xffffff)==0) p[d] = 0;
		}
	}
	if(SDL_MUSTLOCK(font)) SDL_UnlockSurface(font);
    if(NULL==(glyph_font = SDL_CreateTextureFromSurface(renderer, font)))
    {
        return false;
    }
    SDL_FreeSurface(font); font = NULL;

	SDL_SetRenderTarget(renderer, backbuffer);
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_RenderClear(renderer);

	btn_refresh();

	return true;
}

void WinDestroy()
{
    //TODO: destroy them
};

static void DrawAGrid(int grid, int r, int g, int b, int a)
{
	SDL_Rect rect;
	rect.x = (grid%GRIDX)*(nativeWidth/GRIDX);
	rect.y = (grid/GRIDX)*(nativeHeight/GRIDY);
	rect.w = (nativeWidth/GRIDX);
	rect.h = (nativeHeight/GRIDY);
	SDL_SetRenderDrawColor(renderer, r,g,b, a);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
	SDL_RenderFillRect(renderer, &rect);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
}

static void BackFlip()
{
	SDL_Rect rect;

	SDL_SetRenderTarget(renderer, NULL);
	rect.x = 0; rect.y=0; rect.w = ActualWidth; rect.h = ActualHeight;
	SDL_RenderCopy(renderer, backbuffer, &rect, NULL);

	int a = 16;

	if(gridtime>=SDL_GetTicks())
	{
		DrawAGrid(gridshow, 255, 0, 0, 128);
		a = 32;
	}

	DrawAGrid(15, 255, 0, 0, a);
	DrawAGrid(16, 0, 255, 0, a);
	DrawAGrid(17, 255, 0, 0, a);
	DrawAGrid(25, 0, 255, 0, a);
	DrawAGrid(26, 255, 0, 0, a);
	DrawAGrid(27, 0, 255, 0, a);
	DrawAGrid(35, 255, 0, 0, a);
	DrawAGrid(36, 0, 255, 0, a);
	DrawAGrid(37, 255, 0, 0, a);

	//DrawAGrid(22, 0, 0, 255, 16);

	SDL_RenderPresent(renderer);
	SDL_SetRenderTarget(renderer, backbuffer);
}

void try_update()
{
	unsigned long now=SDL_GetTicks();
	if(now-lastupdate>=interval)
	{
		BackFlip();
		needupdate = false;
		lastupdate = now;
	}
	else
	{
		needupdate = true;
	}
}

void DrawWindow(WINDOW *win)
{
    int i,j,drawx,drawy;
    char tmp;

    for (j=0; j<win->height; j++){
        if (win->line[j].touched)
        {
            win->line[j].touched=false;
			needupdate = true;

            for (i=0; i<win->width; i++){
                drawx=((win->x+i)*fontwidth);
                drawy=((win->y+j)*fontheight);//-j;
                if (((drawx+fontwidth)<=WindowWidth) && ((drawy+fontheight)<=WindowHeight)){
                tmp = win->line[j].chars[i];
                int FG = win->line[j].FG[i];
                int BG = win->line[j].BG[i];
                FillRectDIB(drawx,drawy,fontwidth,fontheight,BG);

                if ( tmp > 0 && tmp<128){
                    OutputChar(tmp, drawx,drawy,FG);
                //    }     //and this line too.
                } else {
                    switch ((unsigned char)tmp) {
                    case 0xc4://box bottom/top side (horizontal line)
                        HorzLineDIB(drawx,drawy+halfheight,drawx+fontwidth,1,FG);
                        break;
                    case 0xb3://box left/right side (vertical line)
                        VertLineDIB(drawx+halfwidth,drawy,drawy+fontheight,2,FG);
                        break;
                    case 0xda://box top left
                        HorzLineDIB(drawx+halfwidth,drawy+halfheight,drawx+fontwidth,1,FG);
                        VertLineDIB(drawx+halfwidth,drawy+halfheight,drawy+fontheight,2,FG);
                        break;
                    case 0xbf://box top right
                        HorzLineDIB(drawx,drawy+halfheight,drawx+halfwidth,1,FG);
                        VertLineDIB(drawx+halfwidth,drawy+halfheight,drawy+fontheight,2,FG);
                        break;
                    case 0xd9://box bottom right
                        HorzLineDIB(drawx,drawy+halfheight,drawx+halfwidth,1,FG);
                        VertLineDIB(drawx+halfwidth,drawy,drawy+halfheight+1,2,FG);
                        break;
                    case 0xc0://box bottom left
                        HorzLineDIB(drawx+halfwidth,drawy+halfheight,drawx+fontwidth,1,FG);
                        VertLineDIB(drawx+halfwidth,drawy,drawy+halfheight+1,2,FG);
                        break;
                    case 0xc1://box bottom north T (left, right, up)
                        HorzLineDIB(drawx,drawy+halfheight,drawx+fontwidth,1,FG);
                        VertLineDIB(drawx+halfwidth,drawy,drawy+halfheight,2,FG);
                        break;
                    case 0xc3://box bottom east T (up, right, down)
                        VertLineDIB(drawx+halfwidth,drawy,drawy+fontheight,2,FG);
                        HorzLineDIB(drawx+halfwidth,drawy+halfheight,drawx+fontwidth,1,FG);
                        break;
                    case 0xc2://box bottom south T (left, right, down)
                        HorzLineDIB(drawx,drawy+halfheight,drawx+fontwidth,1,FG);
                        VertLineDIB(drawx+halfwidth,drawy+halfheight,drawy+fontheight,2,FG);
                        break;
                    case 0xc5://box X (left down up right)
                        HorzLineDIB(drawx,drawy+halfheight,drawx+fontwidth,1,FG);
                        VertLineDIB(drawx+halfwidth,drawy,drawy+fontheight,2,FG);
                        break;
                    case 0xb4://box bottom east T (left, down, up)
                        VertLineDIB(drawx+halfwidth,drawy,drawy+fontheight,2,FG);
                        HorzLineDIB(drawx,drawy+halfheight,drawx+halfwidth,1,FG);
                        break;
                    default:
                        // SetTextColor(DC,_windows[w].line[j].chars[i].color.FG);
                        // TextOut(DC,drawx,drawy,&tmp,1);
                        break;
                    }
                    };//switch (tmp)
                }//(tmp < 0)
            };//for (i=0;i<_windows[w].width;i++)
        }
    };// for (j=0;j<_windows[w].height;j++)
    win->draw=false;                //We drew the window, mark it as so
	if(needupdate) try_update();
}

//Check for any window messages (keypress, paint, mousemove, etc)
void CheckMessages()
{
	SDL_Event ev;
    bool quit = false;
	unsigned long now;
	while(SDL_PollEvent(&ev))
	{

		switch(ev.type)
		{
			case SDL_TEXTINPUT:
			{
				lastchar = ev.text.text[0];
			}
			break;
			case SDL_FINGERDOWN:
			{
				//debugx = ev.tfinger.x*nativeWidth;
                //debugy = ev.tfinger.y*nativeHeight;

				int grid = gridshow = GRID(ev.tfinger.x, ev.tfinger.y);
				now = SDL_GetTicks();
				gridtime = now + 200;
				switch(grid)
				{
					case 15: lastchar='7'; break;
					case 25: lastchar=KEY_LEFT; break;
					case 35: lastchar='1'; break;
					case 17: lastchar='9'; break;
					case 27: lastchar=KEY_RIGHT; break;
					case 37: lastchar='3'; break;
					case 16: lastchar=KEY_UP; break;
					case 26: lastchar='5'; break;
					case 36: lastchar=KEY_DOWN; break;
					//case 22: SDL_StartTextInput(); break;
					case 0: 
					{
						keypage--;
						if(keypage<0) keypage = MAXKEYPAGE-1;
						btn_refresh();
					}
					break;
					case 9:
					{
						keypage++;
						if(keypage>=MAXKEYPAGE) keypage = 0;
						btn_refresh();
					}
					break;
					case 19: keycaps = !keycaps; btn_refresh(); break;
					case 29: lastchar = '\033'; break; //esc
					case 39: lastchar = 10; break;
					case 49: lastchar = ' '; break;
					case 59: SDL_StartTextInput(); break;
					case 10: lastchar = btn_key(0);break;
					case 20: lastchar = btn_key(1);break;
					case 30: lastchar = btn_key(2);break;
					case 40: lastchar = btn_key(3);break;
					case 50: lastchar = btn_key(4);break;
					case 51: lastchar = btn_key(5);break;
					case 52: lastchar = btn_key(6);break;
					case 53: lastchar = btn_key(7);break;
					case 54: lastchar = btn_key(8);break;
					case 55: lastchar = btn_key(9);break;
					case 56: lastchar = btn_key(10);break;
					case 57: lastchar = btn_key(11);break;
					case 58: lastchar = btn_key(12);break;
				}

                //debugmsg("%f, %f", debugx, debugy);
				holdkey = lastchar;
                try_update();
			}
			break;
			case SDL_FINGERUP:
				holdkey = 0;
				break;
			case SDL_KEYDOWN:
			{
				if(ev.key.keysym.scancode==SDL_SCANCODE_AC_BACK)
					SDL_StartTextInput();
				else if(ev.key.keysym.sym==SDLK_RETURN)
					lastchar = 10;
				else if(ev.key.keysym.sym==SDLK_BACKSPACE)
					lastchar = 127;
				//else if(ev.key.keysym.scancode==SDL_SCANCODE_MENU)
				//	lastchar = '\033';
				else if(ev.key.keysym.scancode==SDL_SCANCODE_0)
					lastchar = '0';
				else if(ev.key.keysym.scancode==SDL_SCANCODE_1)
					lastchar = '1';
				else if(ev.key.keysym.scancode==SDL_SCANCODE_2)
					lastchar = '2';
				else if(ev.key.keysym.scancode==SDL_SCANCODE_3)
					lastchar = '3';
				else if(ev.key.keysym.scancode==SDL_SCANCODE_4)
					lastchar = '4';
				else if(ev.key.keysym.scancode==SDL_SCANCODE_5)
					lastchar = '5';
				else if(ev.key.keysym.scancode==SDL_SCANCODE_6)
					lastchar = '6';
				else if(ev.key.keysym.scancode==SDL_SCANCODE_7)
					lastchar = '7';
				else if(ev.key.keysym.scancode==SDL_SCANCODE_8)
					lastchar = '8';
				else if(ev.key.keysym.scancode==SDL_SCANCODE_9)
					lastchar = '9';

					/*
				if(ev.key.keysym.sym==SDLK_LEFT) {
					lastchar = KEY_LEFT;
				}
				else if(ev.key.keysym.sym==SDLK_RIGHT) {
					lastchar = KEY_RIGHT;
				}
				else if(ev.key.keysym.sym==SDLK_UP) {
					lastchar = KEY_UP;
				}
				else if(ev.key.keysym.sym==SDLK_DOWN) {
					lastchar = KEY_DOWN;
				}
				else if(ev.key.keysym.sym==SDLK_PAGEUP) {
					lastchar = KEY_PPAGE;
				}
				else if(ev.key.keysym.sym==SDLK_PAGEDOWN) {
					lastchar = KEY_NPAGE;
				  
				}*/
			}
			break;
			case SDL_WINDOWEVENT:
			{
				if(ev.window.event==SDL_WINDOWEVENT_RESTORED)
					BackFlip();
			}
			break;
			case SDL_QUIT:
                quit = true;
				break;

		}
	}
	now = SDL_GetTicks();
	if(gridtime>0 && gridtime<=now)
	{
		gridtime=0;
		needupdate=true;
	}
	if(holdkey==0)
	{
		holdtime=0;
	}
	if(holdtime==0 && holdkey!=0)
	{
		holdtime = now+700;
	}
	if(holdkey!=0 && now>holdtime && repeatn==((now-holdtime)%200>100))
	{
		lastchar = holdkey;
		repeatn = !repeatn;
	}
	if(needupdate) try_update();
    if(quit)
    {
        endwin();
        exit(0);
    }
}

//***********************************
//Psuedo-Curses Functions           *
//***********************************

//Basic Init, create the font, backbuffer, etc
WINDOW *initscr(void)
{
    lastchar=-1;
    inputdelay=-1;

	fontheight=16;
	fontwidth=8;
	
	std::string keybuffer;
	std::ifstream fin;
	fin.open("data/android.keys");
	if (fin.is_open())
	{
        getline(fin, keybuffer);
		strcpy(keys, keybuffer.c_str());
		totalkeys = strlen(keys);
		fin.close();
	}

    halfwidth=fontwidth / 2;
    halfheight=fontheight / 2;
    WindowWidth= (55 + (12 * 2 + 1)) * fontwidth;
    WindowHeight= (12 * 2 + 1) *fontheight;
    if(!WinCreate()) {exit(0);}// do something here

    mainwin = newwin((OPTIONS[OPT_VIEWPORT_Y] * 2 + 1),(55 + (OPTIONS[OPT_VIEWPORT_Y] * 2 + 1)),0,0);
    return mainwin;   //create the 'stdscr' window and return its ref
}

WINDOW *newwin(int nlines, int ncols, int begin_y, int begin_x)
{
    if (begin_y < 0 || begin_x < 0) {
        return NULL; //it's the caller's problem now (since they have logging functions declared)
    }

    // default values
    if (ncols == 0)
    {
        ncols = TERMX - begin_x;
    }
    if (nlines == 0)
    {
        nlines = TERMY - begin_y;
    }

    int i,j;
    WINDOW *newwindow = new WINDOW;
    //newwindow=&_windows[WindowCount];
    newwindow->x=begin_x;
    newwindow->y=begin_y;
    newwindow->width=ncols;
    newwindow->height=nlines;
    newwindow->inuse=true;
    newwindow->draw=false;
    newwindow->BG=0;
    newwindow->FG=8;
    newwindow->cursorx=0;
    newwindow->cursory=0;
    newwindow->line = new curseline[nlines];

    for (j=0; j<nlines; j++)
    {
        newwindow->line[j].chars= new char[ncols];
        newwindow->line[j].FG= new char[ncols];
        newwindow->line[j].BG= new char[ncols];
        newwindow->line[j].touched=true;//Touch them all !?
        for (i=0; i<ncols; i++)
        {
          newwindow->line[j].chars[i]=0;
          newwindow->line[j].FG[i]=0;
          newwindow->line[j].BG[i]=0;
        }
    }
    //WindowCount++;
    return newwindow;
}

//Deletes the window and marks it as free. Clears it just in case.
int delwin(WINDOW *win)
{
    int j;
    win->inuse=false;
    win->draw=false;
    for (j=0; j<win->height; j++){
        delete win->line[j].chars;
        delete win->line[j].FG;
        delete win->line[j].BG;
        }
    delete win->line;
    delete win;
    return 1;
}

inline int newline(WINDOW *win){
    if (win->cursory < win->height - 1){
        win->cursory++;
        win->cursorx=0;
        return 1;
    }
return 0;
}

inline void addedchar(WINDOW *win){
    win->cursorx++;
    win->line[win->cursory].touched=true;
    if (win->cursorx > win->width)
        newline(win);
}


//Borders the window with fancy lines!
int wborder(WINDOW *win, chtype ls, chtype rs, chtype ts, chtype bs, chtype tl, chtype tr, chtype bl, chtype br)
{
/* 
ncurses does not do this, and this prevents: wattron(win, c_customBordercolor); wborder(win, ...); wattroff(win, c_customBorderColor);
    wattron(win, c_white);
*/
    int i, j;
    int oldx=win->cursorx;//methods below move the cursor, save the value!
    int oldy=win->cursory;//methods below move the cursor, save the value!
    if (ls>0)
        for (j=1; j<win->height-1; j++)
            mvwaddch(win, j, 0, 179);
    if (rs>0)
        for (j=1; j<win->height-1; j++)
            mvwaddch(win, j, win->width-1, 179);
    if (ts>0)
        for (i=1; i<win->width-1; i++)
            mvwaddch(win, 0, i, 196);
    if (bs>0)
        for (i=1; i<win->width-1; i++)
            mvwaddch(win, win->height-1, i, 196);
    if (tl>0)
        mvwaddch(win,0, 0, 218);
    if (tr>0)
        mvwaddch(win,0, win->width-1, 191);
    if (bl>0)
        mvwaddch(win,win->height-1, 0, 192);
    if (br>0)
        mvwaddch(win,win->height-1, win->width-1, 217);
    //_windows[w].cursorx=oldx;//methods above move the cursor, put it back
    //_windows[w].cursory=oldy;//methods above move the cursor, put it back
    wmove(win,oldy,oldx);
    wattroff(win, c_white);
    return 1;
}

//Refreshes a window, causing it to redraw on top.
int wrefresh(WINDOW *win)
{
    if (win==0) win=mainwin;
    if (win->draw)
        DrawWindow(win);
    return 1;
}

//Refreshes window 0 (stdscr), causing it to redraw on top.
int refresh(void)
{
    return wrefresh(mainwin);
}

int getch(void)
{
    return wgetch(mainwin);
}

//Not terribly sure how this function is suppose to work,
//but jday helped to figure most of it out
int wgetch(WINDOW* win)
{
	// standards note: getch is sometimes required to call refresh
	// see, e.g., http://linux.die.net/man/3/getch
	// so although it's non-obvious, that refresh() call (and maybe InvalidateRect?) IS supposed to be there
	wrefresh(win);
	lastchar=ERR;//ERR=-1
    if (inputdelay < 0)
	{
        do
        {
            CheckMessages();
            if (lastchar!=ERR) break;
            SDL_Delay(1);
        }
        while (lastchar==ERR);
	}
    else if (inputdelay > 0)
	{
        unsigned long starttime=SDL_GetTicks();
        unsigned long endtime;
        do
        {
            CheckMessages();
            endtime=SDL_GetTicks();
            if (lastchar!=ERR) break;
            SDL_Delay(1);
        }
        while (endtime<(starttime+inputdelay));
	}
	else
	{
		CheckMessages();
	}
    return lastchar;
}

int mvgetch(int y, int x)
{
    move(y,x);
    return getch();
}

int mvwgetch(WINDOW* win, int y, int x)
{
    move(y, x);
    return wgetch(win);
}

int getnstr(char *str, int size)
{
    int startX = mainwin->cursorx;
    int count = 0;
    char input;
    while(true)
    {
	input = getch();
	// Carriage return, Line feed and End of File terminate the input.
	if( input == '\r' || input == '\n' || input == '\x04' )
	{
	    str[count] = '\x00';
	    return count;
	}
	else if( input == 127 ) // Backspace, remapped from \x8 in ProcessMessages()
	{
	    if( count == 0 )
		continue;
	    str[count] = '\x00';
	    if(echoOn == 1)
	        mvaddch(mainwin->cursory, startX + count, ' ');
	    --count;
	    if(echoOn == 1)
	      move(mainwin->cursory, startX + count);
	}
	else
	{
	    if( count >= size - 1 ) // Still need space for trailing 0x00
	        continue;
	    str[count] = input;
	    ++count;
	    if(echoOn == 1)
	    {
	        move(mainwin->cursory, startX + count);
            mvaddch(mainwin->cursory, startX + count, input);
	    }
	}
    }
    return count;
}
//The core printing function, prints characters to the array, and sets colors
inline int printstring(WINDOW *win, char *fmt)
{
 int size = strlen(fmt);
 int j;
 for (j=0; j<size; j++){
  if (!(fmt[j]==10)){//check that this isnt a newline char
   if (win->cursorx <= win->width - 1 && win->cursory <= win->height - 1) {
    win->line[win->cursory].chars[win->cursorx]=fmt[j];
    win->line[win->cursory].FG[win->cursorx]=win->FG;
    win->line[win->cursory].BG[win->cursorx]=win->BG;
    win->line[win->cursory].touched=true;
    addedchar(win);
   } else
   return 0; //if we try and write anything outside the window, abort completely
} else // if the character is a newline, make sure to move down a line
  if (newline(win)==0){
      return 0;
      }
 }
 win->draw=true;
 return 1;
}

//Prints a formatted string to a window at the current cursor, base function
int wprintw(WINDOW *win, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char printbuf[2048];
    vsnprintf(printbuf, 2047, fmt, args);
    va_end(args);
    return printstring(win,printbuf);
};

//Prints a formatted string to a window, moves the cursor
int mvwprintw(WINDOW *win, int y, int x, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char printbuf[2048];
    vsnprintf(printbuf, 2047, fmt, args);
    va_end(args);
    if (wmove(win,y,x)==0) return 0;
    return printstring(win,printbuf);
};

//Prints a formatted string to window 0 (stdscr), moves the cursor
int mvprintw(int y, int x, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char printbuf[2048];
    vsnprintf(printbuf, 2047, fmt, args);
    va_end(args);
    if (move(y,x)==0) return 0;
    return printstring(mainwin,printbuf);
};

//Prints a formatted string to window 0 (stdscr) at the current cursor
int printw(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char printbuf[2078];
    vsnprintf(printbuf, 2047, fmt, args);
    va_end(args);
    return printstring(mainwin,printbuf);
};

//erases a window of all text and attributes
int werase(WINDOW *win)
{
    int j,i;
    for (j=0; j<win->height; j++)
    {
     for (i=0; i<win->width; i++)   {
     win->line[j].chars[i]=0;
     win->line[j].FG[i]=0;
     win->line[j].BG[i]=0;
     }
        win->line[j].touched=true;
    }
    win->draw=true;
    wmove(win,0,0);
//    wrefresh(win);
    return 1;
};

//erases window 0 (stdscr) of all text and attributes
int erase(void)
{
    return werase(mainwin);
};

//pairs up a foreground and background color and puts it into the array of pairs
int init_pair(short pair, short f, short b)
{
    colorpairs[pair].FG=f;
    colorpairs[pair].BG=b;
    return 1;
};

//moves the cursor in a window
int wmove(WINDOW *win, int y, int x)
{
    if (x>=win->width)
     {return 0;}//FIXES MAP CRASH -> >= vs > only
    if (y>=win->height)
     {return 0;}// > crashes?
    if (y<0)
     {return 0;}
    if (x<0)
     {return 0;}
    win->cursorx=x;
    win->cursory=y;
    return 1;
};

//Clears windows 0 (stdscr)     I'm not sure if its suppose to do this?
int clear(void)
{
    return wclear(mainwin);
};

//Ends the terminal, destroy everything
int endwin(void)
{
    WinDestroy();
    return 1;
};

//adds a character to the window
int mvwaddch(WINDOW *win, int y, int x, const chtype ch)
{
   if (wmove(win,y,x)==0) return 0;
   return waddch(win, ch);
};

//clears a window
int wclear(WINDOW *win)
{
    werase(win);
    clearok(win);
    return 1;
};

int clearok(WINDOW *win)
{
    for (int i=0; i<win->y; i++)
    {
        win->line[i].touched = true;
    }
    return 1;
}

//gets the max x of a window (the width)
int getmaxx(WINDOW *win)
{
    if (win==0) return mainwin->width;     //StdScr
    return win->width;
};

//gets the max y of a window (the height)
int getmaxy(WINDOW *win)
{
    if (win==0) return mainwin->height;     //StdScr
    return win->height;
};

//gets the beginning x of a window (the x pos)
int getbegx(WINDOW *win)
{
    if (win==0) return mainwin->x;     //StdScr
    return win->x;
};

//gets the beginning y of a window (the y pos)
int getbegy(WINDOW *win)
{
    if (win==0) return mainwin->y;     //StdScr
    return win->y;
};

//copied from gdi version and don't bother to rename it
inline SDL_Color BGR(int b, int g, int r)
{
    SDL_Color result;
    result.b=b;    //Blue
    result.g=g;    //Green
    result.r=r;    //Red
    //result.a=0;//The Alpha, isnt used, so just set it to 0
    return result;
};

int start_color(void)
{
	colorpairs=new pairs[50];
	windowsPalette[0]= BGR(0,0,0); // Black
	windowsPalette[1]= BGR(0, 0, 255); // Red
	windowsPalette[2]= BGR(0,110,0); // Green
	windowsPalette[3]= BGR(23,51,92); // Brown???
	windowsPalette[4]= BGR(200, 0, 0); // Blue
	windowsPalette[5]= BGR(98, 58, 139); // Purple
	windowsPalette[6]= BGR(180, 150, 0); // Cyan
	windowsPalette[7]= BGR(150, 150, 150);// Gray
	windowsPalette[8]= BGR(99, 99, 99);// Dark Gray
	windowsPalette[9]= BGR(150, 150, 255); // Light Red/Salmon?
	windowsPalette[10]= BGR(0, 255, 0); // Bright Green
	windowsPalette[11]= BGR(0, 255, 255); // Yellow
	windowsPalette[12]= BGR(255, 100, 100); // Light Blue
	windowsPalette[13]= BGR(240, 0, 255); // Pink
	windowsPalette[14]= BGR(255, 240, 0); // Light Cyan?
	windowsPalette[15]= BGR(255, 255, 255); //White
	//SDL_SetColors(screen,windowsPalette,0,256);
	return 0;
};

int keypad(WINDOW *faux, bool bf)
{
return 1;
};

int cbreak(void)
{
    return 1;
};
int keypad(int faux, bool bf)
{
    return 1;
};
int curs_set(int visibility)
{
    return 1;
};

int mvaddch(int y, int x, const chtype ch)
{
    return mvwaddch(mainwin,y,x,ch);
};

int wattron(WINDOW *win, int attrs)
{
    bool isBold = !!(attrs & A_BOLD);
    bool isBlink = !!(attrs & A_BLINK);
    int pairNumber = (attrs & A_COLOR) >> 17;
    win->FG=colorpairs[pairNumber].FG;
    win->BG=colorpairs[pairNumber].BG;
    if (isBold) win->FG += 8;
    if (isBlink) win->BG += 8;
    return 1;
};
int wattroff(WINDOW *win, int attrs)
{
     win->FG=8;                                  //reset to white
     win->BG=0;                                  //reset to black
    return 1;
};
int attron(int attrs)
{
    return wattron(mainwin, attrs);
};
int attroff(int attrs)
{
    return wattroff(mainwin,attrs);
};
int waddch(WINDOW *win, const chtype ch)
{
    char charcode;
    charcode=ch;

    switch (ch){        //LINE_NESW  - X for on, O for off
        case 4194424:   //#define LINE_XOXO 4194424
            charcode=179;
            break;
        case 4194417:   //#define LINE_OXOX 4194417
            charcode=196;
            break;
        case 4194413:   //#define LINE_XXOO 4194413
            charcode=192;
            break;
        case 4194412:   //#define LINE_OXXO 4194412
            charcode=218;
            break;
        case 4194411:   //#define LINE_OOXX 4194411
            charcode=191;
            break;
        case 4194410:   //#define LINE_XOOX 4194410
            charcode=217;
            break;
        case 4194422:   //#define LINE_XXOX 4194422
            charcode=193;
            break;
        case 4194420:   //#define LINE_XXXO 4194420
            charcode=195;
            break;
        case 4194421:   //#define LINE_XOXX 4194421
            charcode=180;
            break;
        case 4194423:   //#define LINE_OXXX 4194423
            charcode=194;
            break;
        case 4194414:   //#define LINE_XXXX 4194414
            charcode=197;
            break;
        default:
            charcode = (char)ch;
            break;
        }


int curx=win->cursorx;
int cury=win->cursory;

//if (win2 > -1){
   win->line[cury].chars[curx]=charcode;
   win->line[cury].FG[curx]=win->FG;
   win->line[cury].BG[curx]=win->BG;


    win->draw=true;
    addedchar(win);
    return 1;
  //  else{
  //  win2=win2+1;

};




//Move the cursor of windows 0 (stdscr)
int move(int y, int x)
{
    return wmove(mainwin,y,x);
};

//Set the amount of time getch waits for input
void timeout(int delay)
{
    inputdelay=delay;
};
void set_escdelay(int delay) { } //PORTABILITY, DUMMY FUNCTION


int echo()
{
    echoOn = 1;
    return 0; // 0 = OK, -1 = ERR
}

int noecho()
{
    echoOn = 0;
    return 0;
}

#endif // TILES
