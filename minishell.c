#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>

#include "parser.h"

/**
 * Maximum number of characters per input line. Adjusting this value allows
 * controlling the maximum length of input lines.
 */
#define MAXIMUM_LINE_LENGTH 1024

/**
 * Text string for the command line prompt when waiting for user input.
 */
#define PROMPT "msh> "

/**
 * Result code for the child process in a fork operation.
 */
#define FORK_CHILD 0

/**
 * Argument to the `exit()` function to indicate that the program encountered
 * an error or failure during execution signaling to the operating system that
 * an issue occurred.
 */
#define EXIT_FAILURE 1

/**
 * Mode for opening a file when reading. This mode is used in `fopen()` to
 * specify read-only access to the file.
 */
#define FILE_READ "r"

/**
 * Mode for opening a file when writing. This mode is used in `fopen()` to
 * specify write-only access to the file.
 */
#define FILE_WRITE "w"

/**
 * Pipe in file descriptors array.
 */
#define PIPE 2

/**
 * Read end of a pipe.
 */
#define PIPE_READ 0

/**
 * Write end of a pipe.
 */
#define PIPE_WRITE 1

/**
 * Environment variable representing the user's home directory.
 */
#define HOME "HOME"

/**
 * Index representing the command part of an argument array.
 */
#define COMMAND 0

/**
 * Index representing the directory part of an argument array.
 */
#define DIRECTORY 1

/**
 * Default Unix mask value for file permissions.
 *
 * Represent the default Unix mask value for file permissions. It is set to 18
 * in octal, which corresponds to 022 in decimal.
 */
#define DEFAULT_UNIX_MASK 18

/**
 * Default Unix formatted mask value for file permissions which is used for
 * display purposes.
 */
#define DEFAULT_UNIX_FORMATTED_MASK 22;

/**
 * Index representing the mask part of an argument array.
 */
#define MASK 1

/**
 * Define the maximum number of processes a job can have.
 */
#define MAXIMUM_PID_LIST_SIZE 25

/**
 * Maximum size allowed for the list of active jobs in the shell.
 */
#define MAXIMUM_JOB_LIST_SIZE 50

/**
 * Signal number used for the kill system call to forcefully terminate a
 * process.
 */
#define KILL 9

/**
 * Index representing the job part of an argument array.
 */
#define JOB 1

/**
 * Wait for the specified child process. It is used as the options
 * argument in the `waitpid` function.
 *
 * Example usage:
 * ```c
 * waitpid(pid, NULL, WAIT);
 * ```
 */
#define WAIT 0

/**
 * Structure representing a job in the shell.
 *
 * Fields:
 *   - instruction: The instruction associated with the job.
 *   - size: The number of processes in the job.
 *   - pids: Array of process identifiers within the job.
 *   - finished: Flag indicating whether the job has finished.
 */
typedef struct
{
    char instruction[MAXIMUM_LINE_LENGTH];
    int size;
    pid_t pids[MAXIMUM_PID_LIST_SIZE];
    int finished;
} tjob;

/**
 * Structure representing the list of active jobs in the shell.
 *
 * Fields:
 *   - list: Pointer to the array of `tjob` structures.
 *   - size: The current size of the list (number of active jobs).
 */
typedef struct
{
    tjob *list;
    int size;
} tjobs;

void store(int *stdinfd, int *stdoutfd, int *stderrfd);
void redirect(const tline *line);
void auxiliarRedirect(char *filename, const char *MODE, const int STD_FILENO);
void run(const tline *line, const int number);
void restore(const int stdinfd, const int stdoutfd, const int stderrfd);
void executeExternalCommands(const tline *line, tjobs *jobs, const char buffer[]);
void mshcd(const char *directory);
void mshumask(const char *mask, int *formattedMask);
void printMask(const int mask);
int octal(const char *number);
void mshexit(tjobs *jobs);
void mshjobs(tjobs *jobs);
int finished(tjob *job);
void mshfg(const char *job, tjobs *jobs);
void delete(const int job, tjobs *jobs);
void ctrlc();
void ctrlc2();

