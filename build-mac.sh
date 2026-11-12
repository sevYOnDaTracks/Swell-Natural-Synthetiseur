#!/bin/bash
# Build Swell - Natural Synthetiser on Mac (Standalone + VST3 + AU, universal arm64/x86_64).
# Needs: Xcode command line tools and CMake (brew install cmake).
set -e
cd "$(dirname "$0")"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSWELL_COPY_AFTER_BUILD=ON
cmake --build build --config Release --parallel
echo ""
echo "OK ! Plugins installes :"
echo "  VST3 : ~/Library/Audio/Plug-Ins/VST3/Swell - Natural Synthetiser.vst3"
echo "  AU   : ~/Library/Audio/Plug-Ins/Components/Swell - Natural Synthetiser.component"
echo "  App  : build/SwellNaturalSynth_artefacts/Release/Standalone/"
