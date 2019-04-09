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

#define NSP_NUMKEYS 14

static uint32_t nspk_keymap[NSP_NUMKEYS];
static uint32_t key_state[NSP_NUMKEYS];
static SDLKey sdlk_keymap[NSP_NUMKEYS];

static int keyboard_fb_;
static struct input_event data;

static void Update_Keyboard(void)
{
	SDL_keysym keysym;
	uint32_t i, bytes;
	bytes = read(keyboard_fb_, &data, sizeof(data));
	
	if (!(bytes > 0))
		return;
	
	for ( i = 0; i < NSP_NUMKEYS; ++i ) 
	{
		uint32_t key_pressed, ks;
		if ( sdlk_keymap[i] != SDLK_UNKNOWN )
		{
			if (data.code == nspk_keymap[i])
			{
				if (data.value == 1 || data.value == 2)
				{
					key_pressed = 1;
					ks = SDL_PRESSED;
				}
				else if (data.value == 0)
				{
					key_pressed = 0;
					ks = SDL_RELEASED;
				}
				keysym.scancode = i;
				keysym.sym = sdlk_keymap[i];
				SDL_PrivateKeyboard(ks, &keysym);
			}
		}
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
		sdlk_keymap[i] = SDLK_UNKNOWN;
		key_state[i] = 0;	
	}
	
	/* Enum value -> KEY_* */
	nspk_keymap[0] =	KEY_UP;
	nspk_keymap[1] =	KEY_DOWN;
	nspk_keymap[2] =	KEY_LEFT;
	nspk_keymap[3] =	KEY_RIGHT;
	
	nspk_keymap[4] =	KEY_LEFTCTRL;
	nspk_keymap[5] =	KEY_LEFTALT;
	nspk_keymap[6] =	KEY_LEFTSHIFT;
	nspk_keymap[7] =	KEY_SPACE;

	nspk_keymap[8] =	KEY_TAB;
	nspk_keymap[9] =	KEY_BACKSPACE;
	nspk_keymap[10] =	KEY_3;
	nspk_keymap[11] =	KEY_END;
	
	nspk_keymap[12] =	KEY_ENTER;
	nspk_keymap[13] =	KEY_ESC;
	
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
	sdlk_keymap[0] =	SDLK_UP;
	sdlk_keymap[1] =	SDLK_DOWN;
	sdlk_keymap[2] =	SDLK_LEFT;
	sdlk_keymap[3] =	SDLK_RIGHT;
	
	sdlk_keymap[4] =	SDLK_LCTRL;
	sdlk_keymap[5] =	SDLK_LALT;
	sdlk_keymap[6] =	SDLK_LSHIFT;
	sdlk_keymap[7] =	SDLK_SPACE;
	
	sdlk_keymap[8] =	SDLK_TAB;
	sdlk_keymap[9] =	SDLK_BACKSPACE;
	sdlk_keymap[10] =	SDLK_3;
	sdlk_keymap[11] =	SDLK_END;
	
	sdlk_keymap[12] =	SDLK_RETURN;
	sdlk_keymap[13] =	SDLK_ESCAPE;
}

/* end of SDL_tinspireevents.c ... */

