// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/x509v3.h>

yfs_client::yfs_client()
{
  ec = NULL;
  lc = NULL;
}

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst, const char* cert_file)
{
    int r = verify(cert_file, &uid);
    printf("usedid:%d\n", uid);
    if( r != OK ) {
        printf("error user CA.\n");
        assert(0);
    }
    ec = new extent_client(extent_dst, username, uid, gid);
    lc = new lock_client(lock_dst);
    if (ec->put(1, "") != extent_protocol::OK)
        printf("error init root dir\n"); // XYB: init root dir
}

int
yfs_client::verify(const char* name, unsigned short *uid)
{
  	int ret = OK;

    // read in certificate file.
    FILE *fp = fopen(name, "r");
    assert(fp);

    X509 *certificate = PEM_read_X509(fp, NULL, NULL, NULL);
    // not a valid CA
    if(certificate == NULL) {
        return ERRPEM;
    }


    // whether issused by trusted CA.
    FILE *trusted = fopen("./cert/ca.pem", "r");
    assert(trusted);
    X509 *trustedCA = PEM_read_X509(trusted, NULL, NULL, NULL);
    int raw = X509_check_issued(trustedCA, certificate);
    if( raw == 29 ) {
        return EINVA;
    } 

    // time validate
    // ASN1_TIME *not_before = X509_get_notBefore(certificate);
    // ASN1_TIME *not_after = X509_get_notAfter(certificate);
    // X509_STORE *xs = X509_STORE_new();
    // X509_STORE_CTX *ctx = X509_STORE_CTX_new();
    // X509_STORE_add_cert(xs, certificate);
    // X509_STORE_CTX_init(ctx, xs, certificate, NULL); 
    // X509_STORE_CTX_set_time()
    // X509_STORE_CTX_set_flags(ctx, X509_V_FLAG_USE_CHECK_TIME);
    // int status = X509_verify_cert(ctx);
    // status = X509_STORE_CTX_get_error(ctx);
    
    // EVP_PKEY * pubkey = X509_get_pubkey(trustedCA);
    // int status = X509_verify(certificate, pubkey);

    // time_t *ptime;
    // int i = X509_cmp_time(X509_get_notAfter(certificate), ptime);

    // printf("\nstatus:i:%d- ptime:%d\n", i, &ptime);


    // get common name and get uid from ect/group file set uid
    assert(certificate);
    X509_NAME *subj = X509_get_subject_name(certificate);
    assert(subj);
    int lastpos = -1;
    lastpos = X509_NAME_get_index_by_NID(subj, NID_commonName, lastpos);
    assert(lastpos != -1);
    X509_NAME_ENTRY *e = X509_NAME_get_entry(subj, lastpos);
    ASN1_STRING *d = X509_NAME_ENTRY_get_data(e);
    char *str = (char *)ASN1_STRING_data(d);
    // printf("common name :%s\n", str);
    std::string commonName = std::string(str);

    std::ifstream in("./etc/group");
    std::string t;
    bool flag = false;
    while(getline(in, t)) {
        std::string::size_type pos = t.find(":", 0);
        std::string name = t.substr(0, pos);
        // std::cout<<name<<std::endl;
        if(name == commonName) {
            flag = true;
            pos = t.find(":", pos + 1);
            std::string tid = t.substr(pos + 1, t.find(":", pos + 1) - pos - 1);
            // printf("get same name:%s\n", tid.c_str());
            // TODO  
            // this is group ID, maybe can pass test. the best way is to get uid from passwd
            *uid = (unsigned short)atoi(tid.c_str()); 
            this->gid = *uid; 
            username = name;
            break;
        }
    }
    if(!flag) {
        return ENUSE;
    }

	return ret;
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
    printf("yfs_client::isfile: %llu\n", inum);
    lc->acquire(inum);
    extent_protocol::attr a;
    bool flag = false;

    int r = ec->getattr(inum, a);
    if (r != extent_protocol::OK) {
        printf("error getting attr\n");
    } else if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        flag = true;
    } 

    lc->release(inum);
    return flag;
}


bool
yfs_client::isdir(inum inum)
{
    printf("yfs_client::isdir %llu\n", inum);
    lc->acquire(inum);
    bool flag = false;
    extent_protocol::attr a;
    int r = ec->getattr(inum, a);
    if ( r != extent_protocol::OK ) {
        printf("error getting attr\n");
    } else if (a.type == extent_protocol::T_DIR) {
        printf("yfs_client::isdir: %lld is a dir\n", inum);
        flag = true;
    } 
    lc->release(inum);
    return flag;
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    printf("yfs_client::getfile %llu\n", inum);
    lc->acquire(inum);
    int r = _getfile(inum, fin);
    lc->release(inum);
    return r;
}

