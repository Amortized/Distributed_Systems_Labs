// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <set>

static void *
releasethread(void *x)
{                            
  lock_client_cache *cc = (lock_client_cache *) x;
  cc->releaser();
  return 0;
}              

int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{                                    
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // assert(gethostname(hname, 100) == 0);
	  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);

  /* register RPC handlers with rlsrpc */
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry);

  /* register RPC handlers with rlsrpc */
  int r = pthread_create(&th, NULL, &releasethread, (void *) this);
  assert (r == 0);

  int ret = cl->call(lock_protocol::subscribe, cl->id(), rlock_port,r);
  assert (ret == lock_protocol::OK);
  

}


lock_client_cache::~lock_client_cache()

{
  //cout << "~lock_client_cache enter" << endl;
  rvk_cv.signal();
  rtr_cv.signal();
  pthread_join(th, NULL);
  int r;
  int ret = cl->call(lock_protocol::unsubscribe, cl->id(), r);
  assert(ret == lock_protocol::OK);
  if (ret == lock_protocol::OK)
  {
    map<lock_protocol::lockid_t, lockids*>::iterator it;
    NewScopedLock l(&m);
    for (it = lockMap.begin(); it != lockMap.end(); it++)
    {
      lock_protocol::lockid_t lid = it->first;
      lockids *lc = it->second;
      if (lc->lstatus != NONE)
      {
        while (lock_protocol::OK !=
               cl->call(lock_protocol::release, cl->id(), lid, r));
        lc->lstatus = NONE;
      }
    }
  }
}



void
lock_client_cache::releaser()
{


  // This method should be a continuous loop, waiting to be notified of
  // freed locks that have been revoked by the server, so that it can
  // send a release RPC.

  while(true) 
  {
    m.lock();
    while(revokeMap.empty()) { //Nothing to be Released	    
      rvk_cv.wait(&m);       
    }                   
 //Just take a Snapshot of locks to be released. Release the lock so revoke and retry are non blocking
    
   map<lock_protocol::lockid_t, unsigned int> tempRevokeMap = revokeMap;
   m.unlock(); //Release the lock

   //Push the Release Calls
   map<lock_protocol::lockid_t, unsigned int>::iterator it;
   for(it = tempRevokeMap.begin(); it!= tempRevokeMap.end(); it++) {  

    m.lock(); //Get the Lock; This will be done everytime for each release
    lockids* lc = lockMap[it->first];   //Lets Check the Status of this lock   

    if(lc->seq_num == it->second /*Will send release only for which client is supposed to send it */  && lc->lstatus == RELEASING /*Now this can be called if all threads have got the lock and some of them get the lock */) {       

 m.unlock(); //Release the lock. DON'T HOLD THE LOCK ACROSS RPC'S 
      if (lu != NULL)  //If there is realease call the function
        {
          lu->dorelease(it->first);
        } 

       //Make a RPC Call
       int r;
       int ret = cl->call(lock_protocol::release, cl->id(), it->first, lc->seq_num, r);
       assert (ret == lock_protocol::OK);

       //Grab the Lock to Change the Status
       m.lock(); 
        lc->lstatus = NONE; //Client Doesn't own the lock  
        lc->seq_num += 1; //Will be serving New Seq No
       cv.signal();

    } 
    m.unlock(); //Directly Release the lock

  }

  //Remove the old stuff from Revoke Map
  m.lock();
  for(it = revokeMap.begin(); it != revokeMap.end(); it++) {
  
    if(it->second < lockMap[it->first]->seq_num) {
       revokeMap.erase(it);
    }
  }
  m.unlock();

 }


/*
  while (true)
  {
    m.lock();
    while (revokeMap.empty())
    {
      rvk_cv.wait(&m);
    }
    map<lock_protocol::lockid_t, unsigned int> temp_revoke_map = revokeMap;
    m.unlock();
    std::set<lock_protocol::lockid_t> to_remove;
    for (map<lock_protocol::lockid_t, unsigned int>::iterator \
         it = temp_revoke_map.begin();
         it != temp_revoke_map.end();
         it++)
    {
      lock_protocol::lockid_t lid = it->first;
      unsigned int remote_seq_num = it->second;
      m.lock();
      lockids *lc = lockMap[lid];
      if (remote_seq_num == lc->seq_num && lc->lstatus == RELEASING)
      {
        int r;
        m.unlock();
        if (lu != NULL)
	        {
          lu->dorelease(lid);
        }
        int ret = cl->call(lock_protocol::release, cl->id(), lid, lc->seq_num, r);
        m.lock();
        assert(ret == lock_protocol::OK);
        lc->lstatus = NONE;
        lc->seq_num += 1;
        to_remove.insert(lid);
        cv.signal();
      }
      m.unlock();
    }
    m.lock();
    for (map<lock_protocol::lockid_t, unsigned int>::iterator \
         it = revokeMap.begin();
         it != revokeMap.end();
         it++)
    {
      if (it->second < lockMap[it->first]->seq_num)
      {
	        revokeMap.erase(it);
      }
    }
    m.unlock();
  }
*/


}


