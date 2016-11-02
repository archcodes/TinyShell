#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <stdlib.h>

#define MAX_BUFFER_SIZE 1024
#define MAX_CHILD 5

// Counter to track background child processes
static int curr_idx = 0;
// Array to store child PIDs
int child_pids[MAX_CHILD];

void interrupt_handler(int signal_no);
int exec_command(char* ext_cmd);
int process_external_command(char* ext_cmd);
void handle_user_input  (char* input_cmd);
void list_jobs();
int get_child_index(int pid);
void remove_child(int index);
void sigchld_handler(int signal);
void kill_all_children();
void kill_child(int pid);

/*
 * Main logic to execute command
 * Command accepts maximum of 2 parameters
 */
int exec_command(char* ext_cmd)
{
//    printf("I am executing inside child\n");
//    printf("Child pid: %d, Parent pid %d\n", getpid(), getppid());
    char *cmd = NULL;
    char *param1 = NULL;
    char *param2 = NULL;

    // For execvp
    char *argv[4];
//    printf("ext_cmd: %s, %lu\n", ext_cmd, strlen(ext_cmd));
    cmd = strtok(ext_cmd, " ");
//    printf("cmd: %s, %lu\n", cmd, strlen(cmd));
    // If command is not null allocate memory
    if(cmd != NULL) {
        argv[0] = (char*) malloc(sizeof(char) * strlen(cmd)+1);
        strcpy(argv[0], cmd);

        param1 = strtok(NULL, " ");
        // If param1 is not null allocate memory
        // Drop & here, Do not push down to bash
        if(param1 != NULL && strcmp(param1, "&") != 0)
        {
            argv[1] = (char*) malloc(sizeof(char) * strlen(param1)+1);
            strcpy(argv[1], param1);

            param2 = strtok(NULL, " ");
            // If param2 is not null allocate memory
            // Drop & here, Do not push down to bash
            if(param2 != NULL && strcmp(param2, "&") != 0)
            {
                argv[2] = (char*) malloc(sizeof(char) * strlen(param2)+1);
                strcpy(argv[2], param2);
                argv[3] = NULL;
            }
            else
            {
                argv[2] = NULL;
            }
        }
        else
        {
            argv[1] = NULL;
            argv[2] = NULL;
        }
        // Execute the command
        int ret_val = 0;
        ret_val = execvp(argv[0], argv);
        // Report error if the command execution failed for whatever reason
        if(ret_val != 0)
            printf("ERROR: Command invalid / not recognized\n");

        // Free all the allocated memory
        // Free only if allocated.
        if(argv[2] != NULL)
            free(argv[2]);
        if(argv[1] != NULL)
            free(argv[1]);
        if(argv[0] != NULL)
            free(argv[0]);
        _exit(0);
    } else
    {
        // Should not come here
        _exit(-1);
    }
}

/*
 *  Identify Foreground / Background commands and handle appropriately
 */
int process_external_command(char* ext_cmd)
{
    char *amp_and = NULL;
    amp_and = strchr(ext_cmd, '&');

    // Check if & exists in the command
    if(amp_and)
    {
        int index = amp_and - ext_cmd;
        // Check if & is the last character in the command
        // Handle upto MAX_CHILD number of processes in background
        if(index == (strlen(ext_cmd) - 1) && curr_idx <= (MAX_CHILD - 1))
        {
//            printf("Background Process\n");
            pid_t forked_pid = fork();

            if(forked_pid == 0)
            {
//                printf("I am running inside background child\n");
//                printf("Child pid: %d, Parent pid %d\n", getpid(), getppid());
                // Inside child process, execute input command
                exec_command(ext_cmd);
//                printf("exiting background child\n");
                return 0;

            } else if(forked_pid < 0)
            {
                printf("ERROR: Could not fork a child\n");
                return -1;
            } else
            {
//                printf("I am running inside parent\n");
//                printf("My pid: %d\n", getpid());
                // DO NOT wait for child to complete
                // Keep PID for background process / child
                child_pids[curr_idx] = forked_pid;
                curr_idx++;
                return 0;
            }

        }
        else if(curr_idx >= (MAX_CHILD))
        {
            printf("ERROR: Cannot create more background process\n");
            return -1;
        }
        else
        {
            // Handle cases such as "ls & -la"
            printf("ERROR: Wrong Syntax\n");
            return -1;
        }
    }
    else
    {
//        printf("Foreground Process\n");
        pid_t forked_pid = fork();

        if(forked_pid == 0)
        {
            //Inside child process, execute input command
            return exec_command(ext_cmd);
        }
        else if(forked_pid < 0)
        {
            printf("ERROR: Could not fork a child\n");
            return -1;
        } else
        {
//            printf("I am running inside parent\n");
//            printf("My pid: %d\n", getpid());
            // Handle ctrl+c here, ignore it as this is parent process
            signal(SIGINT, interrupt_handler);
            // Wait for child process running in foreground
            int status = -1;
            waitpid(forked_pid,&status,WUNTRACED);
//            printf("Finished waiting for children\n");
            return 0;
        }
    }
}

