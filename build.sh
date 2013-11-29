#!/bin/bash
export OS_VERSION=`cat /etc/redhat-release|awk '{print $1$3}'`
make clean
make
cp -rf LiveMediaServer ./bin/ 
cp -rf LiveMediaServer.xml ./bin/
cp -rf ./html/ ./bin/
#tar -czvf  LiveMediaServer-0.1.0.0.tar.gz ./bin/ 
