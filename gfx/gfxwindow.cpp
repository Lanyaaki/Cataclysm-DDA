#if (defined _WIN32 || defined WINDOWS)
#include "game.h"
#include "catajson.h"
#include "mapdata.h"
#include "catacurse.h"
#include "options.h"
#include "output.h"
#include "color.h"
#include <cstdlib>
#include <fstream>

struct gfxtile
{
	int fg;
	int bg;
};

//***********************************
//Globals                           *
//***********************************

const WCHAR *szGfxClass = (L"CataCurseGfx");    //Class name :D
HINSTANCE GfxINST;   //the instance of the window
HWND GfxHandle;      //the handle of the window
HDC GfxDC;           //Device Context of the window, used for backbuffer

HDC gfxbuffer;         //an off-screen DC to prevent flickering, lower cpu
HBITMAP gfxbit;        //the bitmap that is used in conjunction wth the above
unsigned char *gfxdcbits;  //the bits of the screen image, for direct access

HBITMAP tile;
HBITMAP mask;

HBITMAP CreateBitmapMask(HBITMAP hbmColour, COLORREF crTransparent);
LRESULT CALLBACK GfxProcessMessages(HWND__ *hWnd,unsigned int Msg,
                                 WPARAM wParam, LPARAM lParam);

game* lastg = NULL;
std::map<std::string, gfxtile> tilemap;
bool tilemap_loaded = false;
//int tile_width, tile_height, tile_rows, tile_cols;
void load_gfx_config();
void GfxDraw(game* g);

void load_picture()
{
	if(tile) DeleteObject(tile);
	if(mask) DeleteObject(mask);
	tile = (HBITMAP)LoadImage(NULL, "gfx\\tile.bmp", IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
	mask = CreateBitmapMask(tile, RGB(128,255,255));
}

bool GfxWinCreate()
{
    int success;
    BITMAPINFO bmi;
    WNDCLASSEXW WindowClassType;
    int WinBorderHeight;
    int WinBorderWidth;
    int WinTitleSize;
    unsigned int WindowStyle;
    const WCHAR *szTitle=  (L"DDA Gfx Window");

	if(!tilemap_loaded) load_gfx_config();

    WinTitleSize = GetSystemMetrics(SM_CYCAPTION);      //These lines ensure
    WinBorderWidth = GetSystemMetrics(SM_CXDLGFRAME) * 2;  //that our window will
    WinBorderHeight = GetSystemMetrics(SM_CYDLGFRAME) * 2; // be a perfect size
    WindowClassType.cbSize = sizeof(WNDCLASSEXW);
    WindowClassType.style = 0;//No point in having a custom style, no mouse, etc
    WindowClassType.lpfnWndProc = GfxProcessMessages;//the procedure that gets msgs
    WindowClassType.cbClsExtra = 0;
    WindowClassType.cbWndExtra = 0;
    WindowClassType.hInstance = GfxINST;// hInstance
    WindowClassType.hIcon = LoadIcon(GfxINST, IDI_APPLICATION);//Default Icon
    WindowClassType.hIconSm = LoadIcon(GfxINST, IDI_APPLICATION);//Default Icon
    WindowClassType.hCursor = LoadCursor(NULL, IDC_ARROW);//Default Pointer
    WindowClassType.lpszMenuName = NULL;
    WindowClassType.hbrBackground = 0;//Thanks jday! Remove background brush
    WindowClassType.lpszClassName = szGfxClass;
    success = RegisterClassExW(&WindowClassType);
    if ( success== 0){
        return false;
    }
    WindowStyle = WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME) & ~(WS_MAXIMIZEBOX);
    GfxHandle = CreateWindowExW(WS_EX_APPWINDOW || WS_EX_TOPMOST * 0,
                                   szGfxClass , szTitle,WindowStyle, 0,
                                   0, 640 + (WinBorderWidth) * 1,
                                   480 + (WinBorderHeight +
                                   WinTitleSize) * 1, 0, 0, GfxINST, NULL);


    if (GfxHandle == 0){
        return false;
    }
    ShowWindow(GfxHandle,5);
	
    GfxDC = GetDC(GfxHandle);
    gfxbuffer = CreateCompatibleDC(GfxDC);
    ZeroMemory(&bmi, sizeof(BITMAPINFO));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = 640;
    bmi.bmiHeader.biHeight = 480;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount=32;
    bmi.bmiHeader.biCompression = BI_RGB;   //store it in uncompressed bytes
    bmi.bmiHeader.biSizeImage = 640 * 480 * 1;
    //bmi.bmiHeader.biClrUsed=16;         //the number of colors in our palette
    //bmi.bmiHeader.biClrImportant=16;    //the number of colors in our palette
    gfxbit = CreateDIBSection(0, &bmi, DIB_RGB_COLORS, (void**)&gfxdcbits, NULL, 0);
    DeleteObject(SelectObject(gfxbuffer, gfxbit));//load the buffer into DC

	load_picture();

    return true;
};

