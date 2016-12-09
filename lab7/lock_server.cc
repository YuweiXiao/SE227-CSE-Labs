// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

pthread_cond_t lock_server::cond_lock = PTHREAD_COND_INITIALIZER;
pthread_mutex_t lock_server::mutex = PTHREAD_MUTEX_INITIALIZER;
std::map<lock_protocol::lockid_t, LOCK_STATUS> lock_server::lockMap;


lock_server::lock_server():
  nacquire (0)
{}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  printf("lock_server::acquire:%llu\n", lid);
  
  pthread_mutex_lock(&mutex);
  lock_protocol::status ret = lock_protocol::OK;
  if(lockMap.count(lid) == 0 || lockMap[lid] == UNLOCKED) {
    lockMap[lid] = LOCKED;
  } else {
    while(lockMap[lid] == LOCKED) {
      int t = pthread_cond_wait(&cond_lock, &mutex);  
      if( t != 0 ) {
        printf("condition lock error\n");
      }
    }
    lockMap[lid] = LOCKED;
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  // Your lab4 code goes here
  lock_protocol::status ret = lock_protocol::OK;
	printf("lock_server::release:%llu\n", lid);
   
  pthread_mutex_lock(&mutex); 
  lockMap[lid] = UNLOCKED;
  pthread_mutex_unlock(&mutex); 
  pthread_cond_signal(&cond_lock); 
  return ret;
}