int main(void)
{
    char buffer[MAXIMUM_LINE_LENGTH];
    tline *line;
    char **firstCommandArguments;
    int formattedMask;
    tjobs jobs;

    formattedMask = DEFAULT_UNIX_FORMATTED_MASK;
    umask(DEFAULT_UNIX_MASK);

    jobs.list = malloc(sizeof(tjob) * MAXIMUM_JOB_LIST_SIZE);
    jobs.size = 0;

    signal(SIGINT, ctrlc);

    printf(PROMPT);
    while (fgets(buffer, MAXIMUM_LINE_LENGTH, stdin))
    {
        line = tokenize(buffer);

        if (line == NULL || line->ncommands < 1)
        {
            printf(PROMPT);
            continue;
        }

        firstCommandArguments = line->commands[0].argv;

        if (strcmp(firstCommandArguments[COMMAND], "cd") == 0)
        {
            mshcd(firstCommandArguments[DIRECTORY]);
        }
        else if (strcmp(firstCommandArguments[COMMAND], "umask") == 0)
        {
            mshumask(firstCommandArguments[MASK], &formattedMask);
        }
        else if (strcmp(firstCommandArguments[COMMAND], "exit") == 0)
        {
            mshexit(&jobs);
        }
        else if (strcmp(firstCommandArguments[COMMAND], "jobs") == 0)
        {
            mshjobs(&jobs);
        }
        else if (strcmp(firstCommandArguments[COMMAND], "fg") == 0)
        {
            mshfg(firstCommandArguments[JOB], &jobs);
        }
        else
        {
            executeExternalCommands(line, &jobs, buffer);
        }

        printf(PROMPT);
    }

    return 0;
}

/**
 * Store the standard input, output, and error file descriptors for later
 * restoration.
 *
 * @param stdinfd Pointer to the variable to store the original standard input
 * file descriptor.
 * @param stdoutfd Pointer to the variable to store the original standard
 * output file descriptor.
 * @param stderrfd Pointer to the variable to store the original standard error
 * file descriptor.
 */
void store(int *stdinfd, int *stdoutfd, int *stderrfd)
{
    dup2(STDERR_FILENO, *stderrfd);
    dup2(STDIN_FILENO, *stdinfd);
    dup2(STDOUT_FILENO, *stdoutfd);
}

/**
 * Redirect standard input, output, and error based on the information provided
 * in the given command line structure.
 *
 * @param line A pointer to a `tline` structure representing the command line.
 * @param stdinfd Pointer to the variable to store the original standard input
 * file descriptor.
 * @param stdoutfd Pointer to the variable to store the original standard
 * output file descriptor.
 * @param stderrfd Pointer to the variable to store the original standard error
 * file descriptor.
 */
void redirect(const tline *line)
{
    if (line->redirect_error != NULL)
    {
        auxiliarRedirect(line->redirect_error, FILE_WRITE, STDERR_FILENO);
    }

    if (line->redirect_input != NULL)
    {
        auxiliarRedirect(line->redirect_input, FILE_READ, STDIN_FILENO);
    }

    if (line->redirect_output != NULL)
    {
        auxiliarRedirect(line->redirect_output, FILE_WRITE, STDOUT_FILENO);
    }
}

/**
 * Auxiliary function for redirecting a specific file descriptor based on the
 * given filename and mode.
 *
 * @param filename The name of the file to be used for redirection.
 * @param MODE The mode to be used in `fopen()` for opening the file (e.g.,
 * `FILE_READ`, `FILE_WRITE`).
 * @param STD_FILENO The standard file descriptor to be redirected (e.g.,
 * `STDIN_FILENO`, `STDOUT_FILENO`).
 */
void auxiliarRedirect(char *filename, const char *MODE, const int STD_FILENO)
{
    FILE *file;
    int fd;

    file = fopen(filename, MODE);
    if (file == NULL)
    {
        fprintf(stderr, "%s: Error. %s\n", filename, strerror(errno));
    }

    fd = fileno(file);

    dup2(fd, STD_FILENO);

    fclose(file);
    close(fd);
}

/**
 * Run a command specified by the given command line structure.
 *
 * @param line A pointer to a `tline` structure representing the command line.
 * @param number The index of the command to be ran within the command line.
 *
 * If the command execution fails, an error message is printed to `stderr`
 * indicating that was not found, and the program exits with a failure status.
 */
void run(const tline *line, const int number)
{
    char **arguments;
    char *command;

    arguments = line->commands[number].argv;
    command = arguments[COMMAND];

    execvp(command, arguments);

    fprintf(stderr, "%s: Command not found\n", command);
    exit(EXIT_FAILURE);
}

