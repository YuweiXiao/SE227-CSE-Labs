#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
  /*if id is smaller than 0 or larger than BLOCK_NUM 
   *or buf is null, just return.
   */
  if(id < 0 || id >= BLOCK_NUM || buf == NULL) {
    return;
  }
  /*put the content of target block into buf.*/
  memcpy(buf, blocks[id], BLOCK_SIZE);
}

void
disk::write_block(blockid_t id, const char *buf)
{
  /*if id is smaller than 0 or larger than BLOCK_NUM 
   *or buf is null, just return.
   */
  if(id < 0 || id >= BLOCK_NUM || buf == NULL) {
    return;
  }
  /*put the content of target block into buf.*/
  memcpy(blocks[id], buf, BLOCK_SIZE);
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your lab1 code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.

   *hint: use macro IBLOCK and BBLOCK.
          use bit operation.
          remind yourself of the layout of disk.
   */
  char buf[BLOCK_SIZE];
  // find the start block index for files
  // start_index = super_block number + bitmap block number + inode block number
  blockid_t bnum = 2 + sb.nblocks / BPB + sb.ninodes / IPB;
  while(bnum < sb.nblocks) {
    read_block(BBLOCK(bnum), buf);
    uint32_t index = BLOCK_BITMAP_INDEX(bnum);
    uint32_t offset = BLOCK_BITMAP_OFFSET(bnum); 
    if(BIT_STATE(buf[index], offset) == 0) {
      buf[index] = (buf[index] | (1 << offset));
      write_block(BBLOCK(bnum), buf);
      return bnum;
    }
    bnum++;
  }
  printf("block_manager::alloc_block::no more storage.\n");
  return -1;
}

// used for test
void printBit(char c) {
  for(int i = 7; i >= 0; --i){
    if(c & (1<<i))
      printf("1");
    else 
      printf("0");
  }
  printf("\n");
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your lab1 code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
  if(id < 0 || id > sb.nblocks) {
    printf("block_manager::free_bloc::block id is out of range:%u\n", id);
    return;
  }
  
  char buf[BLOCK_SIZE];

  // unmark bitmap
  read_block(BBLOCK(id), buf);
  uint32_t index = BLOCK_BITMAP_INDEX(id);
  uint32_t offset = BLOCK_BITMAP_OFFSET(id);
  buf[index] = (buf[index] & (~(1 << offset)));
  // update bitmap
  write_block(BBLOCK(id), buf);
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;

}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
  /* 
   * your lab1 code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
  
   * if you get some heap memory, do not forget to free it.
   */
  
  //TODO, type check

  struct inode *ino_disk;
  char buf[BLOCK_SIZE];

  // start from 1 root_dir
  int inum = 1;
  while(inum <= INODE_NUM) {
    bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
    ino_disk = (struct inode*)buf + inum%IPB;
    if(ino_disk->type == 0) {
      // TODO: update xtime
      ino_disk->type = type;
      bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
      return inum;
    }
    inum++;
  }

  // no more inode available.
  printf("no block for inode to allocate\n");
  return -1;
  // find free block for new inode to allocate
  // bm->read_block(BBLOCK(0), buf);
  // int p = 1;
  // int inum = -1;
  // for(int i = 0; i < BLOCK_SIZE && p <= INODE_NUM && inum == -1; ++i) {
  //   for(int j = 7; j >= 0 && p <= INODE_NUM && inum == -1; --j) {
  //     if(BIT_STATE(buf[i], j) == 0) {
  //       inum = p;
  //       buf[i] = buf[i] | (1<<j);
  //       break;
  //     } 
  //     p++;
  //   }
  // }
  // // no free block for new inode
  // if(inum == -1) {
  //   printf("no block for inode to allocate\n");
  //   return -1;
  // }
  
  // // update bitmap
  // bm->write_block(BBLOCK(0), buf);

  // // get the inode block, update attr.  
  // // TODO: update xtime
  // bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // ino_disk = (struct inode*)buf + inum%IPB;
  // ino_disk->type = type;
  // bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // return inum;
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your lab1 code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   * do not forget to free memory if necessary.
   */
  if (inum < 0 || inum >= bm->sb.ninodes) {
    printf("inode_manager::free_inode: inum out of range:%u\n", inum);
    return ;
  }

  // clear block data
  char buf[BLOCK_SIZE];
  memset(buf, 0, BLOCK_SIZE);
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // call free block to free inode block, unmark bitmap
  bm->free_block(IBLOCK(inum, bm->sb.nblocks));
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  printf("\tim: get_inode %d\n", inum);
  
  if (inum < 0 || inum >= bm->sb.ninodes) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum, bm->sb.nblocks), buf);
}

