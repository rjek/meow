#!/bin/sh
# Assembler regression tests: each as/NAME.s is assembled; the flat binary
# must match as/NAME.hex (od -An -tx1 -v format).  If as/NAME.err exists
# instead, assembly must fail and every line of it must appear in stderr.
cd "$(dirname "$0")" || exit 2
MAS=${MAS:-../as/mas}
MOBJDUMP=${MOBJDUMP:-../as/mobjdump}
tmp=${TMPDIR:-/tmp}/mastest.$$
pass=0
fail=0
for src in as/*.s; do
	name=${src%.s}
	case "$name" in *.inc) continue ;; esac
	if [ -f "$name.err" ]; then
		if $MAS -o "$tmp.bin" "$src" 2>"$tmp.err"; then
			echo "FAIL $src: assembled but should not have"
			fail=$((fail + 1))
			continue
		fi
		ok=1
		while IFS= read -r line; do
			if ! grep -qF -- "$line" "$tmp.err"; then
				echo "FAIL $src: missing diagnostic: $line"
				cat "$tmp.err"
				ok=0
			fi
		done < "$name.err"
		if [ $ok = 1 ]; then pass=$((pass + 1)); else fail=$((fail + 1)); fi
		continue
	fi
	if [ ! -f "$name.obj" ] && [ ! -f "$name.hex" ] &&
	   head -1 "$src" | grep -q '; mode: elf'; then
		echo "NEW  $src: writing $name.obj"
		$MAS -f elf -o "$tmp.o" "$src" && $MOBJDUMP -d "$tmp.o" |
			sed "s|$tmp.o|OBJ|" > "$name.obj"
	fi
	if [ -f "$name.obj" ]; then
		if $MAS -f elf -o "$tmp.o" "$src" 2>"$tmp.err" &&
		   $MOBJDUMP -d "$tmp.o" | sed "s|$tmp.o|OBJ|" > "$tmp.dump" &&
		   cmp -s "$tmp.dump" "$name.obj"; then
			pass=$((pass + 1))
		else
			echo "FAIL $src: object dump differs"
			cat "$tmp.err"
			diff "$name.obj" "$tmp.dump" | head -20
			fail=$((fail + 1))
		fi
		continue
	fi
	if ! $MAS -o "$tmp.bin" "$src" 2>"$tmp.err"; then
		echo "FAIL $src:"
		cat "$tmp.err"
		fail=$((fail + 1))
		continue
	fi
	od -An -tx1 -v "$tmp.bin" > "$tmp.hex"
	if [ -f "$name.hex" ]; then
		if cmp -s "$tmp.hex" "$name.hex"; then
			pass=$((pass + 1))
		else
			echo "FAIL $src: output differs"
			diff "$name.hex" "$tmp.hex" | head -20
			fail=$((fail + 1))
		fi
	else
		echo "NEW  $src: writing $name.hex"
		cp "$tmp.hex" "$name.hex"
		pass=$((pass + 1))
	fi
done
rm -f "$tmp.bin" "$tmp.err" "$tmp.hex" "$tmp.o" "$tmp.dump"
echo "assembler tests: $pass passed, $fail failed"
[ $fail = 0 ]
