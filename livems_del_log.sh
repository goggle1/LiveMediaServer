#!/bin/sh
DAYS_NUM=7
MY_PATH=/home/LiveMS/log
find ${MY_PATH} -type f -mtime +${DAYS_NUM} -exec rm -f {} \; 
