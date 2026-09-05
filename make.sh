#!/bin/bash

BUILD_DIR=build
BUILD_DIR_PICO=$BUILD_DIR/pico
BUILD_DIR_PICO2=$BUILD_DIR/pico2
BUILD_DIR_CORE2350B=$BUILD_DIR/Core2350B

PICO_SDK_DIR=pico-sdk

# Set picotool directory if one hasn't already been set
if [[ -z "${PICOTOOL_FETCH_FROM_GIT_PATH}" ]];then
	export PICOTOOL_FETCH_FROM_GIT_PATH=$BUILD_DIR/.picotool
fi

main () {
        if [ ! -f "$PICO_SDK_DIR/.git" ]; then
                git submodule sync --recursive
                git submodule update --init --recursive
        fi

        mkdir -p $BUILD_DIR_PICO $BUILD_DIR_PICO2 $BUILD_DIR_CORE2350B $PICOTOOL

        cmake -B $BUILD_DIR_PICO -DPICO_BOARD=pico
	cmake -B $BUILD_DIR_PICO2 -DPICO_BOARD=pico2
	cmake -B $BUILD_DIR_CORE2350B -DPICO_BOARD=waveshare_core2350b

        make -C $BUILD_DIR_PICO
	make -C $BUILD_DIR_PICO2
	make -C $BUILD_DIR_CORE2350B
}

main $@
