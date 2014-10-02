#include "dsh.h"

void seize_tty(pid_t callingprocess_pgid); /* Grab control of the terminal for the calling process pgid.  */
void continue_job(job_t *j); /* resume a stopped job */
void spawn_job(job_t *j, bool fg); /* spawn a new job */
job_t *head_of_jobs = NULL; //keeps track of jobs

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

         if(fg) // if fg is set
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
	for(p = j->first_process; p; p = p->next) {
	  /* YOUR CODE HERE? */
	  /* Builtin commands are already taken care earlier */
	  
	  switch (pid = fork()) {
		
          case -1: /* fork failure */
            perror("fork");
            exit(EXIT_FAILURE);

          case 0: /* child process  */
			printf("line 68");
            p->pid = getpid();	    
            new_child(j, p, fg);
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
	if(fg){
			seize_tty(getpid()); // assign the terminal back to dsh
			printf("fg off, running in background\n");
		}
	else{
		printf("fg on, taking up terminal until done\n");
		int status = 0;
		waitpid(j->first_process->pid, &status, 0);
		//printf("pid %d complete\n", p->pid);
		if(head_of_jobs == j){
			head_of_jobs = j->next;
		}
		free_job(j);
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
            printf(" no jobs");
        }else{
            job_t *copyJobList;
            copyJobList = jobsList;
            int i;
            int updatedSize;
            updatedSize = 0;
            // loop through all jobs and check status
            for (i=0; i<jobsSize; i++) {
                char* s[20];
                if(job_is_completed(copyJobList)){
                    sprintf(s, "%d. %s (PID: %d)\n STATUS: COMPLETE\n", (updatedSize+1), copyJobList->commandinfo, (int) copyJobList->first_process->pid);
                    if(jobsSize == 1){
                        jobsList = NULL;
                    }else{
                        delete_job(jobsList, copyJobList);
                    }
                    jobsSize  -= 1;
                    i -= 1;
                }else if(job_is_stopped(copyJobList)){
                    sprintf(s, "%d. %s (PID: %d)\n STATUS: STOPPED\n", (updatedSize+1), copyJobList->commandinfo, (int) copyJobList->first_process->pid);
                }
                printf(s);
                copyJobList = copyJobList->next;
                updatedSize++;            }
            
        }
        
        last_job->first_process->completed = true;
        last_job->first_process->status = 0;
        return true;
    }
    else if (!strcmp("cd", argv[0])) {
        /* Your code here */
        //check path of child directory and exit the parent
        chdir(argv[1]);   //new directory
        // how to assign it back to shell
        
        last_job->first_process->completed = true;
        last_job->first_process->status = 0;
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
        
        last_job->first_process->completed = true;   //same as quiting from the current process
        last_job->first_process->status = 0;
        return true;
    }

}

/* Build prompt messaage */
char* promptmsg() 
{
	int this_id = getpid();
    /* Modify this to include pid */
	/*code from stackoverflow http://stackoverflow.com/questions/19265370/concatenate-string-and-int-type-variable-in-c?rq=1*/
	char string[]="dsh -";
	char cated_string[sizeof(string) + sizeof(this_id)];
	sprintf(cated_string,"%s%d$ ",string,this_id);
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
        //if(PRINT_INFO) print_job(head_of_jobs);
		
		while(j!=NULL){
			if(!builtin_cmd(j, j->first_process->argc, j->first_process->argv)){
				if(head_of_jobs == NULL){
					head_of_jobs = j;	
				}
				else{
					find_last_job(head_of_jobs)->next = j;
				}
				spawn_job(j, j->bg);
			}
			
			j = j->next;
		}
		
		
		
        /* Your code goes here */
        // We also need to figure out a way to give the new jobs ids
        //help
        //loop through the jobs? Most definitely
        job_t *x;
        x = j;
        process_t *p;
        
        while(x->next != NULL){
            
            p = x->first_process;
            while(p->next !=NULL){
                p->pid = getpid();
                p = p->next;
            }
            if(p->next == NULL){
                p->pid = getpid();
            }
            x->pgid = getpid();
            x = x->next;
        }
        if (x->next == NULL){
            
            p = x->first_process;
            while(p->next != NULL){
                p->pid = getpid();
                p = p->next;
            }
            if(p->next ==NULL){
               
                p->pid = getpid();
            }
            x->pgid = getpid();
        }
        process_t* checkProcess; //keep track of the processes
       
        /* You need to loop through jobs list since a command line can contain ;*/
        while (x->next !=NULL) {
            checkProcess = x->first_process; //set the initial running process
            /* Check for built-in commands */
            while (checkProcess->next !=NULL) {  //loop through all processes
                /* If not built-in */
                if (!(builtin_cmd(x, checkProcess->argc, checkProcess->argv))) {
                    spawn_job(x, !(x->bg));                }
                checkProcess = checkProcess->next;
            }
            /* What about the end of the processes? checkProcess->next == NULL*/
            if (checkProcess->next == NULL) {
                
                if (!(builtin_cmd(x, checkProcess->argc, checkProcess->argv))) {
                    spawn_job(x, !(x->bg));                }
                if(jobsList ==NULL){  //if there are no current jobs
                    jobsList = x;
                    lastJob = jobsList;
                } else{ //complete the jobs first then add the one requested to the end of the list
                    lastJob->next = x;
                    lastJob = lastJob->next;
                }
                jobsSize++;
            }
            x=x->next;
        }
        /* What to do at last job? when j->next ==NULL??  */
        if (x->next == NULL) {
            // still loop through the list of processes and spawn jobs
            //update jobs list
            //last job
            
            checkProcess = x->first_process;
            while (checkProcess->next !=NULL) {  //loop through all processes
                /* If not built-in */
                if (!(builtin_cmd(x, checkProcess->argc, checkProcess->argv))) {
                    spawn_job(x, !(j->bg));                }
                checkProcess = checkProcess->next;
            }
            if (checkProcess->next==NULL) {
            if (!(builtin_cmd(x, checkProcess->argc, checkProcess->argv)))
                spawn_job(x, !(j->bg));
            }
            if(jobsList ==NULL){  //if there are no current jobs
                jobsList = j;
                lastJob = jobsList;
            } else{ //complete the jobs first then add the one requested to the end of the list
                lastJob->next = j;
                lastJob = lastJob->next;
            }
            jobsSize++;        }
        
        
        /* If job j runs in foreground */
        /* spawn_job(j,true) */
        /* else */
        /* spawn_job(j,false) *
    }
}