/**
 * Restore the original standard input, output, and error file descriptors.
 *
 * @param stdinfd The original standard input file descriptor.
 * @param stdoutfd The original standard output file descriptor.
 * @param stderrfd The original standard error file descriptor.
 */
void restore(const int stdinfd, const int stdoutfd, const int stderrfd)
{
    dup2(stderrfd, STDERR_FILENO);
    dup2(stdinfd, STDIN_FILENO);
    dup2(stdoutfd, STDOUT_FILENO);
}

/**
 * Execute a series of commands specified in the given command line structure.
 *
 * @param line A data structure representing a command line with multiple
 * commands.
 * @param jobs A pointer to the structure representing the list of active jobs
 * which could be updated if the command line is executed in background.
 * @param buffer A buffer where the command line instruction is stored.
 *
 * Take a `tline` command line structure as input and executes the commands
 * sequentially managing the flow of input and output through pipes. Also
 * updates the `jobs` data structure if the command line is executed in
 * background.
 *
 * Note:
 *   This function relies on the `parser.h` library and auxiliary functions
 *   like `store`, `redirect`, `run`, `restore`, and assumes the existence of
 *   constants like `PIPE_READ`, `PIPE_WRITE`, etc.
 */
void executeExternalCommands(const tline *line, tjobs *jobs, const char buffer[])
{
    int stdinfd, stdoutfd, stderrfd;
    int commands, command;
    int next, even, last, background;
    pid_t pid;
    int p[PIPE], p2[PIPE];
    tjob *currentJob;

    signal(SIGINT, ctrlc2);

    store(&stdinfd, &stdoutfd, &stderrfd);

    commands = line->ncommands;
    next = commands > 1;
    background = line->background == 1;

    if (next)
    {
        pipe(p);
    }

    pid = fork();

    if (pid == FORK_CHILD)
    {
        redirect(line);

        if (next)
        {
            // Does not receive input from anyone
            close(p[PIPE_READ]);

            // Pipe `stdout` output for next command input
            dup2(p[PIPE_WRITE], STDOUT_FILENO);
            close(p[PIPE_WRITE]);
        }

        run(line, 0);
    }
    else
    {
        // Only reads from pipe to provide input for next command
        close(p[PIPE_WRITE]);

        // Clears `stdout` and `stdin` in case there are following commands
        restore(stdinfd, stdoutfd, stderrfd);

        if (background)
        {
            currentJob = &jobs->list[jobs->size];

            strcpy(currentJob->instruction, buffer);
            currentJob->size = commands;
            currentJob->pids[0] = pid;
            currentJob->finished = 0;

            jobs->size = (jobs->size + 1) % MAXIMUM_JOB_LIST_SIZE;

            if (!next)
            {
                printf("[%i] %i\n", jobs->size, pid);
            }
        }
        else
        {
            wait(NULL);
        }

        for (command = 1; next && command < commands; command++)
        {
            even = command % 2 == 0;
            last = command == commands - 1;

            // Restore the child process writing pipe to prevent errors
            if (even)
            {
                pipe(p);
            }
            else
            {
                pipe(p2);
            }

            pid = fork();

            if (pid == FORK_CHILD)
            {
                redirect(line);

                // Reads from one pipe and writes to another based on parity
                if (even)
                {
                    dup2(p2[PIPE_READ], STDIN_FILENO);
                    if (!last)
                    {
                        dup2(p[PIPE_WRITE], STDOUT_FILENO);
                    }
                }
                else
                {
                    dup2(p[PIPE_READ], STDIN_FILENO);
                    if (!last)
                    {
                        dup2(p2[PIPE_WRITE], STDOUT_FILENO);
                    }
                }
                close(p[PIPE_READ]);
                close(p[PIPE_WRITE]);
                close(p2[PIPE_READ]);
                close(p2[PIPE_WRITE]);

                run(line, command);
            }
            else
            {
                if (even)
                {
                    dup2(STDIN_FILENO, p[PIPE_WRITE]);
                    close(p[PIPE_WRITE]);

                    // Closes unnecessary pipes for the next odd child
                    close(p2[PIPE_READ]);
                    if (last)
                    {
                        close(p[PIPE_READ]);
                        close(p2[PIPE_WRITE]);
                    }
                }
                else
                {
                    dup2(STDIN_FILENO, p2[PIPE_WRITE]);
                    close(p2[PIPE_WRITE]);

                    // Closes unnecessary pipes for the next even child
                    close(p[PIPE_READ]);
                    if (last)
                    {
                        close(p2[PIPE_READ]);
                        close(p[PIPE_WRITE]);
                    }
                }

                if (background)
                {
                    currentJob->pids[command] = pid;

                    if (last)
                    {
                        printf("[%i] %i\n", jobs->size, pid);
                    }
                }
                else
                {
                    wait(NULL);
                }
            }
        }
        
        // Finish by cleaning `stdout` and `stdin` again for next iteration
        restore(stdinfd, stdoutfd, stderrfd);
    }

    signal(SIGINT, ctrlc);
}

