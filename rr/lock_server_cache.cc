// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

static void *
revokethread(void *x)
{   
  lock_server_cache *sc = (lock_server_cache *) x;
  sc->revoker();
  return 0;
}                   

static void *
retrythread(void *x)
{
  lock_server_cache *sc = (lock_server_cache *) x;
  sc->retryer();
  return 0;
}

lock_server_cache::lock_server_cache()
{
  pthread_t thds[2]; //Need two Different Threads
  int r = pthread_create(&thds[0], NULL, &revokethread, (void *) this);
  assert (r == 0);
  r = pthread_create(&thds[1], NULL, &retrythread, (void *) this);
  assert (r == 0);

  has_release = 0;
}

void
lock_server_cache::revoker()
{

  // This method should be a continuous loop, that sends revoke
  // messages to lock holders whenever another client wants the
  // same lock
/*
  while(true) {

        //Get the Current Revoke Map
        m.lock();
  
        while(revokeMap.empty()) {
          rvk_cv.wait(&m);
        }  
        
        map<int, map<lock_protocol::lockid_t, unsigned int> > tempRevokeMap = revokeMap;
        revokeMap.clear(); 
        m.unlock();   


        //Send Revoke RPC's to all the clients 

        map<int, map<lock_protocol::lockid_t, unsigned int> >::iterator it; 
        for(it = tempRevokeMap.begin(); it != tempRevokeMap.end(); it++) {  

             rpcc* rc = clientMap[it->first];  //Get the Client Handler
          
             //For Each of the Locks send the lockid and seq no
             map<lock_protocol::lockid_t, unsigned int>::iterator it1;
             int r, ret;
             for(it1 = (it->second).begin(); it1!= (it->second).end() ; it1++) {
                
		 ret = rc->call(rlock_protocol::revoke, it1->first, it1->second, r);
		 assert(ret == rlock_protocol::OK);
             }


  
        }

  } //end of while
*/

////
    while (true)
  {
    m.lock();
    while (revokeMap.empty())
    {
      rvk_cv.wait(&m);
    }
    //cout << "[revoker]: enter!" << endl;

    map<int, map<lock_protocol::lockid_t, unsigned int> > temp_map = revokeMap;
    revokeMap.clear();
    m.unlock();

      for (map<int, map<lock_protocol::lockid_t, unsigned int> >::iterator \
           it = temp_map.begin();
           it != temp_map.end();
           it++)
      {
        //cout << "[revoker]: to revoke client " << it->first << endl;
        rpcc *rc = clientMap[it->first];
        int r;
        map<lock_protocol::lockid_t, unsigned int> lid_to_seq = it->second;
        for (map<lock_protocol::lockid_t, unsigned int>::iterator \
             rit = lid_to_seq.begin();
             rit != lid_to_seq.end();
             rit++)
        {
          lock_protocol::lockid_t lid = rit->first;
          unsigned int seq_num = rit->second;
          //cout << "[revoker] lid=" << lid << " seq_num=" << seq_num
          //     << " " << rc << endl;
          //cout << "[revoker]: before send revoke rpc" << endl;
	          int ret = rc->call(rlock_protocol::revoke, lid, seq_num, r);
          //cout << "[revoker]: after send revoke rpc" << endl;
          //cout << "[revoker]: revoke lid " << lid << " on client " << it->first << endl;
          //cout << "[revoker]: ret=" << ret << endl;
          assert(ret == lock_protocol::OK);
        }
      }
    //cout << "[revoker]: exit" << endl;
  }


///

}


void
lock_server_cache::retryer()
{

  // This method should be a continuous loop, waiting for locks
  // to be released and then sending retry messages to those who
  // are waiting for it.

  while(true) {
    //Get the Retry Map
    m.lock();

    while(has_release == 0) { 
      rty_cv.wait(&m);
    }

    map<lock_protocol::lockid_t, set<int> > tempRetryMap = retryMap;
    retryMap.clear();
    has_release--;
    m.unlock();
    
    //Send the Retry RPC's for each lock
    map<lock_protocol::lockid_t, set<int> >::iterator it;
    for(it = tempRetryMap.begin(); it != tempRetryMap.end(); it++) {
   
        set<int>::iterator it1;
	int r, ret;
        for(it1 = (it->second).begin(); it1 != (it->second).end(); it1++) {
          rpcc* rc = clientMap[*it1];  //Get the Client Handler
          ret = rc->call(rlock_protocol::retry, it->first, r);
	  assert(ret == rlock_protocol::OK);
        }

    }

  } //end of while

}


