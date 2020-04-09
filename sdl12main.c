#include <SDL.h>
#include <SDL_mixer.h>
#include <math.h>
#include <stdarg.h>
#include <assert.h>
#include <time.h>
#ifdef _3DS
#include <3ds.h>
#endif
#include "celeste.h"

static void ErrLog(char* fmt, ...) {
#ifdef _3DS
	FILE* f = fopen("sdmc:/ccleste.txt", "a");
	if (!f) return;
	fprintf(f, "%li \t", (long int)time(NULL));
#else
	FILE* f = stderr;
#endif

	va_list ap;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);

	if (f != stderr) fclose(f);
}

SDL_Surface* screen = NULL;
SDL_Surface* gfx = NULL;
SDL_Surface* font = NULL;
Mix_Chunk* snd[64] = {NULL};
Mix_Music* mus[6] = {NULL};

#define PICO8_W 128
#define PICO8_H 128

#ifdef _3DS
int scale = 2;
#else
int scale = 4;
#endif

static const SDL_Color base_palette[16] = {
	{0x00, 0x00, 0x00},
	{0x1d, 0x2b, 0x53},
	{0x7e, 0x25, 0x53},
	{0x00, 0x87, 0x51},
	{0xab, 0x52, 0x36},
	{0x5f, 0x57, 0x4f},
	{0xc2, 0xc3, 0xc7},
	{0xff, 0xf1, 0xe8},
	{0xff, 0x00, 0x4d},
	{0xff, 0xa3, 0x00},
	{0xff, 0xec, 0x27},
	{0x00, 0xe4, 0x36},
	{0x29, 0xad, 0xff},
	{0x83, 0x76, 0x9c},
	{0xff, 0x77, 0xa8},
	{0xff, 0xcc, 0xaa}
};
static void ResetPalette(void) {
	SDL_SetPalette(screen, SDL_PHYSPAL|SDL_LOGPAL, (SDL_Color*)base_palette, 0, 16);
	//memcpy(screen->format->palette->colors, base_palette, 16*sizeof(SDL_Color));
}

static char* GetDataPath(char* path, int n, const char* fname) {
#ifdef _3DS
	snprintf(path, n, "romfs:/%s", fname);
#else
#ifdef _WIN32
	char pathsep = '\\';
#else
	char pathsep = '/';
#endif //_WIN32
	snprintf(path, n, "data%c%s", pathsep, fname);
#endif //_3DS

	return path;
}

static Uint32 getpixel(SDL_Surface *surface, int x, int y) {
	int bpp = surface->format->BytesPerPixel;
	/* Here p is the address to the pixel we want to retrieve */
	Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

	switch(bpp) {
		case 1:
			return *p;

		case 2:
			return *(Uint16 *)p;

		case 3:
			if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
				return p[0] << 16 | p[1] << 8 | p[2];
			else
				return p[0] | p[1] << 8 | p[2] << 16;

		case 4:
			return *(Uint32 *)p;

		default:
			return 0;	   /* shouldn't happen, but avoids warnings */
	}
}

static void loadbmpscale(char* filename, SDL_Surface** s) {
	SDL_Surface* surf = *s;
	if (surf) SDL_FreeSurface(surf), surf = *s = NULL;

	char tmpath[4096];
	SDL_Surface* bmp = SDL_LoadBMP(GetDataPath(tmpath, sizeof tmpath, filename));
	if (!bmp) {
		ErrLog("error loading bmp '%s': %s\n", filename, SDL_GetError());
		return;
	}

	int w = bmp->w, h = bmp->h;

	unsigned char* data = SDL_malloc(w*h*scale*scale);
	/*memcpy((_S)->format->palette->colors, base_palette, 16*sizeof(SDL_Color));*/
	for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
		unsigned char pix = getpixel(bmp, x, y);
		for (int i = 0; i < scale; i++) for (int j = 0; j < scale; j++) {
			data[x*scale+i + (y*scale+j)*w*scale] = pix;
		}
	}
	SDL_FreeSurface(bmp);
	surf = SDL_CreateRGBSurfaceFrom(data, w*scale, h*scale, 8, w*scale, 0,0,0,0);
	SDL_SetPalette(surf, SDL_PHYSPAL | SDL_LOGPAL, (SDL_Color*)base_palette, 0, 16);
	SDL_SetColorKey(surf, SDL_SRCCOLORKEY, 0);
	//SDL_SaveBMP(_S, #_S "x.bmp");
	*s = surf;
}