int
yfs_client::_getfile(inum inum, fileinfo &fin)
{
    int r = OK;

    printf("yfs_client::_getfile %llu\n", inum);
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    fin.mode = a.mode;
    fin.uid = a.uid;
    fin.gid = a.gid;
    printf("yfs_client::_getfile %llu -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    printf("yfs_client::getdir: %llu\n", inum);
    lc->acquire(inum);

    int r = OK;
    extent_protocol::attr a;
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        r = IOERR;
        goto release;
    }
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;
    din.uid = a.uid;
    din.gid = a.gid;

release:
    lc->release(inum);
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)


int
yfs_client::setattr_atime(inum ino, unsigned long long time)
{
    int r = OK;
    return r;
}


// Only support set size of attr
int
yfs_client::setattr(inum ino, filestat st, int to_set)
{
    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    printf("yfs_client::setattr: %llu\n", ino);
    lc->acquire(ino);

    extent_protocol::attr attribute;
    std::string content;
    int r = ec->getattr(ino, attribute);
    attribute.mode = st.mode;
    attribute.uid = st.uid;
    attribute.gid = st.gid;
    r = ec->setattr(ino, attribute);
    if(r != OK) {
        std::cout<<"yfs_client::setattr::get attr error: inum:"<<ino<<std::endl;
        lc->release(ino);
        return r;
    }
    int size = st.size;
    r = ec->get(ino, content, true);
    if(r != OK) {
        std::cout<<"yfs_client::setattr::get content error: inum:"<<ino<<std::endl;
        lc->release(ino);
        return r;
    }
    if(size > attribute.size) {
        content = content + std::string(size - attribute.size, '\0');
    } else if(size < attribute.size){
        content = content.substr(0, size);
    }
    r = ec->put(ino, content);
    lc->release(ino);
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
    printf("yfs_client::create: parent inode: %llu, name:%s\n", parent, name);
    lc->acquire(parent);
     // check whether filename already exists.
    bool found = false;
    inum tmpinum;
    int r = _lookup(parent, name, found, tmpinum);
    if( r != OK ) {
        printf("yfs_client::create::lookup error\n");
        lc->release(parent);
        return r;
    }
    if( found ) {
        std::cout<<"yfs_client::create::filename already exists"<<std::endl;
        lc->release(parent);
        return r = yfs_client::EXIST;
    }

    // create file
    std::cout<<"yfs_client::create::mode:"<<mode<<";S_IFLNK:"<<S_IFLNK<<std::endl;
    // symblic link file
    if(mode & S_IFLNK > 0) {
        printf("yfs_client::create::symbolic create\n");
        ec->create(extent_protocol::T_SYMLNK, ino_out, mode);
    } else {
        ec->create(extent_protocol::T_FILE, ino_out, mode);
    }
    
    // change parent content
    std::string dirContent;
    r = ec->get(parent, dirContent, true);
    if( r != OK ) {
        return r;
    }
    appendDirContent(dirContent, name, ino_out);
    // std::cout<<"yfs_client::create:dir content:"<<dirContent<<std::endl;
    r = ec->put(parent, dirContent);

    lc->release(parent);
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
    printf("yfs_client::makedir:parent inode: %llu\n", parent);
    lc->acquire(parent);
    bool found = false;
    int r = _lookup(parent, name, found, ino_out);
    if(r != OK || found) {
        printf("yfs_client::mkdir::error\n");
        lc->release(parent);
        return r;
    }
    std::string dirContent;
    r = ec->get(parent, dirContent, true);

    r = ec->create(extent_protocol::T_DIR, ino_out, mode);
    if(r != OK) {
        printf("yfs_client::mkdir::create dir error\n");
        lc->release(parent);
        return r;
    }

    r = ec->get(parent, dirContent, true);
    if(r != OK) {
        printf("yfs_client::mkdir::get parent dir error\n");
        lc->release(parent);
        return r;
    }

    appendDirContent(dirContent, name, ino_out);
    // std::cout<<"yfs_client::create:dir content"<<dirContent<<std::endl;
    r = ec->put(parent, dirContent);
    lc->release(parent);
    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    printf("yfs_client::lookup: parent inode: %llu\n", parent);
    lc->acquire(parent);
    int r = _lookup(parent, name, found, ino_out);
    lc->release(parent);
    return r;
}


int
yfs_client::_lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    printf("yfs_client::getfile: parent inode: %llu\n", parent);
    std::list<dirent> list;
    int r = _readdir(parent, list);
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
yfs_client::readdir(inum dir, std::list<dirent> &list) {
    printf("yfs_client::readdir: inum: %llu\n", dir);
    lc->acquire(dir);
    int r = _readdir(dir, list);
    lc->release(dir);
    return r;
}

