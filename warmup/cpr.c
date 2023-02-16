#include "common.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>








/* make sure to use syserror() when a system call fails. see common.h */

void
usage()
{
	fprintf(stderr, "Usage: cpr srcdir dstdir\n");
	exit(1);
}

void copy_file(char* src_path, char* des_path)
{
  char buffer[4096];
  ssize_t bytes_read;
  int fd_from;
  int fd_to;
  
  fd_from = open(src_path, O_RDONLY);
  fd_to = open(des_path, O_RDWR|O_CREAT);
  
  if(fd_from < 0){
    syserror(open, src_path);
    return;
  }
  
  if(fd_to < 0){
    syserror(open, des_path);
  }
  
  
  
  
  while ((bytes_read = read(fd_from, buffer, sizeof(buffer))) > 0) {
    ssize_t bytes_written = write(fd_to, buffer, bytes_read);
    if (bytes_written == -1) {
      syserror(write, src_path);
      return;
    }
    assert(bytes_read == bytes_written);
  }
  if (bytes_read == -1) {
    syserror(read, des_path);
    return;
  }
  assert(bytes_read == 0);
  close(fd_to);
  close(fd_from);
  return;
}



int check_dir_file(char* path){
    struct stat info;
    if(stat(path, &info) != -1){
        if(S_ISDIR(info.st_mode) != 0){
            return 0;
        }
        else if(S_ISREG(info.st_mode) != 0){
            return 1;
        }
    }
    return -1;
}


mode_t check_mode(char* path){
    struct stat info;
    stat(path, &info);
    
    return info.st_mode;
}


void copy_directory(char* source_path, char* destination_path){
    char des_add[108] = {0};
    char source_add[108] = {0};
    DIR *src_dp;
    struct dirent *all_src;
    
        
    if((src_dp = opendir(source_path)) == NULL){
        return;
    }
    else{
        if(mkdir(destination_path, 0777) < 0){
            syserror(mkdir, destination_path);
        }
    }
    
    
    


    
    while((all_src = readdir(src_dp)) != NULL){       
        if(strcmp(all_src->d_name, ".") == 0 || strcmp(all_src->d_name, "..") == 0){
            continue;
        }
        
        strcpy(des_add, destination_path);
        strcat(des_add, "/");
        strcat(des_add, all_src->d_name);

        strcpy(source_add,source_path);
        strcat(source_add,"/");
        strcat(source_add, all_src->d_name);
 
       


        if(check_dir_file(source_add) == 0){

            copy_directory(source_add,des_add);
            mode_t dir_mode = check_mode(source_add);
            chmod(des_add, dir_mode);
        }



        if(check_dir_file(source_add) == 1){
            copy_file(source_add, des_add);
            mode_t file_mode = check_mode(source_add);
            chmod(des_add, file_mode);
     
        }
    }

    closedir(src_dp);

}










int main(int argc, char *argv[])
{
	if (argc != 3) {
            usage();
	}
    
    //char* src = "/cad2/ece344f/tester";
    //char* dst = "./tester";
    copy_directory(argv[1], argv[2]);
    mode_t file_mode = check_mode(argv[1]);
    chmod(argv[2], file_mode);
        
	return 0;
}
