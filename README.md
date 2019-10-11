
# MiniShell

This is mini shell assignment done for CS550[OS]

## Instructions to compile

1] Make File

- run "make" command to compile to target "sh550" binary.
- use above generated binary to execute the program.
- run “make clean” command to delete the binary "sh550" .
- run "make check" to run valgrid check

2] Use GCC
- run 'gcc sh550.c  -o sh550' to compile to binary format.

## Instructions to test

I have tried to test all the testcases and handle them accordingly
## The shell can :-

- Redirect I/O of a command form a file
- Apply pipe to redirect input/output to the next process accordingly.
- Handles ctrl-c command using signals. Child executing command(exec) will not be able to handle this interrupt and exit but parent will not exit.
- Combination of above
- Make sure there is a space between the tokens
