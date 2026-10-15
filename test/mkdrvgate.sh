#!/bin/sh
# mkdrvgate.sh NAME "description" "mutants" : create test/ntoskrnl-NAME-gate.sh and -probe.c from the reinit gate
cd "$(dirname "$0")" || exit 1
sed -e "s/ntoskrnl-reinit/ntoskrnl-$1/g" -e "s/sg-ntoskrnl-reinit/sg-ntoskrnl-$1/g" \
    -e "s|^# CORE ntoskrnl: .*|# CORE ntoskrnl: $2|" \
    -e "s|^# Mutant: .*|# Mutants: $3 (ntoskrnl.exe/ntoskrnl.c).|" \
    -e 's/-lntoskrnl -lhal/-lntoskrnl -lhal/g' ntoskrnl-reinit-gate.sh > ntoskrnl-$1-gate.sh
chmod +x ntoskrnl-$1-gate.sh
sed "s/reinit/$1/" ntoskrnl-reinit-probe.c > ntoskrnl-$1-probe.c
