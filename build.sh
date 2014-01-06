#!/bin/bash
export OS_VERSION=`cat /etc/redhat-release|awk '{print $1$3}'`
make clean
make
rm -rf ./bin/*
cp -rf LiveMediaServer ./bin/ 
#tar -czvf  LiveMediaServer-0.1.0.022.tar.gz ./bin/ ./etc/ ./html/ ./log/
