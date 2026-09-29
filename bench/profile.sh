#!/bin/sh
# Profile one benchmark: profile.sh NAME [nmcc flags].  Prints the share
# of instructions per function and the hottest instructions with their
# disassembly, so the cost of a code pattern can be seen directly.
cd "$(dirname "$0")" || exit 2
NMCC=${NMCC:-../../norcroft-ng/bin/nmcc}
MLD=${MLD:-../ld/mld}
MOBJDUMP=${MOBJDUMP:-../as/mobjdump}
MSIM=${MSIM:-../simulator/msim}
RT="../rt/crt0.o ../rt/mul.o ../rt/div.o ../rt/ll.o ../rt/softfp.o ../rt/msim.o"
name=$1; shift
tmp=${TMPDIR:-/tmp}/profile.$$
$NMCC -c "$@" -o "$tmp.o" -I../rt "$name.c" || exit 1
$MLD -f elf -d 0x08000000 -o "$tmp.elf" $RT "$tmp.o" || exit 1
$MLD -f bin -d 0x08000000 -o "$tmp.bin" $RT "$tmp.o" || exit 1
$MSIM -q -s -r "$tmp.bin" -c 500000000 -P "$tmp.prof" > /dev/null
$MOBJDUMP -d "$tmp.elf" > "$tmp.dis"
awk '
	function hex(s,   i, v) { v = 0; for (i = 1; i <= length(s); i++) v = v * 16 + index("0123456789abcdef", substr(s, i, 1)) - 1; return v }
	FILENAME == ARGV[1] {
		# symbol table lines: index value bind type section name
		if ($3 == "GLOBAL" || $3 == "LOCAL") if ($4 == "FUNC" || $4 == "NONE") { nsym++; sa[nsym] = hex($2); sn[nsym] = $NF }
		# disassembly lines: address halfword mnemonic...
		if ($1 ~ /^[0-9a-f]{8}$/ && $2 ~ /^[0-9a-f]{4}$/) { dis[$1] = substr($0, index($0, $3)) }
		next
	}
	{ count[$1] = $2; total += $2 }
	function fn_of(addr,   best, i) {
		best = "?"
		for (i = 1; i <= nsym; i++) if (sa[i] <= addr && (best == "?" || sa[i] >= sa[bi])) { best = sn[i]; bi = i }
		return best
	}
	END {
		for (a in count) { f = fn_of(hex(a)); per[f] += count[a] }
		printf "%s: %d instructions\n\n", "'"$name"'", total
		printf "%-16s %13s %6s\n", "function", "instructions", "share"
		for (f in per) printf "%-16s %13d %5.1f%%\n", f, per[f], per[f] * 100.0 / total | "sort -k2 -n -r | head -12"
		close("sort -k2 -n -r | head -12")
		printf "\n%-8s %11s %6s  %s\n", "address", "count", "share", "instruction"
		for (a in count) printf "%s %11d %5.1f%%  %s\n", a, count[a], count[a] * 100.0 / total, dis[a] | "sort -k2 -n -r | head -40"
	}' "$tmp.dis" "$tmp.prof"
rm -f "$tmp".*