lock_protocol::status 
lock_client_cache::pingLock(lock_protocol::lockid_t lid, lockids* lc) {
  

  //This is awakened by 
  int r;
  int ret = cl->call(lock_protocol::acquire, cl->id(), lid, lc->seq_num, r);
  if(ret != lock_protocol::OK && ret != lock_protocol::RETRY) { 
    assert(false);
  }
  return ret;
}


lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{ 
  

  //Check if it Exists in LockMap; If yes get the status
  lockids* lc = NULL;
{
  m.lock();
    if(lockMap.find(lid) == lockMap.end()) {
       lc = (lockids*)malloc(sizeof(lockids));
       lc->lstatus = NONE;  //Client Does not own the lock yet
       lc->seq_num = 1; // New Seq No
       lockMap[lid] = lc;
       retryMap[lid] = false; //No Need to retry as of now
    }     
    lc = lockMap[lid];  
  m.unlock();
}
  while(true) {
   switch(lc->lstatus) {
     
       case NONE:  //Client Does not own the lock, Try to fetch it
       {       
	  //Step 1 : Check if another thread on the same client got the lock
	  bool owner = false;
	  {
           m.lock();
	     if(lc->lstatus == NONE) 
             {
		lc->lstatus = ACQUIRING;
		owner = true;
             }	

           m.unlock();
	  } 

	
	  if(owner) { //Only Then make a call to the server
	    lock_protocol::status ret = pingLock(lid,lc);
	    while(ret == lock_protocol::RETRY) {  //Lock was not Availaible
	      m.lock();	
		if(!retryMap[lid]) { //No Need to Retry Immediately
		  
		 struct timeval now;
		 struct timespec next_timeout;
		 gettimeofday(&now, NULL);
		 next_timeout.tv_sec = now.tv_sec + 3; //
                 next_timeout.tv_nsec = 0;
		
		 rtr_cv.timedWait(&m, &next_timeout);
	
		}
		retryMap[lid] = false;  //Need to do this because the lock was released
	      m.unlock();
	      ret = pingLock(lid,lc); //Beg the Server Again.
	   }

	   assert(ret == lock_protocol::OK); //LOCK WAS GRANTED
	   {
	      m.lock();
              lc->lstatus = LOCKED; 	
	      m.unlock();	
	   }
	   	 
	   return ret; //Acquire is Done successfully 	
	  }  

	break;
       } 

      case FREE:
      {
	bool owner = false;
	{
          m.lock();
	  if(lc->lstatus == FREE) { //Check if its still free
	    lc->lstatus = LOCKED;
	    owner = true;
	  }
	  m.unlock();
        }
	
	if(owner) {
	  return lock_protocol::OK;
	}
	
      break;	
    }
	
    default:
    {   m.lock();
	if(lc->lstatus == NONE || lc->lstatus == FREE) {
	  m.unlock();	
	  break;
	}
	
	cv.wait(&m);  //Wait for the lock status change from ACQUIRING, RELEASING AND LOCKED
	m.unlock();
	break;
    }

  }
}


/*
  lockids *lc = NULL;
  {
    //MyScopedLock l(&m);
    m.lock();
    if (lockMap.find(lid) == lockMap.end())
    {
      lc = (lockids *)malloc(sizeof(lockids));
      lc->lstatus = NONE;
      lc->seq_num = 1;
      lockMap[lid] = lc;
      retryMap[lid] = false;
    }
    lc = lockMap[lid];
    m.unlock();
  }

  while (true)
  {
    switch (lc->lstatus)
    {
      case NONE:
      {
        bool is_owner = false;
        {
          //MyScopedLock l(&m);
          m.lock();
          if (lc->lstatus == NONE)
            {
            lc->lstatus = ACQUIRING;
            is_owner = true;
          }
          m.unlock();
        }
        if (is_owner)
        {
          lock_protocol::status ret = pingLock(lid, lc);
          while (ret == lock_protocol::RETRY)
          {
            m.lock();
            if (!retryMap[lid])
            {
              struct timeval now;
              struct timespec next_timeout;
              gettimeofday(&now, NULL);
              next_timeout.tv_sec = now.tv_sec + 3;
              next_timeout.tv_nsec = 0;
              rtr_cv.timedWait(&m, &next_timeout);
              //rtr_cv.wait(&m);
            }
            retryMap[lid] = false;
            m.unlock();
            ret = pingLock(lid, lc);
          }
          assert(ret == lock_protocol::OK);
          {
            //MyScopedLock l(&m);
            m.lock();
            lc->lstatus = LOCKED;
            m.unlock();
          }
          return ret;
        }
        break;
      }
      case FREE:
      {
        bool is_owner = false;
        {
          //MyScopedLock l(&m);
          m.lock();
          if (lc->lstatus == FREE)
          {
            lc->lstatus = LOCKED;
            is_owner = true;
          }
          m.unlock();
        }
        if (is_owner)
        {
          return lock_protocol::OK;
        }
        break;
      }
      default:
        m.lock();
        if (lc->lstatus == NONE || lc->lstatus == FREE)
        {
          m.unlock();
          break;
        }
        if (lc->lstatus == LOCKED)
        {
        }
        else if (lc->lstatus == ACQUIRING)
        {
	        }
        else if (lc->lstatus == RELEASING)
        {
        }
        cv.wait(&m);
        m.unlock();
        break;
    }
  }
*/



}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
   m.lock();
   lockids* lc = lockMap[lid]; 
   assert(lc->lstatus == LOCKED);
   