//This function processes any Windows messages we get. Keyboard, OnClose, etc
LRESULT CALLBACK GfxProcessMessages(HWND__ *hWnd,unsigned int Msg,
                                 WPARAM wParam, LPARAM lParam)
{
	extern HWND WindowHandle;
    switch (Msg){
		case WM_CHAR:
			SendMessage(WindowHandle, Msg, wParam, lParam);
			break;
        case WM_KEYDOWN:                //Here we handle non-character input
            switch (wParam){
			case VK_F5:
				load_gfx_config();
				load_picture();
				GfxDraw(lastg);
				break;
			default:
				break;
			}
			SendMessage(WindowHandle, Msg, wParam, lParam);
            break;
        case WM_ERASEBKGND:
            return 1;               //We don't want to erase our backgroundon't want to erase our background
        case WM_PAINT:              //Pull from our backbuffer, onto the screen
            BitBlt(GfxDC, 0, 0, 640, 480, gfxbuffer, 0, 0,SRCCOPY);
            ValidateRect(GfxHandle,NULL);
            break;
        default://If we didnt process a message, return the default value for it
            return DefWindowProcW(hWnd, Msg, wParam, lParam);
    };
    return 0;
};

void DrawMasked(int x, int y, int cx, int cy, int sx, int sy)
{
	HDC hdcMem = CreateCompatibleDC(NULL);
	HBITMAP hbmT = (HBITMAP)SelectObject(hdcMem,tile);
    SelectObject(hdcMem, mask);
    BitBlt(gfxbuffer, x, y, cx, cy, hdcMem, sx, sy, SRCAND);

    SelectObject(hdcMem, tile);
    BitBlt(gfxbuffer, x, y, cx, cy, hdcMem, sx, sy, SRCPAINT);
	SelectObject(hdcMem,hbmT);
	DeleteDC(hdcMem);

}

#define vts(x) case x: return #x;

const char* ter_name(int tid)
{
switch(tid)
{
//terrains
vts(t_hole);	

vts(t_dirt); vts(t_sand); vts(t_dirtmound); vts(t_pit_shallow); vts(t_pit);
vts(t_pit_corpsed); vts(t_pit_covered); vts(t_pit_spiked); vts(t_pit_spiked_covered);
vts(t_rock_floor); vts(t_rubble); vts(t_ash); vts(t_metal); vts(t_wreckage);
vts(t_grass);
vts(t_metal_floor);
vts(t_pavement); vts(t_pavement_y); vts(t_sidewalk);
vts(t_floor);
vts(t_dirtfloor);
vts(t_hay);
vts(t_grate);
vts(t_slime);
vts(t_bridge);

vts(t_canvas_wall); vts(t_canvas_door); vts(t_canvas_door_o); vts(t_groundsheet); vts(t_fema_groundsheet);
vts(t_skin_wall); vts(t_skin_door); vts(t_skin_door_o);  vts(t_skin_groundsheet);

vts(t_skylight); vts(t_emergency_light_flicker); vts(t_emergency_light);

vts(t_wall_log_half); vts(t_wall_log); vts(t_wall_log_chipped); vts(t_wall_log_broken); vts(t_palisade); vts(t_palisade_gate);
vts(t_wall_half); vts(t_wall_wood); vts(t_wall_wood_chipped); vts(t_wall_wood_broken);
vts(t_wall_v); vts(t_wall_h); vts(t_concrete_v); vts(t_concrete_h);
vts(t_wall_metal_v); vts(t_wall_metal_h);
vts(t_wall_glass_v); vts(t_wall_glass_h);
vts(t_wall_glass_v_alarm); vts(t_wall_glass_h_alarm);
vts(t_reinforced_glass_v); vts(t_reinforced_glass_h);
vts(t_bars);
vts(t_door_c); vts(t_door_b); vts(t_door_o); vts(t_door_locked_interior); vts(t_door_locked); vts(t_door_locked_alarm); vts(t_door_frame);
vts(t_chaingate_l); vts(t_fencegate_c); vts(t_fencegate_o); vts(t_chaingate_c); vts(t_chaingate_o); vts(t_door_boarded);
vts(t_door_metal_c); vts(t_door_metal_o); vts(t_door_metal_locked);
vts(t_door_glass_c); vts(t_door_glass_o);
vts(t_bulletin);
vts(t_portcullis);
vts(t_recycler); vts(t_window); vts(t_window_taped); vts(t_window_domestic); vts(t_window_domestic_taped); vts(t_window_open); vts(t_curtains);
vts(t_window_alarm); vts(t_window_alarm_taped); vts(t_window_empty); vts(t_window_frame); vts(t_window_boarded);
vts(t_window_stained_green); vts(t_window_stained_red); vts(t_window_stained_blue);
vts(t_rock); vts(t_fault);
vts(t_paper);

vts(t_tree); vts(t_tree_young); vts(t_tree_apple); vts(t_underbrush); vts(t_shrub); vts(t_indoor_plant); vts(t_shrub_blueberry); vts(t_log);
vts(t_root_wall);
vts(t_wax); vts(t_floor_wax);
vts(t_fence_v); vts(t_fence_h); vts(t_chainfence_v); vts(t_chainfence_h); vts(t_chainfence_posts);
vts(t_fence_post); vts(t_fence_wire); vts(t_fence_barbed); vts(t_fence_rope);
vts(t_railing_v); vts(t_railing_h);

vts(t_marloss); vts(t_fungus); vts(t_tree_fungal);

vts(t_water_sh); vts(t_water_dp); vts(t_water_pool); vts(t_sewage);
vts(t_lava);

vts(t_bed); vts(t_toilet); vts(t_makeshift_bed);

vts(t_sink); vts(t_oven); vts(t_woodstove); vts(t_fireplace); vts(t_bathtub); vts(t_chair); vts(t_armchair); vts(t_sofa); vts(t_cupboard); vts(t_trashcan); vts(t_desk); vts(t_exercise);
vts(t_sandbox); vts(t_slide); vts(t_monkey_bars); vts(t_backboard);
vts(t_bench); vts(t_table); vts(t_pool_table);
vts(t_gas_pump); vts(t_gas_pump_smashed); vts(t_gas_pump_empty);
vts(t_missile); vts(t_missile_exploded);
vts(t_counter);
vts(t_radio_tower); vts(t_radio_controls);
vts(t_console_broken); vts(t_console); vts(t_gates_mech_control); vts(t_barndoor); vts(t_palisade_pulley);
vts(t_sewage_pipe); vts(t_sewage_pump);
vts(t_centrifuge);
vts(t_column);

vts(t_washer); vts(t_dryer);
vts(t_fridge); vts(t_glass_fridge); vts(t_dresser); vts(t_locker);
vts(t_rack); vts(t_bookcase);
vts(t_dumpster);
vts(t_vat); vts(t_crate_c); vts(t_crate_o);

vts(t_stairs_down); vts(t_stairs_up); vts(t_manhole); vts(t_ladder_up); vts(t_ladder_down); vts(t_slope_down);
 vts(t_slope_up); vts(t_rope_up);
vts(t_manhole_cover);

vts(t_card_science); vts(t_card_military); vts(t_card_reader_broken); vts(t_slot_machine);
 vts(t_elevator_control); vts(t_elevator_control_off); vts(t_elevator); vts(t_pedestal_wyrm);
 vts(t_pedestal_temple);

vts(t_rock_red); vts(t_rock_green); vts(t_rock_blue); vts(t_floor_red); vts(t_floor_green); vts(t_floor_blue);
 vts(t_switch_rg); vts(t_switch_gb); vts(t_switch_rb); vts(t_switch_even);

 vts(t_mutpoppy); 

}
return "unknown";
}

