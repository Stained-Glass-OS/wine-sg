# A gate written for bash (#!/bin/bash) and started as "sh gate.sh" ran under
# dash, failed at its first bash-only expansion ("Bad substitution") and looked
# like a regression (sandboxdesk, elevcode and dispcfg, 2026-11-15): it runs
# again under bash. (Sourced on each gate's line 2, so $0 and $@ are the gate's.)
if [ -z "${BASH_VERSION:-}" ] && [ -f "$0" ] && head -n 1 "$0" 2>/dev/null | grep -q '^#!/bin/bash' && command -v bash >/dev/null; then
    exec bash "$0" "$@"
fi
# Sourced first by every gate that runs Wine. A prefix links
# C:\users\<user>\Desktop, Documents, Downloads, Music, Pictures, Videos and
# Templates to $HOME's folders, and winemenubuilder writes menu entries and
# desktop files under $HOME: a gate on the real HOME reads, writes and
# deletes the real user's files. So every gate gets a HOME of its own.
# The cache is the gate's too (Disk Cleanup and Storage Sense empty it); a
# gate that keeps downloads across runs names "$SG_REAL_HOME/.cache" itself.
# Python's user packages stay the real home's.
# Gates leave their scratch behind when they are killed, and the gate HOME
# below is never removed (each gate sets its own EXIT trap): 6000 of them,
# 161 GB, in /var/tmp by 2026-11-01. Every gate start takes away what is
# more than a day old -- a gate's HOME and a gate's mktemp directory
# (sg-<name>.XXXXXX) -- so the host never fills up with them.
find "${TMPDIR:-/var/tmp}" -maxdepth 1 -mindepth 1 -type d -mtime +0 \
    \( -name 'sg-gate-home.??????' -o -name 'sg-*.??????' \) ! -name 'sg-lint-tree-*' \
    -exec sh -c 'chmod -R u+w "$1" 2>/dev/null; rm -rf "$1"' _ {} \; 2>/dev/null
if [ -z "${SG_GATE_HOME:-}" ]; then
    SG_REAL_HOME=${HOME:-/nonexistent}
    export SG_REAL_HOME
    export PYTHONUSERBASE="${PYTHONUSERBASE:-$SG_REAL_HOME/.local}"
    SG_GATE_HOME=$(mktemp -d "${TMPDIR:-/var/tmp}/sg-gate-home.XXXXXX") || exit 1
    export SG_GATE_HOME HOME="$SG_GATE_HOME"
    export XDG_CONFIG_HOME="$HOME/.config" XDG_CACHE_HOME="$HOME/.cache" XDG_DATA_HOME="$HOME/.local/share" XDG_DESKTOP_DIR="$HOME/Desktop"
    mkdir -p "$HOME/Desktop" "$XDG_CONFIG_HOME" "$XDG_DATA_HOME"
    export WINEDLLOVERRIDES="winemenubuilder.exe=d${WINEDLLOVERRIDES:+;$WINEDLLOVERRIDES}"
