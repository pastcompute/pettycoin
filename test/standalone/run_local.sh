#! /bin/sh

set -e

if [ $# -ne 1 ]; then
    echo "Usage: run_local.sh <num-instances>" >&2
    exit 1
fi

trap "killall serve_addresses; killall pettycoin" EXIT

rm -rf home-* addresses

touch addresses
./serve_addresses &

# Only debug for first one.
flags="--log-level=debug"
for i in `seq $1`; do
    mkdir home-$i
    HOME=home-$i ../../pettycoin --developer-test $flags &
    flags=""
done
trap "" EXIT