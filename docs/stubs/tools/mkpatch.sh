#!/bin/bash
# usage: mkpatch.sh NNNN slug "subject" descfile notesfile   (env: B=series-applied source repo (working tree vs its HEAD), R=wine-sg repo/worktree to write into)
set -e
N=$1; SLUG=$2; SUBJ=$3; DESC=$4; NOTES=$5; B=${B:?set B}; R=${R:?set R}
F=$R/patches/sg/$N-$SLUG.patch
{ echo "From: Stained Glass OS <ke7oxh@gmail.com>"; echo "Subject: [PATCH] $SUBJ"; echo; cat $DESC; echo "---"; (cd $B && git add -N . >/dev/null 2>&1; git diff HEAD | grep -v '^diff --git\|^index '); } > $F
cp $NOTES $R/patches/notes/$N.txt
grep -q "sg/$N-$SLUG.patch" $R/patches/series || echo "sg/$N-$SLUG.patch" >> $R/patches/series
(cd $B && git add -A >/dev/null 2>&1 && git -c user.name=d7 -c user.email=d7@x commit -q -m "$N $SLUG")
echo "wrote $F ($(wc -l < $F) lines)"
