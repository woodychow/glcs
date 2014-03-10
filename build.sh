#!/bin/sh
#
# Takes one optional param.
#
# PREFIX: Specify where the project is installed. default is /
#

usage()
{
	echo "usage: $0 destdir [libdir]"
	exit 1
}

mods=("elfhacks" "packetstream" "../glcs")

if ! (( ($# == 1) || ($# == 2) )); then
	usage
fi

DESTDIR=$1
MLIBDIR=${2:-"lib"}
GLCSDIR=$PWD

export CMAKE_INCLUDE_PATH="$GLCSDIR/elfhacks/src:$GLCSDIR/packetstream/src"
export CMAKE_LIBRARY_PATH="$GLCSDIR/elfhacks/build/src:$GLCSDIR/packetstream/build/src"

for mod in ${mods[@]}; do
	echo "Building $mod..."
	[ -d $mod/build ] || mkdir $mod/build
	cd $mod/build

	cmake .. \
		-DCMAKE_INSTALL_PREFIX:PATH="${DESTDIR}" \
		-DCMAKE_BUILD_TYPE:STRING="Release" \
		-DCMAKE_C_FLAGS_RELEASE:STRING="${CFLAGS}" > /dev/null \
		-DMLIBDIR="${MLIBDIR}" \
		|| exit 1
	make || exit 1
	cd ../..
done

