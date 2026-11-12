#!/bin/bash
# Rebuild the separate "Swell Library" sound bank from the PSR-S900 Kontakt folder.
# Usage: ./build-library.sh "<PSR S900 folder>" "<output Swell Library folder>"
set -e
cd "$(dirname "$0")"
PSR="${1:-/Volumes/SSD/Banque Kontakt/Yamaha PSR S900}"
OUT="${2:-/Volumes/SSD/Banque Kontakt/Swell Library}"
python3 -I tools/nki_to_bank.py "$PSR" tools/psr_bank_index.json
cmake -S . -B build-tool -DCMAKE_BUILD_TYPE=Release -DSWELL_BUILD_PLUGIN=OFF
cmake --build build-tool --target SwellBankBuilder --parallel
./build-tool/SwellBankBuilder_artefacts/Release/SwellBankBuilder tools/psr_bank_index.json "$PSR/Samples" "$OUT"
