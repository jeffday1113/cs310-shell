Devil Shell Lab

Team Members: 


 	Andrew Gauthier - 
 	Jeff Day -
 	Clive Mudanda – cmm85

Time Spent: ~ hours

For implementation of commands from the command line of our shell we first checked whether the job was already built in in the builtin_cmd() method. If the job isn’t built in then the main sends it to the spawn_job() with a Boolean that marks whether it runs in the background or in the foreground.

For specific built in commands,  the method returns true if the passed in command is found and false otherwise. Also, when a command if found in the built in method, we mark is as completed assuming it has already been executed.The way we looks for the jobs is just by looping through the job_t structure and add it to our running processes.

When we spawn the jobs, we first create a pipe before we fork into our child processes inorder to be able to run multiple processes. For the child process, we first take care of redirection i.e. redirecting the input and output files then pipe the process before actually perfoming an execvp. The pipe is then closed after the parent process had returned since we wont need to use it any more.




