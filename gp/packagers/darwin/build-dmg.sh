#!/bin/bash

# Pass the directory where the IDE files are as a parameter
exepath=$1
if [ -z "$exepath" ]; then
    echo "Please provide a path to the MacOS .app dir"
    echo "Example:"
    echo
    echo "./build-deb.sh ~/ublocks-mac.app [destination] [version-number]"
    exit 1
fi;

destdir=$2
if [ -z "$destdir" ]; then destdir=".."; fi

version=$3
if [ -z "$version" ]; then version="unknown"; fi

# untested in Mac!
if test -z `which mkfs.hfsplus`; then
    echo "I will now try to install hfstools for you."
    echo "Please provide your sudo password when asked."
    ./hfstools-install
    if test $? != 0; then
        # errors occurred when installing hfstools. We cannot proceed.
        echo "I tried to install hfstools into your system but failed."
        echo "Please install them manually and try again."
        echo "Here's the list of commands I ran for you:"
        echo
        cat hfstools-install
        exit 1
    fi
fi

size=`du -B1048576 $exepath | tail -n1 | cut -f1`
echo
echo "================"
echo "Will now try to build a dmg file for Mac."
echo "Please provide your sudo password if asked:"
sudo ./makedmg "MicroBlocks.dmg" "MicroBlocks" $size $exepath
mv MicroBlocks.dmg $destdir