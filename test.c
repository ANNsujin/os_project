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
	char name[NAME_LENGTH];
	mode_t mode;   
	uid_t uid;
	gid_t gid;
	long size;              
	time_t ctime;            
	time_t etime;
	time_t atime;
	char *data;
	int type; 

	struct Node *parent; 
	struct Node **children;  
	int cNum;

}node;

node * root;
static int OSPJ_getattr(const char *path, struct stat *stbuf);
static int OSPJ_mkdir(const char *path, mode_t mode);
static int OSPJ_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int OSPJ_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
const char * makeFP(const char * path);
node * search(const char *path, int mode);



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
		temp->children[0]->mode = S_IFDIR | 0755;
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
		temp->children[i]->mode = S_IFDIR | 0755;
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

static int OSPJ_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	node * f = search(path,0);
	int mode = fi->flags & 3;
	int oflow = f->size - (offset + size);

	if (f == NULL) return -ENOENT;
	if ((mode == O_WRONLY || mode == O_RDWR) && !(f->mode & S_IWUSR))
		return -EACCES;
	if ((mode == O_RDONLY || mode == O_RDWR) && !(f->mode & S_IRUSR))
		return -EACCES;
	if (f->data == NULL) return 0;
	if (oflow < 0) size += oflow;
	memcpy(buf, f->data + offset, size);
	return size;
}


node * search(const char *path, int mode)
{
	printf("initial %s\n",path);
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

const char * makeFP(const char * path)
{

	char * temp2 = NULL;
	char * parent = NULL;
	char * child = NULL;
	char t_path[PATH_LENGTH];
	char t_path2[PATH_LENGTH];
	char t_path3[PATH_LENGTH];
	char * p_path = (char*)malloc(sizeof(char)*PATH_LENGTH);

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
	.rmdir = OSPJ_rmdir
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
