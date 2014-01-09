#!/usr/bin/python
import sys
import string
import time
import datetime

def print_usage(program_name):
    print '%s [log_file]' % (program_name)

def file_get_suffix(file_name):
    #/livestream/3702892333/78267cf4a7864a887540cf4af3c432dca3d52050/flv/2013/12/25/20131017T171949_03_20131225_153710_3509995.flv
    #print file_name
    items = file_name.split('.')
    suffix = items[1]
    return suffix


def file_get_key(file_name):
    #/livestream/3702892333/78267cf4a7864a887540cf4af3c432dca3d52050/flv/2013/12/25/20131017T171949_03_20131225_153710_3509995.flv
    items = file_name.split('/')
    only_name = items[8]
    parts = only_name.split('_')
    part_date = parts[2]
    part_time = parts[3]
    part_id = parts[4]
    key = "%s_%s_%s" % (part_date, part_time, part_id)
    return key
    
        
def file_get_time(file_name):
    #/livestream/3702892333/78267cf4a7864a887540cf4af3c432dca3d52050/flv/2013/12/25/20131017T171949_03_20131225_153710_3509995.flv
    items = file_name.split('/')
    only_name = items[8]
    parts = only_name.split('_')
    part_date = parts[2]
    part_time = parts[3]
    part_id = parts[4]
    year = part_date[0:4]
    month = part_date[4:6]
    day = part_date[6:8]    
    i_year = string.atoi(year)
    i_month = string.atoi(month)
    i_day = string.atoi(day)
    #print '%s, %s, %s' % ( year, month, day )
    #print '%d, %d, %d' % ( i_year, i_month, i_day )
    hour = part_time[0:2]
    minute = part_time[2:4]
    second = part_time[4:6]
    i_hour = string.atoi(hour)
    i_minute = string.atoi(minute)
    i_second = string.atoi(second)
    #print '%s, %s, %s' % ( hour, minute, second )
    #print '%d, %d, %d' % ( i_hour, i_minute, i_second )    
    dt = datetime.datetime(i_year, i_month, i_day, i_hour, i_minute, i_second)
    #print dt.timetuple()
    timestamp = time.mktime(dt.timetuple())
    #print timestamp
    return timestamp

def section_get_time(section):
    #begin: 1387957182.174209 [Wed Dec 25 15:39:42 2013]
    items = section.split(' ')
    return items[2]

def main():
    if(len(sys.argv) < 2):
        print_usage(sys.argv[0])
        return False
        
    file_name = sys.argv[1]    
    log_file = open(file_name, 'r')
    
    while(True):
        line = log_file.readline()
        if(line == ''):
            break           
        items = line.split(',')        
        if(len(items) < 4):
            continue
        section_file        = items[0]
        section_source      = items[1]
        section_len         = items[2]
        section_begin_time  = items[3]
        section_end_time    = items[4] 
        file_suffix = file_get_suffix(section_file)     
        if(file_suffix != 'flv') and (file_suffix != 'ts'):
            continue               
        segment_key = file_get_key(section_file)
        segment_timestamp = file_get_time(section_file)
        begin_time = section_get_time(section_begin_time)
        end_time = section_get_time(section_end_time)
        if(len(begin_time) == 0):
            continue
        begin_timestamp = string.atof(begin_time)
        if(len(end_time) == 0):
            continue
        end_timestamp = string.atof(end_time)
        download_time = end_timestamp - begin_timestamp
        delay_time = end_timestamp - segment_timestamp
        print '%s, %f, %f, %f, %f, %f' % (segment_key, segment_timestamp, begin_timestamp, end_timestamp, download_time, delay_time)          
        
    return True

    
if __name__ == '__main__':
    main()  
        
        