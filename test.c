#define FUSE_USE_VERSION 30

#define PATH_LENGTH 128
#define NAME_LENGTH 16

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

typedef struct Node
{
    char name[NAME_LENGTH]; // file or directory name
    mode_t mode; // permission
    uid_t uid; // user id
    gid_t gid; // group id
    long size; // file size. directory not use
    time_t ctime; // change time
    time_t mtime; // modification time
    time_t atime; // access time
    char *data; //file data. directory not use
    int type;  // 0 : file 1 : directory
    struct Node *parent; // parent directory
    struct Node **children;  //children, file not use
    int cNum; // number of children. file not use
}node;

static int OSPJ_getattr(const char *path, struct stat *stbuf);
static int OSPJ_mkdir(const char *path, mode_t mode);
static int OSPJ_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int OSPJ_rmdir(const char *path);
static int OSPJ_chmod(const char *path, mode_t mode);
static int OSPJ_chown(const char *path, uid_t uid, gid_t gid);
static int OSPJ_rename(const char* from, const char* to);
static int OSPJ_mknod(const char *path, mode_t mode, dev_t dev);
static int OSPJ_utimens(const char *path, const struct timespec tv[2]);
static int OSPJ_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int OSPJ_open(const char *path, struct fuse_file_info *fi);
static int OSPJ_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int OSPJ_unlink(const char *path);

// function list
const char * makeFP(const char * path);
node * search(const char *path, int mode);
static int RemoveDirectory(const char *path);
void RemoveFile(const char *path);

//global
node * root;

static int OSPJ_getattr(const char *path, struct stat *stbuf)
{
    node * temp = NULL;
    memset(stbuf, 0, sizeof(struct stat));
    
    temp = search(path, 1); // check if the directory is or not.
    if (temp != NULL)
    {
        stbuf->st_mode = temp->mode;
        stbuf->st_uid = temp->uid;
        stbuf->st_gid = temp->gid;
        stbuf->st_atime = temp->atime;
        stbuf->st_ctime = temp->ctime;
        stbuf->st_mtime = temp->mtime;
        return 0;
    }
    
    temp = search(path,0); // check if the file is or not.
    if (temp != NULL)
    {
        stbuf->st_mode = temp->mode;
        stbuf->st_size = temp->size;
        stbuf->st_atime = temp->atime;
        stbuf->st_ctime = temp->ctime;
        stbuf->st_mtime = temp->mtime;
        stbuf->st_uid = temp->uid;
        stbuf->st_gid = temp->gid;
        return 0;
    }
    // There ia no file or directory coincided with given path.
    return -ENOENT; // error ENOENT
}

static int OSPJ_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    node *temp = search(path,1); // check if the directory is or not
    node *temp2;
    int i;
    
    if (temp== NULL) // no directory
        return -ENOENT;
    if (!(temp->mode & S_IRUSR)) // The directory is not allowed to read.
        return -EACCES;

    temp2 = temp->parent;

    while(temp2) // check the permission of upper hierarchy allows to execute
    {
        temp2->atime = time(NULL);
        if(!(temp2->mode)& S_IXUSR) return -EACCES;
        temp2 = temp2->parent;
    }
  
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    
    if(temp->children==NULL)
        return 0;
    else
    {
        for (i = 0; i < temp->cNum; i++)
        {
            temp2 = temp->children[i];
            filler(buf, temp2->name, NULL, 0);
        }
    }
    return 0;
}