lock_protocol::status 
lock_server_cache::subscribe(int client, int serverPort, int& r) {

  NewScopedLock l(&m);

  if(clientMap.find(client) == clientMap.end()) {
  std::ostringstream port;
  port << serverPort;
  sockaddr_in dstsock;
  make_sockaddr(port.str().c_str(), &dstsock);
  rpcc* cl = new rpcc(dstsock); 
  if (cl->bind() < 0) {
    printf("lock_client: call bind\n");
  }
  clientMap[client] = cl;
  }  
  
  r = nacquire;
  return lock_protocol::OK;
}

lock_protocol::status 
lock_server_cache::unsubscribe(int client, int& r) {
 
  
  NewScopedLock l(&m);

  rpcc* rc = clientMap[client]; 
  delete rc;
  rc = NULL;
  clientMap.erase(client);
  
  r = nacquire;
  return lock_protocol::OK;
}


void 
lock_server_cache::add_Retry_Request(lock_protocol::lockid_t lid, int client) {  //Add to Retry Map

  map<lock_protocol::lockid_t, set<int> >::iterator it;
  set<int> clients;

  it = retryMap.find(lid);
  if(it != retryMap.end()) { //Old Lock
     clients = it->second; 
  }
  clients.insert(client); //Add a Client
  retryMap[lid] = clients;
}

void 
lock_server_cache::add_Revoke_Request(int client, lock_protocol::lockid_t lid) { //Add to Revoke Map
  map<int, map<lock_protocol::lockid_t, unsigned int> >::iterator it;
  it = revokeMap.find(client);
  map<lock_protocol::lockid_t, unsigned int> lid_to_seq;

  if (it != revokeMap.end()) { //Old Client
    lid_to_seq = revokeMap[client];
  } else {
    lid_to_seq[lid] = 0; //New Sequence No
  }
  assert(clientLockSeqMap[client][lid] >= lid_to_seq[lid]); //Revoke req is not greater than one client is serving
  lid_to_seq[lid] = clientLockSeqMap[client][lid]; 
  revokeMap[client] = lid_to_seq;

}

lock_protocol::status 
lock_server_cache::acquire(int client, lock_protocol::lockid_t lid, unsigned int seqNo, int &r) {

  int ret = lock_protocol::OK;
  if(seqNo <= clientLockSeqMap[client][lid] ) { 
    cout << "Server has already handled the acquire" << endl;
    return ret; // No need to send revoke or retry again.So retry 
  }

  //Check the Lock Allocation Map 
  lockStatus* ls = NULL;
  NewScopedLock l(&m);

  if ( lockAllocationMap.find(lid) == lockAllocationMap.end()) { //New Lock
    lockStatus* ls = (lockStatus*)malloc(sizeof(lockStatus));
    ls->isAllocated = false;
    ls->client = client;
    ls->seqNo = 0;
    lockAllocationMap[lid] = ls; //Put the New lock
    clientLockSeqMap[client][lid] = 0; //Client Serving a new lock; Give it Seq No 0
  } 
    ls = lockAllocationMap[lid];
  

  //Check the status of Lock 
  if(!ls->isAllocated) { //Lock is Not Allocated
     ls->isAllocated = true;
     ls->client = client; 
     ls->seqNo = seqNo;
     clientLockSeqMap[client][lid] = seqNo;  
  } else {  //Remember Rule 2 of Design:Dnt Call RPC's 
     ret = lock_protocol::RETRY;
     add_Retry_Request(lid, client); //We need to save this so that We can signal the client to retry 
     add_Revoke_Request(ls->client, lid); //ls->client is holding the lock currently; Revoke it from the client
     rvk_cv.signal(); //We only need to wake up the revoke thread 
  } 
  return ret;

}


lock_protocol::status 
lock_server_cache::release(int client, lock_protocol::lockid_t lid, unsigned int seqNo, int& r) {


  NewScopedLock l(&m);
  lockStatus* ls = lockAllocationMap[lid];

  if(seqNo == ls->seqNo && client == ls->client) {
  
   assert(ls->isAllocated == true);

   ls->isAllocated = false;
   ls->seqNo = 0;
   has_release++;
   rty_cv.signal();  //Wake up the release thread to send all the clients Retry RPC's
  }

  return lock_protocol::OK;


}