const char* mon_name(int mid)
{
	switch(mid)
	{

vts(mon_squirrel); vts(mon_rabbit); vts(mon_deer); vts(mon_moose); vts(mon_wolf); vts(mon_coyote); vts(mon_bear); vts(mon_cougar); vts(mon_crow);

vts(mon_dog); vts(mon_cat);

vts(mon_ant_larva); vts(mon_ant); vts(mon_ant_soldier); vts(mon_ant_queen); vts(mon_ant_fungus);

vts(mon_fly); vts(mon_bee); vts(mon_wasp);

vts(mon_graboid); vts(mon_worm); vts(mon_halfworm);

 vts(mon_zombie); vts(mon_zombie_cop); vts(mon_zombie_shrieker); vts(mon_zombie_spitter); vts(mon_zombie_electric);
 vts(mon_zombie_smoker);
 vts(mon_zombie_fast); vts(mon_zombie_brute); vts(mon_zombie_hulk); vts(mon_zombie_fungus);
 vts(mon_boomer); vts(mon_boomer_fungus); vts(mon_skeleton); vts(mon_zombie_necro);
 vts(mon_zombie_scientist); vts(mon_zombie_soldier); vts(mon_zombie_grabber);
 vts(mon_zombie_master);  vts(mon_beekeeper); vts(mon_zombie_child);

vts(mon_triffid); vts(mon_triffid_young); vts(mon_triffid_queen); vts(mon_creeper_hub);
 vts(mon_creeper_vine); vts(mon_biollante); vts(mon_vinebeast); vts(mon_triffid_heart);

vts(mon_fungaloid); vts(mon_fungaloid_dormant); vts(mon_fungaloid_young); vts(mon_spore);
 vts(mon_fungaloid_queen); vts(mon_fungal_wall);

vts(mon_blob); vts(mon_blob_small);

vts(mon_chud); vts(mon_one_eye); vts(mon_crawler);

vts(mon_sewer_fish); vts(mon_sewer_snake); vts(mon_sewer_rat); vts(mon_rat_king);

vts(mon_mosquito); vts(mon_dragonfly); vts(mon_centipede); vts(mon_frog); vts(mon_slug);
 vts(mon_dermatik_larva); vts(mon_dermatik);

vts(mon_spider_wolf); vts(mon_spider_web); vts(mon_spider_jumping); vts(mon_spider_trapdoor);
 vts(mon_spider_widow);

vts(mon_dark_wyrm); vts(mon_amigara_horror); vts(mon_dog_thing); vts(mon_headless_dog_thing);
 vts(mon_thing);

vts(mon_human_snail); vts(mon_twisted_body); vts(mon_vortex);

vts(mon_flying_polyp); vts(mon_hunting_horror); vts(mon_mi_go); vts(mon_yugg); vts(mon_gelatin);
 vts(mon_flaming_eye); vts(mon_kreck); vts(mon_gracke); vts(mon_blank); vts(mon_gozu); vts(mon_shadow); vts(mon_breather_hub);
 vts(mon_breather); vts(mon_shadow_snake);

vts(mon_eyebot); vts(mon_manhack); vts(mon_skitterbot); vts(mon_secubot); vts(mon_hazmatbot); vts(mon_copbot); vts(mon_molebot);
 vts(mon_tripod); vts(mon_chickenbot); vts(mon_tankbot); vts(mon_turret); vts(mon_exploder);

vts(mon_hallu_zom); vts(mon_hallu_bee); vts(mon_hallu_ant); vts(mon_hallu_mom);

vts(mon_generator);
	}
	return "unknown";
}

