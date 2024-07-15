# Readers-Writers Problem

A program that synchronizes many processes (readers and writers) using semaphores and a shared memory segment.

The processes are created by the main program using fork and exec and they try to simultaniously read and write on the same file.

The problem occurs when a process wants to write on a record that another process is reading and vice versa or when a process wants to write on a record that another process is writing on.

This solution is starvation free and uses the method lock per process.

There's a limit to the number of readers or writers that we can have in total. The max number is defined in the code of the program.

To run the program we need to provide an exec file in the command line arguments that will have the exec commands for every reader and writer that will be executed:

## Run
Use the Makefile to compile, run and clean using the following commands:

```bash
make 
make run
make clean
```

Other exec commands:
```bash
./myprog exec1.txt
```
