#include "dsh.h"

void seize_tty(pid_t callingprocess_pgid); /* Grab control of the terminal for the calling process pgid.  */
void continue_job(job_t *j); /* resume a stopped job */
void spawn_job(job_t *j, bool fg); /* spawn a new job */

job_t* jobsList;
job_t* lastJob;
int jobsSize;

/* Sets the process group id for a given job and process */
int set_child_pgid(job_t *j, process_t *p)
{
    if (j->pgid < 0) /* first child: use its pid for job pgid */
        j->pgid = p->pid;
    return(setpgid(p->pid,j->pgid));
}

/* Creates the context for a new child by setting the pid, pgid and tcsetpgrp */
void new_child(job_t *j, process_t *p, bool fg)
{
    /* establish a new process group, and put the child in
     * foreground if requested
     */
    
    /* Put the process into the process group and give the process
     * group the terminal, if appropriate.  This has to be done both by
     * the dsh and in the individual child processes because of
     * potential race conditions.
     * */
    
    p->pid = getpid();
    
    /* also establish child process group in child to avoid race (if parent has not done it yet). */
    set_child_pgid(j, p);
    
    if(!fg) // if fg is set NOTE CHANGED TO NOT HERE DUE TO ORIGINAL CALL
        seize_tty(j->pgid); // assign the terminal
    
    /* Set the handling for job control signals back to the default. */
    signal(SIGTTOU, SIG_DFL);
}

/* Spawning a process with job control. fg is true if the
 * newly-created process is to be placed in the foreground.
 * (This implicitly puts the calling process in the background,
 * so watch out for tty I/O after doing this.) pgid is -1 to
 * create a new job, in which case the returned pid is also the
 * pgid of the new job.  Else pgid specifies an existing job's
 * pgid: this feature is used to start the second or
 * subsequent processes in a pipeline.
 * */

void spawn_job(job_t *j, bool fg)
{
    pid_t pid;
    process_t *p;
	process_t* last_process;
	int pfd[2];
	pipe(pfd);
	int pfd2[2];
	pipe(pfd2);
	int counter = 0;
    for(p = j->first_process; p; p = p->next) {
		counter++;
		printf("counter: %d", counter);
        /* YOUR CODE HERE? */
        /* Builtin commands are already taken care earlier */
		int run1 = 0;
		int run2 = 0;
		
		int checkforlast = 0;
		
        if(p->next !=NULL && counter%2){
			printf("running pipe odd\n");
            pipe(pfd);
			run1 = 1;
		}
		if(counter % 2){
			run1 = 1;
		}
		
		if(p->next != NULL && !(counter%2)){
			printf("running pipe even\n");
            pipe(pfd2);
			run2 = 1;
		}
		if(!(counter%2)){
			run2 = 1;
		}
		
		if(p->next == NULL){
			checkforlast = 1;
		}
		
        switch (pid = fork()) {
                
            case -1: /* fork failure */
                perror("fork");
                exit(EXIT_FAILURE);
                
            case 0: /* child process  */
                p->pid = getpid();
                new_child(j, p, fg);
				
				 if (p->ifile!=NULL){
                    int in;
                    in = open(p->ifile, O_RDONLY);
                    if (in>0){
                        dup2(in,STDIN_FILENO);
                        close(in);
                    }
                    else{
                        perror("Input file does not exist");
                    }
                }

                if (p->ofile!=NULL){
                    int o;
                    o=creat(p->ofile, 0644);
                    dup2(o, STDOUT_FILENO);
                    close(o);
                }
				
				
				//pipe
				if (p == j->first_process && !checkforlast) {
					perror("loop 1");
					close(pfd[0]);
					close(1);
                    dup2(pfd[1],1); //write
                }
				
				else{
					if(run2){
						perror("loop 2 run even");
						close(pfd[1]);
						close(0);
						dup2(pfd[0],0); //read
						
						if(!checkforlast){
							close(pfd2[0]);
							close(1);
							dup2(pfd2[1],1); //write
						}
					}
					if(run1){
						perror("loop 2 run odd");
						close(pfd2[1]);
						close(0);
						dup2(pfd2[0],0); //read
						
						if(!checkforlast){
							close(pfd[0]);
							close(1);
							dup2(pfd[1],1); //write
						}
					}
                }
				
				
                //execute desired code
                execvp(p->argv[0], p->argv);
                
                /* YOUR CODE HERE?  Child-side code for new process. */
                perror("New child should have done an exec");
                exit(EXIT_FAILURE);  /* NOT REACHED */
                break;    /* NOT REACHED */
                
            default: /* parent */
                /* establish child process group */
                p->pid = pid;
                set_child_pgid(j, p);
                /* YOUR CODE HERE?  Parent-side code for new process.  */
				
        }
		
        printf("pid of new child: %d\n", pid);
        /* YOUR CODE HERE?  Parent-side code for new job.*/
        
    }
	close(pfd[0]);
	close(pfd[1]);
	close(pfd2[0]);
	close(pfd2[1]);
    if(fg){
        //seize_tty(getpid()); // assign the terminal back to dsh
		//would never have been assigned
        printf("fg off, running in background\n");
    }
    else{
        printf("fg on, taking up terminal until done\n");
        int status = 0;
		process_t* tempt = j->first_process;
		while(tempt != NULL){
			printf("waiting for: %d\n", tempt->pid);
			waitpid(tempt->pid, &status, WUNTRACED);
			tempt = tempt->next;
			printf("done waiting\n");
		}
        //printf("pid %d complete\n", p->pid);
		if(!WIFSTOPPED(status)){
			if(jobsList == j){
				jobsList = j->next;
			}
			j->first_process->completed = true; //process is completed
			free_job(j);
		}
		else{
			j->first_process->stopped = true; //process is stopped
			printf("job with pgid %d is stopped\n", j->pgid);
			j->notified = true;
		}
		seize_tty(getpid());
    }
}

