// the extent server implementation

#include "extent_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extent_server::extent_server()
{		              
/*  int m;					
  int ret = put(0x00000001,"root", m);		
  assert(ret == extent_protocol::OK);		*/
}					

int extent_server::put(extent_protocol::extentid_t id, std::string buf, int &)
{ 
  std::cout << "Put : id" <<  id << "in the extent server" << std::endl;
  time_t now = time(NULL);
  it = fileContents_.find(id);
  if(it != fileContents_.end()) { //File Exists
      (it->second).first.ctime = now;
      (it->second).first.mtime = now;
      (it->second).first.size = buf.size();
      (it->second).second = buf;
  } else { 
       extent_protocol::attr a;
       a.atime = now;
       a.mtime = now;
       a.ctime = now;
       a.size = buf.size();
       pair<extent_protocol::attr, string> newpair = make_pair(a, buf);
       fileContents_.insert(pair<extent_protocol::extentid_t ,
                        pair<extent_protocol::attr, string> > (id, newpair));  
  } 										
  return extent_protocol::OK;				
} 						

int extent_server::get(extent_protocol::extentid_t id, std::string &buf)
{
  std::cout << "Get : id" <<  id << "in the extent server" << std::endl;
  it = fileContents_.find(id);
  if(it != fileContents_.end()) { //File Exists
    (it->second).first.atime = time(NULL); //Change the Access Time
    buf = (it->second).second;      
    return extent_protocol::OK;
  } 
  return extent_protocol::NOENT;
}  

int extent_server::getattr(extent_protocol::extentid_t id, extent_protocol::attr &a)
{
  // You replace this with a real implementation. We send a phony response
  // for now because it's difficult to get FUSE to do anything (including
  // unmount) if getattr fails.
  it = fileContents_.find(id);
  if ( it == fileContents_.end() ) {
   a.size  = 0;
   a.atime = 0;
   a.mtime = 0;
   a.ctime = 0;
   return extent_protocol::NOENT;
  }                                      

  a.size = (it->second).first.size;
  a.atime = (it->second).first.atime;
  a.mtime = (it->second).first.mtime;
  a.ctime = (it->second).first.ctime;
  return extent_protocol::OK;
}

int extent_server::remove(extent_protocol::extentid_t id, int &)
{   
  it = fileContents_.find(id);
  if ( it == fileContents_.end() ) 
   return extent_protocol::NOENT;

  //Delete the File
  fileContents_.erase(it);

  return extent_protocol::OK;
}

