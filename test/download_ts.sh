#!/bin/bash

if [[ $# < 1 ]]
then
	echo "usage: ./downnload_ts.sh  prefix_path_of_ts_file"
	exit 1
fi

prefix_path_of_ts_file=$1

web_m3u8_filename="web.m3u8"

last_ts_filename=""

cur_ts_filename=""

output_filename=ts.m3u8


if [[ -e $output_filename ]]
then
	rm -rf $output_filename
fi


while [[ 1 ]]

do

	curl "http://192.168.160.202:5050/livestream/78267cf4a7864a887540cf4af3c432dca3d52050.m3u8?codec=ts" -o $web_m3u8_filename

	if [[ ! -e $web_m3u8_filename ]]
	then
		echo "download web m3u8 file error!"
		exit
	fi


	while [[ 1 ]]
	do

		if [[ -z $last_ts_filename ]]
		then
			cur_ts_filename=`grep "\.ts" $web_m3u8_filename| sed -n '1 p'`
			echo $cur_ts_filename
		else
			cur_ts_filename=`grep "\.ts" $web_m3u8_filename| sed -n "/$last_ts_filename/{n;p;}"`
			echo $cur_ts_filename
		fi

		
		if [[ -z $cur_ts_filename ]]
		then
			break
		fi


		cur_ts_filename=`echo -e $cur_ts_filename |tr -d '\n\r'`
				

		echo $cur_ts_filename
		
	
		ts_file_size=`curl -s  "http://192.168.160.202:5050/livestream/$cur_ts_filename?codec=ts" --head|grep Content-Length|awk -F"[ \r\n]" '{print $2}'`

        	echo $ts_file_size
		
        	echo $ts_file_size|awk '{print length($0)}'

        	curl  "http://192.168.160.202:5050/livestream/$cur_ts_filename?codec=ts" -O

        	if [[ -e $cur_ts_filename ]]
        	then
             		curr_ts_file_size=`wc -c  $cur_ts_filename|awk -F " " '{print $1}'`

             		echo $curr_ts_file_size            
             		echo $curr_ts_file_size|awk '{print length($0)}'

             		if [ "$ts_file_size" = "$curr_ts_file_size" ]
             		then
                		echo "$prefix_path_of_ts_file/$cur_ts_filename">>$output_filename
             		else
                 		echo "error! download is not complete! $cur_ts_filename"
             		fi
		else
			echo "error! can't download $cur_ts_filename"
         	fi
		sleep 100
        	last_ts_filename=$cur_ts_filename

	done

done