static int OSPJ_mkdir(const char *path, mode_t mode)
{
    node * p_node = NULL;
    node * temp = NULL;
    char * temp2 = NULL;
    char * parent = NULL;
    char * child = NULL;
    char t_path[PATH_LENGTH];
    char t_path2[PATH_LENGTH];
    char t_path3[PATH_LENGTH];
    int i;
    char p_path[PATH_LENGTH] ;
    
    temp = search(path, 1); // check if the directory is or not
    if (temp != NULL) return -EEXIST; // There is already directory which has a same path
    
    strcpy(t_path, path);
    child = strtok(t_path, "/");
    while (1)
    {
        temp2 = strtok(NULL,"/");
        if (temp2 == NULL)break;
        else strcpy(child,temp2);
    }
    temp2 = NULL;
    strcpy(t_path2, path);
    parent = strtok(t_path2, "/");
    
    while (1)
    {
        temp2 = strtok(NULL, "/");
        if(temp2==NULL)
        {
            
            parent = NULL;
            break;
        }
        if (!strcmp(temp2, child))
            break;
        else
            strcpy(parent,temp2);
    }
    
    strcpy(t_path3,path);
    temp2= strtok(t_path3, "/");
    strcpy(p_path,"/");
    while(1)
    {
        if(!strcmp(temp2,child))
            break;
        else
        {
            strcat(p_path,temp2);
            temp2=strtok(NULL,"/");
            if(!strcmp(temp2,child))
                break;
            strcat(p_path,"/");
        }
        
    }
    
    temp = search(p_path,1); // find the parent directory
    if(!(temp->mode & S_IWUSR))return -EACCES; // check the permission of parent node allows to execute
    
    if(temp->children==NULL) // The parent have no children node
    {
        temp->children = (node**)malloc(sizeof(node*));
        temp->children[0] = (node*)malloc(sizeof(node));
        strcpy(temp->children[0]->name,child);
        temp->children[0]->mode = S_IFDIR | mode;
        temp->children[0]->uid = getuid();
        temp->children[0]->gid = getgid();
        temp->children[0]->parent = temp;
        temp->children[0]->children = NULL;
        temp->children[0]->size = 0;
        temp->children[0]->data = NULL;
        temp->children[0]->cNum = 0;
        temp->cNum = 1;
        temp->children[0]->type = 1;
        temp->children[0]->ctime = time(NULL);
        temp->children[0]->atime = time(NULL);
        temp->children[0]->mtime = time(NULL);
    }
    else // The parent already have some children node
    {
        i =  temp->cNum;
        temp->children = (node**)realloc(temp->children,sizeof(node*)*(i+1));
        temp->children[i] = (node*)malloc(sizeof(node));
        strcpy(temp->children[i]->name,child);
        temp->children[i]->mode = S_IFDIR | mode;
        temp->children[i]->uid = getuid();
        temp->children[i]->gid = getgid();
        temp->children[i]->parent = temp;
        temp->children[i]->children = NULL;
        temp->children[i]->size = 0;
        temp->children[i]->data = NULL;
        temp->children[i]->cNum = 0;
        temp->cNum++;
        temp->children[i]->type = 1;
        temp->children[i]->ctime = time(NULL);
        temp->children[i]->atime = time(NULL);
        temp->children[i]->mtime = time(NULL);
    }
    return 0;
}

static int OSPJ_rmdir(const char *path)
{
    node * temp = search(path,1); // check if the directory is or not
    //node * parent;
    //int i,j;
    if(temp==NULL)return -ENOENT; // There is not the given directory path
    if(temp->cNum !=0)return -ENOTEMPTY;
    RemoveDirectory(path);
    return 0;
}

static int OSPJ_mknod(const char *path, mode_t mode, dev_t dev)
{
    node * temp = search(path,0); // check if the file is or not
    char * child = NULL;
    char * temp2 = NULL;
    char t_path[PATH_LENGTH];
    int i;
    if(temp!=NULL)return -EEXIST; // There is already directory which has a same path
    else
    {
        temp = search(makeFP(path),1); // find parent node
        
        strcpy(t_path, path);
        child = strtok(t_path, "/");
        while (1)
        {
            temp2 = strtok(NULL,"/");
            if (temp2 == NULL)break;
            else strcpy(child,temp2);
        }
        if(temp->children==NULL)
        {
            temp->children = (node**)malloc(sizeof(node*));
            temp->children[0] = (node*)malloc(sizeof(node));
            strcpy(temp->children[0]->name,child);
            temp->children[0]->mode = mode;
            temp->children[0]->uid = getuid();
            temp->children[0]->gid = getgid();
            temp->children[0]->parent = temp;
            temp->children[0]->children = NULL;
            temp->children[0]->size = 0;
            temp->children[0]->data = NULL;
            temp->children[0]->cNum = 0;
            temp->cNum = 1;
            temp->children[0]->type = 0;
            temp->children[0]->ctime = time(NULL);
            temp->children[0]->atime = time(NULL);
            temp->children[0]->mtime = time(NULL);
        }
        else
        {
            i =  temp->cNum;
            temp->children = (node**)realloc(temp->children,sizeof(node*)*(i+1));
            temp->children[i] = (node*)malloc(sizeof(node));
            strcpy(temp->children[i]->name,child);
            temp->children[i]->mode = mode;
            temp->children[i]->uid = getuid();
            temp->children[i]->gid = getgid();
            temp->children[i]->parent = temp;
            temp->children[i]->children = NULL;
            temp->children[i]->size = 0;
            temp->children[i]->data = NULL;
            temp->children[i]->cNum = 0;
            temp->cNum++;
            temp->children[i]->type = 0;
            temp->children[i]->ctime = time(NULL);
            temp->children[i]->atime = time(NULL);
            temp->children[i]->mtime = time(NULL);
        }
    }
    return 0;
}

