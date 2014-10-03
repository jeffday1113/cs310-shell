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
	//initializes pipes so that close calls don't give errors on a single process in job
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
		
		//check if divisible by 1 and refresh pipe1
        if(p->next !=NULL && counter%2){
			printf("running pipe odd\n");
            pipe(pfd);
			run1 = 1;
		}
		if(counter % 2){
			run1 = 1;
		}
		
		//check if divisible by 2 and refresh pipe2
		if(p->next != NULL && !(counter%2)){
			printf("running pipe even\n");
            pipe(pfd2);
			run2 = 1;
		}
		if(!(counter%2)){
			run2 = 1;
		}
		
		//last process in job
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
				
				//checks for input file
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
				
				//checks for output file
                if (p->ofile!=NULL){
                    int o;
                    o=creat(p->ofile, 0644);
                    dup2(o, STDOUT_FILENO);
                    close(o);
                }
				
				
				//pipe first time
				if (p == j->first_process && !checkforlast) {
					perror("loop 1");
					close(pfd[0]);
					close(1);
                    dup2(pfd[1],1); //write
                }
				//manipulates two pipelines to prevent overwrites
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
	close(pfd[0]); //close pipes
	close(pfd[1]);
	close(pfd2[0]);
	close(pfd2[1]);
    if(fg){ //do nothing if in background
    }
    else{
        int status = 0;
		process_t* tempt = j->first_process;
		while(tempt != NULL){ //wait for all children processes
			waitpid(tempt->pid, &status, WUNTRACED);
			tempt = tempt->next;
		}
		
		if(!WIFSTOPPED(status)){ //if process is finished, update jobList
			if(jobsList->next == NULL){
				jobsList = NULL;
			}
			else{
				job_t* temp = jobsList;
				while(temp != NULL){
					if(temp->next == j){
						temp->next = j->next;
					}
					temp = temp->next;
				}
			}
			free_job(j);
		}
		else{ //if process is stopped, alert all the children processes
			process_t* pp = j->first_process;
			while(pp != NULL){
				pp->stopped = true; //process is stopped
				pp = pp->next;
			}
			printf("job with pgid %d is stopped\n", j->pgid);
			j->notified = true;
		}
		
    }
	seize_tty(getpid()); 
}

/* Sends SIGCONT signal to wake up the blocked job */
void continue_job(job_t *j)
{
    if(kill(-j->pgid, SIGCONT) < 0)
        perror("kill(SIGCONT)");
		
	j->notified = false;
	process_t* p = j->first_process; //alert processes that they are no longer stopped
	while(p != NULL){
		p->stopped = false;
		p = p->next;
	}
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
        //check if jobs in list
        //update size
        
        if (jobsList ==NULL) {
            //nothing
            printf("no jobs\n");
        }else{
            print_job(jobsList); //print all jobs
            
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
		job_t* temp = jobsList;
		while(temp != NULL){ //loop through jobs to find stopped job
			if(job_is_stopped(temp)){
				continue_job(temp); //only need to continue it, no need to monitor
				return true; //only continue one job
			}
			temp = temp->next;
		}
		
        return true;
    }
    else if (!strcmp("fg", argv[0])) { //move to foreground
        /* Your code here */
		job_t* temp = jobsList;
		while(temp != NULL){
			if(job_is_stopped(temp)){ //check for first stopped job
				continue_job(temp); //resumes job
				seize_tty(temp->pgid); //gives it the terminal
				int status = 0;
				printf("fg on, taking up terminal until done\n");
				
				process_t* tempt = temp->first_process;
				while(tempt != NULL){ //waiting for process
					waitpid(tempt->pid, &status, WUNTRACED);
					tempt = tempt->next;
				}
		
				if(!WIFSTOPPED(status)){ //if the job completes
					if(jobsList->next == NULL){ //check if it was only job
						jobsList = NULL;
					}
					else{
						job_t* tempCheck = jobsList; //if not, find its location and redirect pointers
						while(tempCheck != NULL){
							if(tempCheck->next == temp){
								tempCheck->next = temp->next;
							}
							tempCheck = tempCheck->next;
						}
					}
					free_job(temp); 
				}
				else{ //if job is stopped again
					process_t* pp = temp->first_process;
					while(pp != NULL){
						pp->stopped = true; //process is stopped, alert all children
						pp = pp->next;
					}
					temp->notified = true;
				}
				seize_tty(getpid()); //give back terminal to shell
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
   //code from stackoverflow, useing sprintf to concatanate int and string
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
        
        while(j!=NULL){
            if(!builtin_cmd(j, j->first_process->argc, j->first_process->argv)){ //check if builtin, if not create a new job
                if(jobsList == NULL){
                    jobsList = j; //initialize jobsList if it is null
                }
                else{
                    find_last_job(jobsList)->next = j;
                }
                spawn_job(j, j->bg); //bg is actually reverse of fg so reversed in all other functions
            }
            
            j = j->next; //read next line
        }
        
    }
}