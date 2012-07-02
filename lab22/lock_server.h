// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include <map>

using std::map;

class Mutex {
public:
  Mutex()         { pthread_mutex_init(&m_, NULL); }
  ~Mutex()        { pthread_mutex_destroy(&m_); }

  void lock()     { pthread_mutex_lock(&m_); }
  void unlock()   { pthread_mutex_unlock(&m_); }

private:
  friend class ConditionVar;

  pthread_mutex_t m_;

  // Non-copyable, non-assignable
  Mutex(Mutex &);
  Mutex& operator=(Mutex&);
};


class ConditionVar {
public:
  ConditionVar()          { pthread_cond_init(&cv_, NULL); }
  ~ConditionVar()         { pthread_cond_destroy(&cv_); }

  void wait(Mutex* mutex) { pthread_cond_wait(&cv_, &(mutex->m_)); }
  void signal()           { pthread_cond_signal(&cv_); }
  void signalAll()        { pthread_cond_broadcast(&cv_); }

  void timedWait(Mutex* mutex, const struct timespec* timeout) {
    pthread_cond_timedwait(&cv_, &(mutex->m_), timeout);
  }

private:
  pthread_cond_t cv_;

  // Non-copyable, non-assignable
  ConditionVar(ConditionVar&);
  ConditionVar& operator=(ConditionVar&);
};

class lock_server {
 private:
  struct locksStatus_ {
    locksStatus_() {
      granted = false;
    }
    bool granted;
    Mutex* m;
    ConditionVar* cv;
  };

  map<lock_protocol::lockid_t, locksStatus_*> locks_; 
  map<lock_protocol::lockid_t, locksStatus_*>::iterator it;
  pthread_mutex_t mutex_;
 protected:
  int nacquire;
  
 public:
  lock_server();
  ~lock_server();
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);

};

#endif 