static void LoadData(void) {
	loadbmpscale("gfx.bmp", &gfx);
	loadbmpscale("font.bmp", &font);

	static signed char sndids[] = {0,1,2,3,4,5,6,7,8,9,13,14,15,16,23,35,37,38,40,50,51,54,55, -1};
	for (signed char* iid = sndids; *iid != -1; iid++) {
		int id = *iid;
		char fname[20];
		sprintf(fname, "snd%i.wav", id);
		char path[4096];
		GetDataPath(path, sizeof path, fname);
		snd[id] = Mix_LoadWAV(path);
		if (!snd[id]) {
			ErrLog("snd%i: Mix_LoadWAV: %s\n", id, Mix_GetError());
		}
	}
	static signed char musids[] = {0,10,20,30,40, -1};
	for (signed char* iid = musids; *iid != -1; iid++) {
		int id = *iid;
		char fname[20];
		sprintf(fname, "mus%i.xm", id);
		char path[4096];
		GetDataPath(path, sizeof path, fname);
		mus[id/10] = Mix_LoadMUS(path);
		if (!mus[id/10]) {
			ErrLog("mus%i: Mix_LoadMUS: %s\n", id, Mix_GetError());
		}
	}
}
#include "data/tilemap.h"

int buttons_state = 0;

#define SDL_CHECK(r) do {                               \
	if (!(r)) {                                           \
		ErrLog("%s:%i, fatal error: `%s` -> %s\n", \
		        __FILE__, __LINE__, #r, SDL_GetError());    \
		exit(2);                                            \
	}                                                     \
} while(0)

int main(int argc, char** argv) {
	SDL_CHECK(SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO) == 0);
#ifdef _3DS
	romfsInit();
	sdmcInit();
	SDL_N3DSKeyBind(KEY_CPAD_UP|KEY_CSTICK_UP, SDLK_UP);
	SDL_N3DSKeyBind(KEY_CPAD_DOWN|KEY_CSTICK_DOWN, SDLK_DOWN);
	SDL_N3DSKeyBind(KEY_CPAD_LEFT|KEY_CSTICK_LEFT, SDLK_LEFT);
	SDL_N3DSKeyBind(KEY_CPAD_RIGHT|KEY_CSTICK_RIGHT, SDLK_RIGHT);
