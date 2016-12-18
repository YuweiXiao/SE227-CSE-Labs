// inode layer interface.

#ifndef inode_h
#define inode_h

#include <stdint.h>
#include "extent_protocol.h"
#include "gettime.h"

#define DISK_SIZE  1024*1024*16
#define BLOCK_SIZE 512
#define BLOCK_NUM  (DISK_SIZE/BLOCK_SIZE)

typedef uint32_t blockid_t;

// disk layer -----------------------------------------

class disk {
 private:
  unsigned char blocks[BLOCK_NUM][BLOCK_SIZE];

 public:
  disk();
  void read_block(uint32_t id, char *buf);
  void write_block(uint32_t id, const char *buf);
};

// block layer -----------------------------------------

typedef struct superblock {
  uint32_t size;
  uint32_t nblocks;
  uint32_t ninodes;
} superblock_t;

class block_manager {
 private:
  disk *d;
  std::map <uint32_t, int> using_blocks;
  static pthread_mutex_t mutex;
  
 public:
  block_manager();
  struct superblock sb;

  uint32_t alloc_block();
  void free_block(uint32_t id);
  void read_block(uint32_t id, char *buf);
  void write_block(uint32_t id, const char *buf);
};

// inode layer -----------------------------------------

#define INODE_NUM  1024

// Inodes per block.
#define IPB           1
//(BLOCK_SIZE / sizeof(struct inode))

// Block containing inode i
#define IBLOCK(i, nblocks)     ((nblocks)/BPB + (i)/IPB + 3)

// Bitmap bits per block
#define BPB           (BLOCK_SIZE*8)

// Block containing bit for block b
#define BBLOCK(b) ((b)/BPB + 2)

#define NDIRECT 32
#define NINDIRECT (BLOCK_SIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

#define BIT_STATE(c, p) ((c>>p)&1)
// get the index of the byte which store the usage info of block id
#define BLOCK_BITMAP_INDEX(id) (((id) % BPB) / 8)
// get the offset of block id in the byte which store the usage info of it
#define BLOCK_BITMAP_OFFSET(id) (7 - ((id) % BPB) % 8)

typedef struct inode {
  //short type;
  unsigned int type;
  unsigned int size;
  unsigned int atime;
  unsigned int mtime;
  unsigned int ctime;
  mode_t mode;
  unsigned short uid;
  unsigned short gid;
  blockid_t blocks[NDIRECT+1];   // Data block addresses
} inode_t;

class inode_manager {
 private:
  block_manager *bm;
  struct inode* get_inode(uint32_t inum);
  void put_inode(uint32_t inum, struct inode *ino);
  unsigned int getTime();


 public:
  inode_manager();
  uint32_t alloc_inode(uint32_t type, unsigned short uid = 0, unsigned short gid = 777, mode_t mode = 666);
  void free_inode(uint32_t inum);
  void read_file(uint32_t inum, char **buf, int *size);
  void write_file(uint32_t inum, const char *buf, int size);
  void remove_file(uint32_t inum);
  void getattr(uint32_t inum, extent_protocol::attr &a);
  void setattr(uint32_t inum, extent_protocol::attr &a);
};

#endif