/* Sends SIGCONT signal to wake up the blocked job */
void continue_job(job_t *j)
{
    if(kill(-j->pgid, SIGCONT) < 0)
        perror("kill(SIGCONT)");
}


/*
 * builtin_cmd - If the user has typed a built-in command then execute
 * it immediately.
 */
bool builtin_cmd(job_t *last_job, int argc, char **argv)
{
    
    /* check whether the cmd is a built in command
     */
    
    if (!strcmp(argv[0], "quit")) {
        
        /* Your code here */
        exit(EXIT_SUCCESS);
        last_job->first_process->completed = true;   //complete all processes after exiting
        last_job->first_process->status = 0;
        return true;
    }
    else if (!strcmp("jobs", argv[0])) {
        /* Your code here */
        // This is why we have to keep a list of all jobs
        
        //check if jobs in list
        //update size
        
        if (jobsList ==NULL) {
            //nothing
            printf("no jobs\n");
        }else{
            print_job(jobsList);
            
        }
		
        return true;
    }
    else if (!strcmp("cd", argv[0])) {
        /* Your code here */
        //check path of child directory and exit the parent
        chdir(argv[1]);   //new directory
        // how to assign it back to shell
        
        return true;
    }
    else if (!strcmp("bg", argv[0])) {
        /* Your code here */
        last_job->first_process->completed = true;   //Not required
        last_job->first_process->status = 0;
        return true;
    }
    else if (!strcmp("fg", argv[0])) {
        /* Your code here */
		//NEED TO ADJUST TO CHECK FOR ARGV0 TOO
		job_t* temp = jobsList;
		while(temp != NULL){
			if(temp->first_process->stopped = true){
				continue_job(temp); //resumes job
				seize_tty(temp->pgid); //gives it the terminal
				int status = 0;
				//don't need to check for bg because in fg function
				waitpid(temp->first_process->pid, &status, WUNTRACED); //resumes waiting on it
				//printf("pid %d complete\n", p->pid);
				if(!WIFSTOPPED(status)){
				//THIS IS WRONG CODE BLAH BLAH FIX TO REMOVE JOB FROM LIST
					if(jobsList == temp){
						jobsList = temp->next;
					}
					temp->first_process->completed = true; //process is completed
					free_job(temp);
				}
				else{
					temp->first_process->stopped = true; //process is stopped
				}
				seize_tty(getpid());
				return true;
			}
			temp = temp->next;
		}
		return true;
    }
    return false;       /* not a builtin command */
}

/* Build prompt message */
char* promptmsg()
{
    /* Modify this to include pid */
    //char* message[16];
   // sprintf(message, "dsh-%d$ ",(int) getpid());
	char string[]="dsh ";
	int number=getpid();
	char end[] = "$";
	char cated_string[sizeof(string) + sizeof(number) + sizeof(end)];
	sprintf(cated_string,"%s%d%s",string,number,end);
    return cated_string;
}

int main()
{
    
    init_dsh();
    DEBUG("Successfully initialized\n");
    
    while(1) {
        job_t *j = NULL;
        if(!(j = readcmdline(promptmsg()))) {
            if (feof(stdin)) { /* End of file (ctrl-d) */
                fflush(stdout);
                printf("\n");
                exit(EXIT_SUCCESS);
            }
            continue; /* NOOP; user entered return or spaces with return */
        }
        
        /* Only for debugging purposes to show parser output; turn off in the
         * final code */
        //  if(PRINT_INFO) print_job(j);
        
        /* Your code goes here */
        // We also need to figure out a way to give the new jobs ids
        //help
        //loop through the jobs? Most definitely
        
        while(j!=NULL){
            if(!builtin_cmd(j, j->first_process->argc, j->first_process->argv)){
                if(jobsList == NULL){
                    jobsList = j;
                }
                else{
                    find_last_job(jobsList)->next = j;
                }
                spawn_job(j, j->bg); //bg is actually reverse of fg so reversed in all other functions
            }
            
            j = j->next;
        }
        
    }
}