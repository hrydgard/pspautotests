#!/bin/sh
# Regenerates sample.mp4: a synthetic, copyright-free movie in the format the PSP's sceMp4 plays.
# 480x272 H.264 Main@3.0 at 29.97 fps and ~768 kb/s (like a typical PSP game movie), AAC-LC
# 44.1 kHz stereo, 10 seconds. The PSP decoder rejects B-pyramid and weighted P prediction,
# which x264 enables by default.
ffmpeg -y \
	-f lavfi -i "mandelbrot=size=480x272:rate=30000/1001" \
	-f lavfi -i "aevalsrc=0.2*sin(2*PI*(220+110*sin(2*PI*0.25*t))*t)|0.2*sin(2*PI*(330+110*sin(2*PI*0.2*t))*t):s=44100" \
	-t 10 \
	-c:v libx264 -profile:v main -level 3.0 -pix_fmt yuv420p -b:v 768k -maxrate 1000k -bufsize 2000k \
	-x264-params ref=3:bframes=0:b-pyramid=none:weightp=0:repeat-headers=1:keyint=300 \
	-c:a aac -b:a 128k -ar 44100 -ac 2 \
	-movflags +faststart -f psp sample.mp4
