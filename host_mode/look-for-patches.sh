echo fetch

REF=sync-4328
TO=${1:-`(cd ${0%/*}; pwd)`}
echo TO:$TO

if [ ! -e projects.lst ]; then
    find . -name '.git' | sed 's|/.git||g' > projects.lst
fi

if [ -e projects.changed ]
then
    cp projects.changed projects.curr
else
    cp projects.lst projects.curr
fi

rm $PWD/projects.curr.changed
for P in `cat projects.curr`
do
#    PROJ=${P%/*}
    PROJ=${P}
    TOP=$PWD
    pushd $PWD > /dev/null
    cd $PROJ
    CID=`git show | grep "^commit " | sed 's|^commit ||g'`
    if [ ! "$CID" == "" ]; then
        (git format-patch --binary $REF > /dev/null; if [ -e 0001*.patch ]; then echo $PROJ; mkdir -p $TO/$PROJ; mv *.patch $TO/$PROJ; fi) | tee -a $TOP/projects.curr.changed
    else
        echo $PROJ: CID is empty
    fi
    popd > /dev/null
done