#!/usr/bin/env python

#url = http://124.205.11.249:5050/livestream/0b49884b3b85f7ccddbe4e96e4ae2eae7a6dec56.m3u8?len=1&time=9066

#for u in `curl -s http://220.181.167.45/livestream/0b49884b3b85f7ccddbe4e96e4ae2eae7a6dec56.m3u8 | grep -v '#'`

import os
import sys
import urllib2
import httplib

#http://113.57.155.195:8182/live/ch2.m3u8
#location='220.181.167.45:80'
#path='/livestream/0b49884b3b85f7ccddbe4e96e4ae2eae7a6dec56.m3u8'
#http://183.61.252.137:5050/livestream/78267cf4a7864a887540cf4af3c432dca3d52050.m3u8

#location="192.168.219.101"
location="192.168.160.202:5050"
path='/livestream/'
channel_name="78267cf4a7864a887540cf4af3c432dca3d52050"

base_path="./"

#destfile = http://113.57.155.195:8182/live/hls2-177.ts
#http://192.168.219.101/live/ch2.m3u8

def check_file_exist(filename):
  return os.access(filename,os.R_OK)

def downloadfile(url,filename):
  try:
      t = filename.split('/')
      f = t[len(t)-1]
      local_file = base_path + "/" + channel_name + "/" + f

      '''
      if check_file_exist(local_file) :
        return True
      '''

      request = urllib2.Request(filename)
      #f = open(local_file,'wb')
      data = urllib2.urlopen(request).read()
      #f.write(data)
      #f.close()
  except Exception,e:    
    print 'downloadfile: '
    print e
    sys.exit(1)
    return False
  return True

def get_filelist(location,channel_name):
  dir='http://'+location+path
  url = dir + channel_name +  ".m3u8"

  try:
      conn = httplib.HTTPConnection(location)
      conn.request("GET",path+channel_name+".m3u8")
      data = conn.getresponse().read().split("\n")
  except Exception,e:
      print 'get_filelist: '
      print e
      sys.exit(1)

  for filename in data:
    if filename.find('#') == -1 and len(filename) > 0 :
      downloadfile(dir,filename)

if __name__ == "__main__":
  if not os.access(base_path+channel_name,os.R_OK):
    os.mkdir(base_path+channel_name)

  while True:
    get_filelist(location,channel_name)
