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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <errno.h>
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

#define APPLY_KEY(a, b) \
	key_pressed[i] = a; \
	ks = b; \
	keysym.scancode = i; \
	keysym.sym = sdlk_keymap[i]; \
	posted += SDL_PrivateKeyboard(ks, &keysym); \


static uint32_t nspk_keymap[NSP_NUMKEYS];
static SDLKey sdlk_keymap[NSP_NUMKEYS];
static int32_t key_pressed[NSP_NUMKEYS];

static int keyboard_fb_;
static struct input_event data;

static int32_t posted = 0;

enum
{
	DPAD_UP = 12,
	DPAD_DOWN = 5,
	DPAD_LEFT = 0,
	DPAD_RIGHT = 11,
	BUTTON_A = 14,
	BUTTON_B = 15,
	BUTTON_Y = 9,
	BUTTON_X = 13,
	BUTTON_SELECT = 4,
	BUTTON_START = 1,
	BUTTON_L2 = 6,
	BUTTON_R2 = 7,
	BUTTON_L = 8,
	BUTTON_R = 3
};

/* GPIO */

/****************************************************************************************/
/* Definitions                                                                          */
/****************************************************************************************/

/* SUNXI GPIO control IO base address */
#define SUNXI_GPIO_IO_BASE                      0x01c20800

/* Macros used to configure GPIOs */
#define SUNXI_GPIO_BANK(pin)                    ((pin) >> 5)
#define SUNXI_GPIO_NUM(pin)                     ((pin) & 0x1F)
#define SUNXI_GPIO_CFG_INDEX(pin)               (((pin) & 0x1F) >> 3)
#define SUNXI_GPIO_CFG_OFFSET(pin)              ((((pin) & 0x1F) & 0x7) << 2)


/* SUNXI GPIO pin function configuration */
#define SUNXI_GPIO_INPUT                        0
#define SUNXI_GPIO_OUTPUT                       1
#define SUNXI_GPIO_PER                          2

/* SUNXI GPIO macro */
#define SUNXI_GPIO_PIN(port, pin)               ((port - 'A') << 5) + pin


/* SUNXI GPIO Bank */
struct sunxi_gpio_bank {
  volatile unsigned int cfg[4];
  volatile unsigned int dat;
  volatile unsigned int drv[2];
  volatile unsigned int pull[2];
};

/* SUNXI GPIO Interrupt control */
struct sunxi_gpio_int {
  volatile unsigned int cfg[3];
  volatile unsigned int ctl;
  volatile unsigned int sta;
  volatile unsigned int deb;
};

/* SUNXI GPIO Registers */
struct sunxi_gpio_reg {
  volatile struct sunxi_gpio_bank gpio_bank[9];
  volatile unsigned char res[0xbc];
  volatile struct sunxi_gpio_int gpio_int;
};


/****************************************************************************************/
/* Global variables                                                                     */
/****************************************************************************************/

/* SUNXI GPIO registers */
static volatile struct sunxi_gpio_reg *sunxi_gpio_registers = NULL;


/****************************************************************************************/
/* Exported functions                                                                   */
/****************************************************************************************/

/**
 * Initialize GPIO interface, to be called once
 * @return 0 if the function succeeds, error code otherwise
 */
static int sunxi_gpio_init() {
  
  int fd;
  unsigned int page_size, page_mask;
  unsigned int addr_start, addr_offset;
  void *pc;

  /* Open device */
  fd = open("/dev/mem", O_RDWR);
  if (fd < 0) {
    return -errno;
  }
  
  /* Map to device */
  page_size = sysconf(_SC_PAGESIZE);
  page_mask = (~(page_size-1));
  addr_start = SUNXI_GPIO_IO_BASE & page_mask;
  addr_offset = SUNXI_GPIO_IO_BASE & ~page_mask;
  pc = (void *)mmap(NULL, (((sizeof(struct sunxi_gpio_reg) + addr_offset) / page_size) + 1) * page_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, addr_start);
  if (pc == MAP_FAILED) {
    return -errno;
  }

  /* Retrieve registers address used later in this library */
  sunxi_gpio_registers = (struct sunxi_gpio_reg *)(pc + addr_offset);

  /* Close device */
  close(fd);
  
  return 0;
}

