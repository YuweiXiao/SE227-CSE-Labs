// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::yfs_client()
{
    ec = new extent_client();

}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
    ec = new extent_client();
    printf("yfs_client::yfs_client::init root dir\n");
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}


void
yfs_client::appendDirContent(std::string& content, std::string &name, inum &ino_out) {
    content = content + name + '*' + filename(ino_out) + ' ';
}

void
yfs_client::appendDirContent(std::string& content, const char* name, inum & ino_out) {
    std::string filename = std::string(name);
    appendDirContent(content, filename, ino_out);
}


bool
yfs_client::isfile(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is not a file\n", inum);
    return false;
}
/** Your code here for Lab...
 * You may need to add routines such as
 * readlink, issymlink here to implement symbolic link.
 * 
 * */

bool
yfs_client::isdir(inum inum)
{
    extent_protocol::attr a;

    if (ec->getattr(inum, a) != extent_protocol::OK) {
        printf("error getting attr\n");
        return false;
    }

    if (a.type == extent_protocol::T_DIR) {
        printf("isfile: %lld is a dir\n", inum);
        return true;
    } 
    return false;
    // // Oops! is this still correct when you implement symlink?
    // return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    extent_protocol::attr attribute;
    std::string content;
    int r = ec->getattr(ino, attribute);
    if(r != OK) {
        printf("yfs_client::setattr::get attr error: inum:%u\n", ino);
        return r;
    }
    r = ec->get(ino, content);
    if(r != OK) {
        printf("yfs_client::setattr::get content error: inum:%u\n", ino);
        return r;
    }
    if(size == attribute.size) {
        return r;
    } else if(size > attribute.size) {
        content = content + std::string(size - attribute.size, '\0');
    } else if(size < attribute.size){
        content = content.substr(0, size);
    }
    r = ec->put(ino, content);
    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    
     // check whether filename already exists.
    bool found = false;
    inum tmpinum;
    int r = lookup(parent, name, found, tmpinum);
    if( r != OK ) {
        printf("yfs_client::create::lookup error\n");
        return r;
    }
    if( found ) {
        std::cout<<"yfs_client::create::filename already exists"<<std::endl;
        return r = extent_protocol::IOERR;
    }

    // create file
    std::cout<<"yfs_client::create::mode:"<<mode<<";S_IFLNK:"<<S_IFLNK<<std::endl;
    if(mode == S_IFLNK) {
        printf("yfs_client::create::symbolic create\n");
        ec->create(extent_protocol::T_SYMLNK, ino_out);
    } else {
        ec->create(extent_protocol::T_FILE, ino_out);
    }
    
    // change parent content
    std::string dirContent;
    r = ec->get(parent, dirContent);
    if( r != OK ) {
        return r;
    }
    appendDirContent(dirContent, name, ino_out);
    // dirContent = dirContent + std::string(name) + '*' + filename(ino_out) + ' ';
    std::cout<<"yfs_client::create:dir content:"<<dirContent<<std::endl;
    r = ec->put(parent, dirContent);
    
    return r;
}

