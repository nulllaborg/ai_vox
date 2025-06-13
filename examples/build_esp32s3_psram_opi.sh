#!/bin/bash

arduino-cli compile -b esp32:esp32:esp32s3 $1 -j 0 -v --warnings all --board-options "PartitionScheme=huge_app" --board-options "PSRAM=opi" --build-property "build.extra_flags=-Wall -Werror -DWIFI_SSID=\"emakefun\" -DWIFI_PASSWORD=\"501416wf\" -DCLOGGER_SEVERITY=0"