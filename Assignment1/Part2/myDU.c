#include<stdio.h>
#include<stdlib.h>
#include<math.h>
#include<string.h>
#include<dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

unsigned long getFilesize(const char* filename) {
    struct stat st;
    if(stat(filename, &st) != 0) {
        return 0;
    }
    return (unsigned long) st.st_size;   

}

unsigned long solve(const char* path){
    unsigned long ans=getFilesize(path);
    char p1[1000];
    unsigned long fd,sz;
    struct dirent *dp;
    DIR *dir = opendir(path);

	struct stat buf;
	if(stat(path, &buf) != 0) {
        return 0;
    }
    if(!S_ISDIR(buf.st_mode)) return ans;
    while((dp = readdir(dir)) != NULL){
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) continue;
        strcpy(p1, path);
        strcat(p1, "/");
        strcat(p1, dp->d_name);
		unsigned long sz=solve(p1);
        ans+=sz;
    }
    return ans;
}


int main(int argc, char* argv[]){
    
    char bp[2000];
	strcpy(bp,argv[1]);
	unsigned long cnt=0;

    struct dirent *dp;
    char path[2000];
    DIR *dir = opendir(bp);
    if (!dir)
        return 0;
    
    unsigned long ans=getFilesize(bp);
    pid_t pid;
    unsigned long fd,sz;
	unsigned long read_and[2000]; 

    while ((dp = readdir(dir)) != NULL)
    {
        if (strcmp(dp->d_name, ".") == 0 || strcmp(dp->d_name, "..") == 0) continue;
        fd = open(dp->d_name, O_RDONLY);
		if(fd<0){
			perror("Unable to execute\n");
			exit(-1);
		}

		strcpy(path, bp);
        strcat(path, "/");
        strcat(path, dp->d_name);

        sz = getFilesize(path);

		struct stat buf;
		if(stat(path, &buf) != 0) {
			return 0;
		}

        if(S_ISDIR(buf.st_mode)){
            int d[2]; 
            if(pipe(d)<0){ 
                perror("Unable to execute\n");
                exit(-1);
            } 
			read_and[cnt]=d[0]; 
			cnt++;
            pid = fork();
			if(pid < 0){
				perror("Unable to execute");
				exit(-1);
    		}
			
			if(!pid){
				close(1);
				dup(d[1]);
				printf("%lu\n",solve(path));
				return 0;
			}
        }
		else ans+=sz;
    }

    wait(NULL);		
	while(cnt--){
		char buff[512];
		read(read_and[cnt],buff,512);
		char* pt;
		ans+=strtoul(buff,&pt,10);
	}
    printf("%lu\n",ans);
    return 0;
}