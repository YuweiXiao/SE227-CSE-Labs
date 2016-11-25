// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include <pthread.h>
#include <map>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"

enum LOCK_STATUS{
    LOCKED,
    UNLOCKED
  };

class lock_server {
private: 
  static std::map<lock_protocol::lockid_t, LOCK_STATUS> lockMap;
  static pthread_cond_t cond_lock;
  static pthread_mutex_t mutex;

protected:
  int nacquire;

public:
  lock_server();
  ~lock_server() {};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 







