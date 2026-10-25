#!/bin/sh
# Checks that the Haiku platform code compiles against the real Haiku headers
# on another system (syntax only). Needs git and g++.
#   tools/check/haiku_syntax.sh
set -e
H=${HAIKU_HEADERS:-/tmp/haiku-headers}
if [ ! -d "$H/headers/os" ]; then
	git clone -q --depth 1 --filter=blob:none --sparse https://github.com/haiku/haiku "$H"
	(cd "$H" && git sparse-checkout set headers/os headers/config)
fi
INC=""
for d in $(find "$H/headers/os" -type d); do INC="$INC -idirafter $d"; done
PRE=$(mktemp)
printf '#include <config/types.h>\ntypedef __haiku_addr_t addr_t;\ntypedef __haiku_phys_addr_t phys_addr_t;\n' > "$PRE"
for f in src/platform/PlatformHaiku.cpp src/main.cpp; do
	g++ -std=c++17 -fsyntax-only -Wall -Wno-multichar -D__HAIKU__ $INC -idirafter "$H/headers" \
		-include "$PRE" -Isrc "$f"
done
rm -f "$PRE"
echo "Haiku syntax check passed"