static int OSPJ_unlink(const char *path)
{
    node * temp = search(path,0);
    if(temp==NULL)return -ENOENT; // no file
    RemoveFile(path);
    return 0;
}

// change mode(permission) of file or directory
static int OSPJ_chmod(const char *path, mode_t mode)
{
    node * temp = NULL;
    
    temp = search(path,1); // check if the directory is or not.
    if(temp==NULL) // no directory
    {
        temp = search(path,0); // check if the file is or not.
        if(temp == NULL) return -ENOENT; // no file
    }
    if(!(temp->mode & S_IWUSR)) return -EACCES; // check the permission of node allows to execute
    
    temp->mode = mode;
    temp->ctime = time(NULL);
    temp->atime = time(NULL);
    return 0;
}

 // change owner of file or directory
 static int OSPJ_chown(const char *path, uid_t uid, gid_t gid)
 {
    node * temp = NULL;
    
    temp = search(path,1); // check if the directory is or not.
    if(temp==NULL) // no directory
    {
        temp = search(path,0); // check if the file is or not.
        if(temp == NULL) return -ENOENT; // no file, so this is not both directory and file
    }
    if(!(temp->mode & S_IWUSR)) return -EACCES; // check the permission of node allows to execute
    
    temp->uid = uid;
    temp->gid = gid;
    temp->ctime = time(NULL);
    temp->atime = time(NULL);
    return 0;
 }
 
 //Rename the file, directory, or other object
 static int OSPJ_rename(const char* from, const char* to)
 {
    node * from_temp = NULL;
    node * to_temp = NULL;
    int check_from = 1;  // check type of node -> 0 : file 1 : directory
    node * tp_temp = NULL;
    // to check the node coincided with given 'from' path is directory or file.
    from_temp = search(from,1); // check if the node coincided with given 'from' path is directory or not
    if(from_temp==NULL) // This is not directory
    {   check_from = 0;
        from_temp = search(from,0); // check if the node coincided with given 'from' path is file or not
        if(from_temp == NULL) return -ENOENT; // no file, so this is not both directory and file
    }
    if(!(from_temp->mode & S_IWUSR)) // check the permission of node allows to execute
        return -EACCES;
    
    char t_path[PATH_LENGTH];
    char * temp;
    char * re_name;

    strcpy(t_path, to);
    temp = strtok(t_path, "/");
    while(temp != NULL)
    {
        re_name = temp;
        temp = strtok(NULL, "/");
    }
    
    // find each parent of the node coincided with given 'from' path and 'to' path
    tp_temp = search(makeFP(to),1);
    if(!(tp_temp->mode & S_IWUSR))return -EACCES; // check the permission of parent node allows to execute
    if(tp_temp->children==NULL) // The parent have no children node
    {
        tp_temp->children = (node**)malloc(sizeof(node*));
        tp_temp->children[0] = (node*)malloc(sizeof(node));
        strcpy(tp_temp->children[0]->name,re_name);
        tp_temp->children[0]->mode = from_temp->mode;
        tp_temp->children[0]->uid = from_temp->uid;
        tp_temp->children[0]->gid = from_temp->gid;
        tp_temp->children[0]->parent = tp_temp;
        tp_temp->children[0]->children = from_temp->children;
        tp_temp->children[0]->size = from_temp->size;
        tp_temp->children[0]->data = from_temp->data;
        tp_temp->children[0]->cNum = from_temp->cNum;
        tp_temp->cNum = 1;
        tp_temp->children[0]->type = from_temp->type;
        tp_temp->children[0]->ctime = time(NULL);
        tp_temp->children[0]->atime = time(NULL);
        tp_temp->children[0]->mtime = time(NULL);
    }
    else // The parent already have some children node
    {
        int i = tp_temp->cNum;
        tp_temp->children = (node**)realloc(tp_temp->children,sizeof(node*)*(i+1));
        tp_temp->children[i] = (node*)malloc(sizeof(node));
        strcpy(tp_temp->children[i]->name,re_name);
        tp_temp->children[i]->mode = from_temp->mode;
        tp_temp->children[i]->uid = from_temp->uid;
        tp_temp->children[i]->gid = from_temp->gid;
        tp_temp->children[i]->parent = tp_temp;
        tp_temp->children[i]->children = from_temp->children;
        tp_temp->children[i]->size = from_temp->size;
        tp_temp->children[i]->data = from_temp->data;
        tp_temp->children[i]->cNum = from_temp->cNum;
        tp_temp->cNum++;
        tp_temp->children[i]->type = from_temp->type;
        tp_temp->children[i]->ctime = time(NULL);
        tp_temp->children[i]->atime = time(NULL);
        tp_temp->children[i]->mtime = time(NULL);
    }

    if (check_from == 1) // rename about 'directory'
    {
        RemoveDirectory(from);
        return 0; 
    }
    else // rename about 'file' 
    {
        RemoveFile(from);
        return 0;
    }
 }
 
