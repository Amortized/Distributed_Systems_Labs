#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>
#include "lock_protocol.h"
#include "lock_client.h"
#include "lock_client_cache.h"

class yfs_client : public lock_release_user {
  extent_client *ec;
  // <lab4>
//  lock_client *lc;

// </lab4>
// <lab5>
  lock_client_cache *lc;
//<lab5>
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, FBIG, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    unsigned long long inum;
  };
  
  static std::string filename(inum);
  inum get_new_inum(bool);
  static inum n2i(std::string);

  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  //Get the attributes
  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  //Lab2
  int put(inum, std::string); 
  int get_fileDir_content(inum, std::string&);
  //Lab2 

  //Lab 4
  int remove(inum);
  int acquire(inum);
  int release(inum);
  //Lab 4


  void dorelease(lock_protocol::lockid_t);
  
};

#endif 
