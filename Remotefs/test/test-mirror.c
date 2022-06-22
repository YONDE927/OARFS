#include "mirror.h"

/*検証用*/
int main(int argc, char** argv){
    Mirror* mirror;
    MirrorFile file;
    MirrorFile* pfile;
    Connector* conn;
    int rc;
    char* sshconfig;

    if(argc < 2){
        printf("mirror [sshconfig]\n");
        exit(EXIT_FAILURE);
    }

    sshconfig = argv[1];
    conn = getConnector(sshconfig);

    mirror = constructMirror("db/sample.db", "mirrors", sshconfig); 
    if(mirror == NULL){
        printf("constructMirror fail\n");
        exit(EXIT_FAILURE);
    }

    //reset db
    resetMirrorDB(mirror->dbsession);

    //mirror thread
    startMirroring(mirror);
    request_mirror(mirror, "/home/yonde/Documents/RemoteFS/src/attr.c");
    request_mirror(mirror, "/home/yonde/Documents/cpro/hello/hello.c");
    sleep(1);

    //lookup mirrorfile
    pfile = search_mirror(mirror, "/home/yonde/Documents/cpro/hello/hello.c");

    //open mirrorfile
    rc = openMirrorFile(pfile);
    if(rc < 0){
        printf("openMirrorFile fail\n");
        exit(EXIT_FAILURE);
    }

    //read mirrorfile
    char readbuf[1024] = {0};
    rc = readMirrorFile(pfile, 0, 1024, readbuf);
    if(rc < 0){
        printf("readMirrorFile fail\n");
        exit(EXIT_FAILURE);
    }
    printf("[readMirrorFile]\n%s\n", readbuf);

    //writeMirrorFile
    rc = writeMirrorFile(pfile, 0, 5, "hello");
    if(rc < 0){
        printf("readMirrorFile fail\n");
        exit(EXIT_FAILURE);
    }
    
    //close mirrorfile
    rc = closeMirrorFile(pfile);
    if(rc < 0){
        printf("closeMirrorFile fail\n");
        exit(EXIT_FAILURE);
    }

    freeMirror(mirror);
    
    return 0;
}

