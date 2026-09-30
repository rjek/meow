#!/bin/sh
# run.sh FIRST LAST [nmcc flags]: differential testing of nmcc.  For each
# seed gen.py writes a random C program that prints one checksum; the host
# compiler's build of it is the oracle for the build nmcc makes, run under
# msim.  A seed that fails leaves its program in fuzz/failed/SEED.c, for
# reduce.py to shrink.  Not part of make check: it needs a host compiler
# and is slow.  Set NMCC to test another compiler, JOBS to run in parallel.
cd "$(dirname "$0")" || exit 2
M=../..
NMCC=${NMCC:-$M/../norcroft-ng/bin/nmcc}
RT="$M/rt/crt0.o $M/rt/exit.o $M/rt/mul.o $M/rt/div.o $M/rt/ll.o $M/rt/softfp.o $M/rt/msim.o"
[ $# -ge 2 ] || { echo "usage: run.sh FIRST LAST [nmcc flags]" >&2; exit 2; }
first=$1; last=$2; shift 2
if [ "${JOBS:-1}" -gt 1 ]; then
	jobs=$JOBS; JOBS=1; export JOBS NMCC
	seq $first $last | xargs -P $jobs -I{} ./run.sh {} {} "$@" | grep -v '^seeds ' | sort -V
	exit 0
fi
t=${TMPDIR:-/tmp}/fuzz.$$
trap 'rm -f $t.*' EXIT
fails=0
fail() {
	echo "seed $seed: $1"
	mkdir -p failed; cp $t.c failed/$seed.c
	fails=$((fails + 1))
}
for seed in $(seq $first $last); do
	./gen.py $seed > $t.c
	if ! cc -w -O1 -fwrapv -fno-strict-aliasing -ffp-contract=off -o $t.host -I$M/rt $t.c $M/rt/msim-host.c 2>$t.err; then
		echo "seed $seed: host compile failed"; head -3 $t.err; continue
	fi
	timeout 10 $t.host > $t.hout 2>&1 || { echo "seed $seed: host run failed"; continue; }
	if ! $NMCC "$@" -c -o $t.o -I$M/rt $t.c > $t.err 2>&1; then
		fail "nmcc failed: $(grep -v Warning $t.err | sed -n 2p)"; continue
	fi
	$M/ld/mld -f bin -d 0x08000000 -o $t.bin $RT $t.o 2>$t.err || { fail "link failed: $(head -1 $t.err)"; continue; }
	$M/simulator/msim -q -r $t.bin -m 1024 -c 400000000 > $t.mout 2>$t.err
	if ! cmp -s $t.hout $t.mout; then
		fail "MISMATCH host $(cat $t.hout) meow $(head -c 40 $t.mout) $(head -c 60 $t.err)"
	elif [ -s $t.err ]; then
		fail "right answer, but $(head -c 70 $t.err)"
	fi
done
echo "seeds $first..$last: $fails failures"
[ $fails = 0 ]