#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your lab1 code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_out
   */
  
  // printf("inode_manager::read_file : inum:%u  size:%d\n", inum, *size);
  if(inum < 0 || inum > bm->sb.ninodes || buf_out == NULL || size ==NULL || *size < 0) {
    printf("inode_manager::read_file::parameters error\n");
    return;
  }

  // inode buf, block buf
  char inodeBuf[BLOCK_SIZE], blockBuf[BLOCK_SIZE];
  struct inode *ino_disk;
  
  // get inode
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), inodeBuf);
  ino_disk = (struct inode*)inodeBuf + inum % IPB;
  
  // get correct size, initialize content buffer
  if(*size == 0) {
    *size = ino_disk->size;
  }
  *size = MIN((uint32_t)*size, ino_disk->size);
  char *content = (char*)malloc(sizeof(char)*(*size));
  // printf("inode_manager::read_file : file-size:%u\n", ino_disk->size);

  // p : current content pointer, 
  int p = 0;

  // get block content according to direct blocks
  for(int i = 0; i < NDIRECT && p < *size; ++i) {
    bm->read_block(ino_disk->blocks[i], blockBuf);
    // printf("inode_manager::read_file: bnum:%u\n", ino_disk->blocks[i]);
    int tSize = MIN(BLOCK_SIZE, *size - p);
    memcpy(content + p, blockBuf, tSize);
    p += tSize;
  }

  // if there is more content in indirect blocks
  if(p < *size) {
    // get the indirect block
    char indirectINode[BLOCK_SIZE];
    bm->read_block(ino_disk->blocks[NDIRECT], indirectINode);
    uint *blocks = (uint*)indirectINode;

    for(uint32_t i = 0; i < NINDIRECT && p < *size; ++i) {
      bm->read_block( blocks[i], blockBuf);
      int tSize = MIN(BLOCK_SIZE, *size - p);
      memcpy(content + p, blockBuf, tSize);
      p += tSize;
    }
  }

  // set return value
  *buf_out = content;
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *buf, int size)
{
  /*
   * your lab1 code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode.
   * you should free some blocks if necessary.
   */
  // printf("inode_manager::write_file, inum:%u - size :%d\n", inum, size);
  
  if(inum < 0 || inum > bm->sb.ninodes || buf == NULL || size < 0) {
    printf("inode_manager::write_file:error parameters\n");
    return;
  }
  if((uint32_t)size > MAXFILE * BLOCK_SIZE) {
    printf("inode_manager::write_file:file to write is too big, size:%d\n", size);
    return; 
  }

  char inodeBuf[BLOCK_SIZE], blockBuf[BLOCK_SIZE];
  struct inode *ino_disk;
  
  // get inode
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), inodeBuf);
  ino_disk = (struct inode*)inodeBuf + inum % IPB;
  
  int originBlockNum = (ino_disk->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
  if(ino_disk->size == 0) {
    originBlockNum = 0;
  }
  // p : current content pointer, 
  int p = 0;
  
  // store content into direct blocks
  int i = 0;
  blockid_t bnum;
  for(; i < NDIRECT && p < size; ++i) {
    int tSize = MIN(BLOCK_SIZE, size - p);
    memcpy(blockBuf, buf + p, tSize);
    if(i < originBlockNum) {
      bnum = ino_disk->blocks[i];
    } else {
      bnum = bm->alloc_block();
      ino_disk->blocks[i] = bnum;  
    }
    // printf("inode_manager::write_file: write block bnum:%d \n", bnum);
    bm->write_block(bnum, blockBuf);
    p = p + tSize;
  }
  // if there is more content to write, store in indirect blocks
  if(p < size) {
    if(i > originBlockNum) {
      ino_disk->blocks[NDIRECT] = bm->alloc_block();
    }
    
    // get the indirect block
    char indirectINode[BLOCK_SIZE];
    bm->read_block(ino_disk->blocks[NDIRECT], indirectINode);
    uint *blocks = (uint*)indirectINode;
    uint32_t index = 0;
    while(p < size) {
      int tSize = MIN(BLOCK_SIZE, size - p);
      memcpy(blockBuf, buf + p, tSize);
      if(i < originBlockNum) {
        bnum = blocks[index];
      } else {
        bnum = bm->alloc_block();
        blocks[index] = bnum;
      }
      bm->write_block(bnum, blockBuf);
      p = p + tSize;
      index++;
      i++;
    }
    bm->write_block(ino_disk->blocks[NDIRECT], indirectINode);
  }
  //free unused block
  int blockCount = i + 1;
  if(size == 0) {
    blockCount = 0;
  }
  if(originBlockNum > blockCount) {
    // free direct blocks
    for(int j = blockCount; j < NDIRECT && j < originBlockNum; ++j) {
      bm->free_block(ino_disk->blocks[j]);
    }
    // free indirect blocks
    if(originBlockNum >= NDIRECT) {
      char indirectINode[BLOCK_SIZE];
      bm->read_block(ino_disk->blocks[NDIRECT], indirectINode);
      uint *blocks = (uint*)indirectINode;
      int j = blockCount >= NDIRECT ? blockCount - NDIRECT + 1 : NDIRECT - NDIRECT;
      while(NDIRECT + j < originBlockNum) {
        bm->free_block(blocks[j]);
        j++;
      }
      if(blockCount < NDIRECT) {
        bm->free_block(ino_disk->blocks[NDIRECT]);
      }
    }
  }

  ino_disk->size = size;
  // bm->write_block(IBLOCK(inum), ino_disk);
  put_inode(inum, ino_disk);
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
  /*
   * your lab1 code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
  if(inum < 0 || inum > INODE_NUM) {
    printf("inode_manager::getattr:: inum is out of range: %u\n", inum);
    return;
  }

  // get inode, can not use get_inode(), the test program will crash because it may return NULL.
  char buf[BLOCK_SIZE];
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), buf);
  struct inode* t = (struct inode*)buf + inum%IPB;
  a.type = t->type;
  a.atime = t->atime; 
  a.mtime = t->mtime;
  a.ctime = t->ctime;
  a.size = t->size;
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   * do not forget to free memory if necessary.
   */
  
  // get inode
  char inodeBuf[BLOCK_SIZE];
  struct inode* ino_disk;
  bm->read_block(IBLOCK(inum, bm->sb.nblocks), inodeBuf);
  ino_disk = (struct inode*)inodeBuf + inum%IPB;
  // printf("inode_manager::remove_file: inum:%u\n", inum);

  int fileSize = ino_disk->size;
  int p = 0;

  // free inode first, for sake of security.
  free_inode(inum);
  
  for(int i = 0; i < NDIRECT && p < fileSize; ++i) {
    // printf("%inode_manager::remove_file: bnum:%d\n", ino_disk->blocks[i]);
    bm->free_block(ino_disk->blocks[i]);
    p += BLOCK_SIZE;
  }

  // there are more data to release
  if( p < fileSize) {
    // get the indirect block
    char indirectINode[BLOCK_SIZE];
    bm->read_block(ino_disk->blocks[NDIRECT], indirectINode);
    // need to free the indirect inode first, for sake of security
    bm->free_block(ino_disk->blocks[NDIRECT]);

    uint* blocks = (uint *)indirectINode;
    int index = 0;
    while(p < fileSize) {
      bm->free_block(blocks[index]);
      index++;
      p += BLOCK_SIZE;
    }
  }
}