const char* trap_name(int t)
{
	switch(t)
	{
 vts(tr_bubblewrap);
 vts(tr_cot);
 vts(tr_brazier);
 vts(tr_funnel);
 vts(tr_rollmat);
 vts(tr_beartrap);
 vts(tr_beartrap_buried);
 vts(tr_snare);
 vts(tr_nailboard);
 vts(tr_tripwire);
 vts(tr_crossbow);
 vts(tr_shotgun_2);
 vts(tr_shotgun_1);
 vts(tr_engine);
 vts(tr_blade);
 vts(tr_light_snare);
 vts(tr_heavy_snare);
 vts(tr_landmine);
 vts(tr_landmine_buried);
 vts(tr_telepad);
 vts(tr_goo);
 vts(tr_dissector);
 vts(tr_sinkhole);
 vts(tr_pit);
 vts(tr_spike_pit);
 vts(tr_lava);
 vts(tr_portal);
 vts(tr_ledge);
 vts(tr_boobytrap);
 vts(tr_temple_flood);
 vts(tr_temple_toggle);
 vts(tr_glow);
 vts(tr_hum);
 vts(tr_shadow);
 vts(tr_drain);
 vts(tr_snake);
	}
	return "unknown";
}

const char* fd_name(int t)
{
	switch(t)
	{
 vts(fd_blood);
 vts(fd_bile);
 vts(fd_web);
 vts(fd_slime);
 vts(fd_acid);
 vts(fd_sap);
 vts(fd_fire);
 vts(fd_rubble);
 vts(fd_smoke);
 vts(fd_toxic_gas);
 vts(fd_tear_gas);
 vts(fd_nuke_gas);
 vts(fd_gas_vent);
 vts(fd_fire_vent);
 vts(fd_flame_burst);
 vts(fd_electricity);
 vts(fd_fatigue);
 vts(fd_push_items);
 vts(fd_shock_vent);
 vts(fd_acid_vent);
	}
	return "unknown";
}

#define LINE_OOOO  0xabcdef
//copied and modified from map.cpp
long determine_wall_corner(map& m, const int x, const int y, const long orig_sym)
{
    long sym = orig_sym;
    //LINE_NESW
    const long above = terlist[m.ter(x, y-1)].sym;
    const long below = terlist[m.ter(x, y+1)].sym;
    const long left  = terlist[m.ter(x-1, y)].sym;
    const long right = terlist[m.ter(x+1, y)].sym;

    const bool above_connects = above == sym || (above == '"' || above == '+' || above == '\'');
    const bool below_connects = below == sym || (below == '"' || below == '+' || below == '\'');
    const bool left_connects  = left  == sym || (left  == '"' || left  == '+' || left  == '\'');
    const bool right_connects = right == sym || (right == '"' || right == '+' || right == '\'');

    // -
    // |      this = - and above = | or a connectable
    if(sym == LINE_OXOX &&  (above == LINE_XOXO || above_connects))
    {
        //connects to upper
        if(left_connects)
            sym = LINE_XOOX; // ©¼ left coming wall
        else if(right_connects)
            sym = LINE_XXOO;//©¸   right coming wall
        if(left_connects && right_connects)
            sym = LINE_XXOX; // ©Ø passing by
    }

    // |
    // -      this = - and below = | or a connectable
    else if(sym == LINE_OXOX && (below == LINE_XOXO || below_connects))
    {
        //connects to lower
        if(left_connects)
            sym = LINE_OOXX; // ©´ left coming wall
        else if(right_connects)
            sym = LINE_OXXO;//©°   right coming wall
        if(left_connects && right_connects)
            sym = LINE_OXXX; // ©Ð passing by
    }

    // -|       this = | and left = - or a connectable
    else if(sym == LINE_XOXO && (left == LINE_OXOX || left_connects))
    {
        //connexts to left
        if(above_connects)
            sym = LINE_XOOX; // ©¼ north coming wall
        else if(below_connects )
            sym = LINE_OOXX;//©´   south coming wall
        if(above_connects && below_connects)
            sym = LINE_XOXX; // ©È passing by
    }

    // |-       this = | and right = - or a connectable
    else if(sym == LINE_XOXO && (right == LINE_OXOX || right_connects))
    {
        //connects to right
        if(above_connects)
            sym = LINE_XXOO; // ©¸ north coming wall
        else if(below_connects)
            sym = LINE_OXXO;// ©°   south coming wall
        if(above_connects && below_connects)
            sym = LINE_XXXO; // ©À passing by
    }

    if(above == LINE_XOXO && left == LINE_OXOX && above == below && left == right)
        sym = LINE_XXXX; // ©à crossway

    return sym;
}


