#!/usr/bin/env bash

# Generate .zip set of icons for Steam

# Make icons with transparent backgrounds and all sizes
for s in 16 24 32 48 64 128 256 512 1024; do
  convert -resize ${s}x$s -antialias \
          -background transparent \
          ../../icon.svg icon$s.png
done

# 16px tga file for library
convert icon16.png icon16.tga

# zip for Linux
zip godot-icons.zip icon*.png

rm -f icon*.png
