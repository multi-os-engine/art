function patch_art32() {
  echo patch_art32 SKIPPED
}

function build_art32() {
    export MM="dalvikvm core-libart libart libartd libartd-compiler libart-compiler dex2oat oatdump core-libart-hostdex apache-xml-hostdex jni_compiler_test jni_internal_test jasmin test-art-host-dependencies"
    export MM="dalvikvm core-libart oatdump core-libart-hostdex apache-xml-hostdex jasmin test-art-host-dependencies"
    export JJ=24
    export CM=

    CFG="-j$JJ SHOW_COMMANDS=$CM WITH_REGION_GC=true WITH_TLA=true"
#    CFG="$CFG ART_BUILD_HOST_NDEBUG=false"
#    CFG="$CFG ART_BUILD_HOST_NDEBUG=false"
#    CFG="$CFG ART_BUILD_DEBUG=false"
#    CFG="$CFG ART_BUILD_HOST_DEBUG=false"
    CFG="$CFG ART_BUILD_TARGET_DEBUG=false"
    CFG="$CFG ART_BUILD_TARGET_NDEBUG=false"
    MYOUT=out

    BUILD_RESULT=`cat build-host-art32.log | grep make_error_code | sed 's|.*=||g'`
    if [ ! "$BUILD_RESULT" == "0" ]; then
        ( (export DEX_PREOPT_DEFAULT=nostripping; export BUILD_HOST_64bit=; export OUT_DIR=$MYOUT; make $CFG $MM ; echo make_error_code=$?; ) 2>&1 | tee build-host-art32.log)
    else
        echo "=== SKIP ART 32 bit host rebuild. Remove out/build-host-art32.log to force rebuild.  ==="
        sleep 1
    fi
}

function build_fast_art32() {
    CFG="-j$JJ SHOW_COMMANDS=$CM WITH_REGION_GC=true WITH_TLA=true"
#    CFG="$CFG ART_BUILD_HOST_NDEBUG=false"
#    CFG="$CFG ART_BUILD_HOST_NDEBUG=false"
#    CFG="$CFG ART_BUILD_DEBUG=false"
#    CFG="$CFG ART_BUILD_HOST_DEBUG=false"
    CFG="$CFG ART_BUILD_TARGET_DEBUG=false"
    CFG="$CFG ART_BUILD_TARGET_NDEBUG=false"
#    CFG="$CFG DEX2OAT=out32/host/linux-x86/bin/dex2oatd"

#    (export BUILD_HOST_64bit=; export OUT_DIR=$MYOUT; export WITH_HOST_DALVIK=true; cd art; mm $CFG build-art-host dump-oat; echo fast make_error_code=$?;)
    (export BUILD_HOST_64bit=; export OUT_DIR=$MYOUT; export WITH_HOST_DALVIK=true; cd art; mm $CFG build-art-host; echo fast make_error_code=$?;)
    sleep 1

}

function symlink_art32() {
    echo symlink_art32
#    symlink $PATCH_DIR/art32 out/host/linux-x86/bin/art32
#    symlink $PATCH_DIR/dtests.sh out/host/linux-x86/bin/dtests.sh
#    symlink $PATCH_DIR/build.sh out/host/linux-x86/bin/ds_art32_build.sh
}

function build_host32_art() {
    echo "=== build_host32_art ==="
#    patch_art32
    build_art32
    build_fast_art32
    symlink_art32
}
#patch_art32