fi
# A Windows program that listens tells the firewall (wine-sg 1500), and the firewall asks the
# person on their desktop. A gate's probes must never do that (David 2026-11-21: prompts popped
# up from an agent's gate sweep). Gates that test the notice set their own SG_FIREWALL_NOTIFY.
export SG_FIREWALL_NOTIFY="$SG_GATE_HOME/no-firewall-notify"
# Shut down / Restart from a Windows program (ExitWindowsEx, InitiateSystemShutdown, shutdown.exe) runs
# SG_POWERCTL, else sg-session's /usr/bin/sg-settingsctl, which asks logind -- as the person running the
# gate, that powers off the real PC (David's desktop went down twice, 2026-11-21/22, from shutdown-gate
# in an agent's sweep). A path that does not exist: nothing is run. Gates that test it set their own.
export SG_POWERCTL="$SG_GATE_HOME/no-power-helper"
# Nothing a gate runs may reach the person's own session: Wine's Wayland
# driver connects to $XDG_RUNTIME_DIR/wayland-0 even with WAYLAND_DISPLAY
# unset (2026-10-06: a printing run reached the host's desktop; 2026-11-16:
# headless wineboots in agents' ad-hoc runs reached the live sg-compositor
# and looped on "Failed to read display config"), and DISPLAY=:0 is the
# host's X server. A gate starts its own display and gets a runtime
# directory of its own (in its HOME, so it goes with it), and the Wayland
# display is a name that exists nowhere, so even a gate that points
# XDG_RUNTIME_DIR somewhere else reaches no compositor by default.
# SG_REAL_XDG_RUNTIME_DIR keeps the person's for the rare gate that must
# reach their user services (none does yet). Done even when SG_GATE_HOME was
# given, unless the runtime directory is already the gate's.
case "${XDG_RUNTIME_DIR:-}" in
"$SG_GATE_HOME"/*) ;;
*)  export SG_REAL_XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-}"
    XDG_RUNTIME_DIR="$SG_GATE_HOME/run"
    mkdir -p "$XDG_RUNTIME_DIR" && chmod 700 "$XDG_RUNTIME_DIR"
    export XDG_RUNTIME_DIR
    unset DISPLAY ;;
esac
export WAYLAND_DISPLAY=sg-gate-no-wayland
# Vulkan on a gate's own display (Xvfb) is lavapipe's: the host's GPU driver
# may have no instance there -- NVIDIA's needs its own X server -- and the
# loader took it first: every Vulkan gate skipped on a host with NVIDIA's
# driver (2026-11-15). A gate that wants the host's driver unsets both.
if [ -z "${VK_ICD_FILENAMES:-}${VK_DRIVER_FILES:-}" ]; then
    for _sg_lvp in /usr/share/vulkan/icd.d/lvp_icd*.json; do
        [ -f "$_sg_lvp" ] && export VK_ICD_FILENAMES="$_sg_lvp" VK_DRIVER_FILES="$_sg_lvp" && break
    done
fi
# sg_prefix_safe PREFIX: fail when any folder of PREFIX's user profiles
# resolves outside the prefix and the gate's HOME (an old prefix made on the
# real HOME). Gates that reuse a prefix call it before starting Wine.
sg_prefix_safe() {
    _bad=$(find "$1/drive_c/users" -maxdepth 6 -type l 2>/dev/null | while read -r _l; do
        _t=$(readlink -f "$_l")
        case "$_t" in "$SG_GATE_HOME"*|"$1"*) ;; *) echo "$_l -> $_t" ;; esac
    done)
    [ -z "$_bad" ] && return 0
    echo "refusing to run: $1 links outside the gate's home:" >&2
    echo "$_bad" >&2
    return 1
}
# sg_wineserver: the wineserver that goes with $WINE -- $WINESERVER, the one
# beside wine (installed), or server/wineserver (a build tree, which has none
# beside wine). A gate's "-k" on a path that does not exist kills nothing: its
# prefix was deleted and the prefix's winedevice.exe and services ran on for
# days (57 of them, 4 to 20 days old, on 2026-11-14). Gates call
# "$(sg_wineserver)" -k in their cleanup.
sg_wineserver() {
    if [ -n "${WINESERVER:-}" ] && [ -x "$WINESERVER" ]; then echo "$WINESERVER"; return; fi
    _sg_wd=$(dirname "${WINE:-/opt/wine-sg/bin/wine}")
    if [ -x "$_sg_wd/wineserver" ]; then echo "$_sg_wd/wineserver"; else echo "$_sg_wd/server/wineserver"; fi
}
# Wine's link to the compositor's control socket (wtsapi32's session state,
# magnification.dll's Magnifier) goes to SG_LOCK_CONTROL, else the session's
# own socket under /run/stained-glass-seat -- the person's live compositor: a
# gate's Magnifier would magnify their screen. A gate reaches only a compositor
# of its own (it sets SG_LOCK_CONTROL after this, to its socket).
case "${SG_LOCK_CONTROL:-}" in
"$SG_GATE_HOME"/*|/var/tmp/*|/tmp/*) ;;
*) export SG_LOCK_CONTROL="$SG_GATE_HOME/no-compositor-control.sock" ;;
esac
# sg_gate_display: the display a gate's windows go to. SG_GATE_DISPLAY when the
# caller gives one; otherwise an Xvfb of the gate's own, on the first free
# display from SG_GATE_XVFB_FIRST (default 720) on, gone when the gate's shell
# is. 27 gates said "needs a display (SG_GATE_DISPLAY)" and used whatever
# DISPLAY the caller had; since the line above unsets it (10.0-920) they ran
# with none, created no window and failed (win32u-menu-autodismiss on every
# release, 2026-11-22). Sets and exports DISPLAY; returns 77 (skip) without Xvfb.
sg_gate_display() {
    if [ -n "${SG_GATE_DISPLAY:-}" ]; then export DISPLAY="$SG_GATE_DISPLAY"; return 0; fi
    command -v Xvfb >/dev/null || { echo "SKIP: no display (SG_GATE_DISPLAY) and no Xvfb"; return 77; }
    _sg_n=${SG_GATE_XVFB_FIRST:-720}
    while [ "$_sg_n" -lt $(( ${SG_GATE_XVFB_FIRST:-720} + 30 )) ]; do
        if [ ! -e "/tmp/.X$_sg_n-lock" ] && [ ! -e "/tmp/.X11-unix/X$_sg_n" ]; then
            Xvfb ":$_sg_n" -screen 0 1280x1024x24 -nolisten tcp >/dev/null 2>&1 &
            _sg_x=$!
            _sg_i=0
            while [ "$_sg_i" -lt 50 ] && kill -0 "$_sg_x" 2>/dev/null && [ ! -e "/tmp/.X11-unix/X$_sg_n" ]; do
                sleep 0.1; _sg_i=$((_sg_i + 1))
            done
            if kill -0 "$_sg_x" 2>/dev/null && [ -e "/tmp/.X11-unix/X$_sg_n" ]; then
                # the Xvfb goes with the gate, whatever trap the gate sets
                ( _sg_p=$$; while kill -0 "$_sg_p" 2>/dev/null; do sleep 1; done; kill "$_sg_x" 2>/dev/null ) \
                    >/dev/null 2>&1 </dev/null &
                export DISPLAY=":$_sg_n"
                return 0
            fi
            kill "$_sg_x" 2>/dev/null   # another gate took it first
        fi
        _sg_n=$((_sg_n + 1))
    done
    echo "SKIP: no free display for an Xvfb"; return 77
}