static int OSPJ_utimens(const char *path, const struct timespec tv[2])
{
    node * temp = search(path,0);
    temp->atime = tv[0].tv_sec;
    temp->mtime = tv[1].tv_sec;
    return 0;
}

static int OSPJ_open(const char *path, struct fuse_file_info *fi)
{
    node * temp = search(path,0); // find the directory coincided with given path.
    int mode = fi->flags & 3; // extract file flag from fuse file information. O_RDONLY, O_WRONLY, O_RDWR
    
    if(temp==NULL)return -ENOENT; // no file
    if(!(temp->mode & S_IWUSR) && (mode == O_WRONLY || mode == O_RDWR))
        return -EACCES;     // If attempt to open file as 'Write only mode' or 'Read-Write MODE',
    // but MODE of file is not allowed write. -> Permission denied
    if(!(temp->mode & S_IRUSR) && (mode == O_RDONLY || mode == O_RDWR))
        return -EACCES;     // If attempt to open file as 'Read only mode' or 'Read-Write MODE',
    // but MODE of file is not allowed read. -> Permission denied

   return 0;
}

static int OSPJ_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    node * temp = search(path,0);
    int mode = fi->flags & 3; // extract file flag from fuse file information. O_RDONLY, O_WRONLY, O_RDWR
    int oflow = temp->size - (offset + size); // check overflow!
    
    if (temp == NULL) return -ENOENT; // no file
    if(!(temp->mode & S_IRUSR) && (mode == O_RDONLY || mode == O_RDWR))
        return -EACCES;     // If attempt to open file as 'Read only mode' or 'Read-Write MODE',
    // but MODE of file is not allowed read. -> Permission denied
    if (temp->data == NULL) return 0; // There are no data
    if (oflow < 0) size += oflow;
    memcpy(buf, temp->data + offset, size);
    temp->atime = time(NULL);
    return size;
}

static int OSPJ_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    node *temp = search(path,0);
    int mode = fi->flags & 3; // extract file flag from fuse file information. O_RDONLY, O_WRONLY, O_RDWR
    int oflow = temp->size - (offset+size); // check overflow!
    
    if(temp == NULL) return -ENOENT; // no file
    if(!(temp->mode & S_IWUSR) && (mode == O_WRONLY || mode == O_RDWR))
        return -EACCES;     // If attempt to open file as 'Write only mode' or 'Read-Write MODE',
    // but MODE of file is not allowed write. -> Permission denied
    if(temp->data == NULL)  // There are no data.
    {
        temp->size = size;
        temp->data = malloc(size);
        memcpy(temp->data, buf, size);
        temp->ctime = time(NULL);
        temp->atime = time(NULL);
        temp->mtime = time(NULL);
    }
    else    // There are already data
    {
        if(oflow < 0)   // overflow occurs!
        {
            temp->size -= oflow;
            temp->data = realloc(temp->data, temp->size); // reallocate size!
        }
        memcpy(temp->data + offset, buf, size);
        temp->ctime = time(NULL);
        temp->atime = time(NULL);
        temp->mtime = time(NULL);
    }
    return size;
}

static struct fuse_operations OSPJ_oper =
{
    .getattr = OSPJ_getattr,
    .readdir = OSPJ_readdir,
    // make and remove directory
    .mkdir = OSPJ_mkdir,
    .rmdir = OSPJ_rmdir,
    // make and unlink(remove) file
    .mknod = OSPJ_mknod,
    .unlink = OSPJ_unlink,
    // change mode(permission) and owner
    .chmod = OSPJ_chmod,
    .chown = OSPJ_chown,
    // rename 
    .rename = OSPJ_rename,
    // file open, read and write
    .open = OSPJ_open,
    .read = OSPJ_read,
    .write = OSPJ_write,
    // time
    .utimens = OSPJ_utimens
};

