[system programming lecture]
[20181662 이건영]

-project 1 phase 3

myshell.h
        CS:APP3e functions

myshell.c
        <phase1>
        Simple shell can execute the basic internal shell commands. 
        - cd : to navigate the directories in shell
        - ls : listing the directory contents
        - etc...

        <phase2>
        Can execute commands with pipe. 
        - usage : command1 {args} | command2 {args}

        <phase3>
        Can run processes in background by adding '&' in the end of a command line. 
        - jobs : list the running and stopped background jobs
        - bg : change a stopped background job to a running background job
        - fg : change a background job to a running background job
        - kill : terminate a job