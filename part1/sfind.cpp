//C includes
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <sys/wait.h>
//C++ includes
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <regex>

using namespace std;

void analyzeFilter(struct dirent * obj, int argc, char * argv[]);
void analyzeAction(struct dirent * obj, int argc, char * argv[]);

char tgd[1024];//target dir

int main(int argc, char * argv[]){
    //variables
    DIR * dirp;
    struct dirent * direntp;
    struct stat stat_buf;
    pid_t child;

    //test number of arguments
    if(argc < 2){
        cout << "Invalid number of arguments" << endl;
        exit(1);
    }

    //try to open dir
    if ((dirp = opendir( argv[1])) == NULL){
        perror(argv[1]);
        exit(2);
    }

    char cwd[1024];//current working dir
    if (getcwd(cwd, sizeof(cwd)) == NULL){
        perror("getcwd() error");
        exit(3);
    }
    chdir(argv[1]);//change the working dir to the one passed
    if (getcwd(tgd, sizeof(tgd)) == NULL){
        perror("getcwd() error for tgd");
        exit(4);
    }
    //iterate content of passed folder
    while ((direntp = readdir(dirp)) != NULL){
        if (lstat(direntp->d_name, &stat_buf)==-1){
            perror("lstat ERROR");
            exit(5);
        }
        if(strcmp(direntp->d_name,".") == 0 || strcmp(direntp->d_name,"..") == 0)
            continue;
        if(S_ISDIR(stat_buf.st_mode)){
            chdir(cwd);//change the working dir to the original one
            if((child = fork()) == 0){//child
                char res[0];//string to build next path
                strcat(res, argv[1]);
                strcat(res, direntp->d_name);
                strcat(res, "/");
                argv[1] = res;
                execv(argv[0], argv);//execute itself
                cout << "->execv failed " << endl;
                exit(6);
            }else if(child != -1){
                int status;
                waitpid(child, &status, 0);
            }
            chdir(argv[1]);//change the working dir to the one passed
        }
        analyzeFilter(direntp, argc, argv);
    }
    closedir(dirp);
    return 0;
}



void analyzeFilter(struct dirent * obj, int argc, char * argv[]){
    if(argc == 2)//show all matches
        cout << tgd << "/" << obj->d_name << endl;
    if(argc < 4)//do nothing
        return;

    if(strcmp(argv[2], "-name") == 0){//name search
        regex nameRegex(argv[3]);
        smatch match;
        string name = obj->d_name;
        if(regex_match(name, match, nameRegex))
            analyzeAction(obj, argc, argv);
    }else if(strcmp(argv[2], "-type") == 0){//search by type
        struct stat stat_buf;
        lstat(obj->d_name, &stat_buf);//should work because it has worked before
        if(strcmp(argv[3], "l") == 0 && S_ISLNK(stat_buf.st_mode))//type is link and file is link
            analyzeAction(obj, argc, argv);
        else if(strcmp(argv[3], "f") == 0 && S_ISREG(stat_buf.st_mode))//type is a regular file
            analyzeAction(obj, argc, argv);
        else if(strcmp(argv[3], "d") == 0 && S_ISDIR(stat_buf.st_mode))//type is a folder
            analyzeAction(obj, argc, argv);
    }else if(strcmp(argv[2], "-perm") == 0){//search by permission
        struct stat stat_buf;
        lstat(obj->d_name, &stat_buf);//should work because it has worked before
        char inputPermission[100];
        char * end;
        sprintf(inputPermission, "%s", argv[3]);
        int filePermission = stat_buf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
        int userPermission = strtol(inputPermission, &end, 8);
        if(filePermission == userPermission)//if the user permission matches the file
            analyzeAction(obj, argc, argv);
    }
}


void analyzeAction(struct dirent * obj, int argc, char * argv[]){
    if(argc == 4)
        return;
    if(strcmp(argv[4], "-print") == 0){
        cout << tgd << "/" << obj->d_name << endl;
    }else if(strcmp(argv[4], "-delete") == 0){
        stringstream ss;
        ss << tgd << "/" << obj->d_name;
        unlink(ss.str().c_str());
    }else if(strcmp(argv[4], "-exec") == 0 && argc > 5){
        char * newArgc[argc - 4];
        strcpy(newArgc[0], "/bin/");
        strcat(newArgc[0], argv[5]);
        for(int i = 6; i < argc; i++){
            if(strcmp(argv[i], "{}") == 0){
                newArgc[i - 5] = tgd;
                strcat(newArgc[i - 5], "/");
                strcat(newArgc[i - 5], obj->d_name);
            }else
                newArgc[i - 5] = argv[i];
        }
        newArgc[argc - 5] = NULL;
        pid_t execChild;
        if((execChild = fork())==0){
            execv(newArgc[0], newArgc);
            cout << "exec action failed" << endl;
            exit(7);
        }
        int status;
        waitpid(execChild, &status, 0);
    }
}

























//
