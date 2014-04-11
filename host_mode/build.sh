echo "=== Host mode config ==="
echo "Sample usage: source art/host_mode/build.sh [64|32]"
PATCH_DIR=$PWD/art/host_mode

function symlink() {
    WHAT=$1
    WHERE=$2

    WHERE_PATH=${WHERE%/*}
    if [ -e $WHERE_PATH ]; then

        echo "Creating symlink $WHERE"
        if [ ! "`ls $WHERE`" == "" ]; then
#            echo "    Symlink: update"
            rm $WHERE
        else
            echo "    Symlink: create"
        fi
        ln -s $WHAT $WHERE
#        echo $WHAT: $WHERE
    else
        echo SYMLINK ERROR: $WHERE_PATH not exists, skip $WHERE
    fi
}

source $PATCH_DIR/host_art32.sh
source $PATCH_DIR/host_art64.sh

MODE=${1:-64}

if [ "$MODE" == "64" ]; then
  build_host64_art
else
  build_host32_art
fi

#source $PATCH_DIR/host_dalvik32.sh
#source $PATCH_DIR/host_dalvik64.sh
#build_host32_dalvik
#build_host64_dalvik
