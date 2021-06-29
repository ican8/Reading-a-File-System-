#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/fs.h>
#include <ext2fs/ext2_fs.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include<string.h>

unsigned long int block_size;
struct ext2_super_block sb; 
unsigned long int group_count;
unsigned int INODE_SIZE;

char** get_parsed(char *input, char* separator) {
    char** command = (char **)malloc(8 * sizeof(char *));
    char *parsed;
    int index = 0;

    parsed = strtok(input, separator);
    while (parsed != NULL) {
        command[index] = parsed;
        index++;
        parsed = strtok(NULL, separator);
    }

    command[index] = NULL;
    return command;
}

int total_parsed(char** command){
	int n = 0;
	char* tmp = command[0];
	while(tmp != NULL){
		n++;
		tmp = command[n];
	}
	return n;
}

void print_inode(struct ext2_inode *inode){
    // printf("\nRead the following inode :- \n");
	char type[20];
	if( S_ISREG(inode->i_mode) ){
		strcpy(type,"File");
	}
	else if( S_ISDIR(inode->i_mode)  ){
		strcpy(type,"Directory");
	}
	else{
		strcpy(type,"Other");
	}
	printf("\nType: %s\n",type);
	printf("Mode: %u\tSize: %u\t uid:%u\n",inode->i_mode, inode->i_size,inode->i_uid);
	//printf("Access time:\t%d\nCreation time:\t%d\nDeletion time:\t%d\nModification time:\t%d\n",inode.i_atime,inode.i_ctime, inode.i_dtime,inode.i_mtime);
	printf("Links Count: %u\tBlocks Count: %d\t Flags:%u\n\n",inode->i_links_count, inode->i_blocks, inode->i_flags);
}


void debug_inode(int fd,unsigned int offset){
	unsigned int base = 131*block_size;
	struct ext2_inode inode;
	lseek(fd,base + offset*(sb.s_inode_size),SEEK_SET);
	read(fd,&inode,sizeof(inode));
	print_inode(&inode);
}
// return block number of inode table from inode number
unsigned long int inode_tabl_of(int fd,unsigned long int group_no){
	lseek(fd, block_size, SEEK_SET);
	struct ext2_group_desc bgdesc;
	// printf("Group no is %d\n",group_no);
	for(unsigned int i = 0; i <= group_no; i++){
		read(fd, &bgdesc, sizeof(struct ext2_group_desc));
	}
	return bgdesc.bg_inode_table;
}

void read_inode(int fd, unsigned long int inode_num,struct ext2_inode* inode){
    unsigned long int group_num = (inode_num-1) / sb.s_inodes_per_group;
	unsigned long int inode_table_location = inode_tabl_of(fd,group_num);
	// printf("Inode table location of inode %lu is : %lu\n",inode_num, inode_table_location);
	unsigned long int relative_inode_num = (inode_num -1) % sb.s_inodes_per_group;
	lseek(fd, inode_table_location*block_size + relative_inode_num*INODE_SIZE, SEEK_SET);
	// printf("Relative inode number of %lu is : %lu\n",inode_num,relative_inode_num);
	// printf("Size of struct inode from sb: %lu",INODE_SIZE);
	// printf("Reading inode : %lu from offset : %lu;", inode_num,inode_table_location*block_size + relative_inode_num*sizeof(sb.s_inode_size) );
	read(fd, inode, sizeof(struct ext2_inode));
    // print_inode(inode);
}