/**
 * Set pin configuration
 * @param pin Expected pin, see SUNXI_GPIO_PIN macros
 * @param val Expected function, SUNXI_GPIO_INPUT or SUNXI_GPIO_OUTPUT
 * @return 0 if the function succeeds, error code otherwise
 */
static int sunxi_gpio_set_cfgpin(unsigned int pin, unsigned int val) {
  
  unsigned int cfg;
  unsigned int bank = SUNXI_GPIO_BANK(pin);
  unsigned int index = SUNXI_GPIO_CFG_INDEX(pin);
  unsigned int offset = SUNXI_GPIO_CFG_OFFSET(pin);

  /* Check if initialization has been performed */
  if (sunxi_gpio_registers == NULL) {
    return -EPERM;
  }

  /* Set pin configuration */
  volatile struct sunxi_gpio_bank *pio = &(sunxi_gpio_registers->gpio_bank[bank]);
  cfg = *(&pio->cfg[0] + index);
  cfg &= ~(0xf << offset);
  cfg |= val << offset;
  *(&pio->cfg[0] + index) = cfg;

  return 0;
}

/**
 * Get pin input value
 * @param pin Expected pin, see SUNXI_GPIO_PIN macros
 * @return Pin input value if the function succeeds, error code otherwise
 */
static int sunxi_gpio_input(unsigned int pin) {
  
  unsigned int dat;
  unsigned int bank = SUNXI_GPIO_BANK(pin);
  unsigned int num = SUNXI_GPIO_NUM(pin);
  
  /* Check if initialization has been performed */
  if (sunxi_gpio_registers == NULL) {
    return -EPERM;
  }
  
  /* Get pin value */
  volatile struct sunxi_gpio_bank *pio = &(sunxi_gpio_registers->gpio_bank[bank]);
  dat = *(&pio->dat);
  dat >>= num;
  return (dat & 0x1);
}

/*             */
/* END of GPIO */
/*             */

void emit(int fd, int type, int code, int val)
{
	struct input_event ie;
	ie.type = type; ie.code = code;
	ie.value = val;
	ie.time.tv_sec = 0; ie.time.tv_usec = 0;
	write(fd, &ie, sizeof(ie));
}

static void Update_Keyboard(void)
{
	int32_t ks, i;
	int32_t value[14];
	SDL_keysym keysym;
	
	do 
	{
		value[0] = sunxi_gpio_input(SUNXI_GPIO_PIN('D', DPAD_UP));
		value[1] = sunxi_gpio_input(SUNXI_GPIO_PIN('D', DPAD_DOWN));
		value[2] = sunxi_gpio_input(SUNXI_GPIO_PIN('D', DPAD_LEFT));
		value[3] = sunxi_gpio_input(SUNXI_GPIO_PIN('D', DPAD_RIGHT));
		value[4] = sunxi_gpio_input(SUNXI_GPIO_PIN('D', BUTTON_A));
		value[5] = sunxi_gpio_input(SUNXI_GPIO_PIN('D', BUTTON_B));
		value[6] = sunxi_gpio_input(SUNXI_GPIO_PIN('D', BUTTON_X));
		value[7] = sunxi_gpio_input(SUNXI_GPIO_PIN('D', BUTTON_Y));
		value[8] = sunxi_gpio_input(SUNXI_GPIO_PIN('D', BUTTON_L));
		value[9] = sunxi_gpio_input(SUNXI_GPIO_PIN('D', BUTTON_R));
		value[10] = sunxi_gpio_input(SUNXI_GPIO_PIN('D', BUTTON_L2));
		value[11] = sunxi_gpio_input(SUNXI_GPIO_PIN('D', BUTTON_R2));
		value[12] = sunxi_gpio_input(SUNXI_GPIO_PIN('D', BUTTON_START));
		value[13] = sunxi_gpio_input(SUNXI_GPIO_PIN('D', BUTTON_SELECT));
		
		posted = 0;
		
		for ( i = 0; i < NSP_NUMKEYS; ++i ) 
		{
			if ( sdlk_keymap[i] == SDLK_UNKNOWN )
			{
				continue;
			}
			
			switch(key_pressed[i])
			{
				case 0:
				if (value[i] == 0)
				{
					APPLY_KEY(1, SDL_PRESSED)
				}
				break;
				case 1:
				if (value[i] == 1)
				{
					APPLY_KEY(2, SDL_RELEASED)
				}
				break;
				case 2:
					key_pressed[i] = 0;
					ks = 0;
				break;
			}
		}
	} while ( posted );
}

