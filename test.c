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


typedef struct Node           
{
	char name[NAME_LENGTH]; // file or directory name
	mode_t mode; // permission
	uid_t uid; // user id
	gid_t gid; // group id
	long size; // file size. directory not use
	time_t ctime; // change time           
	time_t mtime;// modification time
	time_t atime;// access time
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

static int OSPJ_mknod(const char *path, mode_t mode, dev_t dev);
static int OSPJ_utimens(const char *path, const struct timespec tv[2]);
static int OSPJ_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int OSPJ_open(const char *path, struct fuse_file_info *fi);
static int OSPJ_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int OSPJ_unlink(const char *path);

const char * makeFP(const char * path);
node * search(const char *path, int mode);

node * root;

static int OSPJ_getattr(const char *path, struct stat *stbuf)
{
	node * temp = NULL;
	memset(stbuf, 0, sizeof(struct stat));
	
	temp = search(path, 1);
	if (temp != NULL)
	{
		stbuf->st_mode = temp->mode;
		stbuf->st_uid = temp->uid;
		stbuf->st_gid = temp->gid;
		return 0;
	}
	
	temp = search(path,0);
	if (temp != NULL)
	{
		stbuf->st_mode = temp->mode;
		stbuf->st_size = temp->size;
		stbuf->st_atime = temp->atime;
		stbuf->st_mtime = temp->mtime;
		stbuf->st_uid = temp->uid;
		stbuf->st_gid = temp->gid;
		return 0;

	}
		
	return -ENOENT;
	

}

static int OSPJ_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	node *temp = search(path,1);
	node *temp2;
	int i;

	if (temp== NULL)
		return -ENOENT;

	if (!(temp->mode & S_IRUSR))
		return -EACCES;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);
	
	if(temp->children==NULL)return 0;	
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

	temp = search(path, 1);
	if (temp != NULL) return -EEXIST;
	
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
	
	temp = search(p_path,1);
	if(temp==NULL)return -EACCES;
	if(temp->children==NULL)
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
	}
	else
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
	}

	return 0;
}

static int OSPJ_rmdir(const char *path)
{
	node * temp = search(path,1);
	node * parent;
	int i,j;
 	if(temp==NULL)return -EACCES;
 	if(temp->cNum !=0)return -ENOTEMPTY;
 	parent = temp->parent;
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
 			{
	 			parent->children = (node**)realloc(parent->children,sizeof(node*)*(parent->cNum-1));
	 		}
	 		free(temp);
	 		parent->cNum--;
 		}
 	}


 	return 0;
}

static int OSPJ_mknod(const char *path, mode_t mode, dev_t dev)
{
	node * temp = search(path,0);
	char * child = NULL;
	char * temp2 = NULL;
	char t_path[PATH_LENGTH];
	int i;
	if(temp!=NULL)return -EEXIST;
	else
	{
		temp = search(makeFP(path),1);

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
		}
	}

	return 0;
}

static int OSPJ_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	node * temp = search(path,0);
	int mode = fi->flags & 3;
	int oflow = temp->size - (offset + size);

	if (temp == NULL) return -ENOENT;
	if (temp->data == NULL) return 0;
	if (oflow < 0) size += oflow;
	memcpy(buf, temp->data + offset, size);
	return size;
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
	node * temp = search(path,0);
	if(temp==NULL)return -ENOENT;
	return 0;
}





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
		if(mode==1)return root;
		else return NULL;
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
						if (temp3->type == mode)return temp3;
						else return NULL;
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

static int OSPJ_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	node *temp = search(path,0);
	int oflow = temp->size - (offset+size);

	if(temp == NULL) return -ENOENT;

	if(temp->data == NULL)
	{
		temp->size = size;
		temp->data = malloc(size);
		memcpy(temp->data, buf, size);
	}
	else
	{
		if(oflow < 0)
		{
			temp->size -= oflow;
			temp->data = realloc(temp->data, temp->size);
		}
	memcpy(temp->data + offset, buf, size);
	}
	return size;
}

static int OSPJ_unlink(const char *path)
{
	node * parent = search(makeFP(path),1);
	node * temp = search(path,0);
	int i,j;
	if(temp==NULL)return -EACCES;
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
 			{
	 			parent->children = (node**)realloc(parent->children,sizeof(node*)*(parent->cNum-1));
	 		}
	 		free(temp);
	 		parent->cNum--;
 		}
 	}
	return 0;
}

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


static struct fuse_operations OSPJ_oper =
{
	.getattr = OSPJ_getattr,
	.readdir = OSPJ_readdir,
	.mkdir = OSPJ_mkdir,
	.rmdir = OSPJ_rmdir,
	.mknod = OSPJ_mknod,
	.read = OSPJ_read,
	.utimens = OSPJ_utimens,
	.open = OSPJ_open,
	.write = OSPJ_write,
	.unlink = OSPJ_unlink
};


int main(int argc, char *argv[])
{
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