// search for 'name' from inode table of 'this_inode' and return its inode number
unsigned int get_inode(int fd, char* name, struct ext2_inode* this_inode, int is_directory){
	// printf("Looking for %s in inode with is_file--> :%d  is_dir--> : %d\n",name,S_ISREG(this_inode->i_mode),S_ISDIR(this_inode->i_mode) );
	if(is_directory){
		if( !S_ISDIR(this_inode->i_mode)){
			printf("Error: %s No such directory\n",name);
			exit(1);
		}
	}
	long int count = 0;
	struct ext2_dir_entry_2 dirent;
	lseek(fd, (this_inode->i_block[0] ) * block_size, SEEK_SET);
	count += read(fd, &dirent, sizeof(struct ext2_dir_entry_2));
	count += dirent.rec_len - sizeof(struct ext2_dir_entry_2);
	lseek(fd, dirent.rec_len - sizeof(struct ext2_dir_entry_2), SEEK_CUR);
	// printf("\tInode no: %lu Name:%s \n", dirent.inode, dirent.name);
	
	while( strcmp(name,dirent.name) != 0 && count < this_inode->i_size){
		count += read(fd, &dirent, sizeof(struct ext2_dir_entry_2));
		count += dirent.rec_len - sizeof(struct ext2_dir_entry_2);
		lseek(fd, dirent.rec_len - sizeof(struct ext2_dir_entry_2), SEEK_CUR);
	}
	if(strcmp(dirent.name,name) != 0){
		printf("Error: %s No such file/directory\n",name);
		exit(1);
	}
	// printf("\nGO to Inode no: %lu Name:%s \n", dirent.inode, dirent.name);
	return dirent.inode;
	// read_inode(fd,(dirent.inode),this_inode);
	// return 0;
}


void print_file_data(int fd, struct ext2_inode* inode){
	// printf("Data from file: -\n");
	char* data = malloc(block_size);
	lseek(fd,block_size*inode->i_block[0],SEEK_SET);
	for(int i = 0; i <= (inode->i_size)/block_size; i++){
		read(fd,data,block_size);
		printf("%s",data);
	}
	printf("\n");
}

void print_directory_data(int fd, struct ext2_inode* inode){
	// printf("Directory entries :- \n");
	int count = 0;
	struct ext2_dir_entry_2 dirent;
	lseek(fd, (inode->i_block[0] ) * block_size, SEEK_SET);
	while(count < inode->i_size){
		count += read(fd, &dirent, sizeof(struct ext2_dir_entry_2));
		count += dirent.rec_len - sizeof(struct ext2_dir_entry_2);
		lseek(fd, dirent.rec_len - sizeof(struct ext2_dir_entry_2), SEEK_CUR);
		printf("%s \n",dirent.name);
	}	
}

int main(int argc, char *argv[]) {
	int fd = open(argv[1], O_RDONLY); // argv[1] = /dev/sdb1
	int count;
	int mode;
    int levels;
	char path[128];
	int is_directory;
	char** dirs;
	struct ext2_group_desc bgdesc;
	struct ext2_inode inode;
	struct ext2_dir_entry_2 dirent;
	if(fd == -1) {
		perror("Invalid device file:\n");
		exit(errno);
	}
	if(argv[2] == NULL){
		perror("Path not found!\n");
		printf("Usage: ./a.out <device-file> <path> <mode>\n");
		exit(errno);
	}
	if(strcmp(argv[3],"inode") == 0){
		mode = 0;
	}
	else if(strcmp(argv[3],"data") == 0){
		mode = 1;
	}
	else{
		perror("Invalid mode");
		exit(1);
	}

    strcpy(path,argv[2]); 
	dirs = get_parsed(path,"/");
    levels = total_parsed(dirs);
	lseek(fd, 1024, SEEK_SET);
	count = read(fd, &sb, sizeof(struct ext2_super_block));
	INODE_SIZE = sb.s_inode_size;
	// printf("\n\nSIZE OF STRUCT INODE: %u\n\n",sb.s_inode_size);
	block_size = 1024 << sb.s_log_block_size;
	group_count = 1 + (sb.s_blocks_count-1) / sb.s_blocks_per_group;

    read_inode(fd,2,&inode);
    struct ext2_inode* parent = &inode;
    struct ext2_inode* child;
    unsigned long int next_inode_to_read;
	is_directory = 1;
    for(int i = 0; i < levels; i++){
		if(i == levels-1){
			is_directory = 0;
		}
        next_inode_to_read = get_inode(fd,dirs[i],parent,is_directory);
       //  printf("Inode number of %s is %lu",dirs[i],next_inode_to_read);
        read_inode(fd,next_inode_to_read,parent);
    }

	if(mode == 0){
		// printf("\n Print inode --\n");
		print_inode(parent);
	}
	else{
		// printf("We can now read from this inodes blocks\n");
		if(S_ISREG(parent->i_mode)){
			print_file_data(fd,parent);
		}
		else if(S_ISDIR(parent->i_mode)){
			print_directory_data(fd,parent);
		}
		else{
			printf("Specified inode is not a file or directory!\n");
		}
	}
	
	close(fd);
    return 0;
}