int
yfs_client::mkdir(inum parent, const char *name, mode_t mode, inum &ino_out)
{
    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if directory exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    bool found = false;
    int r = lookup(parent, name, found, ino_out);
    if(r != OK || found) {
        printf("yfs_client::mkdir::error\n");
        return r;
    }
    std::string dirContent;
    r = ec->get(parent, dirContent);

    r = ec->create(extent_protocol::T_DIR, ino_out);
    if(r != OK) {
        printf("yfs_client::mkdir::create dir error\n");
        return r;
    }

    r = ec->get(parent, dirContent);
    if(r != OK) {
        printf("yfs_client::mkdir::get parent dir error\n");
        return r;
    }

    appendDirContent(dirContent, name, ino_out);
    // dirContent = dirContent + std::string(name) + '*' + filename(ino_out) + ' ';
    std::cout<<"yfs_client::create:dir content"<<dirContent<<std::endl;
    r = ec->put(parent, dirContent);

    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    std::list<dirent> list;
    int r = readdir(parent, list);
    if(r != OK) {
        return r;
    }

    std::list<dirent>::iterator it = list.begin();
    while(it != list.end()) {
        struct dirent t = *it;
        if(t.name.compare(name) == 0) {
            found = true;
            ino_out = t.inum;
            break;
        }
        it++;
    }
    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    std::string dirContent;
    std::cout<<"yfs_client::readdir::inum:"<<dir<<std::endl;
    int r = ec->get(dir, dirContent);
    if(r != extent_protocol::OK) {
        printf("yfs_client::readdir::read dir error\n");
        return r;
    }
    std::cout<<"yfs_client::readdir::dircontent:"<<dirContent<<std::endl;
    size_t p = 0;
    while(p != dirContent.size()) {
        std::string name;
        struct dirent entry;
        while(dirContent[p] != '*' && p != dirContent.size()) {
            name += dirContent[p];
            p++;
        }
        p++;
        if(p >= dirContent.size()) {
            std::cout<<name<<std::endl;
            printf("yfs_client::readdir::error dir format\n");
            return extent_protocol::IOERR;
        }
        entry.name = name;
        size_t num_pos = dirContent.find(' ', p);
        entry.inum = n2i(dirContent.substr(p, num_pos - p));
        p = num_pos + 1;
        std::cout<<"yfs_client::readdir::filename:"<<name<<";inum:"<<entry.inum<<std::endl;
        list.push_back(entry);
    }

    // std::istringstream in(dirContent);
    
    // std::cout<<"yfs_client<<readdir::istream dircount:"<<dirContent.size()<<";count:"<<in.gcount()<<std::endl;
    // while(!in.eof()) {
    //     std::string name;
        
        
    //     entry.name = name;
    //     in>>entry.inum;
    //     std::cout<<"yfs_client::readdir::filename:"<<name<<";inum:"<<entry.inum<<std::endl;
    //     list.push_back(entry);
    // }
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */
    std::cout<<"yfs_client::read:inum:"<<ino<<";size:"<<size<<";off:"<<off<<std::endl;
    std::string content;
    struct fileinfo fileInfo;
    getfile(ino, fileInfo);
    int r = ec->get(ino, content);
    if( r != OK ) {
        std::cout<<"yfs_client::read::read content error"<<std::endl;
        return r;
    }
    if(off >= fileInfo.size) {
        data = "";
        std::cout<<"yfs_client:read::data:offset is bigger than size"<<std::endl;
    } else {
        // if(size == -1) {
        //     size = fileInfo.size;
        // }
        data = content.substr(off, size);
        std::cout<<"yfs_client:read::data:"<<data<<std::endl;
    }
    
    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    /*
     * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
    std::cout<<"yfs_client::write::data:inum:"<<ino<<";size:"<<size<<";off:"<<off<<std::endl;
    struct fileinfo fileInfo;
    std::string content;
    getfile(ino, fileInfo);
    std::cout<<"yfs_client::write::size of original file:"<<fileInfo.size<<std::endl;
    int r = ec->get(ino, content);
    if( r != OK ) {
        std::cout<<"yfs_client::read::read content error"<<std::endl;
        return r;
    }
    // size_t newSize = fileInfo.size > off + size ? off + size : fileInfo.size
    std::cout<<off-fileInfo.size<<std::endl;
    if( fileInfo.size < off ) {
        content += std::string(off - fileInfo.size, '\0');
    }
    std::string newContent = content.substr(0, off) + std::string().assign(data, size);
    if(off + size < fileInfo.size) {
        newContent += content.substr(off + size);
    }
    
    r = ec->put(ino, newContent);
    std::cout<<"yfs_client::write::data::new content size:"<<newContent.size()<<std::endl;
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */

    bool found;
    inum ino_out;
    int r = lookup(parent, name, found, ino_out);
    if( r != OK || !found ) {
        printf("yfs_client::unlink::error\n");
        return r;
    }

    std::string filename = std::string(name);    
    std::list<dirent> list;
    readdir(parent, list);
    std::string newDirContent;
    std::list<dirent>::iterator it = list.begin();
    while(it != list.end()) {
        struct dirent tmp = *it;
        if(tmp.name != filename) {
            appendDirContent(newDirContent, tmp.name, tmp.inum);
        }
        it++;
    }
    r = ec->put(parent, newDirContent);
    r = ec->remove(ino_out);
    return r;
}

