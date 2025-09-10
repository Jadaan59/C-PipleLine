#!/bin/bash
set -euo pipefail

# toolchain
CC="gcc"
CFLAGS="-Wall -Wextra -O2 -fPIC -std=c99"
CPPFLAGS="-I. -Iplugins -Iplugins/sync"
LDFLAGS="-ldl -lpthread"

# colors + tiny logger
BLUE='\033[0;34m'; GREEN='\033[0;32m'; RED='\033[0;31m'; NC='\033[0m'
log(){ printf "%b[BUILD]%b %s\n" "$BLUE" "$NC" "$*"; }
ok(){ printf "%b[SUCCESS]%b %s\n" "$GREEN" "$NC" "$*"; }

mkdir -p output

# build main
log "Building main -> output/analyzer"
$CC $CFLAGS $CPPFLAGS -o output/analyzer main.c $LDFLAGS
ok "Built output/analyzer"

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
  log "No plugin sources found"; exit 0
fi

log "Building plugins into output/"
for src in "${plugins[@]}"; do
  name="${src##*/}"; name="${name%.c}"
  out="output/$name.so"
  log "  -> $name.so"
  $CC $CFLAGS $CPPFLAGS -shared -o "$out" \
      "$src" \
      plugins/plugin_common.c \
      plugins/sync/monitor.c \
      plugins/sync/consumer_producer.c \
      $LDFLAGS
  ok "  Built $out"
done

ok "All plugins built."
echo "Run example:"
echo "  echo 'hello' | ./output/analyzer 20 uppercaser rotator logger"
