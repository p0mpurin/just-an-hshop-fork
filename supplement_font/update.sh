#!/bin/sh
root=`dirname $0`
cd $root
bcfnt.py -c -v -y -f supplemental_font.bcfnt
mv supplemental_font.bcfnt ../romfs
rm -f ../3hs.3dsx
cd -