#endif
	int mixflag = MIX_INIT_MOD;
	if (Mix_Init(mixflag) != mixflag) {
		ErrLog("Mix_Init: %s\n", Mix_GetError());
	}
	if (Mix_OpenAudio(22050, AUDIO_U16, 1, 1024) < 0) {
		ErrLog("Mix_Init: %s\n", Mix_GetError());
	}
	SDL_CHECK(screen = SDL_SetVideoMode(PICO8_W*scale, PICO8_H*scale, 8, SDL_SWSURFACE | SDL_HWPALETTE));
	ResetPalette();
	SDL_ShowCursor(0);

	{
		const unsigned char loading_bmp[] = {
			0x42, 0x4d, 0xca, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x82, 0x00,
			0x00, 0x00, 0x6c, 0x00, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x09, 0x00,
			0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00,
			0x00, 0x00, 0x23, 0x2e, 0x00, 0x00, 0x23, 0x2e, 0x00, 0x00, 0x02, 0x00,
			0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x42, 0x47, 0x52, 0x73, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00,
			0x00, 0x00, 0xe0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00,
			0x00, 0x00, 0x66, 0x3e, 0xf1, 0x24, 0xf0, 0x00, 0x00, 0x00, 0x49, 0x44,
			0x92, 0x24, 0x90, 0x00, 0x00, 0x00, 0x49, 0x3c, 0x92, 0x24, 0x90, 0x00,
			0x00, 0x00, 0x49, 0x04, 0x92, 0x24, 0x90, 0x00, 0x00, 0x00, 0x46, 0x38,
			0xf0, 0x3c, 0xf0, 0x00, 0x00, 0x00, 0x40, 0x00, 0x12, 0x00, 0x00, 0x00,
			0x00, 0x00, 0xc0, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00
		};
		const unsigned int loading_bmp_len = 202;
		SDL_RWops* rw = SDL_RWFromConstMem(loading_bmp, loading_bmp_len);
		SDL_Surface* loading = SDL_LoadBMP_RW(rw, 1);
		if (!loading) goto skip_load;

		SDL_Rect rc = {60, 60};
		SDL_BlitSurface(loading,NULL,screen,&rc);
		
		SDL_Flip(screen);
		SDL_FreeSurface(loading);
	} skip_load:

	LoadData();

	Celeste_P8_val pico8emu(CELESTE_P8_CALLBACK_TYPE call, ...);
	Celeste_P8_set_call_func(pico8emu);

	Celeste_P8_init();

	int running = 1;
	while (running) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) switch (ev.type) {
			case SDL_QUIT: running = 0; break;
			case SDL_KEYDOWN: {
				if (ev.key.keysym.sym == SDLK_ESCAPE) {
					running = 0;
					break;
				}
				//fallthrough
			};
			case SDL_KEYUP: {
				int down = ev.type == SDL_KEYDOWN;
				int b = -1;
				switch (ev.key.keysym.sym) {
					case SDLK_LEFT:  b = 0; break;
					case SDLK_RIGHT: b = 1; break;
					case SDLK_UP:    b = 2; break;
					case SDLK_DOWN:  b = 3; break;
					case SDLK_z: case SDLK_c: case SDLK_n: case SDLK_a:
						b = 4; break;
					case SDLK_x: case SDLK_v: case SDLK_m: case SDLK_b:
						b = 5; break;
					default: break;
				}
				if (b >= 0) {
					if (down) buttons_state |=  (1<<b);
					else      buttons_state &= ~(1<<b);
				}
			}
		}

		Celeste_P8_update();
		
		Celeste_P8_draw();

		/*for (int i = 0 ; i < 16;i++) {
			SDL_Rect rc = {i*8*scale, 0, 8*scale, 4*scale};
			SDL_FillRect(screen, &rc, i);
		}*/

		SDL_Flip(screen);

		static int t = 0;
		static unsigned frame_start = 0;
		unsigned frame_end = SDL_GetTicks();
		unsigned frame_time = frame_end-frame_start;
		static const unsigned target_millis = 1000/30;
		if (frame_time < target_millis) {
			SDL_Delay((target_millis - frame_time) + (t & 1));
		}
		t++;
		frame_start = SDL_GetTicks();
	}

	SDL_FreeSurface(gfx);
	SDL_FreeSurface(font);
	for (int i = 0; i < (sizeof snd)/(sizeof *snd); i++) {
		Mix_FreeChunk(snd[i]);
	}
	for (int i = 0; i < (sizeof mus)/(sizeof *mus); i++) {
		Mix_FreeMusic(mus[i]);
	}

	Mix_Quit();
	SDL_Quit();
}

static int gettileflag(int, int);
static void Xline(int,int,int,int,unsigned char);

