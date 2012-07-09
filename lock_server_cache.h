#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"
#include <set>

using namespace std;

class lock_server_cache {
 private:                                                               
  struct lockStatus {
    bool isAllocated;
    int  client;
    unsigned int seqNo;
  }; 

  Mutex m;
  ConditionVar rty_cv;
  ConditionVar rvk_cv;


  map<int,rpcc*> clientMap; //Registering the Client Handlers
  map<int, map<lock_protocol::lockid_t, unsigned int> > clientLockSeqMap; //Remember the Seq No per Client per Lock
  map<lock_protocol::lockid_t, lockStatus*> lockAllocationMap; // Status and Holder of Locks
  map<int, map<lock_protocol::lockid_t, unsigned int> > revokeMap; //To send revoke RPC's to Clients
  map<lock_protocol::lockid_t, set<int> > retryMap; // To send retry RPC's to all clients for that particular lock
  
  //Helper functions
  unsigned int has_release;
  void add_Retry_Request(lock_protocol::lockid_t lid, int client);  //Add to Retry Map
  void add_Revoke_Request(int client, lock_protocol::lockid_t lid); //Add to Revoke Map

               
 protected:
 int nacquire;
                                                    
 public:
 
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  lock_protocol::status subscribe(int client, int serverPort,int&);
  lock_protocol::status unsubscribe(int client, int&);
  lock_protocol::status acquire(int client, lock_protocol::lockid_t , unsigned int, int&);
  lock_protocol::status release(int client, lock_protocol::lockid_t, unsigned int, int&);  
  void revoker();
  void retryer();

};                                                                     

#endif
