#!/bin/bash
set -euo pipefail

# toolchain
CC="gcc"

# header search paths (so plugin_common.h can find consumer_producer.h)
INC="-I. -Iplugins -Iplugins/sync"

CFLAGS="-std=c11 -Wall -Wextra -Werror -O2 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE"

# Colors for echo (green, blue, red, purple)
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

mkdir -p output

log_build() {
    echo -e "${GREEN}[BUILD]${BLUE} $1"
}

log_success() {
    echo -e "${PURPLE}[OK]${NC} $1"
}
# build main (needs -ldl for dlopen/dlsym)
log_build "analyzer -> output/analyzer"
$CC $CFLAGS $INC -o output/analyzer main.c -ldl
log_success "Built output/analyzer"

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
  echo -e "${RED}[ERROR]${NC} No plugin sources found in plugins/*.c"
  exit 1
fi

log_build "Building plugins into output/"
for src in "${plugins[@]}"; do
  name="${src##*/}"; name="${name%.c}"
  out="output/$name.so"
  echo -e "${BLUE}  -> $name.so${NC}"
  $CC -fPIC -shared $CFLAGS $INC -o "$out" \
      "plugins/${name}.c" \
      plugins/plugin_common.c \
      plugins/sync/monitor.c \
      plugins/sync/consumer_producer.c \
      -ldl -lpthread
  log_success "Built $out"
done

log_success "All plugins built."
echo "Run example:"
echo "  echo -e 'hello\n<END>' | ./output/analyzer 20 uppercaser rotator logger"
