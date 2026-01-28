set -eu
mkdir -vp "$(dirname "${DESTDIR:-}$2")"
if [ "$(dirname $1)" = . ]; then
    ln -fs -T "$1" "${DESTDIR:-}$2"
else
    ln -fs -T --relative "${DESTDIR:-}$1" "${DESTDIR:-}$2"
fi
