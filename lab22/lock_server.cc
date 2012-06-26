// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
 pthread_mutex_init(&mutex_, NULL);
}

lock_server::~lock_server()
{
 pthread_mutex_destroy(&mutex_);
 for(it = locks_.begin(); it!= locks_.end(); it++) {
  delete it->second->m;
  delete it->second->cv;
  delete it->second;
 }
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int& r) {

  pthread_mutex_lock(&mutex_);
  it = locks_.find(lid);
  if(it != locks_.end() ) { //Same lock was requested before
    
    pthread_mutex_unlock(&mutex_);

    (it->second->m)->lock();
    while(it->second->granted) {  //Another thread is holding the lock
      ((it->second)->cv)->wait(it->second->m);
    }		 
    it->second->granted = true; //Take the lock
    (it->second->m)->unlock();

  } else {  //New lock

  locksStatus_* ls = (locksStatus_*)malloc(sizeof(locksStatus_));
  ls->granted = true; 
  ls->m = new Mutex();
  ls->cv = new ConditionVar();
  locks_.insert(pair<lock_protocol::lockid_t, locksStatus_*> (lid, ls)); //Grant the lock
  pthread_mutex_unlock(&mutex_);
  }
  lock_protocol::status ret = lock_protocol::OK;
  r = nacquire;
  return ret;
}

lock_protocol::status 
lock_server::release(int clt, lock_protocol::lockid_t lid, int& r) {

  lock_protocol::status ret;
  it = locks_.find(lid);
  if(it!= locks_.end()) {
    ret = lock_protocol::OK;
    (it->second->m)->lock();
    it->second->granted = false; // Release the lock
    (it->second->cv)->signal();
    (it->second->m)->unlock();
  } else {
    ret = lock_protocol::RPCERR;
  }

  r = nacquire;
  return ret;
}