/**
 * Changes the current working directory.
 *
 * Changes the current working directory to the specified directory. If no
 * directory is provided (NULL), it changes to the HOME directory.
 *
 * @param directory The path of the target directory. If NULL, changes to the
 * HOME directory.
 */
void mshcd(const char *directory)
{
    if (directory == NULL)
    {
        chdir(getenv(HOME));
    }
    else
    {
        chdir(directory);
    }
}

/**
 * Set the umask value based on the provided mask and update the formatted mask.
 *
 * @param mask The octal string representing the new umask value.
 * @param formattedMask Pointer to the variable to store the formatted mask.
 */
void mshumask(const char *mask, int *formattedMask)
{
    int mappedMask;

    if (mask == NULL)
    {
        printMask(*formattedMask);
        return;
    }

    if (!octal(mask))
    {
        fprintf(stderr, "%s: Error. Invalid argument\n", mask);
        return;
    }

    sscanf(mask, "%o", &mappedMask);
    umask(mappedMask);

    *formattedMask = atoi(mask);
    printMask(*formattedMask);
}

/**
 * Prints the octal representation of a file permissions mask.
 *
 * This function takes an integer `mask` representing a file permissions mask
 * and prints its octal representation. It ensures that the printed octal
 * number always consists of at least four digits by padding with leading zeros
 * if needed.
 *
 * @param mask An integer representing the file permissions mask.
 *
 * Example:
 *   printMask(644);  // Output: 0644
 *   printMask(7);    // Output: 0007
 */
void printMask(const int mask)
{
    int auxiliarMask;
    int zeros;
    int _;

    auxiliarMask = mask;
    zeros = 4;

    // Calculate the number of leading zeros needed
    while (auxiliarMask > 0)
    {
        auxiliarMask = auxiliarMask / 10;
        zeros--;
    }

    // Print leading zeros
    for (_ = 0; _ < zeros; _++)
    {
        printf("0");
    }

    printf("%i\n", mask);
}

/**
 * Check if a given string represents a valid octal number.
 *
 * Ensure that the string is not NULL, has a length of at most 4 characters,
 * and each character represents a valid octal digit (0-7).
 *
 * @param number The string to be checked for octal validity.
 * @return 1 if the string is a valid octal number, 0 otherwise.
 */
int octal(const char *number)
{
    int length;
    int index;
    int mappedDigit;

    if (number == NULL)
    {
        return 0;
    }

    length = strlen(number);

    if (length > 4)
    {
        return 0;
    }

    for (index = 0; index < length; index++)
    {
        mappedDigit = number[index] - 48;

        if (mappedDigit < 0 || mappedDigit > 7)
        {
            return 0;
        }
    }

    return 1;
}

/**
 * Terminate all running processes associated with active jobs and exit the
 * shell.
 *
 * Iterates through the list of active jobs, terminates each process within the
 * job, frees memory associated with the job list and exits the shell.
 *
 * @param jobs A pointer to the structure representing the list of active jobs.
 */
void mshexit(tjobs *jobs)
{
    int j, pid;
    int jobsSize, jobSize;
    tjob *job;

    jobsSize = jobs->size;

    for (j = 0; j < jobsSize; j++)
    {
        job = &jobs->list[j];
        jobSize = job->size;

        for (pid = 0; pid < jobSize; pid++)
        {
            kill(job->pids[pid], KILL);
        }
    }

    free(jobs->list);

    exit(EXIT_SUCCESS);
}

/**
 * Display the status of jobs in the provided job list.
 *
 * Checks the status of each job in the list and prints whether it is done or
 * running.
 *
 * If a job is done, it prints its completion status and saves it in
 * `finishedJobs` local variable to remove it later.
 *
 * @param jobs A pointer to the structure representing the list of active jobs.
 */
