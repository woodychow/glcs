#!/bin/bash

usage() {
  printf "Usage: %s video_size pixel_format framerate base_output_filename\n" "$0"
}

# validate num params
if ! (( $# == 4 )); then
  usage
  exit -1
fi

exec >/tmp/pipe_ffmpeg.out 2>&1

echo "cmdline: $@"

#
# tweaking suggesting:
# Depending on your hardware, you might be able to use an higher quality
# preset. Possible value for preset are:
#
# - superfast
# - veryfast
# - faster
# - fast
#
exec schedtool -I -e ffmpeg -nostats -y \
 -f rawvideo -video_size $1 -pixel_format $2 -framerate $3 -i /dev/stdin \
 -f alsa -acodec pcm_s16le -ar 44100 -ac 2 -i loop_capture \
 -f alsa -acodec pcm_s16le -ar 32000 -ac 2 -i hw:3,0 \
 -f v4l2 -input_format yuyv422 -video_size 320x240 -framerate 30 -i /dev/video0 \
 -filter_complex "overlay;amix" \
 -c:a libfdk_aac -profile:a aac_low -b:a 128k -ar 44100 \
 -c:v libx264 -preset veryfast -profile:v main -level 4.1 -pix_fmt yuv420p \
 -x264opts keyint=60:bframes=2 -maxrate 6000k -bufsize 12000k -shortest $4.mkv

