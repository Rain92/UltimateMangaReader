#!/bin/bash
# Builds the test harness and runs the tests, see tests/README.md.
#   tests/run.sh                  all tests
#   tests/run.sh NAME...          some of them: a source (MangaDex), the update of its list (MangaHere-list),
#                                 the script of a source in sources/ (MangaTown-lua), jobs, storage, scripts,
#                                 index (sources/index.yaml lists the sources, they load)
#   tests/run.sh record [NAME...] records the responses of the sites again and the golden output with them
# QMAKE (default qmake6) and BUILD_DIR (default ../build-UltimateMangaReader-tests next to the repository)
# can be set in the environment.
set -u

TESTS=$(cd "$(dirname "$0")" && pwd)
SOURCES_DIR=$(dirname "$TESTS")/sources
BUILD=${BUILD_DIR:-$(dirname "$(dirname "$TESTS")")/build-UltimateMangaReader-tests}
QMAKE=${QMAKE:-qmake6}
PORT=${TEST_SERVER_PORT:-8780}

SOURCES="Comick MangaDex MangaHere Mangakakalot MangaPlus MangaTown"
# the list updates of the other sources take too many requests (MangaDex ~700 pages, MangaTown ~800 pages,
# Comick ~200 MB of sitemaps)
LISTS="MangaHere-list Mangakakalot-list MangaPlus-list"
# the scripts of sources in sources/, against the responses and the golden output of the C++ sources
SCRIPTS="MangaTown-lua"

record=0
if [ "${1:-}" = record ]; then
    record=1
    shift
fi
names=("$@")
if [ ${#names[@]} -eq 0 ]; then
    names=($SOURCES $LISTS)
    [ $record = 0 ] && names+=($SCRIPTS jobs storage scripts index)
fi

mkdir -p "$BUILD" && cd "$BUILD" || exit 2
if [ ! -f Makefile ]; then
    "$QMAKE" "$TESTS/harness/harness.pro" CONFIG+=debug > /dev/null || exit 2
fi
if ! make -j"$(nproc)" > make.log 2>&1; then
    grep -E "error|Error" make.log | head -30
    exit 2
fi

# the harness's temporary caches (images, manga lists) can be large
export TMPDIR=$BUILD/tmp
mkdir -p "$TMPDIR" output "$TESTS/golden" "$TESTS/fixtures"

server=
cleanup() { [ -n "$server" ] && kill "$server" 2> /dev/null; }
trap cleanup EXIT
start_server() {
    [ -n "$server" ] && return
    python3 -I "$TESTS/harness/testserver.py" "$PORT" &
    server=$!
    sleep 0.5
}

failed=0
report() {
    printf '%-20s %s\n' "$1" "$2"
    [ "$2" = ok ] || failed=1
}

# a test of a source: replayed and compared with the golden output, or recorded
source_test() {
    local name=$1 base=${1%-lua}
    shift
    local fixtures=$TESTS/fixtures/$base golden=$TESTS/golden/$base.txt
    if [ $record = 1 ]; then
        ./harness --record "$fixtures" "$@" > "$golden" 2> "output/$name.log"
        local status=$?
        [ $status = 0 ] && report "$name" ok || report "$name" "recorded with errors (exit $status), see $golden"
        return
    fi

    if [ ! -f "$fixtures/index.json" ]; then
        report "$name" "no recorded responses: tests/run.sh record $name"
        return
    fi
    ./harness --replay "$fixtures" "$@" > "output/$name.txt" 2> "output/$name.log"
    local status=$?
    if [ ! -f "$golden" ]; then
        report "$name" "no golden output, see $BUILD/output/$name.txt"
    elif ! diff -u "$golden" "output/$name.txt" > "output/$name.diff"; then
        report "$name" "DIFFERS, see $BUILD/output/$name.diff"
    elif [ $status != 0 ]; then
        report "$name" "FAILED (exit $status), see $BUILD/output/$name.log"
    else
        report "$name" ok
    fi
}

for name in "${names[@]}"; do
    case $name in
        jobs | storage | scripts)
            [ $record = 1 ] && continue
            start_server
            ./harness --sources "$SOURCES_DIR" "$name" "$PORT" > "output/$name.txt" 2> "output/$name.log"
            status=$?
            [ $status = 0 ] && report "$name" ok || report "$name" "FAILED, see $BUILD/output/$name.txt"
            ;;
        index)
            [ $record = 1 ] && continue
            ./harness index "$SOURCES_DIR" > "output/$name.txt" 2> "output/$name.log"
            [ $? = 0 ] && report "$name" ok || report "$name" "FAILED, see $BUILD/output/$name.txt"
            ;;
        *-list)
            source_test "$name" list "${name%-list}"
            ;;
        *-lua)
            [ $record = 1 ] && continue
            source_test "$name" --sources "$SOURCES_DIR" capture "${name%-lua}" \
                "$TESTS/picks/${name%-lua}.txt"
            ;;
        *)
            source_test "$name" capture "$name" "$TESTS/picks/$name.txt"
            ;;
    esac
done

exit $failed
