#!/bin/sh
# Benchmarks: each NAME.c is compiled with nmcc -c ($NMCCFLAGS added),
# linked with the runtime, run under msim and its output checked against
# the host compiler.  Prints instructions executed and code bytes for each,
# with the change since baseline.txt.  -b rewrites the baseline.
cd "$(dirname "$0")" || exit 2
NMCC=${NMCC:-../../norcroft-ng/bin/nmcc}
MLD=${MLD:-../ld/mld}
MOBJDUMP=${MOBJDUMP:-../as/mobjdump}
MSIM=${MSIM:-../simulator/msim}
RT="../rt/crt0.o ../rt/exit.o ../rt/mul.o ../rt/div.o ../rt/ll.o ../rt/softfp.o ../rt/msim.o"
tmp=${TMPDIR:-/tmp}/bench.$$
results=$tmp.results
write_baseline=0
[ "$1" = "-b" ] && write_baseline=1
if [ ! -x "$NMCC" ]; then
	echo "no compiler at $NMCC"
	exit 2
fi
: > "$results"
fail=0
for src in *.c; do
	name=${src%.c}
	if ! $NMCC -c $NMCCFLAGS -o "$tmp.o" -I../rt "$src" > "$tmp.err" 2>&1; then
		echo "FAIL $name: does not compile"; cat "$tmp.err"; fail=1; continue
	fi
	bytes=$($MOBJDUMP "$tmp.o" | awk '$2 == ".text" { for (i = 1; i <= NF; i++) if ($i == "size") print $(i + 1) }')
	bytes=$(printf '%d' "$bytes")
	if ! $MLD -f bin -d 0x08000000 -o "$tmp.bin" $RT "$tmp.o" 2>"$tmp.err"; then
		echo "FAIL $name: does not link"; cat "$tmp.err"; fail=1; continue
	fi
	$MSIM -q -s -r "$tmp.bin" -c 500000000 > "$tmp.out" 2>"$tmp.err"
	instr=$(sed -n 's/^msim: \([0-9]*\) instructions$/\1/p' "$tmp.err")
	if [ -z "$instr" ]; then
		echo "FAIL $name: did not finish"; cat "$tmp.err"; fail=1; continue
	fi
	if [ ! -f "$name.out" ]; then
		if cc -w -o "$tmp.host" -I../rt "$src" ../rt/msim-host.c 2>"$tmp.err"; then
			"$tmp.host" > "$name.out"; rm -f "$tmp.host"
		else
			echo "FAIL $name: no host answer"; cat "$tmp.err"; fail=1; continue
		fi
	fi
	if ! cmp -s "$tmp.out" "$name.out"; then
		echo "FAIL $name: wrong answer"; diff "$name.out" "$tmp.out" | head -5; fail=1; continue
	fi
	echo "$name $instr $bytes" >> "$results"
done
awk -v write="$write_baseline" '
	FILENAME == "baseline.txt" { bi[$1] = $2; bb[$1] = $3; next }
	{ n++; name[n] = $1; instr[n] = $2; bytes[n] = $3; ti += $2; tb += $3 }
	function delta(new, old) { return old == "" || old == 0 ? "" : sprintf("%+.1f%%", (new - old) * 100.0 / old) }
	END {
		printf "%-10s %13s %8s %8s %8s\n", "benchmark", "instructions", "change", "bytes", "change"
		for (i = 1; i <= n; i++) {
			printf "%-10s %13d %8s %8d %8s\n", name[i], instr[i], delta(instr[i], bi[name[i]]), bytes[i], delta(bytes[i], bb[name[i]])
			bti += bi[name[i]]; btb += bb[name[i]]
		}
		printf "%-10s %13d %8s %8d %8s\n", "total", ti, delta(ti, bti), tb, delta(tb, btb)
	}' baseline.txt "$results" 2>/dev/null || awk '{ print }' "$results"
if [ $write_baseline = 1 ] && [ $fail = 0 ]; then
	cp "$results" baseline.txt
	echo "baseline.txt written"
fi
rm -f "$tmp".*
exit $fail