int
yfs_client::_readdir(inum dir, std::list<dirent> &list)
{
    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    printf("yfs_client::readdir: inum: %llu\n", dir);

    std::string dirContent;
    int r = ec->get(dir, dirContent, false);
    if(r != extent_protocol::OK) {
        printf("yfs_client::readdir::read dir error\n");
        return r;
    }
    // std::cout<<"yfs_client::readdir::dircontent:"<<dirContent<<std::endl;
    size_t p = 0;
    while(p != dirContent.size()) {
        std::string name;
        struct dirent entry;
        while(dirContent[p] != '*' && p != dirContent.size()) {
            name += dirContent[p];
            p++;
        }
        p++;    // ignore the '*' character
        // the name must make pair with inum
        if(p >= dirContent.size()) {
            std::cout<<name<<std::endl;
            printf("yfs_client::readdir::error dir format\n");
            return extent_protocol::IOERR;
        }
        entry.name = name;
        size_t num_pos = dirContent.find(' ', p);
        entry.inum = n2i(dirContent.substr(p, num_pos - p));
        p = num_pos + 1;
        // std::cout<<"yfs_client::readdir::filename:"<<name<<";inum:"<<entry.inum<<std::endl;
        list.push_back(entry);
    }
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
    lc->acquire(ino);

    if( off < 0 ) {
        off = 0;
    }
    std::string content;
    struct fileinfo fileInfo;
    int r = _getfile(ino, fileInfo);
    if(r != OK ) {
        std::cout<<"yfs_client::read::read file info error:inum:"<<ino<<std::endl;
        lc->release(ino);
        return r;
    } 
    r = ec->get(ino, content, false);
    if( r != OK ) {
        std::cout<<"yfs_client::read::read content error:inum:"<<ino<<std::endl;
        lc->release(ino);
        return r;
    }
    if((unsigned long long)off >= fileInfo.size) {
        data = "";
        std::cout<<"yfs_client:read::data:offset is bigger than size.offset:"<<off<<"file size"<<fileInfo.size<<std::endl;
    } else {
        data = content.substr(off, size);
        // std::cout<<"yfs_client:read::data:"<<data<<std::endl;
    }
    
    lc->release(ino);
    return r;
}

int 
yfs_client::clear(inum ino) {
    printf("yfs_client::write: inum: %llu\n", ino);
    lc->acquire(ino);
    int r = ec->put(ino, "");
    lc->release(ino);
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
    printf("yfs_client::write: inum: %llu\n", ino);
    lc->acquire(ino);

    if( off < 0 ) {
        off = 0;
    }
    std::cout<<"yfs_client::write::data:"<<data<<"inum:"<<ino<<";size:"<<size<<";off:"<<off<<std::endl;
    struct fileinfo fileInfo;
    std::string content;
    int r = _getfile(ino, fileInfo);
    if( r != OK ) {
        std::cout<<"yfs_client::read::read file info error:inum:"<<ino<<std::endl;
    }
    // std::cout<<"yfs_client::write::size of original file:"<<fileInfo.size<<std::endl;
    r = ec->get(ino, content, true);
    if( r != OK ) {
        std::cout<<"yfs_client::read::read content error:inum"<<ino<<std::endl;
        lc->release(ino);
        return r;
    }
    // size_t newSize = fileInfo.size > off + size ? off + size : fileInfo.size
    // std::cout<<off-fileInfo.size<<std::endl;
    // add hole into the content
    if( (unsigned long long)off > fileInfo.size) {
        content += std::string(off - fileInfo.size, '\0');
    }
    std::string newContent = content.substr(0, off) + std::string().assign(data, size);
    // add content not be overwritten in the original content
    if(off + size < fileInfo.size) {
        newContent += content.substr(off + size);
    }
    
    // std::cout<<"yfs_client::write::data::new content size:"<<newContent.size()<<std::endl;    
    r = ec->put(ino, newContent);

    // std::cout<<"yfs_client::write::data::new content size:"<<newContent.size()<<std::endl;
    lc->release(ino);
    return r;
}

int yfs_client::unlink(inum parent, const char *name)
{
    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */
    printf("yfs_client::unlink: inum: %llu\n", parent);
    lc->acquire(parent);
    
    bool found;
    inum ino_out;
    int r = _lookup(parent, name, found, ino_out);
    if( r != OK || !found ) {
        printf("yfs_client::unlink::error\n");
    } else {
        std::string filename = std::string(name);    
        std::list<dirent> list;
        _readdir(parent, list);
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
    }
    
    lc->release(parent);
    
    return r;
}

