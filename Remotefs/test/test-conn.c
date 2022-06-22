#include <fcntl.h>
#include <stdio.h>
#include "conn.h"
#include "attr.h"
#include "list.h"

int main(int argc, char**argv)
{
    List* list;
    Node* node;
    Attribute* attr;
    if(argc < 2)
    {
    printf("conn.o [SSHCONFIG]\n");
    exit(EXIT_FAILURE);
    }
    char* configpath = argv[1];

    /* getConnector */
    Connector* connector = getConnector(configpath);
    if(connector==NULL){
        exit(EXIT_FAILURE);
    }
    Connector* con2 = getConnector(NULL);
    if(con2 != NULL)
    {
        printf("static conn succeed\n");
    }

    /* connReaddir */
    list = connReaddir("..");
    printList(list, printAttr);
    freeList(list, NULL);
    printf("\n");

    /* connStat */
    printf("connStat\n");
    attr = connStat("/etc/ssh/sshd_config");
    if(attr != NULL)
    {
    printf("%s %ld %ld\n",attr->path,attr->st.st_size,attr->st.st_mtime);        
    }

    /* Read Write*/
    char buf[256] = {0};
    int nbytes = 0;
    int read_size = 100;
    int write_size = 64;
    char write_buf[64] = "Hello connWrite test.\n";
    char path[64] = "/tmp/connWrite.txt";
    FILE* file;
    FileSession* rf1,* rf2;
 
    /* connOpen */
    rf1 = connOpen("/tmp/connRead.txt", O_RDONLY);
    if(rf1 < 0)
    {
        printf("connOpen error\n");
        exit(EXIT_FAILURE);
    }
    rf2 = connOpen("/tmp/connWrite.txt", O_RDWR);
    if(rf2 < 0)
    {
        printf("connOpen error\n");
        exit(EXIT_FAILURE);
    }
    /* connRead */
    printf("\nconnRead\n");
    nbytes = connRead(rf1, 0, buf, read_size);
    if(nbytes > 0)
    {
        buf[read_size] = '\0';
        printf("%s\n",buf);
    }
    else
    {
        puts("connRead error");
    }
    
    /* connWrite */
    printf("connWrite\n");
    memset(buf, 0, 256);
    creat(path, S_IRWXU);
    nbytes = connWrite(rf2, 0, write_buf, write_size);
    if(nbytes > 0)
    {
        file = fopen(path, "r");
        nbytes = fread(buf, sizeof(char), write_size, file);
        buf[write_size] = '\0';
        printf("%s\n", buf);
    }else{
        printf("connwrite fail\n");
    }

    return 0;
}
