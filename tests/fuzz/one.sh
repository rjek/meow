#!/bin/sh
# one.sh FILE.c [nmcc flags]: compile one program with the host compiler and
# with nmcc, run both, and print what each wrote.  Exit 0 if they agree, 1
# if the MEOW program printed a different answer, 3 if it printed something
# that is no answer at all, and 2 if the comparison could not be made:
# the host build traps on undefined behaviour and that counts as the last,
# so that a reduction cannot wander into it.
here=$(cd "$(dirname "$0")" && pwd)
M=$here/../..
NMCC=${NMCC:-$M/../norcroft-ng/bin/nmcc}
RT="$M/rt/crt0.o $M/rt/exit.o $M/rt/mul.o $M/rt/div.o $M/rt/ll.o $M/rt/softfp.o $M/rt/msim.o"
src=$1; shift
t=${TMPDIR:-/tmp}/fuzzone.$$
trap 'rm -f $t.*' EXIT
cc -w -O1 -fwrapv -fno-strict-aliasing -ffp-contract=off -fsanitize=undefined,address \
	-fno-sanitize-recover=all -o $t.host -I$M/rt "$src" $M/rt/msim-host.c || exit 2
h=$(timeout 10 $t.host 2>/dev/null) || { echo "host failed"; exit 2; }
$NMCC "$@" -c -o $t.o -I$M/rt "$src" >/dev/null 2>&1 || { echo "nmcc failed"; exit 2; }
$M/ld/mld -f bin -d 0x08000000 -o $t.bin $RT $t.o || exit 2
m=$($M/simulator/msim -q -r $t.bin -m 1024 -c 400000000 2>/dev/null | head -c 60)
echo "host $h meow $m"
[ "$h" = "$m" ] && exit 0
case "$m" in *[!0-9a-f]*|"") exit 3;; esac
exit 1
