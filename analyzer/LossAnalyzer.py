#!/usr/bin/python
import sys
import string
import time
import datetime

def print_usage(program_name):
    print '%s [log_file]' % (program_name)


def file_get_id(file_name):
    #/livestream/3702892333/78267cf4a7864a887540cf4af3c432dca3d52050/flv/2013/12/25/20131017T171949_03_20131225_153710_3509995.flv
    items = file_name.split('/')
    only_name = items[8]
    columns = only_name.split('.')
    base_name = columns[0]
    parts = base_name.split('_')
    part_date = parts[2]
    part_time = parts[3]
    part_id = parts[4]    
    return part_id


def main():
    if(len(sys.argv) < 2):
        print_usage(sys.argv[0])
        return False
        
    file_name = sys.argv[1]    
    log_file = open(file_name, 'r')
    
    last_chunk_id = 0
    line_num = 0
    total_loss_num = 0
    loss_num_404 = 0
    loss_num_break = 0
    while(True):
        line = log_file.readline()
        if(line == ''):
            break           
        items = line.split(',')        
        if(len(items) < 4):
            continue
        line_num = line_num + 1
        section_file        = items[0]
        section_len         = items[1]
        section_begin_time  = items[2]
        section_end_time    = items[3]        
        str_chunk_id = file_get_id(section_file)
        now_chunk_id = string.atoi(str_chunk_id)
        if(line_num > 1 and now_chunk_id - last_chunk_id > 1):
            this_loss_num = now_chunk_id-last_chunk_id-1
            print '%s, %d loss, just loss' % (section_file, this_loss_num)
            total_loss_num = total_loss_num + this_loss_num
            loss_num_break = loss_num_break + this_loss_num
            last_chunk_id = now_chunk_id
            continue
                
        columns = section_len.split(':')
        if(columns[0] == 'error'):
            print '%s, %d loss, error' % (section_file, 1)
            total_loss_num = total_loss_num + 1
            loss_num_404 = loss_num_404 + 1
            last_chunk_id = now_chunk_id
            continue
        print '%s, %d loss, ok' % (section_file, 0)  
        last_chunk_id = now_chunk_id
    
    print 'statistics: line_num=%d, total_loss_num=%d (error_num=%d, break_num=%d)' % (line_num, total_loss_num, loss_num_404, loss_num_break)  
        
    return True

    
if __name__ == '__main__':
    main()  
        
        