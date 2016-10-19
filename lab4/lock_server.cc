// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0), 
  con_lock(PTHREAD_COND_INITIALIZER), 
  mutex(PTHREAD_MUTEX_INITIALIZER)
{
  printf("in constructor. lock_server.\n");
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
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  printf("in server acquire. %d\n", clt);
  fflush(stdout);
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab4 code goes here
  // while(true) {
  //   int t = pthread_cond_wait(&con_lock, &mutex);
  //   if( t == 0 )
  //     break;
  // }
  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab4 code goes here
  pthread_mutex_unlock(&mutex);
  pthread_cond_signal(&con_lock); 
  return ret;
}
