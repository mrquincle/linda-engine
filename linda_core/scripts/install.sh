#!/bin/sh

sudo cp ../Debug/liblinda.so /usr/lib
sudo rsync -avzcuL --exclude '.svn*' --include '*.h' --exclude '*' ../inc/ /usr/include/linda
