#!/bin/bash
#
# Takes one optional param.
#
# PREFIX: Specify where the project is installed. default is /
#

usage()
{
	echo "usage: $0 destdir"
	exit 1
}

mods=("elfhacks" "packetstream" "../glcs")

if ! (( $# == 1 )); then
	usage
fi

DESTDIR=$1

GLCSDIR=$PWD

for mod in ${mods[@]}; do
	echo "Installing $mod to $DESTDIR ..."
	cd $GLCSDIR/$mod/build
	make install || exit 1
done

install -d -m755 $DESTDIR/share/glcs/scripts
install -m755 $GLCSDIR/scripts/capture.sh $DESTDIR/share/glcs/scripts/capture.sh   
install -m755 $GLCSDIR/scripts/pipe_ffmpeg.sh $DESTDIR/share/glcs/scripts/pipe_ffmpeg.sh   
install -m755 $GLCSDIR/scripts/webcam_overlay_mix_audio.sh $DESTDIR/share/glcs/scripts/webcam_overlay_mix_audio.sh   
install -d -m755 $DESTDIR/share/licenses/glcs
install -m644 $GLCSDIR/COPYING $DESTDIR/share/licenses/glcs/COPYING

