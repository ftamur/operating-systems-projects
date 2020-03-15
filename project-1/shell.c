#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>            //termios, TCSANOW, ECHO, ICANON
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/types.h>
#include <signal.h>
#include <dirent.h>

const char * sysname = "shellgibi";
char ** commands;

enum return_codes {
	SUCCESS = 0,
	EXIT = 1,
	UNKNOWN = 2,
};
struct command_t {
	char *name;
	bool background;
	bool auto_complete;
    char *path;
	int arg_count;
	char **args;
	char *redirects[3]; // in/out redirection
	struct command_t *next; // for piping
};

int handle_in_out(struct command_t *command);
int handle_auto_complete(char *command, char* complete);

int filter(const char *command, char * dirs[], int dirs_len, char *matched[]);
void listFiles(const char *path, char * dirs[], int index);

char *path_google;
char *path_sendmail;

/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t * command)
{
	int i=0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background?"yes":"no");
	printf("\tNeeds Auto-complete: %s\n", command->auto_complete?"yes":"no");
    printf("\tPath: %s\n", command->path);
	printf("\tRedirects:\n");
	for (i=0;i<3;i++)
		printf("\t\t%d: %s\n", i, command->redirects[i]?command->redirects[i]:"N/A");
	printf("\tArguments (%d):\n", command->arg_count);
	for (i=0;i<command->arg_count;++i)
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	if (command->next)
	{
		printf("\tPiped to:\n");
		print_command(command->next);
	}

}
/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command)
{
	if (command->arg_count)
	{
		for (int i=0; i<command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}
	for (int i=0;i<3;++i)
		if (command->redirects[i])
			free(command->redirects[i]);
	if (command->next)
	{
		free_command(command->next);
		command->next=NULL;
	}
	free(command->name);
	free(command);
	return 0;
}
/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt()
{

	char cwd[1024], hostname[1024];
    gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	// printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
	printf("shell-awesome$ ");
    return 0;
}
/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command)
{
	if (strlen(buf) == 0)
		return 0;

	const char *splitters=" \t"; // split at whitespace
	int index, len;
	len=strlen(buf);
	while (len>0 && strchr(splitters, buf[0])!=NULL) // trim left whitespace
	{
		buf++;
		len--;
	}
	while (len>0 && strchr(splitters, buf[len-1])!=NULL)
		buf[--len]=0; // trim right whitespace

	if (len>0 && buf[len-1]=='?') // auto-complete
		command->auto_complete=true;
	if (len>0 && buf[len-1]=='&') // background
		command->background=true;

	char *pch = strtok(buf, splitters);
	command->name=(char *)malloc(strlen(pch)+1);

	if (pch==NULL){
		command->name[0]=0;
		const char * commands[6] = {"clear", "cd", "grep", "sleep", "touch", "mkdir"};	
	}else
		strcpy(command->name, pch);

    // ################################### Getting Path ###################################
	
	char cwd[1024];
	char pathCommand[] = "which ";
	strcat(pathCommand, command->name);

    FILE *fptr = popen(pathCommand, "r"); 
    
	if (fptr == NULL) {
    	printf("Failed to run command\n" );
    	exit(1);
  	}

	fgets(cwd, sizeof(cwd), fptr);

	pclose(fptr);

    cwd[strlen(cwd)-1] = '\0';
	// printf("Getcwd read: <%s>\n", cwd);

    command->path = (char *) malloc(strlen(cwd)+1);
    strcpy(command->path, cwd);

	// ################################### Getting Path ###################################

	command->args=(char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index=0;
	char temp_buf[1024], *arg;
	while (1)
	{
		// tokenize input on splitters
		pch = strtok(NULL, splitters);
		if (!pch) break;
		arg=temp_buf;
		strcpy(arg, pch);
		len=strlen(arg);

		if (len==0) continue; // empty arg, go for next
		while (len>0 && strchr(splitters, arg[0])!=NULL) // trim left whitespace
		{
			arg++;
			len--;
		}
		while (len>0 && strchr(splitters, arg[len-1])!=NULL) arg[--len]=0; // trim right whitespace
		if (len==0) continue; // empty arg, go for next

		// piping to another command
		if (strcmp(arg, "|")==0)
		{
			struct command_t *c=malloc(sizeof(struct command_t));
			int l=strlen(pch);
			pch[l]=splitters[0]; // restore strtok termination
			index=1;
			while (pch[index]==' ' || pch[index]=='\t') index++; // skip whitespaces

			parse_command(pch+index, c);
			pch[l]=0; // put back strtok termination
			command->next=c;
			continue;
		}

		// background process
		if (strcmp(arg, "&")==0)
			continue; // handled before

		// handle input redirection
		redirect_index=-1;
		if (arg[0]=='<')
			redirect_index=0;
		if (arg[0]=='>')
		{
			if (len>1 && arg[1]=='>')
			{
				redirect_index=2;
				arg++;
				len--;
			}
			else redirect_index=1;
		}
		if (redirect_index != -1)
		{
			command->redirects[redirect_index]=malloc(len);
			strcpy(command->redirects[redirect_index], arg+1);
			continue;
		}

		// normal arguments
		if (len>2 && ((arg[0]=='"' && arg[len-1]=='"')
			|| (arg[0]=='\'' && arg[len-1]=='\''))) // quote wrapped arg
		{
			arg[--len]=0;
			arg++;
		}
		command->args=(char **)realloc(command->args, sizeof(char *)*(arg_index+1));
		command->args[arg_index]=(char *)malloc(len+1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count=arg_index;
	return 0;
}
void prompt_backspace()
{
	putchar(8); // go back 1
	putchar(' '); // write empty over
	putchar(8); // go back 1 again
}
/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command)
{

	int index=0;
	char c;
	char buf[4096] = "\0";
	static char oldbuf[4096];

	char ** dirs = (char **) malloc(2048 * sizeof(char *));
    char ** matched = (char **) malloc(2048 * sizeof(char *));

	for (int i=0; i<2048; i++) {
		dirs[i] = malloc(512 * sizeof(char *));
		matched[i] =  malloc(512 * sizeof(char *));
	}

    // tcgetattr gets the parameters of the current terminal
    // STDIN_FILENO will tell tcgetattr that it should write the settings
    // of stdin to oldt
    static struct termios backup_termios, new_termios;
    tcgetattr(STDIN_FILENO, &backup_termios);
    new_termios = backup_termios;
    // ICANON normally takes care that one line at a time will be processed
    // that means it will return if it sees a "\n" or an EOF or an EOL
    new_termios.c_lflag &= ~(ICANON | ECHO); // Also disable automatic echo. We manually echo each char.
    // Those new settings will be set to STDIN
    // TCSANOW tells tcsetattr to change attributes immediately.
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

    //FIXME: backspace is applied before printing chars
	show_prompt();
	int multicode_state=0;
	buf[0]=0;

	int space_index = 0;

  	while (1)
  	{
		c=getchar();
		// printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging


		if (c==9) // handle tab
		{
			// buf[index++]='?'; // autocomplete

			int match_count;

			if (command->auto_complete){
				listFiles(".", dirs, 0);

				char dir_name[256];

				for (int i=0; i<index-space_index; i++) {
					dir_name[i] = buf[space_index+i]; 
				}

				dir_name[index-space_index] = '\0';

				match_count = filter(dir_name, dirs, 2048, matched);
			}else {
				match_count = filter(buf, commands, 2048, matched);
			}

			if (match_count > 1) {
				printf("\n");

				for (int i=0; i<match_count; i++)
					puts(matched[i]);
					
				buf[0]=0;
				break;

			}else {

				char *complete = matched[0];

				if (strlen(complete) > 0){
			
					for (int i=index - space_index; i<strlen(complete); i++){
						putchar((char) complete[i]);
						buf[index++]= complete[i];
					}	 
		
				}
			}
					
		}
				
		if (c==127) // handle backspace
		{
			if (index>0)
			{
				prompt_backspace();
				index--;
			}
			buf[index] = '\0';
			continue;
		}

		if (c==27 && multicode_state==0) // handle multi-code keys
		{
			multicode_state=1;
			continue;
		}

		if (c==91 && multicode_state==1)
		{
			multicode_state=2;
			continue;
		}

		if (c == 32){
			space_index = index + 1;
			command->auto_complete = true;
		}

		if (c==65 && multicode_state==2) // up arrow
		{
			int i;
			while (index>0)
			{
				prompt_backspace();
				index--;
			}
			for (i=0;oldbuf[i];++i)
			{
				putchar(oldbuf[i]);
				buf[i]=oldbuf[i];
			}
			index=i;
			continue;
		}
		else
			multicode_state=0;

		if (c != 9){
			putchar(c); // echo the character
			buf[index++]=c;
		}

		if (index>=sizeof(buf)-1) break;

		if (c=='\n') // enter key
			break;

		if (c==4) // Ctrl+D
			return EXIT;
  	}

  	if (index>0 && buf[index-1]=='\n'){ // trim newline from the end
		index--;
	} 
  		
  	buf[index++]=0; // null terminate string

  	strcpy(oldbuf, buf);

	// printf("buffer: %s", buf);
	// printf("old buffer: %s", oldbuf);

  	parse_command(buf, command);

  	// print_command(command); // DEBUG: uncomment for debugging

    // restore the old settings
    tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
  	return SUCCESS;
}

int process_command(struct command_t *command);
void setAlarm(char *music, char *hour, char *min);
int getProcesses(char* pid, char* outfile);
int drawProcessTree(char pids[128][128], int pids_size, char *outfile);

int main() {

	path_google = (char *) malloc(sizeof(char) * 1024);
	path_sendmail = (char *) malloc(sizeof(char) * 1024);

	char buf[128];
	path_google = realpath("searchgoogle.py", buf);
	
	char buf2[128];
	path_sendmail = realpath("sendmail.py", buf2);

	commands = (char **) malloc(2048 * sizeof(char *));

	char *builtins[] = {"google", "sendmail", "myfg", "mybg", "alarm", "psvis", "pause", "alias", "bg", "bind", "break", "builtin",
       "case", "cd", "clear", "command", "compgen", "complete", "continue", "declare", "dirs", "disown", "echo", "enable", "eval",
       "exec",  "exit",  "export",  "fc",  "fg",  "getopts", "hash", "help", "history", "if", "jobs", "kill", "let", "local",
       "logout", "popd", "printf", "pushd", "pwd", "read",  "readonly",  "return",  "set",  "shift",  "shopt",  "source",
       "suspend",  "test",  "times",  "trap",  "type", "typeset", "ulimit", "umask", "unalias", "unset", "until", "wait",
       "while"};

	int builtins_len = 64;
    
	for (int i=0; i<2048; i++) {
		commands[i] = malloc(512 * sizeof(char *));
	}

	for (int i=0; i<builtins_len; i++){
		commands[i] = builtins[i];
	}

	listFiles("/bin", commands, builtins_len);

	while (1)
	{
		struct command_t *command=malloc(sizeof(struct command_t));
		memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

		int code;
		code = prompt(command);
		if (code==EXIT) break;

		code = handle_in_out(command);
		if (code==EXIT) break;
		
		free_command(command);
	}

	popen("sudo rmmod psvis", "r");

	printf("\n");
	return 0;
}

int process_command(struct command_t *command)
{

	if(strcmp(command->name, "myjobs") == 0)
	{
		char *usr = getenv("USER");
		char *arg[] = {"ps", "-o", "pid,state,comm", "-u", usr, NULL};
		execv("/bin/ps", arg);
	}
	else if(strcmp(command->name, "pause") == 0)
	{
		char *PID = command->args[0];
		int pid_proc = atoi(PID);
		int stat = kill(pid_proc, SIGSTOP);
		if(stat != 0){
			printf("pause failed");
		}
	} 
	else if(strcmp(command->name, "mybg") == 0)
	{
		FILE *fp;
		char name[100], path[100];
		char *pid_comm = command->args[0];
		sprintf(path, "/bin/ps -p %s -o args=", pid_comm);
		
		fp = popen(path, "r");
		if (fp == NULL) {
			printf("Failed to run command\n");
			exit(1);
		}
		fgets(name, sizeof(name), fp);
		pclose(fp);

		int PID = atoi(pid_comm);
		int stat = kill(PID, SIGCONT);
		if(stat != 0){
			printf("mybg %d failed", PID);
			exit(0);
		}
		printf("continued process %d:\t %s \n",PID, name);

	} 
	else if(strcmp(command->name, "myfg") == 0)
	{
		FILE *fp, *sp;
		char name[100], proc[100], proc2[100];
		char *pid_comm = command->args[0];
		sprintf(proc, "/bin/ps -p %s -o args=", pid_comm);
		
		fp = popen(proc, "r");
		if (fp == NULL) {
			printf("Failed to run proc\n");
			exit(1);
		}
		fgets(name, sizeof(name), fp);
		pclose(fp);
		int PID = atoi(pid_comm);
		int stat = kill(PID, SIGCONT);
		if(stat != 0){
			printf("myfg %d failed", PID);
			exit(0);
		}
		printf("continued process %d: \t%s \n",PID, name);
		int check;
		while((check = kill(PID,0)) == 0);
			
	
	} 
	else if(strcmp(command->name, "alarm") == 0)
	{
		char *hour = strtok(command->args[0], ".");
		char *min = strtok(NULL, ".");
		char *music = command->args[1];
		setAlarm(music, hour, min);
	}
	else if(strcmp(command->name, "psvis") == 0)
	{
		char *pid = command->args[0];
		char *outfile = command->args[1];
		getProcesses(pid, outfile);
	}

	else if (strcmp(command->name, "sendmail") == 0) {

		execlp("python", "python", path_sendmail, "test", (char*) NULL);
	}
	else if (strcmp(command->name, "google") == 0) {

		execlp("python", "python", path_google, "test", (char*) NULL);
	}
	else
	{
		command->args=(char **)realloc(
		command->args, sizeof(char *)*(command->arg_count+=2));
		// shift everything forward by 1
		for (int i=command->arg_count-2;i>0;--i)
			command->args[i]=command->args[i-1];
		// set args[0] as a copy of name
		command->args[0]=strdup(command->name);
		// set args[arg_count-1] (last) to NULL
		command->args[command->arg_count-1]=NULL;

		char path[128] = "/bin/";
		char path2[128] = "/usr/bin/";

		strcat(path, command->name);
		strcat(path2, command->name);

		int status = execv(path, command->args);

		if (status < 0){
			status = execv(path2,command->args);
		}
		
	}
	
	return 0;
}

int handle_in_out(struct command_t *command) {

	if (!command->name){
		putchar('\n');
		return 0;
	}
		
	int r;

	if (strcmp(command->name, "")==0) return SUCCESS;
	if (strcmp(command->name, "exit")==0)
		return EXIT;
	if (strcmp(command->name, "cd")==0)
	{
		if (command->arg_count > 0)
		{
			r=chdir(command->args[0]);
			if (r==-1)
				printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
			return SUCCESS;
		}
	}

	int std_in = dup(0);
	int std_out = dup(1);
	
	int file_in;

	if (command->redirects[0]) {
		file_in = open(command->redirects[0], O_RDWR);
	}else {
		file_in = dup(std_in);
	}

	int file_out;
	int fhchild;

	while (command != NULL) {

		dup2(file_in, 0);
		close(file_in);

		if (command->next == NULL) {

			// last command
			if (command->redirects[1] || command->redirects[2]) {
				if (command->redirects[1]){
					file_out = open(command->redirects[1], O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
				}

				if (command->redirects[2]){
					file_out = open(command->redirects[2], O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
				}

			}else {
				file_out = dup(std_out); 
			}


		}else {
			// more commands

			// firat&harun pipe
			int fhpipe[2];

			// open the roads we are coming...
			pipe(fhpipe);

			file_out = fhpipe[1];
			file_in = fhpipe[0];
			
		}

		// redirect outputs
		dup2(file_out, 1);
		// close(file_out);

		fhchild = fork();

		if (fhchild == 0) {
			process_command(command);
			exit(0);
		}

		if (command->next == NULL)
			break;

		command = command->next;
		
	} // while

	// give what you have taken
	dup2(std_in, 0);
	dup2(std_out, 1);
	close(std_in);
	close(std_out);

	if (!command->background)
		wait(NULL);

	return 0;
}

void setAlarm(char *music, char *hour, char *min){

	// path of music file
	char pathbuf[300];
	char *musicpath = realpath(music,pathbuf);

	// create cronjob file and fill it with time schedule
	char cron[300] = "\0"; // create crontab directory file
	strcat(cron, min);
	strcat(cron, " ");
	strcat(cron, hour);
	strcat(cron, " * * * bash -c \"export XDG_RUNTIME_DIR=/run/user/$(id -u) && play ");
	strcat(cron, musicpath);
	strcat(cron, "\"");	

	// create file path to add cronjob list.
	char *user = getenv("USER");
	char filepath[100] = "/home/";
	strcat(filepath, user);
	strcat(filepath, "/alarmFile");

 	FILE *job_file = fopen(filepath, "a");
	// write everything to a job file
	fprintf(job_file, "%s\n", cron);
 	fclose(job_file);

	// run final file with bash command to complete.
	char *args[] = {"crontab", filepath, NULL};
  	execv("/usr/bin/crontab", args);
	
}

void listFiles(const char *path, char * dirs[], int index) {
    struct dirent *dp;
    DIR *dir = opendir(path);

    // Unable to open directory stream
    if (!dir) 
        return; 

    while ((dp = readdir(dir)) != NULL)
    {
        // puts(dp->d_name);
        strcpy(dirs[index], dp->d_name);
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
                // puts(dirs[i]);
                strcpy(matched[matched_index], dirs[i]);
                matched_index++;
                break; 
            }
        }
    }

    return matched_index;

}

int getProcesses(char* pid, char* outfile){

	FILE *dmg = popen("sudo dmesg -c", "r");
	pclose(dmg);

	char command[100] = "sudo insmod psvis.ko pid=";
	strcat(command, pid);

	FILE *fp = popen(command, "r");
	pclose(fp);

	fp = popen("sudo dmesg -c", "r");
	char line[64];
	char pids[128][128];
	int counter = 0;
	while(fgets(line, sizeof(line), fp)){

		char *tok = strtok(line, "]");
		tok = strtok(NULL, " ");

		char command[64] = "ps -o pid,lstart | grep ";
		strcat(command, tok);

		printf("command: %s\n", command);

		FILE *fp = popen(command, "r");
		char buf[128];
		fgets(buf, sizeof(buf), fp);
		pclose(fp);

		printf("buf: %s\n", buf);

		strcpy(pids[counter], buf);

		printf("pids: %s\n", pids[counter]);
		counter++;
	}
	pclose(fp);

	drawProcessTree(pids, counter, outfile);
	return 0;
}

int drawProcessTree(char pids[128][128], int pids_size, char *outfile)
{

	FILE *graph = fopen("graph.dot", "w");
	fprintf(graph, "digraph G {\n");
	for(int i = 1; i < pids_size; i++){
		fprintf(graph, "	\"%s\" -> \"%s\"\n", pids[0], pids[i]);
	}
	fprintf(graph,"\"%s\" [style=filled, fillcolor=red]\n", pids[0]);
	fprintf(graph, "}\n");
	fclose(graph);

	int fd = open(outfile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    dup2(fd, 1);
	close(fd);

	char *args[] = {"dot", "graph.dot", "-Tpng", NULL};
	execv("/usr/bin/dot", args);

	return 0;

}