//lots of code from https://github.com/SDL-mirror/SDL/blob/bc59d0d4a2c814900a506d097a381077b9310509/src/video/SDL_surface.c#L625
static inline void Xblit(SDL_Surface* src, SDL_Rect* srcrect, SDL_Surface* dst, SDL_Rect* dstrect, int color, int flipx, int flipy) {
	assert(src && dst && !src->locked && !dst->locked);
	SDL_Rect fulldst;
	/* If the destination rectangle is NULL, use the entire dest surface */
	if (!dstrect)
		dstrect = (fulldst = (SDL_Rect){0,0,dst->w,dst->h}, &fulldst);

	int srcx, srcy, w, h;
	
	/* clip the source rectangle to the source surface */
	if (srcrect) {
		int maxw, maxh;

		srcx = srcrect->x;
		w = srcrect->w;
		if (srcx < 0) {
			w += srcx;
			dstrect->x -= srcx;
			srcx = 0;
		}
		maxw = src->w - srcx;
		if (maxw < w)
			w = maxw;

		srcy = srcrect->y;
		h = srcrect->h;
		if (srcy < 0) {
			h += srcy;
			dstrect->y -= srcy;
			srcy = 0;
		}
		maxh = src->h - srcy;
		if (maxh < h)
			h = maxh;

	} else {
		srcx = srcy = 0;
		w = src->w;
		h = src->h;
	}

	/* clip the destination rectangle against the clip rectangle */
	{
		SDL_Rect *clip = &dst->clip_rect;
		int dx, dy;

		dx = clip->x - dstrect->x;
		if (dx > 0) {
			w -= dx;
			dstrect->x += dx;
			srcx += dx;
		}
		dx = dstrect->x + w - clip->x - clip->w;
		if (dx > 0)
			w -= dx;

		dy = clip->y - dstrect->y;
		if (dy > 0) {
			h -= dy;
			dstrect->y += dy;
			srcy += dy;
		}
		dy = dstrect->y + h - clip->y - clip->h;
		if (dy > 0)
			h -= dy;
	}

	if (w && h) {
		unsigned char* srcpix = src->pixels;
		int srcpitch = src->pitch;
		unsigned char* dstpix = dst->pixels;
		int dstpitch = dst->pitch;
		if (color) {
			for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
				unsigned char p = srcpix[srcx+x+(srcy+y)*srcpitch];
				if (p) dstpix[dstrect->x+x + (dstrect->y+y)*dstpitch] = color;
			}
		} else {
			for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
				unsigned char p = srcpix[srcx+x+(srcy+y)*srcpitch];
				if (p) dstpix[dstrect->x+x + (dstrect->y+y)*dstpitch] = p;
			}
		}
	}
}

