#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CC=${CC:-clang}
FLEX=${FLEX:-flex}
BISON=${BISON:-bison}
PKG_CONFIG=${PKG_CONFIG:-pkg-config}
for tool in "$CC" "$FLEX" "$BISON" "$PKG_CONFIG"; do
    command -v "$tool" >/dev/null 2>&1 || { echo "Required tool not found: $tool" >&2; exit 1; }
done
version_at_least() {
    awk -v actual="$1" -v minimum="$2" 'BEGIN {
        split(actual,a,"."); split(minimum,b,".");
        for(i=1;i<=3;i++) { if(a[i]+0>b[i]+0) exit 0; if(a[i]+0<b[i]+0) exit 1; }
    }'
}
flex_version=$("$FLEX" --version | awk 'NR==1 {print $2}')
bison_version=$("$BISON" --version | awk 'NR==1 {print $NF}')
"$PKG_CONFIG" --exists json-c || { echo 'json-c development files are required' >&2; exit 1; }
json_version=$("$PKG_CONFIG" --modversion json-c)
pkg_version=$("$PKG_CONFIG" --version)
version_at_least "$pkg_version" 0.29 || { echo 'pkg-config 0.29+ or compatible pkgconf required' >&2; exit 1; }
version_at_least "$flex_version" 2.6.4 || { echo 'Flex 2.6.4+ required' >&2; exit 1; }
version_at_least "$bison_version" 3.8.2 || { echo 'GNU Bison 3.8.2+ required' >&2; exit 1; }
version_at_least "$json_version" 0.16 || { echo 'json-c 0.16+ required' >&2; exit 1; }
mkdir -p "$root/build/tools"
cat > "$root/build/tools/c23.c" <<'EOF'
#include <stddef.h>
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#error Strict ISO C23 is required
#endif
#if defined(__clang__)
#if __clang_major__ < 18
#error Clang 18+ is required
#endif
#elif defined(__GNUC__)
#if __GNUC__ < 14
#error GCC 14+ is required
#endif
#endif
static_assert(alignof(max_align_t) >= alignof(long double));
int main(void) { return 0; }
EOF
"$CC" -std=c23 -Wall -Wextra -Wpedantic "$root/build/tools/c23.c" -o "$root/build/tools/c23"
"$root/build/tools/c23"
"$CC" --version | sed -n '1p'
printf 'Flex %s\nGNU Bison %s\njson-c %s\npkg-config %s\n' "$flex_version" "$bison_version" "$json_version" "$("$PKG_CONFIG" --version)"
