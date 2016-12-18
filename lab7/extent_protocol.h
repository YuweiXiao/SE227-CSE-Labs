// extent wire protocol

#ifndef extent_protocol_h
#define extent_protocol_h

#include "rpc.h"

class extent_protocol {
 public:
  typedef int status;
  typedef unsigned long long extentid_t;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, NOPEM };
  enum rpc_numbers {
    put = 0x6001,
    get,
    getattr,
    remove,
    create,
    setattr
    // init
  };

  enum types {
    T_DIR = 1,
    T_FILE,
    T_SYMLNK
  };

  struct attr {
    uint32_t type;
    // unsigned int atime;
    // unsigned int mtime;
    // unsigned int ctime;
    unsigned int size;
    mode_t mode;
    unsigned short uid;
    unsigned short gid;
  };

  struct AccessControl{
    unsigned short uid;
    unsigned short gid;
    AccessControl(unsigned short u = 0, unsigned short g = 777) {
      uid = u;
      gid = g;
    }
  };

  struct GroupInfo {
    std::string groupname;
    std::string pw;
    unsigned short gid;
    std::vector<std::string> members;
  };

  static extent_protocol::GroupInfo parseGroup(std::string line) {
    extent_protocol::GroupInfo info;
    std::string::size_type t = line.find(":", 0);
    info.groupname = line.substr(0, t);
    std::string::size_type n = line.find(":", t + 1);
    info.pw = line.substr(t + 1, n - t - 1);
    t = line.find(":", n + 1);
    info.gid = atoi(line.substr(n + 1, t - n -1).c_str());
    line = line.substr(t+1);
    while(!line.empty()) {
        t = line.find(",");
        if(t == std::string::npos) {
            info.members.push_back(line);
            break;
        } else {
            std::string m = line.substr(0, t-1);
            if(!m.empty());
                info.members.push_back(m);
        }
        line = line.substr(t+1);
    }
    return info;
}
};


inline unmarshall &
operator>>(unmarshall &u, extent_protocol::AccessControl &a)
{
  u >> a.uid;
  u >> a.gid;
  return u;
}

inline marshall &
operator<<(marshall &m, extent_protocol::AccessControl a)
{
  m << a.uid;
  m << a.gid;
  return m;
}

inline unmarshall &
operator>>(unmarshall &u, extent_protocol::attr &a)
{
  u >> a.type;
  // u >> a.atime;
  // u >> a.mtime;
  // u >> a.ctime;
  u >> a.size;
  u >> a.uid;
  u >> a.gid;
  u >> a.mode;
  printf("unmarshall >> extent_protocol::mode:%o\n", a.mode);
  return u;
}

inline marshall &
operator<<(marshall &m, extent_protocol::attr a)
{
  m << a.type;
  // m << a.atime;
  // m << a.mtime;
  // m << a.ctime;
  m << a.size;
  m << a.uid;
  m << a.gid;
  m << a.mode;
  printf("marshall << extent_protocol::mode:%o\n", a.mode);
  return m;
}

#endif 