Celeste_P8_val pico8emu(CELESTE_P8_CALLBACK_TYPE call, ...) {
	static int camera_x = 0, camera_y = 0;

	va_list args;
	Celeste_P8_val ret = {.i = 0};
	va_start(args, call);
	
	#define   INT_ARG() va_arg(args, int)
	#define  BOOL_ARG() va_arg(args, Celeste_P8_bool_t)
	#define FLOAT_ARG() va_arg(args, double)
	#define RET_INT(_i)   do {ret.i = (_i); goto end;} while (0)
	#define RET_FLOAT(_f) do {ret.f = (_f); goto end;} while (0)
	#define RET_BOOL(_b) RET_INT(!!(_b))
	#define CASE(t, ...) case t: {__VA_ARGS__;} break;

	switch (call) {
		CASE(CELESTE_P8_RND, //rnd(max)
			float max = FLOAT_ARG();
			float v = max * ((float)rand() / (float)RAND_MAX);
			RET_FLOAT(v);
		)
		CASE(CELESTE_P8_MUSIC, //music(idx,fade,mask)
			int index = INT_ARG();
			int fade = INT_ARG();
			int mask = INT_ARG();

			if (mus[index/10]) Mix_PlayMusic(mus[index/10], -1);
		)
		CASE(CELESTE_P8_SPR, //spr(sprite,x,y,cols,rows,flipx,flipy)
			int sprite = INT_ARG();
			float x = FLOAT_ARG();
			float y = FLOAT_ARG();
			int cols = INT_ARG();
			int rows = INT_ARG();
			int flipx = BOOL_ARG();
			int flipy = BOOL_ARG();

			if (sprite >= 0) {
				SDL_Rect srcrc = {
					8*(sprite % 16),
					8*(sprite / 16)
				};
				srcrc.x *= scale;
				srcrc.y *= scale;
				srcrc.w = srcrc.h = scale*8;
				SDL_Rect dstrc = {
					(x - camera_x)*scale, (y - camera_y)*scale,
					scale, scale
				};
				//TODO flipping
				Xblit(gfx, &srcrc, screen, &dstrc, 0,flipx,flipy);
			}
		)
		CASE(CELESTE_P8_BTN, //btn(b)
			RET_BOOL(buttons_state & (1 << (INT_ARG())));
		)
		CASE(CELESTE_P8_SFX, //sfx(id)
			int id = INT_ARG();
		
			if (id < (sizeof snd) / (sizeof*snd) && snd[id])
				Mix_PlayChannel(-1, snd[id], 0);
		)
		CASE(CELESTE_P8_PAL, //pal(a,b)
			int a = INT_ARG();
			int b = INT_ARG();
			if (a >= 0 && a < 16 && b >= 0 && b < 16) {
				//swap palette colors
				SDL_Color* c1 = &screen->format->palette->colors[a];
				SDL_Color* c2 = &screen->format->palette->colors[b];
				SDL_Color tmp = *c1;
				*c1 = *c2;
				*c2 = tmp;
			}
		)
		CASE(CELESTE_P8_PAL_RESET, //pal()
			ResetPalette();
		)
		CASE(CELESTE_P8_CIRCFILL, //circfill(x,y,r,col)
			float cx = FLOAT_ARG() - camera_x;
			float cy = FLOAT_ARG() - camera_y;
			float r = FLOAT_ARG();
			int col = INT_ARG();
			{	r *= scale, cx *= scale, cy *= scale;
				int f = 1 - r; //used to track the progress of the drawn circle (since its semi-recursive)
				int ddFx = 1; //step x
				int ddFy = -2 * r; //step y
				int x = 0;
				int y = r;

				//this algorithm doesn't account for the diameters
				//so we have to set them manually
				Xline(cx,cy-y, cx,cy+r, col);
				Xline(cx+r,cy, cx-r,cy, col);

				while (x < y) {
					if (f >= 0) {
						y--;
						ddFy += 2;
						f += ddFy;
					}
					x++;
					ddFx += 2;
					f += ddFx;

					//build our current arc
					Xline(cx+x,cy+y, cx-x,cy+y, col);
					Xline(cx+x,cy-y, cx-x,cy-y, col);
					Xline(cx+y,cy+x, cx-y,cy+x, col);
					Xline(cx+y,cy-x, cx-y,cy-x, col);
				}
			}
		)
		CASE(CELESTE_P8_PRINT, //print(str,x,y,col)
			char* str = va_arg(args, char*);
			float x = FLOAT_ARG() - camera_x;
			float y = FLOAT_ARG() - camera_y;
			int col = INT_ARG() % 16;

			for (char c = *str; c; c = *(++str)) {
				c &= 127;
				SDL_Rect srcrc = {8*(c%16), 8*(c/16)};
				srcrc.x *= scale;
				srcrc.y *= scale;
				srcrc.w = srcrc.h = 8*scale;
				
				SDL_Rect dstrc = {x*scale, y*scale, scale, scale};
				//TODO color
				Xblit(font, &srcrc, screen, &dstrc, col, 0,0);
				x += 4;
			}
		)
		CASE(CELESTE_P8_RECTFILL, //rectfill(x0,y0,x1,y1,col)
			float x0 = FLOAT_ARG() - camera_x;
			float y0 = FLOAT_ARG() - camera_y;
			float x1 = FLOAT_ARG() - camera_x;
			float y1 = FLOAT_ARG() - camera_y;
			int col = INT_ARG();
			
			SDL_Rect rc = {x0*scale,y0*scale,(x1-x0)*scale,(y1-y0)*scale};
			SDL_FillRect(screen, &rc, col);
		)
		CASE(CELESTE_P8_LINE, //line(x0,y0,x1,y1,col)
			float x0 = FLOAT_ARG() - camera_x;
			float y0 = FLOAT_ARG() - camera_y;
			float x1 = FLOAT_ARG() - camera_x;
			float y1 = FLOAT_ARG() - camera_y;
			int col = INT_ARG();

			Xline(x0,y0,x1,y1,col);
		)
		CASE(CELESTE_P8_MGET, //mget(tx,ty)
			int tx = INT_ARG();
			int ty = INT_ARG();

			RET_INT(tilemap_data[tx+ty*128]);
		)
		CASE(CELESTE_P8_CAMERA, //camera(x,y)
			camera_x = INT_ARG();
			camera_y = INT_ARG();
		)
		CASE(CELESTE_P8_FGET, //fget(tile,flag)
			int tile = INT_ARG();
			int flag = INT_ARG();

			RET_INT(gettileflag(tile, flag));
		)
		CASE(CELESTE_P8_MAP, //map(mx,my,tx,ty,mw,mh,mask)
			int mx = INT_ARG(), my = INT_ARG();
			int tx = INT_ARG(), ty = INT_ARG();
			int mw = INT_ARG(), mh = INT_ARG();
			int mask = INT_ARG();
			
			for (int x = 0; x < mw; x++) {
				for (int y = 0; y < mh; y++) {
					int tile = tilemap_data[x + mx + (y + my)*128];
					//hack
					if (mask == 0 || (mask == 4 && tile_flags[tile] == 4) || gettileflag(tile, mask != 4 ? mask-1 : mask)) {
						//al_draw_bitmap(sprites[tile], tx+x*8 - camera_x, ty+y*8 - camera_y, 0);
						SDL_Rect srcrc = {
							8*(tile % 16),
							8*(tile / 16)
						};
						srcrc.x *= scale;
						srcrc.y *= scale;
						srcrc.w = srcrc.h = scale*8;
						SDL_Rect dstrc = {
							(tx+x*8 - camera_x)*scale, (ty+y*8 - camera_y)*scale,
							scale*8, scale*8
						};

						if (0) {
							srcrc.x = srcrc.y = 0;
							srcrc.w = srcrc.h = 8;
							dstrc.x = x*8, dstrc.y = y*8;
							dstrc.w = dstrc.h = 8;
						}

						SDL_BlitSurface(gfx, &srcrc, screen, &dstrc);
					}
				}
			}
		)
	}

	end:
	va_end(args);
	return ret;
}

