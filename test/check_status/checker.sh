#!/bin/sh

RIGHT_VERSION=`cat ms_version`

#echo "command "
#echo "curl -s -m 10 http://ip/macross/?cmd=queryversion"
#echo "curl -s -m 10 http://ip/macross/?cmd=query_channel&liveid=$id"

echo "check version start..."

for IP in `cat ./livems_ip`
do
	SERVER_VERSION=`curl -s -m 10 "http://$IP/macross/?cmd=queryversion" | grep server | cut -d'=' -f2 | cut -d'/' -f2 | cut -d '@' -f1`
	if [ "$RIGHT_VERSION" == "$SERVER_VERSION" ]
	then
		echo "$IP version is $SERVER_VERSION"
	else
		echo "$IP version is $SERVER_VERSION, error"
	fi
done

echo "check channel ..."

for IP in `cat ./livems_ip`
do  
    for ID in `cat ./channel_id`
    do
		CHANNEL_INFO=`curl -s -m 10 "http://$IP/macross/?cmd=query_channel&liveid=$ID"| grep result | cut -d'=' -f2`
		RESULT=`echo $CHANNEL_INFO | cut -d'[' -f1`
		if [ "$RESULT" == "failure" ]
		then
			echo "$IP check $ID, $CHANNEL_INFO, error"
		else
			echo "$IP check $ID, $CHANNEL_INFO"
		fi
    done  
done
