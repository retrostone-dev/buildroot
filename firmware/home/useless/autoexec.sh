#!/bin/sh
export HOME="/home/useless"
export SDL_NOMOUSE="1"
export SDL_VIDEODRIVER="fbcon"
export SDL_FBDEV="/dev/fb0"
export SDL_FBACCEL="1"
modprobe ump
modprobe drm
modprobe mali
modprobe mali_drm
modprobe tv
cd /home/useless
./tvout -r
./tvout -m -x 33 -y 18
#MyApphere
