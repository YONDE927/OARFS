#include "attr.h"
#include <stdio.h>

void printAttr(void* _dstat){
    if(_dstat == NULL){
        return;
    }
    struct Attribute* dstat = _dstat;
    printf("%s %ld %ld %ld \n", dstat->path, dstat->st.st_size, dstat->st.st_mtime, dstat->st.st_ctime);
}


void printStat(Attribute* attr){
    if(attr == NULL){return;}
    printf("'%s': %ld,%d,%d,%d,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld ",
                attr->path,attr->st.st_size,attr->st.st_mode,attr->st.st_uid,attr->st.st_gid,attr->st.st_blksize,attr->st.st_blocks,
                attr->st.st_ino,attr->st.st_dev,attr->st.st_rdev,attr->st.st_nlink,attr->st.st_mtime,attr->st.st_atime,attr->st.st_ctime);
}
