#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>

void listFiles(const char *path, char * dirs[]);
int filter(const char *command, char * dirs[], int dirs_len, char *matched[]);

int main() {

    char ** dirs = (char **) malloc(2048 * sizeof(char *));
    char ** matched = (char **) malloc(2048 * sizeof(char *));

    for (int i=0; i<2048; i++) {
        dirs[i] = malloc(512 * sizeof(char *));
        matched[i] =  malloc(512 * sizeof(char *));
    }

    listFiles(".", dirs);
    int matched_count = filter("s", dirs, 2048, matched);

    return 0;
}

void listFiles(const char *path, char * dirs[]) {
    struct dirent *dp;
    DIR *dir = opendir(path);

    // Unable to open directory stream
    if (!dir) 
        return; 

	int index = 0;
    while ((dp = readdir(dir)) != NULL)
    {
        // puts(dp->d_name);
        sprintf(dirs[index], dp->d_name);
		index++;
    }

    // Close directory stream
    closedir(dir);

}

int filter(const char *command, char * dirs[], int dirs_len, char *matched[]) {
    
    int index;
    int matched_index = 0;

    for (int i=0; i<dirs_len; i++) {
        index = 0;

        while (command[index] == dirs[i][index]){
            index++;
            if (command[index] == '\0'){
                puts(dirs[i]);
                sprintf(matched[matched_index], dirs[i]);
                matched_index++;
                break; 
            }
        }
    }

    return matched_index;

}