/*
 * Identify Internal or External command and handle appropriately
 */

void handle_user_input(char* input_cmd)
{
    if(input_cmd != NULL)
    {
//        printf("LENGTH: %lu\n", strlen(input_cmd));
        //Don't mess with input buffer, make copies
        char *temp = (char*) malloc(sizeof(char) * (strlen(input_cmd) + 1));
        //Pass this buffer for further processing
        char *pass_input = (char*) malloc(sizeof(char) * (strlen(input_cmd) + 1));
        memset(temp, '\0', strlen(input_cmd));
        memset(pass_input, '\0', strlen(input_cmd));
        strcpy(temp, input_cmd);
        strcpy(pass_input, input_cmd);

        char *temp_input = strtok(temp, " ");
        if(temp_input == NULL)
            return;
        if (strcmp(temp_input, "bye") == 0) {
//            printf("BYE!\n");
            kill_all_children();
            exit(0);
        }
        else if (strcmp(temp_input, "jobs") == 0) {
//            printf("jobs\n");
            list_jobs();
        }
        else if (strcmp(temp_input, "kill") == 0) {
//            printf("kill\n");
            // Get PID to kill
            char* kill_pid = strtok(NULL, " ");
//            printf("Killing pid: %s\n", kill_pid);

            // Kill only if pid is provided, else ignore
            if(kill_pid != NULL)
                kill_child(atoi(kill_pid));

        }
        else {
//            printf("External command: %s, :%lu\n", pass_input, strlen(pass_input));
            process_external_command(pass_input);
        }
        // Free only if allocated
        if(temp != NULL)
            free(temp);
        if(pass_input != NULL)
            free(pass_input);
        fflush(stdout);
    } else {
//        printf("Ignore\n");
    }


}

/*
 * Entry point, Main function
 */
int main(int argc, char **argv)
{
    char cmd_line[MAX_BUFFER_SIZE];

    // Register handler for SIGCHILD
    signal(SIGCHLD, sigchld_handler);
    while(1) {
        memset(cmd_line, '\0', MAX_BUFFER_SIZE);
        printf("tish>> ");
        fflush(stdout);

        //Take user input
        fgets(cmd_line, MAX_BUFFER_SIZE, stdin);

        handle_user_input(strtok(cmd_line, "\n"));
    }
    return 0;
}


/*
 * List the information for all active background children
 */
void list_jobs()
{
    int i = 0;
    char* pid_as_string = NULL;
    char ps_cmd[6] = "ps -p";
    for(i = 0; i < curr_idx; i++)
    {
        pid_as_string = (char*) malloc(sizeof(char) * 20);
        memset(pid_as_string, '\0', 20);
//        printf("PID: %d\n", child_pids[i]);
        // Generate ps command
        sprintf(pid_as_string, "%s %d", ps_cmd, child_pids[i]);
//        printf("PID as String: %s\n", pid_as_string);
        // Use own implementation to execute command
        process_external_command(pid_as_string);

        free(pid_as_string);
    }
}

/*
 * Handler for SIGINT
 */
void interrupt_handler(int signal_no)
{
    if(signal_no == SIGINT) {
//        printf("Caught SIGINT\n");
    }
}

/*
 * Returns the index of pid, if tracked
 */
int get_child_index(int pid)
{
    int i;
    for(i = 0; i < curr_idx; i++)
    {
        if(child_pids[i] == pid && i < MAX_CHILD)
            return i;
    }
    return -1;
}

/*
 * Remove child from the array
 * Shift array elements to left, to preserve the order
 */
void remove_child(int index)
{
    int i = index;

    while(i < (curr_idx - 1) && i < (MAX_CHILD - 1))
    {
        child_pids[i] = child_pids[i+1];
        i++;
    }
    child_pids[i] = 0;
    curr_idx--;
}

/*
 * Handler for SIGCHLD
 */
void sigchld_handler(int signal)
{
    int status = -1;
    pid_t pid;


    while((pid=waitpid(-1,&status,WNOHANG|WUNTRACED))>0)
    {
        if(WIFEXITED(status) || WIFSIGNALED(status) || WIFSTOPPED(status))
        {
//            printf("Child process finished: %d\n", pid);
            // Remove child if it already has not been removed
            if(get_child_index(pid) != -1)
            {
//                printf("Removing child entry\n");
                remove_child(get_child_index(pid));
            }

        }
    }

    return;
}

/*
 * Kill all active children before exiting
 */
void kill_all_children()
{
    int i;
    for(i = 0; i < curr_idx; i++)
    {
        if(i < MAX_CHILD)
        {
            kill(child_pids[i], SIGTERM);
            child_pids[i] = 0;
        }
    }

    // Once children are killed, reset curr_idx
    curr_idx = 0;
}

/*
 * Kill child with specific PID
 */
void kill_child(int pid)
{
    if(get_child_index(pid) != -1)
    {
//        printf("Child %d found, killing it\n", pid);
        remove_child(get_child_index(pid));
        kill(pid, SIGKILL);
    }
//    else
//        printf("Child %d NOT found\n", pid);
}