// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include "extent_protocol.h"
#include "extent_server.h"

class extent_client {
 private:
  rpcc *cl;
  extent_protocol::AccessControl ac;
  std::string username;
  int getGroup(unsigned int uid, unsigned int gid);

 public:
  extent_client(std::string dst, std::string name, unsigned short uid, unsigned short gid);
  extent_protocol::status create(uint32_t type, extent_protocol::extentid_t partent, extent_protocol::extentid_t &eid, mode_t mode);
  extent_protocol::status get(extent_protocol::extentid_t eid, 
			                        std::string &buf, bool forwrite);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				                          extent_protocol::attr &a);
  extent_protocol::status setattr(extent_protocol::extentid_t eid, 
                                  extent_protocol::attr a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);
};

#endif 

