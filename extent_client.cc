// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// The calls assume that the caller holds a lock on the extent

extent_client::extent_client(std::string dst)
{  	  																	
  sockaddr_in dstsock;						
  make_sockaddr(dst.c_str(), &dstsock);					
  cl = new rpcc(dstsock);				
  if (cl->bind() != 0) {                   						
    printf("extent_client: bind failed\n");			
  }															  
}      			

extent_protocol::status 
extent_client::fileGetter(extent_protocol::extentid_t eid,
                                      std::string &buf,extent_protocol::attr &a) 
{   					
   extent_protocol::status ret = extent_protocol::OK;
   fileCacheLock.lock();       										
     if(fileCache.find(eid) != fileCache.end()) {  //Extent is Cached		
   										
	if(fileCache[eid].exist_in_server == true  /*If this is false then u checked previously and file was not there*/
	     && fileCache[eid].removed == false)  {  /* You shouldn't have removed the file */
							 	   
	    buf = fileCache[eid].content;  				   			                                			    
	    fileCache[eid].fileAttr.atime = time(NULL); //Because you are not pushing the 
            a.atime = fileCache[eid].fileAttr.atime;    													  a.mtime = fileCache[eid].fileAttr.mtime;	
            a.ctime = fileCache[eid].fileAttr.ctime;	
            a.size  = fileCache[eid].fileAttr.size;	    			    

	    //Key Point
	    fileCache[eid].isDirty = true; // You are changing access time. 
		
	 } else {				
//	   fileCacheLock.unlock();
//	   return extent_protocol::NOENT; 					
           ret = extent_protocol::NOENT;
	 }
								 							                   
     } else {                                          
	 extent_protocol::status ret = cl->call(extent_protocol::get, eid, buf);
         if(ret == extent_protocol::OK) {
	   fileCache[eid].exist_in_server = true;					
	 } else if (ret == extent_protocol::NOENT) {
	    fileCache[eid].exist_in_server = false; //Not in Server
	 }


	//Get the attributes
       	ret = cl->call(extent_protocol::getattr, eid, a);
		
        //Now Set Other Attributes
         fileCache[eid].removed = false;
         fileCache[eid].isDirty = false;
	 fileCache[eid].content = buf;	

	 fileCache[eid].fileAttr.atime = a.atime;
	 fileCache[eid].fileAttr.mtime = a.mtime;
	 fileCache[eid].fileAttr.ctime = a.ctime;
	 fileCache[eid].fileAttr.size  = a.size;
     }              

     fileCacheLock.unlock();
     return ret;	
}




extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
 // ret = cl->call(extent_protocol::get, eid, buf);
  extent_protocol::attr attributes; //Dummy
  ret = fileGetter(eid, buf, attributes);
  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
//  ret = cl->call(extent_protocol::getattr, eid, attr);
  std::string buf;
  ret = fileGetter(eid, buf, attr);
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{										
				
  fileCacheLock.lock();							
  extent_protocol::status ret = extent_protocol::OK;						
//  int r;							
										
  time_t now = time(NULL);							

  fileDetails fd;
  fd.content = buf;
  fd.exist_in_server = true; //Get on this client should see this put even though not push.
  fd.removed = false; 
  fd.isDirty = true;  //We need this to be able to know if to flush this to server or not.

  fd.fileAttr.atime = now;
  fd.fileAttr.mtime = now;
  fd.fileAttr.ctime = now;
  fd.fileAttr.size  = buf.size();  

  fileCache[eid] = fd;

  fileCacheLock.unlock();

//  ret = cl->call(extent_protocol::put, eid, buf, r);
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  fileCacheLock.lock();
  extent_protocol::status ret = extent_protocol::OK;
 
  fileDetails fd;
  fd.removed = true;  
  fd.isDirty = true;
  fileCache[eid] = fd;
  
 // int r;
//  ret = cl->call(extent_protocol::remove, eid, r);
  fileCacheLock.unlock();
  return ret;
}

extent_protocol::status 
extent_client::flush(extent_protocol::extentid_t eid) 
{
  fileCacheLock.lock();
  extent_protocol::status ret = extent_protocol::OK;
  std::cout << "Push:" << std::endl;
  if(fileCache[eid].isDirty) 
  {
	int r;
	if(fileCache[eid].removed) { //Yes the file needs to be removed.
	   std::cout << "Calling Remove:" << std::endl;
	   ret = cl->call(extent_protocol::remove, eid, r);	  	
//	   assert(ret == extent_protocol::OK);
	} else { //Dirty but not removed.Then probably a new put or get changed attributes
	   
	  //First Push
       	  ret = cl->call(extent_protocol::put, eid, fileCache[eid].content, r);
	  assert(ret == extent_protocol::OK);

	}
  }     
	
  fileCache.erase(eid); //Erase the entry;Client has done everything possible with this entry.
  fileCacheLock.unlock();

  return ret;

}

