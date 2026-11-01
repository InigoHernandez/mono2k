#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# MONO2K onto the Move over Wi-Fi (after move/build.sh): the module copied under a temporary name beside Schwung's
# modules, then renamed into place, so the Move never sees a half-copied module and a dsp.so the Move still has
# mapped is replaced by a rename, never written over.
#   sh move/deploy.sh               (MOVE_HOST=user@host to use another; ~/.ssh/config: Host move.local, User ableton)
# Then on the Move: open MONO2K from Schwung's overtake menu. If it was open, close it first (Shift + Back, or Shift +
# touch the volume knob + jog click): Back alone only parks it, and the old dsp.so stays loaded.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
HOST=${MOVE_HOST:-move.local}
BASE=/data/UserData/schwung
DEST=$BASE/modules/overtake/mono2k
STAGE=$BASE/.mono2k-incoming          # outside modules/: the overtake menu's scan never lists it
OLD=$BASE/.mono2k-old
SRC=build/move/modules/overtake/mono2k
SSH_OPTS="-o BatchMode=yes -o ConnectTimeout=8"

for f in dsp.so module.json ui.js; do
    [ -f "$SRC/$f" ] || { echo "no $SRC/$f: run sh move/build.sh first"; exit 1; }
done

# shellcheck disable=SC2086
ssh $SSH_OPTS "$HOST" "rm -rf '$STAGE' && mkdir -p '$BASE/modules/overtake'"
# shellcheck disable=SC2086
scp $SSH_OPTS -r "$SRC" "$HOST:$STAGE"
# shellcheck disable=SC2086
ssh $SSH_OPTS "$HOST" "set -e
    for f in dsp.so module.json ui.js; do [ -s '$STAGE'/\$f ] || { echo \"copy incomplete: \$f\"; exit 1; }; done
    rm -rf '$OLD'
    if [ -e '$DEST' ]; then mv '$DEST' '$OLD'; fi
    mv '$STAGE' '$DEST'
    rm -rf '$OLD'
    ls -l '$DEST'"
echo "MONO2K is on $HOST at $DEST"
