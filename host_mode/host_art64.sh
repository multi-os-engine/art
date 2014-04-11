function patch_gcc64() {
echo patch_gcc64
#symlink $PWD/prebuilts/gcc/linux-x86 prebuilts/gcc/linux-x86_64
#symlink $PWD/prebuilts/gcc/linux-x86/host/i686-linux-glibc2.7-4.6/sysroot/usr/include/linux/capability.h prebuilts/gcc/linux-x86/host/x86_64-linux-glibc2.7-4.6/sysroot/usr/include/sys/capability.h
}

function patch_art64() {
   echo patch_art64 IS NOT IMPLEMENTED
}

function build_art64() {
#! for fast
#    rm build-host-art64.log
    export MM="dalvikvm libart libartd libartd-compiler libart-compiler dex2oat dex2oatd oatdump core-libart-hostdex apache-xml-hostdex jni_compiler_test jni_internal_test jasmin test-art-host-dependencies core-libart"

    export JJ=24
#    export JJ=1
    export CM=1
    export CM=
    export MYOUT=out
#    export MYOUT=out64

    CFG="-j$JJ SHOW_COMMANDS=$CM WITH_REGION_GC=true WITH_TLA=true"
#    CFG="$CFG ART_BUILD_HOST_NDEBUG=false"
#    CFG="$CFG ART_BUILD_DEBUG=false"
#    CFG="$CFG ART_BUILD_HOST_DEBUG=false"

    CFG="$CFG ART_BUILD_TARGET_DEBUG=false"
    CFG="$CFG ART_BUILD_TARGET_NDEBUG=false"
#    CFG="$CFG DEX2OAT=out64/host/linux-x86/bin/dex2oatd --compiler-filter=interpret-only"
    CFG="$CFG WITH_HOST_DALVIK=true"

    BUILD_RESULT=`cat build-host-art64.log | grep make_error_code | sed 's|.*=||g'`
    if [ ! "$BUILD_RESULT" == "0" ]; then
        ( (export DEX_PREOPT_DEFAULT=nostripping; export BUILD_HOST_64bit=1; export OUT_DIR=$MYOUT; make $CFG $MM acp; echo make_error_code=$?; ) 2>&1 | tee build-host-art64.log)
#echo "!!! skip ART 64 build temp"
    else
        echo "=== SKIP ART 64 bit host rebuild. Remove out/build-host-art64.log to force rebuild.  ==="
        sleep 1
    fi
}

function build_fast_art64() {
    echo build_fast_art64 NOT IMPLEMENTED

    CFG="-j$JJ SHOW_COMMANDS=$CM WITH_REGION_GC=true WITH_TLA=true"
#    CFG="$CFG ART_BUILD_HOST_NDEBUG=false"
#    CFG="$CFG ART_BUILD_DEBUG=false"
#    CFG="$CFG ART_BUILD_HOST_DEBUG=false"

    CFG="$CFG ART_BUILD_TARGET_DEBUG=false"
    CFG="$CFG ART_BUILD_TARGET_NDEBUG=false"
#    CFG="$CFG DEX2OAT=out64/host/linux-x86/bin/dex2oatd --compiler-filter=interpret-only"
    CFG="$CFG WITH_HOST_DALVIK=true"

#    (export BUILD_HOST_64bit=1; export OUT_DIR=$MYOUT; export WITH_HOST_DALVIK=true; cd art; mm $CFG build-art-host dump-oat; echo fast make_error_code=$?;)
LOG=build-host-art64-fast.log
    ( (export BUILD_HOST_64bit=1; export OUT_DIR=$MYOUT; export WITH_HOST_DALVIK=true; cd art; mm $CFG build-art-host; echo fast make_error_code=$?;) 2>&1 | tee $LOG)
    sleep 1

dex2oat_output=$LOG
total=`cat $dex2oat_output | grep "Method compiled\|Method filtered" | wc -l`
compiled=`cat $dex2oat_output | grep "Method compiled" | wc -l`
if [ ! "$total" == "0" ]; then
  percent=`echo "$compiled/$total*100" | bc -l | sed 's|\..*$||g'`
  echo "Compilation Rate: $compiled of $total ($percent%)"
fi
}

function symlink_art64() {
    echo symlink_art64
#    symlink $PATCH_DIR/art64 out/host/linux-x86/bin/art64
#    symlink $PATCH_DIR/dtests.sh out/host/linux-x86/bin/dtests.sh
#    symlink $PATCH_DIR/build.sh out/host/linux-x86/bin/ds_art64_build.sh
#x    symlink $PATCH_DIR/dex2oat64 $MYOUT/host/linux-x86/bin/dex2oat
#x    symlink $PATCH_DIR/dex2oatd64 $MYOUT/host/linux-x86/bin/dex2oatd
}

function build_host64_art() {
    echo "=== build_host64_art ==="
    patch_art64
    patch_gcc64
    build_art64
#export JJ=4
    build_fast_art64
    symlink_art64
}
#patch_art64
