#!/bin/sh
# Linker regression tests: the sources in each ld/NAME/ are assembled to
# ELF and linked in name order.  The flat binary must match ld/NAME.hex
# (od -An -tx1 -v format) and the dump of the ELF executable must match
# ld/NAME.obj.  If ld/NAME.err exists instead, linking must fail and every
# line of it must appear in stderr.  ld/NAME.flags holds extra mld options.
cd "$(dirname "$0")" || exit 2
MAS=${MAS:-../as/mas}
MLD=${MLD:-../ld/mld}
MOBJDUMP=${MOBJDUMP:-../as/mobjdump}
tmp=${TMPDIR:-/tmp}/ldtest.$$
pass=0
fail=0
for dir in ld/*/; do
	name=${dir%/}
	objs=
	ok=1
	flags=
	if [ -f "$name.flags" ]; then
		flags=$(cat "$name.flags")
	fi
	for src in "$dir"*.s; do
		obj="$tmp.$(basename "$src" .s).o"
		if ! $MAS -f elf -o "$obj" "$src" 2>"$tmp.err"; then
			echo "FAIL $name: $src does not assemble"
			cat "$tmp.err"
			ok=0
		fi
		objs="$objs $obj"
	done
	if [ $ok = 0 ]; then
		fail=$((fail + 1))
		continue
	fi
	if [ -f "$name.err" ]; then
		if $MLD $flags -o "$tmp.bin" $objs 2>"$tmp.raw"; then
			echo "FAIL $name: linked but should not have"
			fail=$((fail + 1))
			continue
		fi
		sed "s|$tmp\.||g" "$tmp.raw" > "$tmp.err"
		while IFS= read -r line; do
			if ! grep -qF -- "$line" "$tmp.err"; then
				echo "FAIL $name: missing diagnostic: $line"
				cat "$tmp.err"
				ok=0
			fi
		done < "$name.err"
		if [ $ok = 1 ]; then pass=$((pass + 1)); else fail=$((fail + 1)); fi
		continue
	fi
	if ! $MLD $flags -o "$tmp.bin" $objs 2>"$tmp.err" ||
	   ! $MLD $flags -o "$tmp.elf" $objs 2>>"$tmp.err"; then
		echo "FAIL $name:"
		cat "$tmp.err"
		fail=$((fail + 1))
		continue
	fi
	od -An -tx1 -v "$tmp.bin" > "$tmp.hex"
	$MOBJDUMP -d "$tmp.elf" | sed "s|$tmp.elf|EXE|" > "$tmp.dump"
	if [ ! -f "$name.hex" ]; then
		echo "NEW  $name: writing $name.hex"
		cp "$tmp.hex" "$name.hex"
	fi
	if [ ! -f "$name.obj" ]; then
		echo "NEW  $name: writing $name.obj"
		cp "$tmp.dump" "$name.obj"
	fi
	if ! cmp -s "$tmp.hex" "$name.hex"; then
		echo "FAIL $name: flat output differs"
		diff "$name.hex" "$tmp.hex" | head -20
		ok=0
	fi
	if ! cmp -s "$tmp.dump" "$name.obj"; then
		echo "FAIL $name: executable dump differs"
		diff "$name.obj" "$tmp.dump" | head -20
		ok=0
	fi
	if [ $ok = 1 ]; then pass=$((pass + 1)); else fail=$((fail + 1)); fi
done
rm -f "$tmp".*
echo "linker tests: $pass passed, $fail failed"
[ $fail = 0 ]