static int natural_sight_range;
static int light_sight_range;
static int lowlight_sight_range;
static int max_sight_range;
static bool u_is_boomered;
static int  u_clairvoyance;
static bool u_sight_impaired;
static int  g_light_level;

int light_at(game* g, int realx, int realy)
{
   const int dist = rl_dist(g->u.posx, g->u.posy, realx, realy);
   int sight_range = light_sight_range;
   int low_sight_range = lowlight_sight_range;
   
   // While viewing indoor areas use lightmap model
   if (!g->m.is_outside(realx, realy)) {
    sight_range = natural_sight_range;
   // Don't display area as shadowy if it's outside and illuminated by natural light
   } else if (dist <= g->u.sight_range(g_light_level)) {
    low_sight_range = std::max(g_light_level, natural_sight_range);
    
   }

   // I've moved this part above loops without even thinking that
   // this must stay here...
   int real_max_sight_range = light_sight_range > max_sight_range ? light_sight_range : max_sight_range;
   int distance_to_look = real_max_sight_range;
   if (OPTIONS[OPT_GRADUAL_NIGHT_LIGHT] > 0.) {
    // in this case we'll be always looking at maximum distance
    // and light level should do rest of the work....
    distance_to_look = DAYLIGHT_LEVEL;
   }

   bool can_see = g->m.pl_sees(g->u.posx, g->u.posy, realx, realy, distance_to_look);
   lit_level lit = g->m.light_at(realx, realy);

   if (OPTIONS[OPT_GRADUAL_NIGHT_LIGHT] > 0.) {
    // now we're gonna adjust real_max_sight, to cover some nearby "highlights",
	// but at the same time changing light-level depending on distance,
	// to create actual "gradual" stuff
	// Also we'll try to ALWAYS show LL_BRIGHT stuff independent of where it is...
    if (lit != LL_BRIGHT) {
     if (dist > real_max_sight_range) {
      int intLit = (int)lit - (dist - real_max_sight_range)/2;
      if (intLit < 0) intLit = LL_DARK;
      lit = (lit_level)intLit;
     }
    }
	// additional case for real_max_sight_range
	// if both light_sight_range and max_sight_range were small
	// it means we really have limited visibility (e.g. inside a pit)
	// and we shouldn't touch that
	if (lit > LL_DARK && real_max_sight_range > 1) {
     real_max_sight_range = distance_to_look;
    }
   }

   if (dist > real_max_sight_range ||
       (dist > light_sight_range &&
         (lit == LL_DARK ||
         (u_sight_impaired && lit != LL_BRIGHT) ||
	  !can_see))) {
    if (u_is_boomered)
   	 return 3;
    else
         return 4;
   } else if (dist > light_sight_range && u_sight_impaired && lit == LL_BRIGHT) {
    if (u_is_boomered)
     return 1;
    else
     return 2;
   } else if (dist <= u_clairvoyance || can_see) {
    return 0;
   }
	return -1;
}

void init_light(game* g)
{
	g->m.build_map_cache(g);
	g->reset_light_level();
	natural_sight_range = g->u.sight_range(1);
	light_sight_range = g->u.sight_range(g->light_level());
	lowlight_sight_range = std::max((int)g->light_level() / 2, natural_sight_range);
	max_sight_range = g->u.unimpaired_range();
	u_is_boomered = g->u.has_disease(DI_BOOMERED);
	u_clairvoyance = g->u.clairvoyance();
	u_sight_impaired = g->u.sight_impaired();
	g_light_level = (int)g->light_level();
}

