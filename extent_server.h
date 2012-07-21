// this is the extent server

#ifndef extent_server_h
#define extent_server_h

#include <string>
#include <map>
#include "extent_protocol.h"
#include "lock_protocol.h"

using namespace std;

class extent_server {

 public:
  extent_server();

  int put(extent_protocol::extentid_t id, std::string, int &);
  int get(extent_protocol::extentid_t id, std::string &);
  int getattr(extent_protocol::extentid_t id, extent_protocol::attr &);
  int remove(extent_protocol::extentid_t id, int &);
 private:
  
  //Lab 2//
  extent_protocol::extentid_t inum_;
  map<extent_protocol::extentid_t , 
			pair<extent_protocol::attr, string> > fileContents_;

  map<extent_protocol::extentid_t , 
                        pair<extent_protocol::attr, string> >::iterator it;
  //Lab 2//


  // <lab6>
  Mutex fileMutex;
  
};

#endif 







