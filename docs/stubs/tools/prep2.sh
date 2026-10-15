#!/usr/bin/env bash
# usage: prep.sh REPO DEST   -- tarball + REPO's series + theme generation
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1
HERE=$1; DEST=$2
TB=/var/tmp/d7/dl/wine-10.0.tar.xz
mkdir -p "$DEST.tmp"; tar -C "$DEST.tmp" -xf $TB
T=$DEST.tmp/wine-10.0
while read -r p; do
  [[ -z "$p" || "$p" == \#* ]] && continue
  if patch -d "$T" -p1 -s --dry-run -i "$HERE/patches/$p" >/dev/null 2>&1; then patch -d "$T" -p1 -s -i "$HERE/patches/$p"; else echo "SKIP $p" >> "$DEST.skipped"; fi
done < "$HERE/patches/series"
echo series-applied
python3 "$HERE/theme/icons.py" "$T" >/dev/null
dir=$T/dlls/light.msstyles
for svg in $(grep -v '^#' "$HERE/theme/images.list"); do
  base=$T/${svg%.svg}
  for ext in bmp cur ico; do [[ -f $base.$ext ]] && CONVERT=convert ICOTOOL=icotool RSVG=rsvg-convert perl $T/tools/buildimage $T/$svg $base.$ext >/dev/null 2>&1 || true; done
done
for svg in $dir/*.svg; do base=${svg%.svg}
  for ext in bmp cur ico; do [[ -f $base.$ext ]] && CONVERT=convert ICOTOOL=icotool RSVG=rsvg-convert perl $T/tools/buildimage $svg $base.$ext >/dev/null 2>&1 || true; done
done
for s in dark rounded eras; do grep -q "$s.rc" $dir/light.rc && python3 "$HERE/theme/$s.py" $dir >/dev/null; done
mv $T "$DEST"; rmdir "$DEST.tmp"; echo done
