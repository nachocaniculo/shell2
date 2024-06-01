# minishell

## Table of Contents

1. [Overview](#overview)
2. [Installation](#installation)
3. [Features](#features)
   - [Command Execution](#command-execution)
   - [Input and Output Redirection](#input-and-output-redirection)
   - [Background Execution](#background-execution)
   - [Internal Commands](#internal-commands)
     - [`cd`](#cd-command)
     - [`umask`](#umask-command)
     - [`exit`](#exit-command)
     - [`jobs`](#jobs-command)
     - [`fg`](#fg-command)
   - [Signal Handling](#signal-handling)
4. [Code Design](#code-design)
   - [Execution Strategy and Pipeline Management](#execution-strategy-and-pipeline-management)
   - [Background Implementation](#background-implementation)
   - [`jobs` and `fg` Commands](#jobs-and-fg-commands)
   - [Signal Handling Implementation](#signal-handling-implementation)
5. [Acknowledgments](#acknowledgments)
6. [License](#license)

## Overview

Reduced version of a real shell. It supports the execution of external commands, input and output redirection, command piping, background execution and various internal commands such as `cd`, `umask`, `exit`, `jobs`, and `fg`.

## Installation

### Prerequisites

Ensure that the GCC compiler is installed.

### Cloning the Repository

```shell
git clone https://github.com/antonioalanxs/minishell
cd minishell
```

### Usage

Compile the project and execute the `minishell` executable:

```shell
chmod u+x ./compile.sh
./compile.sh
chmod u+x ./minishell
./minishell
```

The minishell is now running. To exit the shell, simply execute the `exit` command.

## Features

### Command Execution

`minishell` can execute external commands and handle command piping, allowing users to chain commands together using the `|` character.

```shell
msh> ls | grep lib | wc -l
```

### Input and Output Redirection

Users can redirect command input, output, and errors using `<`, `>`, and `>&` respectively.

```shell
msh> head -3 < input.txt > output.txt &>error.txt
```

### Background Execution

Commands can be sent to the background using the `&` character, enabling users to continue using the shell while a command is running.

```shell
msh> sleep 20 &
[1] 4449
msh> sleep 500 &
[2] 6754
msh> sleep 30 &
[3] 7643
```

### Internal Commands

#### `cd` Command

Allows users to change the current working directory. If no directory is specified, changes the current working directory to `$HOME`.

```shell
msh> cd
msh> pwd
/home/user
msh> cd ./dir/dir2/dir3
msh> pwd
/home/user/dir/dir2/dir3
msh> cd ..
msh> pwd
/home/user/dir/dir2
```

#### `umask` Command

Enables users to change the system mask for file creation permissions.

```shell
msh> umask 0022
0022
msh> umask
0022
```

#### `exit` Command

Terminates all running processes associated with active jobs and exits the minishell.

#### `jobs` Command

Displays tasks running in the background.

```shell
msh> jobs
[2] Running       sleep 500 &
[3] Running       sleep 30 &
[1] Done          sleep 20 &
```

#### `fg` Command

Brings background tasks to the foreground.

```shell
msh> fg
sleep 500 &
```

```shell
msh> fg 3
sleep 30 &
```

### Signal Handling

Handles the `SIGNINT` (Ctrl-C) signal gracefully, ensuring that pressing it does not close the shell. If a command is running in the foreground, pressing Ctrl-C cancels its execution.

## Code Design

### Execution Strategy and Pipeline Management

The execution and pipeline management follow distinct approaches for different scenarios:

* **Single command execution**: The parent process forks, letting its child execute the command, and waits for it.

* **Execution of two commands**: The parent process performs the first fork, allowing the first child to execute the first command and waiting for it. After the first child finishes, it performs a second fork, waiting for the second child to execute the second command. If the two commands need to communicate, a pipeline is used.

* **Execution of more than two commands**: A two-pipeline system is employed, where depending on the command's position (even or odd), it reads from one pipe and writes to another. Two pipes, `p` and `p2`, are utilized. If the command is odd, it reads from `p` and writes to `p2`. If the command is even, it reads from `p2` and writes to `p`. The first command reads from standard input/its redirection, and the last command writes to standard output/its redirection. The parent process waits for each child to execute its corresponding command before invoking the next child.

### Background Implementation

Background execution is achieved without resorting to the conventional use of the `waitpid` command. This decision is made to allow users to continue using the minishell without waiting for the completion of running processes. Instead, processes will run continuously in the background.

When the user enters any instruction, a `waitpid` is performed with the `WHOHANG` flag to check if the processes have concluded or are still running. This approach enables smooth interaction with the `minishell`, as it does not pause to wait for the completion of background processes.

### `jobs` and `fg` Commands

The system maintains an array of jobs, each containing the user's command line, an array of process IDs (PIDs), and a boolean variable indicating whether the job has finished (all child processes have terminated). The array has a maximum capacity of 25 jobs, and each job can hold up to 50 PIDs.

#### `jobs`

The `jobs` command checks the status of each job using the `finished` function and displays the results. If a job has finished, its index is saved in an array of finished job indices for subsequent removal from the active jobs array.

#### `fg`

* **Without job number**: If no job number is specified, the `fg` command uses a restrictive `waitpid`, actively waiting for all child processes associated with the first job in the active jobs array to finish.

* **With job number**: If a job number is provided, the same action is performed for the job at the specified position in the array. When the job completes, it is removed from the active jobs array.

### Signal Handling Implementation

The signal handling implementation distinguishes the following cases:

* **Nothing is running in the foreground**: The signal is reprogrammed to display the prompt again.

* **Something is running in the foreground**: Default signal behavior. It terminates the ongoing execution and displays the prompt again.

* **Something is running in the background**: The signal is ignored.

## Acknowledgments

This minishell project is inspired by the bash shell, and understanding its functionality is enhanced by referring to the [bash manual](https://www.gnu.org/software/bash/manual/bash.html).

## License

This project is licensed under the **Apache License 2.0** - see the [LICENSE](LICENSE) file for details.