static int gettileflag(int tile, int flag) {
	return tile < sizeof(tile_flags)/sizeof(*tile_flags) && (tile_flags[tile] & (1 << flag)) != 0;
}

static void Xline(int x0, int y0, int x1, int y1, unsigned char color) {
	#define CLAMP(v,min,max) v = v < min ? min : v >= max ? max-1 : v;
	CLAMP(x0,0,screen->w);
	CLAMP(y0,0,screen->h);
	CLAMP(x1,0,screen->w);
	CLAMP(y1,0,screen->h);
	#undef CLAMP
	//x0 *= scale, x1 *= scale, y0 *= scale, y1 *= scale;
	#define PLOT(x,y,c) (((Uint8*)screen->pixels)[x+y*screen->pitch] = (c))
	int sx, sy, dx, dy, err, e2;
	dx = abs(x1 - x0);
	dy = abs(y1 - y0);
	if (!dx && !dy) return;

	if (x0 < x1) sx = 1; else sx = -1;
	if (y0 < y1) sy = 1; else sy = -1;
	err = dx - dy;

	if (!dy) { //horizontal line
		memset(screen->pixels+(x0<x1?x0:x1) + y0*screen->pitch, color, dx);
	} else if (!dx) { //vertical line
		for (int y = y0; y != y1; y += sy) {
			PLOT(x0,y,color);
		}
	} else while (x0 != x1 || y0 != y1) {
		PLOT(x0, y0, color);
		e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			x0 += sx;
		}
		if (e2 < dx) {
			err += dx;
			y0 += sy;
		}
	}
	#undef PLOT
}