void GfxDraw(game* g)
{
	if(NULL==(lastg = g)) return;

	static RECT rect = {0,0,640,480};
	FillRect(gfxbuffer, &rect, HBRUSH(GetStockObject(BLACK_BRUSH)));
	
	int posx = g->u.posx;// + g->u.view_offset_x;
	int posy = g->u.posy;// + g->u.view_offset_y;

	init_light(g);
	

	for(int mx=0; mx<40; mx++) {
		for(int my=0; my<30; my++){
			int x = posx+mx-20;
			int y = posy+my-15;
			int l = light_at(g, x, y);
			switch(l)
			{
			case -1:DrawMasked(mx*16,my*16, 16, 16, 16*(128%16), 16*(128/16));continue; break;
			case 1:DrawMasked(mx*16,my*16, 16, 16, 16*(130%16), 16*(130/16));continue;break;
			case 2:DrawMasked(mx*16,my*16, 16, 16, 16*(134%16), 16*(134/16));continue;break;
			case 3:DrawMasked(mx*16,my*16, 16, 16, 16*(132%16), 16*(132/16));continue;break;
			case 4:DrawMasked(mx*16,my*16, 16, 16, 16*(136%16), 16*(136/16));continue;break;
			}
			int t = g->m.ter(x, y);
			if (t != t_null )
			{
				int s = terlist[t].sym;
				int bgt = 263;
				bool wallhack = false;

				if(s == LINE_XOXO || s == LINE_OXOX)//vertical or horizontal
					s = determine_wall_corner(g->m, x, y, s);

				switch(s)
				{
				case LINE_XOXO:case LINE_OXOX:case LINE_XXOO:case LINE_OXXO:case LINE_OOXX:case LINE_XOOX:case LINE_XXXO:case LINE_XXOX:case LINE_XOXX:case LINE_OXXX:case LINE_XXXX:
					//s = 258; //default wall
					wallhack = true;
					break;
				default:
					s &= 0xff;
					break;
				}

				std::map<std::string, gfxtile>::iterator found;
				
				found = tilemap.find(ter_name(t));
				if(found!=tilemap.end())
				{
					gfxtile tile = found->second;
					if(tile.fg) 
					{
						if(wallhack)
						{
							switch(s)
							{
								case LINE_XOXO: // |
									s = tile.fg+1; break;
								case LINE_OXOX: // -
									s = tile.fg+2; break;
								case LINE_XXOO: // ©¸
									s = tile.fg+3; break;
								case LINE_OXXO: // ©°
									s = tile.fg+4; break;
								case LINE_OOXX: // ©´
									s = tile.fg+5; break;
								case LINE_XOOX: // ©¼
									s = tile.fg+6; break;
								case LINE_XXXO: // ©À 
									s = tile.fg+7; break;
								case LINE_XXOX: // ©Ø
									s = tile.fg+8; break;
								case LINE_XOXX: // ©È
									s = tile.fg+9; break;
								case LINE_OXXX: // ©Ð
									s = tile.fg+10; break;
								case LINE_XXXX: // ©à
									s = tile.fg+11; break;
							}
						}
						else s = tile.fg;
					}
					if(tile.bg) bgt = tile.bg;
				}
				
				if(bgt) DrawMasked(mx*16,my*16, 16, 16, 16*(bgt%16), 16*(bgt/16));
				if(s) DrawMasked(mx*16,my*16, 16, 16, 16*(s%16), 16*(s/16));

				//int show_items = true;
				s = bgt = 0;
				int tr = g->m.tr_at(x, y);
				if (tr != tr_null )
				{
					if ((g->traps)[tr]->sym == '%') {
						switch(rng(1, 5)) {
							case 1: s = '*'; break;
							case 2: s = '0'; break;
							case 3: s = '8'; break;
							case 4: s = '&'; break;
							case 5: s = '+'; break;
						}
					} else
						s = (g->traps)[g->m.tr_at(x, y)]->sym;
					s &= 0xff;
					found = tilemap.find(trap_name(tr));
					if(found!=tilemap.end())
					{
						gfxtile tile = found->second;
						if(tile.fg) s = tile.fg;
						if(tile.bg) bgt = tile.bg;
					}
					if(bgt) DrawMasked(mx*16,my*16, 16, 16, 16*(bgt%16), 16*(bgt/16));
					if(s) DrawMasked(mx*16,my*16, 16, 16, 16*(s%16), 16*(s/16));
				}

				field &f = g->m.field_at(x, y);
				s = bgt = 0;
				// If there's a field here, draw that instead (unless its symbol is %)
				if (f.type != fd_null)
				{
					if(fieldlist[f.type].sym != '&')
					{
						if (fieldlist[f.type].sym == '*') {
							switch (rng(1, 5)) {
							case 1: s = '*'; break;
							case 2: s = '0'; break;
							case 3: s = '8'; break;
							case 4: s = '&'; break;
							case 5: s = '+'; break;
							}
						} else if(fieldlist[f.type].sym != '%') {
							s = fieldlist[f.type].sym;
						}
						s &= 0xff;
					}
					found = tilemap.find(fd_name(f.type));
					if(found!=tilemap.end())
					{
						gfxtile tile = found->second;
						if(tile.fg) s = tile.fg;
						if(tile.bg) bgt = tile.bg;
					}
					if(bgt) DrawMasked(mx*16,my*16, 16, 16, 16*(bgt%16), 16*(bgt/16));
					if(s) DrawMasked(mx*16,my*16, 16, 16, 16*(s%16), 16*(s/16));
				}

				// If there's items here, draw those instead
				std::vector<item> &items = g->m.i_at(x, y);
				s = bgt = 0;
				if (!g->m.has_flag(container, x, y) && items.size() > 0 )
				{
					item &it = items[items.size() - 1];
					s = it.symbol();
					s &= 0xff;

					found = tilemap.find(it.type->id);
					if(found!=tilemap.end())
					{
						gfxtile tile = found->second;
						if(tile.fg) s = tile.fg;
						if(tile.bg) bgt = tile.bg;
					}
					if(bgt) DrawMasked(mx*16,my*16, 16, 16, 16*(bgt%16), 16*(bgt/16));
					if(s) DrawMasked(mx*16,my*16, 16, 16, 16*(s%16), 16*(s/16));
				}

				int veh_part = 0;
				vehicle *veh = g->m.veh_at(x, y, veh_part);
				if (veh) {
					int s = veh->part_sym(veh_part);
					int d = veh->face.dir4();

					switch (s)
					{
					case 'j':
						switch (d)
						{
						default:
						case 0: s=2016;break;
						case 1: s=2018;break;
						case 2: s=2016;break;
						case 3: s=2018;break;
						}
						break;
					case 'h':
						switch (d)
						{
						default:
						case 0: s=2018;break;
						case 1: s=2016;break;
						case 2: s=2018;break;
						case 3: s=2016;break;
						}
						break;
					case 'y':
						switch (d)
						{
						default:
						case 0: s=1988;break;
						case 1: s=2004;break;
						case 2: s=2036;break;
						case 3: s=2020;break;
						}
						break;
					case 'u':
						switch (d)
						{
						default:
						case 0: s=2004;break;
						case 1: s=2036;break;
						case 2: s=2020;break;
						case 3: s=1988;break;
						}
						break;
					case 'n':
						switch (d)
						{
						default:
						case 0: s=2036;break;
						case 1: s=2020;break;
						case 2: s=1988;break;
						case 3: s=2004;break;
						}
						break;
					case 'b':
						switch (d)
						{
						default:
						case 0: s=2020;break;
						case 1: s=1988;break;
						case 2: s=2004;break;
						case 3: s=2036;break;
						}
						break;
					case 'c':
						s=1972;break;
					case '^':
						switch (d)
						{
						default:
						case 0: s=2032;break;
						case 1: s=2033;break;
						case 2: s=2034;break;
						case 3: s=2035;break;
						}
						break;
					case '[':
						switch (d)
						{
						default:
						case 0: s=2016;break;
						case 1: s=2018;break;
						case 2: s=2016;break;
						case 3: s=2018;break;
						}
						break;
					case ']':
						switch (d)
						{
						default:
						case 0: s=2016;break;
						case 1: s=2018;break;
						case 2: s=2016;break;
						case 3: s=2018;break;
						}
						break;
					case '|':
						switch (d)
						{
						default:
						case 0: s=2016;break;
						case 1: s=2018;break;
						case 2: s=2016;break;
						case 3: s=2018;break;
						}
						break;
					case '-':
						switch (d)
						{
						default:
						case 0: s=2018;break;
						case 1: s=2016;break;
						case 2: s=2018;break;
						case 3: s=2016;break;
						}
						break;
					case '=':
						switch (d)
						{
						default:
						case 0: s=1968;break;
						case 1: s=1969;break;
						case 2: s=1968;break;
						case 3: s=1969;break;
						}
						break;
					case 'H':
						switch (d)
						{
						default:
						case 0: s=1969;break;
						case 1: s=1968;break;
						case 2: s=1969;break;
						case 3: s=1968;break;
						}
						break;
					case '\\':
						switch (d)
						{
						default:
						case 0: s=1921;break;
						case 1: s=1920;break;
						case 2: s=1921;break;
						case 3: s=1920;break;
						}
						break;
					case '/':
						switch (d)
						{
						default:
						case 0: s=1920;break;
						case 1: s=1921;break;
						case 2: s=1920;break;
						case 3: s=1921;break;
						}
						break;
					case '.':
						s=1987;break;
					case '"':
						switch (d)
						{
						default:
						case 0: s=1953;break;
						case 1: s=1952;break;
						case 2: s=1953;break;
						case 3: s=1952;break;
						}
						break;
					case '+':
						s=1970;break;
					case '\'':
						s=1971;break;
					case 'X':
					case 'x':
						switch (d)
						{
						default:
						case 0: s=2019;break;
						case 1: s=2003;break;
						case 2: s=2019;break;
						case 3: s=2003;break;
						}
						break;
					case '0':
					case 'O':
						switch (d)
						{
						default:
						case 0: s=1985;break;
						case 1: s=1984;break;
						case 2: s=1985;break;
						case 3: s=1984;break;
						}
						break;
					case 'o':
						switch (d)
						{
						default:
						case 0: s=2002;break;
						case 1: s=2017;break;
						case 2: s=2002;break;
						case 3: s=2017;break;
						}
						break;
					case '#':
						s=1986;break;
					default:;
					}
					//s = special_symbol (veh->face.dir_symbol(veh->part_sym(veh_part)));

					DrawMasked(mx*16,my*16, 16, 16, 16*(s%16), 16*(s/16));
					//if (normal_tercol)
					//tercol = veh->part_color(veh_part);
				}

				int mi = g->mon_at(x, y);
				if(mi>=0)
				{
					monster& m = g->z[mi];
					if(!m.dead)
					{
						s = m.symbol()&0xff;
						bgt = 0;
						found = tilemap.find(mon_name(m.type->id));
						if(found!=tilemap.end())
						{
							gfxtile tile = found->second;
							if(tile.fg) s = tile.fg;
							if(tile.bg) bgt = tile.bg;
						}
						if(bgt) DrawMasked(mx*16,my*16, 16, 16, 16*(bgt%16), 16*(bgt/16));
						if(s) DrawMasked(mx*16,my*16, 16, 16, 16*(s%16), 16*(s/16));
					}
				}

				int ni = g->npc_at(x, y);
				if(ni>=0)
				{
					npc& m = *g->active_npc[ni];
					if(!m.dead)
					{
						s = '@'&0xff;
						bgt = 0;
						found = tilemap.find(g->u.male?"npc_male":"npc_female");
						if(found!=tilemap.end())
						{
							gfxtile tile = found->second;
							if(tile.fg) s = tile.fg;
							if(tile.bg) bgt = tile.bg;
						}
						if(bgt) DrawMasked(mx*16,my*16, 16, 16, 16*(bgt%16), 16*(bgt/16));
						if(s) DrawMasked(mx*16,my*16, 16, 16, 16*(s%16), 16*(s/16));
					}
				}

			}
		}
	}

	//if(g->u.hp_cur>0)
	{
		int s = g->u.male?1035:1036;
		int bgt = 0;
		std::map<std::string, gfxtile>::iterator found = tilemap.find(g->u.male?"player_male":"player_female");
		if(found!=tilemap.end())
		{
			gfxtile tile = found->second;
			if(tile.fg) s = tile.fg;
			if(tile.bg) bgt = tile.bg;
		}
		if(bgt) DrawMasked(20*16,15*16, 16, 16, 16*(bgt%16), 16*(bgt/16));
		if(s) DrawMasked(20*16,15*16, 16, 16, 16*(s%16), 16*(s/16));
	}

	RedrawWindow(GfxHandle, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

HBITMAP CreateBitmapMask(HBITMAP hbmColour, COLORREF crTransparent)
{
    HDC hdcMem, hdcMem2;
    HBITMAP hbmMask;
    BITMAP bm;

    // Create monochrome (1 bit) mask bitmap.  

    GetObject(hbmColour, sizeof(BITMAP), &bm);
    hbmMask = CreateBitmap(bm.bmWidth, bm.bmHeight, 1, 1, NULL);

    // Get some HDCs that are compatible with the display driver

    hdcMem = CreateCompatibleDC(0);
    hdcMem2 = CreateCompatibleDC(0);

    SelectObject(hdcMem, hbmColour);
    SelectObject(hdcMem2, hbmMask);

    // Set the background colour of the colour image to the colour
    // you want to be transparent.
    SetBkColor(hdcMem, crTransparent);

    // Copy the bits from the colour image to the B+W mask... everything
    // with the background colour ends up white while everythig else ends up
    // black...Just what we wanted.

    BitBlt(hdcMem2, 0, 0, bm.bmWidth, bm.bmHeight, hdcMem, 0, 0, SRCCOPY);

    // Take our new mask and use it to turn the transparent colour in our
    // original colour image to black so the transparency effect will
    // work right.
    BitBlt(hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, hdcMem2, 0, 0, SRCINVERT);

    // Clean up.

    DeleteDC(hdcMem);
    DeleteDC(hdcMem2);

    return hbmMask;
}



// config file
void load_gfx_config(){
    catajson all_items("gfx/tileset.json");

    //Crawl through and extract the items
    for (all_items.set_begin(); all_items.has_curr(); all_items.next()) {
        catajson entry = all_items.curr();
        // The one element we absolutely require for an item definition is an id
        if(!entry.has("id") || !entry.get("id").is_string())
        {
            ;//debugmsg("Item definition skipped, no id found or id was malformed.");
        }
        else
        {
            std::string new_id = entry.get("id").as_string();
			gfxtile tile = {0,0};
			if(entry.has("fg") )
				tile.fg = entry.get("fg").as_int();
			if(entry.has("bg") )
				tile.bg = entry.get("bg").as_int();
			if(tile.fg || tile.bg)
				tilemap[new_id] = tile;
        }
    }
}

#endif