// --------------------------------- other funciton list -----------------------
// search a node coincided with given path.
node * search(const char *path, int mode)
{
    char t_path[PATH_LENGTH];
    char * temp;
    node * temp2 = root;
    node * temp3;
    int i;
    strcpy(t_path, path);
    temp = strtok(t_path, "/");
    
    if (temp == NULL)
    {
        if(mode==1)
            return root;
        else
            return NULL;
    }
    while (1)
    {
        if(temp2->children!=NULL)
        {
            for (i = 0; i < temp2->cNum; i++)
            {
                temp3 = temp2->children[i];
                if (!strcmp(temp3->name, temp))
                {
                    temp = strtok(NULL, "/");
                    if (temp == NULL)
                    {
                        if (temp3->type == mode)
                            return temp3;
                        else
                            return NULL;
                    }
                    else
                    {
                        temp2 = temp3;
                        break;
                    }
                }
            }
            if (temp2 != temp3)return NULL;
        }
        else return NULL;
    }
}

// find parent path of given node
const char * makeFP(const char * path)
{
    char * temp2 = NULL;
    char * parent = NULL;
    char * child = NULL;
    char t_path[PATH_LENGTH];
    char t_path2[PATH_LENGTH];
    char t_path3[PATH_LENGTH];
    char * p_path = (char*)malloc(sizeof(char)*PATH_LENGTH);
    
    strcpy(t_path,path);
    child = strtok(t_path, "/");
    while (1)
    {
        temp2 = strtok(NULL,"/");
        if (temp2 == NULL)break;
        else strcpy(child,temp2);
    }
    
    temp2 = NULL;
    strcpy(t_path2, path);
    parent = strtok(t_path2, "/");
    
    while (1)
    {
        temp2 = strtok(NULL, "/");
        if(temp2==NULL)
        {
            
            parent = NULL;
            break;
        }
        if (!strcmp(temp2, child))
        {
            break;
        }
        else
        {
            strcpy(parent,temp2);
        }
    }
    strcpy(t_path3,path);
    temp2= strtok(t_path3, "/");
    strcpy(p_path,"/");
    while(1)
    {
        if(!strcmp(temp2,child))
        {
            break;
        }
        else
        {
            strcat(p_path,temp2);
            temp2=strtok(NULL,"/");
            if(!strcmp(temp2,child))
            {
                break;
            }
            strcat(p_path,"/");
        }
        
    }
    return p_path;
}

// romove file
void RemoveFile(const char *path)
{
    node * parent = search(makeFP(path),1); // find parent
    node * temp = search(path,0);
    int i,j;
    for(i=0;i<parent->cNum;i++)
    {
        if(parent->children[i]==temp) // find it!
        {
            for(j=i;j<parent->cNum-1;j++) // relocate sibling of given node
            {
                parent->children[j] = parent->children[j+1];
            }
            if(parent->cNum==1)
            {
                free(parent->children);
                parent->children=NULL;
            }
            else
                parent->children = (node**)realloc(parent->children,sizeof(node*)*(parent->cNum-1));
            free(temp); // unlink given node
            parent->cNum--; // decrease count of parent's children
        }
    }
}

// romove Directory
static int RemoveDirectory(const char *path)
{
    node * temp = search(path,1); // check if the directory is or not
    node * parent;
    int i,j;
    parent = temp->parent;
    if(!(parent->mode & S_IWUSR))return -EACCES; // check the permission of parent node allows to execute
    
    for(i=0;i<parent->cNum;i++)
    {
        if(parent->children[i]==temp)
        {
            for(j=i;j<parent->cNum-1;j++)
            {
                parent->children[j] = parent->children[j+1];
            }
            if(parent->cNum==1)
            {
                free(parent->children);
                parent->children=NULL;
            }
            else
                parent->children = (node**)realloc(parent->children,sizeof(node*)*(parent->cNum-1));
            free(temp);
            parent->cNum--;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    // create root node and initialize setting
    root = (node *)malloc(sizeof(node));
    strcpy(root->name,"root");
    root->mode = S_IFDIR | 0755;
    root->uid = getuid();
    root->gid = getgid();
    root->data = NULL;
    root->parent = NULL;
    root->children = NULL;
    root->size = 0;
    root->cNum = 0;
    root->type = 1;
    return fuse_main(argc, argv, &OSPJ_oper, NULL);
}
