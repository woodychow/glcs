#!/bin/sh
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

if [ ! -d $DESTDIR ]; then
	echo "$DESTDIR does not exist"
	exit 1
fi

MLIBDIR="lib"
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
		-DCMAKE_C_FLAGS_RELEASE_RELEASE:STRING="${CFLAGS}" > /dev/null \
		-DMLIBDIR="${MLIBDIR}" \
		|| return 1
	make || return 1
	cd ../..
done

