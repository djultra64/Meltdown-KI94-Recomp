#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
project_dir="$project_root/work/ghidra"
script_dir="$project_root/ghidra_scripts"
log_dir="$project_dir/logs"

mkdir -p "$log_dir"

"$project_root/tools/ghidra-headless-flatpak.sh" \
  "$project_dir" ki15d \
  -import "$project_root/work/mame/dumps/mainram-10s.bin" \
  -overwrite \
  -processor MIPS:LE:64:64-32addr \
  -cspec n32 \
  -loader BinaryLoader \
  -loader-baseAddr 0x88000000 \
  -preScript SeedKi15d.java \
  -scriptPath "$script_dir" \
  -analysisTimeoutPerFile 900 \
  -max-cpu 4 \
  -log "$log_dir/mainram.log" \
  -scriptlog "$log_dir/mainram-script.log"

"$project_root/tools/ghidra-headless-flatpak.sh" \
  "$project_dir" ki15d \
  -import "$project_root/work/mame/dumps/bootrom.bin" \
  -overwrite \
  -processor MIPS:LE:64:64-32addr \
  -cspec n32 \
  -loader BinaryLoader \
  -loader-baseAddr 0xbfc00000 \
  -preScript SeedKi15d.java \
  -scriptPath "$script_dir" \
  -analysisTimeoutPerFile 300 \
  -max-cpu 4 \
  -log "$log_dir/bootrom.log" \
  -scriptlog "$log_dir/bootrom-script.log"
