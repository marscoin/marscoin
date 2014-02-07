#!/bin/bash
# create multiresolution windows icon
ICON_SRC=../../src/qt/res/icons/marscoin.png
ICON_DST=../../src/qt/res/icons/marscoin.ico
convert ${ICON_SRC} -resize 16x16 marscoin-16.png
convert ${ICON_SRC} -resize 32x32 marscoin-32.png
convert ${ICON_SRC} -resize 48x48 marscoin-48.png
convert marscoin-16.png marscoin-32.png marscoin-48.png ${ICON_DST}

