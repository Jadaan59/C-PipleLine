#!/bin/bash
set -euo pipefail

# toolchain
CC="gcc"

# header search paths (so plugin_common.h can find consumer_producer.h, monitor.h)
INC="-I. -Iplugins -Iplugins/sync"

mkdir -p output

# build main (needs -ldl for dlopen/dlsym)
echo "[BUILD] analyzer -> output/analyzer"
$CC $INC -o output/analyzer main.c -ldl
echo "[OK] Built output/analyzer"

# build plugins: plugins/*.c excluding plugin_common.c and *_test.c
shopt -s nullglob
plugins=()
for src in plugins/*.c; do
  base=$(basename "$src")
  [[ "$base" == "plugin_common.c" ]] && continue
  [[ "$base" == *_test.c ]] && continue
  plugins+=("$src")
done

if ((${#plugins[@]} == 0)); then
  echo "[BUILD] No plugin sources found"
  exit 0
fi

echo "[BUILD] Building plugins into output/"
for src in "${plugins[@]}"; do
  name="${src##*/}"; name="${name%.c}"
  out="output/$name.so"
  echo "  -> $name.so"
  $CC -fPIC -shared $INC -o "$out" \
      "plugins/${name}.c" \
      plugins/plugin_common.c \
      plugins/sync/monitor.c \
      plugins/sync/consumer_producer.c \
      -ldl -lpthread
  echo "  [OK] Built $out"
done

echo "[OK] All plugins built."
echo "Run example:"
echo "  echo 'hello' | ./output/analyzer 20 uppercaser rotator logger"
