#! /bin/bash

# this script will copy all necessary files from OpenOCD repo into a dedicated
# folder in this repo

set -e

if [ $# -ne 1 ]; then
    echo "Usage: $0 OPENOCD_FOLDER"
    exit 1;
fi

mkdir -p openocd


OPENOCD_FOLDER=$1

desired_files=(
    "COPYING"
    "src/jtag/drivers/mpsse.c"
    "src/jtag/drivers/mpsse.h"
    "src/jtag/drivers/ftdi.c"
    "src/helper/binarybuffer.h"
    "src/helper/binarybuffer.c"
    "src/helper/list.h"
    "src/helper/types.h"
    "src/helper/time_support.h"
    "src/helper/time_support.c"
    "src/jtag/interface.h"
    "src/jtag/interface.c"
    "src/jtag/jtag.h"
)

for desired_file in "${desired_files[@]}"; do
    echo "Copying $desired_file"
    mkdir -p openocd/$(dirname $desired_file)
    cp ${OPENOCD_FOLDER}/${desired_file} openocd/$(dirname $desired_file)
done

