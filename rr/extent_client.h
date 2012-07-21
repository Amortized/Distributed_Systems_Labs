// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "rpc.h"
#include <map>
#include "lock_protocol.h" //For Mutex

class extent_client {
 private:
  rpcc *cl;

  //Local Cache
  struct fileDetails {                          
    extent_protocol::attr fileAttr;		
    std::string content;					
    bool isDirty;  		        		
    bool removed; //To check if this client has previously removed this file  		 	
    bool exist_in_server; // If while holding the lock, you make a get call once then no need to 
			  // make the git call again just check this variable
  };					             
			
  Mutex fileCacheLock;						
  std::map<extent_protocol::extentid_t , fileDetails> fileCache;		
  extent_protocol::status fileGetter(extent_protocol::extentid_t eid,		
				      std::string &buf,extent_protocol::attr &a);	
    

 public:
  extent_client(std::string dst);

  extent_protocol::status get(extent_protocol::extentid_t eid, 
			      std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				  extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);

  extent_protocol::status flush(extent_protocol::extentid_t eid);

};

#endif 

