#!/bin/bash

MERGE_PATCHES=${1:-`(cd ${0%/*}; pwd)`}

#MERGE_PATCHES=${1:-$PWD}
#libcore patch: https://android-review.googlesource.com/#/c/79921/
COMPONENTS=${2:-art libcore system/core}
COMPONENTS=${2:-art libcore}
MERGE_WORKDIR=$PWD
echo "----------"

COMPONENTS=`find $MERGE_PATCHES -name *.patch | sed 's|\/[^\/]*$||g' | sort -u | sed "s|$MERGE_PATCHES\/||g" | grep -v orig`
echo COMPONENTS: $COMPONENTS

for COMPONENT in $COMPONENTS; do
#echo DO:   $COMPONENT
patch=1
CC=`echo $COMPONENT | tr '/' '-'`

[ ! -d "$MERGE_PATCHES" ] && echo "invalid path to patches: $MERGE_PATCHES" && exit 1
[ ! -d "$MERGE_WORKDIR" ] && echo "invalid workdir: $MERGE_WORKDIR" && exit 1
[ -z "$patch" ] && echo "invalid patch: $patch" && exit 1

while true; do
#  echo $(printf "%04d" $patch)-$CC
#  fName=$(ls $MERGE_PATCHES/$(printf "%04d" $patch)-$CC-*.patch 2>/dev/null)
  fName=$(ls $MERGE_PATCHES/$COMPONENT/$(printf "%04d" $patch)-*.patch 2>/dev/null)
#  echo fName0: $fName
#exit
  [ ! -e "$fName" ] && break
  fName=${fName#$MERGE_PATCHES/}
#  echo fName: $fName
  CID=`cat $MERGE_PATCHES/$fName | grep "Change-Id:" | sed 's|.*Change-Id: ||g'`
#  CID=`cat $MERGE_PATCHES/$fName | grep "^commit " | sed 's|^commit ||g'`
  if [ "$CID" == "" ]; then
    echo CID EMPTY $COMPONENT $fName
    exit
  fi
  pushd $PWD >/dev/null
  cd $COMPONENT
  status=$?
  [ "$status" -ne 0 ] && exit 1
#  PRESENT=`git log HEAD^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^..HEAD | grep $CID`
  PRESENT=`git log -40 | grep $CID`
#  echo PRESENT: $PRESENT CID: $CID
  if [ "$PRESENT" == "" ]; then
     status=0
    git am -3 $MERGE_PATCHES/$fName
#    git am $MERGE_PATCHES/$fName
    status=$?
    echo "git am -3 $fName ==> $status" | tee -a $MERGE_WORKDIR/apply.log
  else
    echo Already present: $fName
    status=0
  fi
  popd >/dev/null
  [ "$status" -ne 0 ] && exit 1
  patch=$(($patch+1))
done
echo DONE: $COMPONENT
done

exit 0