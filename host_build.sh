cd $ANDROID_BUILD_TOP
LOG=host-build-art64.log
JOB=24
#JOBS=1

MODE=${1:-build}

NEED_REBUILD=`cat $LOG-fast.log | grep "No rule to make target"`
#NEED_REBUILD=
if [ ! "$NEED_REBUILD" == "" ]; then
  rm $LOG.log
  rm $LOG-fast.log
fi

BUILD_RESULT=`cat $LOG.log | grep make_error_code | sed 's|.*=||g'`
if [ ! "$BUILD_RESULT" == "0" ]; then
  source build/envsetup.sh
  lunch aosp_x86_64-userdebug
  export WITH_DEXPREOPT=false
  export BUILD_HOST_64bit=1
  CFG="ART_BUILD_TARGET_DEBUG=false ART_BUILD_TARGET_NDEBUG=false"
  CFG=
  (rm -rf out/host/linux-x86/framework/x86*; mm $CFG -j$JOBS test-art-host-default dalvikvm; echo make_error_code=$?;) 2>&1 | tee $LOG.log
fi

BUILD_RESULT=`cat $LOG.log | grep make_error_code | sed 's|.*=||g'`
if [ "$BUILD_RESULT" == "0" ]; then
  # Build fast
  export WITH_DEXPREOPT=false
  export BUILD_HOST_64bit=1
  export ART_BUILD_TARGET_DEBUG=false
  export ART_BUILD_TARGET_NDEBUG=false
  CFG="ART_BUILD_TARGET_DEBUG=false ART_BUILD_TARGET_NDEBUG=false"
  CFG=
  MM="libart libartd libart-compiler libartd-compiler dex2oat dalvikvm"
  if [ "$MODE" == "test" ]; then
    MM=test-art-host-default
  fi
  (rm -rf out/host/linux-x86/framework/x86*; cd art; mm $CFG -j$JOBS $MM; echo fast_make_error_code=$?;) 2>&1 | tee $LOG-fast.log
#  cat $LOG-fast.log | grep FAILED
#else
#  cat $LOG.log | grep FAILED
fi
