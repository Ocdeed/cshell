# cshell - A Unix Shell in C

A minimal yet feature-rich Unix shell implementation in C, built from scratch for educational purposes.

## Features

### Core Features
- **Built-in Commands**: `cd`, `pwd`, `echo`, `exit`, `help`, `jobs`, `fg`, `bg`, `kill`, `set`, `export`, `history`
- **I/O Redirection**: `< input`, `> output`, `>> append`
- **Pipes**: Chain commands with `|`
- **Background Jobs**: Run commands with `&`
- **Command History**: Stores last 100 commands

### Job Control
- Background job tracking with job IDs
- Foreground/background job switching (`fg`, `bg`)
- Signal sending with `kill`

## Quick Start

### Build
```bash
make
```

### Run
```bash
./cshell
```

### Clean Build Artifacts
```bash
make clean
```

### Debug Build (with AddressSanitizer)
```bash
make debug
```

## Usage Examples

```bash
# Basic commands
cshell:~$ pwd
cshell:~$ echo Hello World

# Change directory
cshell:~$ cd /tmp
cshell:/tmp~$ cd -
/home/user
cshell:~$ 

# I/O Redirection
cshell:~$ echo hello > file.txt
cshell:~$ cat < file.txt
hello

# Pipes
cshell:~$ ls -la | head -5

# Background jobs
cshell:~$ sleep 10 &
[1] 12345
cshell:~$ jobs
[1] Running sleep 10 &
cshell:~$ fg %1

# Environment variables
cshell:~$ set MYVAR hello
cshell:~$ echo $MYVAR
hello
cshell:~$ export MYVAR
```

## Project Structure

```
cshell/
├── Makefile           # Build system
├── include/
│   ├── shell.h       # Core types & declarations
│   ├── parser.h      # Parser declarations
│   ├── executor.h    # Execution engine declarations
│   ├── builtins.h    # Built-in command declarations
│   └── jobs.h        # Job control declarations
└── src/
    ├── main.c        # Entry point + REPL loop
    ├── parser.c      # Tokenizer + command parsing
    ├── executor.c    # Process creation, execvp, pipes
    ├── builtins.c    # cd, exit, pwd, echo, etc.
    └── jobs.c        # Background job management
```

## Architecture

### REPL Loop
The shell uses a Read-Eval-Print Loop:
1. **Read**: Get input from user
2. **Parse**: Convert text to command structures
3. **Execute**: Fork processes and run commands
4. **Loop**: Return to step 1

### Module Responsibilities

| Module | Responsibility |
|--------|----------------|
| `main.c` | REPL loop, prompt display |
| `parser.c` | Tokenization, command parsing |
| `executor.c` | `fork()`, `execvp()`, redirections, pipes |
| `builtins.c` | In-process commands (cd, echo, etc.) |
| `jobs.c` | Background job tracking, signals |

### Key System Calls

- `fork()` - Create new process
- `execvp()` - Replace process image with new program
- `pipe()` - Create IPC channel
- `dup2()` - Set up file descriptor redirections
- `waitpid()` - Wait for child process
- `chdir()` - Change working directory

## Technical Interview Topics

This project demonstrates understanding of:

1. **Process Management**
   - `fork()` vs `vfork()`, copy-on-write semantics
   - Process lifecycle: parent vs child execution
   - Zombie process prevention with `waitpid()`

2. **IPC (Inter-Process Communication)**
   - Anonymous pipes for `|` operator
   - File descriptors as IPC channels
   - `dup2()` for redirecting stdin/stdout

3. **Signal Handling**
   - `SIGCHLD` for child status changes
   - `SIGINT` (Ctrl+C), `SIGTSTP` (Ctrl+Z)
   - Signal masks and `sigaction()`

4. **Memory Management**
   - Dynamic allocation for command structures
   - Proper cleanup to prevent leaks
   - Buffer management

5. **Job Control**
   - Process groups
   - Foreground vs background execution
   - Terminal control

## Building Blocks Explained

### fork() and execvp()

```c
pid_t pid = fork();

if (pid == 0) {
    // Child process
    execvp(cmd, args);  // Replace with new program
} else {
    // Parent process
    waitpid(pid, &status, 0);  // Wait for child
}
```

### Redirections with dup2()

```c
int fd = open("file.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
dup2(fd, STDOUT_FILENO);  // stdout now writes to file
close(fd);
```

### Pipes

```c
int pipefd[2];
pipe(pipefd);  // pipefd[0] = read, pipefd[1] = write

if (fork() == 0) {
    dup2(pipefd[1], STDOUT_FILENO);  // Child writes to pipe
    close(pipefd[0]);
    close(pipefd[1]);
    execvp(...);
}
```

## Requirements

- GCC or Clang compiler
- POSIX-compliant system (Linux, macOS)
- GNU Make

## License

MIT License - feel free to use for learning and education.

## Contributing

This is an educational project. Issues and improvements welcome!
