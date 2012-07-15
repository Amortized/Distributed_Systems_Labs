// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include "lock_client_cache.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


yfs_client::yfs_client(std::string extent_dst, std::string lock_dst) 
{                                         
  ec = new extent_client(extent_dst);                                

//   lc = new lock_client(lock_dst);

  lc = new lock_client_cache(lock_dst);
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
  std::istringstream ist(n);
  unsigned long long finum;
  ist >> finum;
  return finum;
}

std::string
yfs_client::filename(inum inum)
{
  std::ostringstream ost;
  ost << inum;
  return ost.str();
}

bool
yfs_client::isfile(inum inum)
{
  if(inum & 0x80000000)
    return true;
  return false;
}

bool
yfs_client::isdir(inum inum)
{
  return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
  int r = OK;


  printf("getfile %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }

  fin.atime = a.atime;
  fin.mtime = a.mtime;
  fin.ctime = a.ctime;
  fin.size = a.size;
  printf("getfile %016llx -> sz %llu\n", inum, fin.size);

 release:

  return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
  int r = OK;


  printf("getdir %016llx\n", inum);
  extent_protocol::attr a;
  if (ec->getattr(inum, a) != extent_protocol::OK) {
    r = IOERR;
    goto release;
  }
  din.atime = a.atime;
  din.mtime = a.mtime;
  din.ctime = a.ctime;

 release:
  return r;
}

int 
yfs_client::put(inum inum, std::string content)
{
  int r = OK;

  printf("put %016llx\n", inum);
  extent_protocol::attr a;

  if (ec->put(inum, content) != extent_protocol::OK) {
    r = IOERR;
  } 
  return r;  
}

int 
yfs_client::get_fileDir_content(inum inum, std::string& fd) 
{
  int r = OK;
  printf("get file or dir content  %016llx\n", inum);

  extent_protocol::attr a;
  if(ec->get(inum, fd) != extent_protocol::OK) {
    r = IOERR;
  }
  return r;
}

//Lab 4

int
yfs_client::remove(inum inum) 
{
  int r = OK;
  printf("remove1  %016llx\n", inum);
  if(ec->remove(inum) != extent_protocol::OK) {
    r = IOERR;
  } 
  return r;
}



int
yfs_client::acquire(inum inum)
{
  int r = OK;
  printf("acquire  %016llx\n", inum);
  lock_protocol::lockid_t lid = inum;
  if(lc->acquire(lid) != lock_protocol::OK) {
    r = IOERR;
  }
  return r;
}

int
yfs_client::release(inum inum)
{
  int r = OK;
  printf("Remove1  %016llx\n", inum);
  lock_protocol::lockid_t lid = inum;
  if(lc->release(lid) != lock_protocol::OK) {
    r = IOERR;
  }
  return r;
}






//Lab 4
