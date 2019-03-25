# C Stdio 

Tiny implementation of stdio.h from C. This library contains various functions for performing input and output on files such as:</br>
-so_fopen - opens a file and returns a SO_FILE structure or NULL when an error appears.</br>
-so_fgetc - gets a single char from the file. Returns the read char or SO_EOF(-1) if error.</br>
-so_ferror - returns != 0 if there is was an error while working with a file. It usually returns the error code.</br>
