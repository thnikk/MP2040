#!/usr/bin/env bash
#
# Build (and optionally run) the MP2040 display simulator.
#
#   tools/display_sim/build.sh                 # build for the default board
#   tools/display_sim/build.sh -b Springboard  # another board config
#   tools/display_sim/build.sh -c              # clean first
#   tools/display_sim/build.sh -r -- --screen menu   # build then run
#
# The generated nanopb sources are reused from build/proto when present;
# otherwise they are regenerated with the vendored nanopb generator.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
BUILD_DIR="$HERE/build"
PROTO_DIR="$ROOT/build/proto"
BOARD="Fightboard"
CLEAN=0
RUN=0

while getopts "b:crh" opt; do
    case "$opt" in
        b) BOARD="$OPTARG" ;;
        c) CLEAN=1 ;;
        r) RUN=1 ;;
        h) sed -n '2,12p' "$0"; exit 0 ;;
        *) echo "usage: build.sh [-b board] [-c] [-r] [-- sim-args]" >&2; exit 2 ;;
    esac
done
shift $((OPTIND - 1))
# Everything after -- is passed to the simulator when -r is used.

if [ ! -d "$ROOT/configs/$BOARD" ]; then
    echo "error: no configs/$BOARD" >&2
    exit 2
fi

generate_proto() {
    echo "generating protobuf into $PROTO_DIR"
    mkdir -p "$PROTO_DIR" "$ROOT/build/nanopb_pb2_temp"
    local GEN="$ROOT/lib/nanopb/generator/nanopb_generator.py"
    local PY="$ROOT/build/venv/bin/python"
    [ -x "$PY" ] || PY="python3"
    for proto in enums config; do
        NANOPB_PB2_TEMP_DIR="$ROOT/build/nanopb_pb2_temp" "$PY" "$GEN" -q \
            -D "$PROTO_DIR" \
            -I "$ROOT/proto" \
            -I "$ROOT/lib/nanopb/generator/proto" \
            "$ROOT/proto/$proto.proto"
    done
}

if [ ! -f "$PROTO_DIR/config.pb.h" ]; then
    generate_proto
fi

if [ "$CLEAN" = 1 ]; then
    rm -rf "$BUILD_DIR"
fi

cmake -S "$HERE" -B "$BUILD_DIR" -G Ninja -DSIM_BOARD="$BOARD"
cmake --build "$BUILD_DIR"

# Record the baked board name so the screenshot scripts pick the matching
# configs/<Board>/board.svg.
echo "$BOARD" > "$BUILD_DIR/board"

echo
echo "built: $BUILD_DIR/mp2040-display-sim"

if [ "$RUN" = 1 ]; then
    exec "$BUILD_DIR/mp2040-display-sim" "$@"
fi