void FB_PumpEvents(_THIS)
{
	Update_Keyboard();
}

int FB_OpenKeyboard(_THIS)
{
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
		key_pressed[i] = 0;
	}
	
	sunxi_gpio_init();
	sunxi_gpio_set_cfgpin(SUNXI_GPIO_PIN('D', DPAD_UP), SUNXI_GPIO_INPUT);
	sunxi_gpio_set_cfgpin(SUNXI_GPIO_PIN('D', DPAD_DOWN), SUNXI_GPIO_INPUT);
	sunxi_gpio_set_cfgpin(SUNXI_GPIO_PIN('D', DPAD_LEFT), SUNXI_GPIO_INPUT);
	sunxi_gpio_set_cfgpin(SUNXI_GPIO_PIN('D', DPAD_RIGHT), SUNXI_GPIO_INPUT);
	sunxi_gpio_set_cfgpin(SUNXI_GPIO_PIN('D', BUTTON_A), SUNXI_GPIO_INPUT);
	sunxi_gpio_set_cfgpin(SUNXI_GPIO_PIN('D', BUTTON_B), SUNXI_GPIO_INPUT);
	sunxi_gpio_set_cfgpin(SUNXI_GPIO_PIN('D', BUTTON_Y), SUNXI_GPIO_INPUT);
	sunxi_gpio_set_cfgpin(SUNXI_GPIO_PIN('D', BUTTON_X), SUNXI_GPIO_INPUT);
	sunxi_gpio_set_cfgpin(SUNXI_GPIO_PIN('D', BUTTON_SELECT), SUNXI_GPIO_INPUT);
	sunxi_gpio_set_cfgpin(SUNXI_GPIO_PIN('D', BUTTON_START), SUNXI_GPIO_INPUT);
	sunxi_gpio_set_cfgpin(SUNXI_GPIO_PIN('D', BUTTON_L), SUNXI_GPIO_INPUT);
	sunxi_gpio_set_cfgpin(SUNXI_GPIO_PIN('D', BUTTON_R), SUNXI_GPIO_INPUT);
	sunxi_gpio_set_cfgpin(SUNXI_GPIO_PIN('D', BUTTON_L2), SUNXI_GPIO_INPUT);
	sunxi_gpio_set_cfgpin(SUNXI_GPIO_PIN('D', BUTTON_R2), SUNXI_GPIO_INPUT);

	/* Enum value -> KEY_* */
	nspk_keymap[0] =	DPAD_UP;
	nspk_keymap[1] =	DPAD_DOWN;
	nspk_keymap[2] =	DPAD_LEFT;
	nspk_keymap[3] =	DPAD_RIGHT;
	
	nspk_keymap[4] =	BUTTON_A;
	nspk_keymap[5] =	BUTTON_B;
	nspk_keymap[6] =	BUTTON_X;
	nspk_keymap[7] =	BUTTON_Y;

	nspk_keymap[8] =	BUTTON_L;
	nspk_keymap[9] =	BUTTON_R;
	nspk_keymap[10] =	BUTTON_L2;
	nspk_keymap[11] =	BUTTON_R2;
	
	nspk_keymap[12] =	BUTTON_START;
	nspk_keymap[13] =	BUTTON_SELECT;
	

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