void mshjobs(tjobs *jobs)
{
    int j, jobsSize, formattedJ;
    tjob *job;
    int finishedJobs[MAXIMUM_JOB_LIST_SIZE];
    int finishedJobsSize = 0;

    jobsSize = jobs->size;

    for (j = 0; j < jobsSize; j++)
    {
        job = &jobs->list[j];

        formattedJ = j + 1;

        if (finished(job))
        {
            printf("[%i] Done\t%s", formattedJ, job->instruction);

            finishedJobs[finishedJobsSize] = j;
            finishedJobsSize = (finishedJobsSize + 1) % MAXIMUM_JOB_LIST_SIZE;
        }
        else
        {
            printf("[%i] Running\t%s", formattedJ, job->instruction);
        }
    }

    for (j = 0; j < finishedJobsSize; j++)
    {
        delete (finishedJobs[j], jobs);
    }
}

/**
 * Check if a job has completed.
 *
 * A job is considered finished when all of its commands have completed.
 *
 * @param job The structure representing the job.
 * @return 1 if all commands of the job have finished, 0 otherwise.
 */
int finished(tjob *job)
{
    int index;
    int jobSize;
    int pid;
    int finished;

    if (job->finished == 1)
    {
        return 1;
    }

    jobSize = job->size;

    for (index = 0; index < jobSize; index++)
    {
        pid = job->pids[index];

        finished = waitpid(pid, NULL, WNOHANG) == pid;

        if (!finished)
        {
            return 0;
        }
    }

    job->finished = 1;

    return 1;
}

/**
 * Execute the specified job in the foreground, waiting for its completion.
 *
 * Take a job identifier and a pointer to a structure containing currently
 * running jobs. It brings the specified job to the foreground, waits for its
 * completion, and then updates the job information.
 *
 * If the specified job identifier is invalid or the job has already terminated,
 * appropriate error messages are displayed.
 *
 * @param job A string representing the job identifier or number to be brought
 * to the foreground.
 * @param jobs A pointer to the structure representing the list of active jobs.
 */

void mshfg(const char *job, tjobs *jobs)
{
    int mappedJob;
    tjob *ranJob;
    int jobSize;
    int index;

    if (job == NULL)
    {
        mshfg("1", jobs);
        return;
    }

    if (jobs->size == 0)
    {
        printf("fg: There are no jobs available\n");
        return;
    }

    mappedJob = atoi(job) - 1;

    if (mappedJob < 0 || mappedJob > jobs->size - 1)
    {
        fprintf(stderr, "fg: Error. No such job\n");
        return;
    }

    signal(SIGINT, SIG_IGN);

    ranJob = &jobs->list[mappedJob];

    if (finished(ranJob))
    {
        printf("fg: job has terminated\n");
        printf("[%s] Done\t%s", job, ranJob->instruction);
    }
    else
    {
        printf("%s", ranJob->instruction);

        jobSize = ranJob->size;

        for (index = 0; index < jobSize; index++)
        {
            waitpid(ranJob->pids[index], NULL, WAIT);
        }
    }

    delete (mappedJob, jobs);

    signal(SIGINT, ctrlc);
}

/**
 * Delete a inactive job from a list of active jobs.
 *
 * @param job Inactive job that will be removed from active jobs list.
 * @param jobs A pointer to the structure representing the list of active jobs.
 */
void delete(const int job, tjobs *jobs)
{
    int index, jobsSize;

    jobsSize = jobs->size;

    for (index = job; index < jobsSize; index++)
    {
        jobs->list[index] = jobs->list[index + 1];
    }

    jobs->size = (jobs->size - 1) % MAXIMUM_JOB_LIST_SIZE;
}

/**
 * Signal handler for the `Ctrl+C` signal (`SIGINT`).
 *
 * When `Ctrl+C` is pressed, it prints a newline character to move to a new
 * line, displays the shell prompt, and flushes the standard output.
 */
void ctrlc()
{
    printf("\n");
    printf(PROMPT);
    fflush(stdout);
}

/**
 * Signal handler for the `Ctrl+C` signal (`SIGINT`).
 *
 * When `Ctrl+C` is pressed, it prints a newline character to move to a new
 * line.
 */
void ctrlc2()
{
    printf("\n");
}