//Check if its in the Revoke List. As soon as one thread releases a lock 
//it checks if the lock has been released if yes give the lock back immediately
// this will prevent other clients from starving


   if(revokeMap.find(lid) != revokeMap.end() && revokeMap[lid] == lc->seq_num)
   /*Make Sure the lock whose status is being changed/revoked is the one the client is considering. It should not happen that just because revoke came in late it should be done*/
    {
	lc->lstatus = RELEASING;     
        rvk_cv.signal(); //Wake up the release thread

    } 
    else {  //Don't Give the Lock Back; Just Change its status and awaken the acquire thread
        
        lc->lstatus = FREE;
        cv.signal();  //Wake up the acquire thread
    }
   m.unlock(); 
   return lock_protocol::OK;
}


lock_protocol::status 
lock_client_cache::revoke(lock_protocol::lockid_t lid, unsigned int seqNo, int &r) 
{ 

  m.lock();
  if( ( revokeMap.find(lid) == revokeMap.end() /* New Lock*/ 
      ||  seqNo > revokeMap[lid]  /*New Seq no for this lock   */   ) 
      && (lockMap[lid]->seq_num == seqNo ) /* Revoke is for lock which client owns(either free or locked) */)  {
     revokeMap[lid] = seqNo;  //Put this New Lock's Seq No;Note in Release() we only remove less than seqNo's
     lockids* lc = lockMap[lid];
     if(lc->lstatus == FREE) {  //If This Lock is Free; just call the release thread
        lc->lstatus = RELEASING;
        rvk_cv.signal();  //Wake up the release thread
     }  
  }        
  m.unlock();
  return lock_protocol::OK;  
} 


//To make it non blocking we just put them into a data structure like map
// wakeup the threads on client to handle it

lock_protocol::status 
lock_client_cache::retry(lock_protocol::lockid_t lid, int &) 
{         
   //We need to retry for this lock
  m.lock();
  retryMap[lid] = true;  //Put it in the datastructure and wake up the thread.
  rtr_cv.signal();  //Indirectly wake up the thread doing the acquire
  m.unlock();
  return lock_protocol::OK; 
}



