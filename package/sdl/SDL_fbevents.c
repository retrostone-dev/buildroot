/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2012 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/

/* Patch to apply against SDL 1.2 fbcon for working input on the Retrostone */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h> 

#include <linux/fb.h>
#include <linux/input.h>
#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/keyboard.h>
#include <linux/input-event-codes.h>
#include "SDL_config.h"

#include "SDL.h"
#include "SDL_keysym.h"
#include "SDL_fbkeys.h"
#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"
#include "../SDL_cursor_c.h"

#include "SDL_fbvideo.h"
#include "SDL_fbevents_c.h"

#define SCANCODE_ESCAPE			1
#define SCANCODE_3			4
#define SCANCODE_BACKSPACE		14
#define SCANCODE_TAB			15
#define SCANCODE_ENTER			28
#define SCANCODE_LEFTCONTROL		29
#define SCANCODE_LEFTSHIFT		42
#define SCANCODE_RIGHTSHIFT		54
#define SCANCODE_LEFTALT		56
#define SCANCODE_SPACE			57
#define SCANCODE_CURSORUPLEFT		71
#define SCANCODE_CURSORUP		72
#define SCANCODE_CURSORUPRIGHT		73
#define SCANCODE_CURSORLEFT		75
#define SCANCODE_CURSORRIGHT		77
#define SCANCODE_CURSORDOWNLEFT		79
#define SCANCODE_CURSORDOWN		80
#define SCANCODE_CURSORDOWNRIGHT	81
#define SCANCODE_KEYPADENTER		96
#define SCANCODE_RIGHTCONTROL		97
#define SCANCODE_CONTROL		97
#define SCANCODE_END			107
#define NSP_NUMKEYS SCANCODE_END+1

#define NSP_UPDATE_KEY_EVENT(s, sc, ks, kp) do { \
	SDL_keysym keysym; \
	keysym.scancode = sc; \
	keysym.sym = s; \
	if ( ks == SDL_RELEASED ) { \
		if ( kp ) { \
			SDL_PrivateKeyboard(SDL_PRESSED, &keysym); \
			ks = SDL_PRESSED; \
		} \
	} else if ( ! kp ) { \
		SDL_PrivateKeyboard(SDL_RELEASED, &keysym); \
		ks = SDL_RELEASED; \
	} \
} while (0)

static uint32_t nspk_keymap[NSP_NUMKEYS];
static uint32_t key_state[NSP_NUMKEYS];
static SDLKey sdlk_keymap[NSP_NUMKEYS];

static int keyboard_fb_;
static struct input_event data;

static uint32_t returnkey(uint32_t key)
{
	uint32_t bytes;
	bytes = read(keyboard_fb_, &data, sizeof(data));
	if (data.code == key)
	{
		if (bytes > 0 && data.value == 1)
		{
			return 1;
		}
	}
	return 0;
}

static void Update_Keyboard(void)
{
	uint32_t i;
	
	for ( i = 0; i < NSP_NUMKEYS; ++i ) 
	{
		uint32_t key_pressed;
		if ( sdlk_keymap[i] == SDLK_UNKNOWN )
			continue;
		key_pressed = returnkey(nspk_keymap[i]);
		NSP_UPDATE_KEY_EVENT(sdlk_keymap[i], i, key_state[i], key_pressed);
	}
}

void FB_PumpEvents(_THIS)
{
	Update_Keyboard();
}

int FB_OpenKeyboard(_THIS)
{
	if (!keyboard_fb_)
	{
		keyboard_fb_ = open("/dev/input/event3", O_RDONLY | O_NONBLOCK);
		if (!keyboard_fb_) 
		{
			keyboard_fb_ = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
			if (!keyboard_fb_) 
			{
				printf("Error: Cannot open Keyboard\n");
				return -1;
			}
		}	
	}
	return 0;
}

int FB_EnterGraphicsMode(_THIS)
{
	return 0;
}

int FB_OpenMouse(_THIS)
{
	return 0;
}

void FB_CloseKeyboard(_THIS)
{
	if (keyboard_fb_) close(keyboard_fb_);
}

void FB_CloseMouse(_THIS)
{
}

int FB_InGraphicsMode(_THIS)
{
	return 0;
}


void FB_InitOSKeymap(_THIS)
{
	uint32_t i;
	for (i=0;i<NSP_NUMKEYS;i++)
	{
		nspk_keymap[i] = 0;
		sdlk_keymap[i] = 0;
		key_state[i] = 0;	
	}
	
	/* Enum value -> KEY_* */
	nspk_keymap[SCANCODE_CURSORUP] =	KEY_UP;
	nspk_keymap[SCANCODE_CURSORDOWN] =	KEY_DOWN;
	nspk_keymap[SCANCODE_CURSORLEFT] =	KEY_LEFT;
	nspk_keymap[SCANCODE_CURSORRIGHT] =	KEY_RIGHT;
	
	nspk_keymap[SCANCODE_LEFTCONTROL] =	KEY_LEFTCTRL;
	nspk_keymap[SCANCODE_LEFTALT] =	KEY_LEFTALT;
	nspk_keymap[SCANCODE_LEFTSHIFT] =	KEY_LEFTSHIFT;
	nspk_keymap[SCANCODE_SPACE] =	KEY_SPACE;

	nspk_keymap[SCANCODE_TAB] =	KEY_TAB;
	nspk_keymap[SCANCODE_BACKSPACE] =	KEY_BACKSPACE;
	nspk_keymap[SCANCODE_3] =	KEY_3;
	nspk_keymap[SCANCODE_END] =	KEY_END;
	
	nspk_keymap[SCANCODE_ENTER] =	KEY_ENTER;
	nspk_keymap[SCANCODE_ESCAPE] =	KEY_ESC;
	
	keyboard_fb_ = open("/dev/input/event3", O_RDONLY | O_NONBLOCK);
	if (!keyboard_fb_) 
	{
		keyboard_fb_ = open("/dev/input/event0", O_RDONLY | O_NONBLOCK);
		if (!keyboard_fb_) 
		{
			printf("Error: Cannot open Keyboard\n");
			return;
		}
	}

	/* Enum value -> SDLK_* This is the actual key mapping part. */
	sdlk_keymap[SCANCODE_CURSORUP] =	SDLK_UP;
	sdlk_keymap[SCANCODE_CURSORDOWN] =	SDLK_DOWN;
	sdlk_keymap[SCANCODE_CURSORLEFT] =	SDLK_LEFT;
	sdlk_keymap[SCANCODE_CURSORRIGHT] =	SDLK_RIGHT;
	
	sdlk_keymap[SCANCODE_LEFTCONTROL] =	SDLK_LCTRL;
	sdlk_keymap[SCANCODE_LEFTALT] =	SDLK_LALT;
	sdlk_keymap[SCANCODE_LEFTSHIFT] =	SDLK_LSHIFT;
	sdlk_keymap[SCANCODE_SPACE] =	SDLK_SPACE;
	
	sdlk_keymap[SCANCODE_TAB] =	SDLK_TAB;
	sdlk_keymap[SCANCODE_BACKSPACE] =	SDLK_BACKSPACE;
	sdlk_keymap[SCANCODE_3] =	SDLK_3;
	sdlk_keymap[SCANCODE_END] =	SDLK_END;
	
	sdlk_keymap[SCANCODE_ENTER] =	SDLK_RETURN;
	sdlk_keymap[SCANCODE_ESCAPE] =	SDLK_ESCAPE;
}

/* end of SDL_tinspireevents.c ... */

