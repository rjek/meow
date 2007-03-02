#!/bin/sh

for i in *.svg; do
  BASE=`basename $i .svg`
  OUTPUT=$BASE.pdf
  echo -n "$i -> $BASE.eps "
  inkscape -z -f $i -E $BASE.eps
  echo -n "-> $OUTPUT "
  epstopdf $BASE.eps
  echo "done"
done;
