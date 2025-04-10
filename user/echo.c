#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define PROGRAM_NAME "echo"

// Handles --help option
void process_command(char command[]) {
    if (strcmp(command, "--help") == 0) {
        printf("Usage: echo [options] [string]\n");
        printf("Options:\n");
        printf("  -n    Do not output a trailing newline\n");
        printf("  -e    Enable interpretation of backslash escapes\n");
        printf("  -E    Disable interpretation of backslash escapes (default)\n");
        exit(0);
    }
}

// Validates options like -n, -e, -E
int validate_process_cmd(char C, int *newline, int *interp) {
    switch (C) {
        case 'n':
            *newline = 1; // Suppress trailing newline
            return 0;
        case 'e':
            *interp = 0; // Enable escape sequence interpretation
            return 0;
        case 'E':
           
            *interp = 1; // Disable escape sequence interpretation
            return 0;
        default:
            return 1; // Not a valid flag
    }
}

int main(int argc, char *argv[]) {
    int newline = 0; // 0 means print newline, 1 means suppress newline
    int interp = 1;  // Default behavior: do not interpret escape sequences
    
    // Ensure argv[1] exists before accessing it
    if (argc > 1) {
        process_command(argv[1]); // Handle --help
    } else {
        printf("\n");
        exit(0);
    }

    // Argument parsing
    while (argc > 1 && argv[1][0] == '-') {
        char C = argv[1][1];
        if (validate_process_cmd(C, &newline, &interp) == 0) {
            argv++;
            argc--;
        } else {
            break;
        }
    }

    argv++; 
    argc--; 

    // Process and print arguments
    while (argc > 0) {
        if (interp==0) {
            int size = strlen(argv[0]);
            char temp[size + 1];
            strcpy(temp, argv[0]);

            for (int i = 0; i < size; i++) {
                char C = temp[i];
                if (C == '\\') { // Process escape sequences
                    i++;
                    C = temp[i];
                    switch (C) {
                        case 'b': C = '\b'; break;
                        case 'a': C = '\a'; break;
                        case 'c': exit(0); break;
                        case 'f': C = '\f'; break;
                        case 'n': C = '\n'; break;
                        case 'r': C = '\r'; break;
                        case 't': C = '\t'; break;
                        case 'v': C = '\v'; break;
                        default: break;
                    }
                }
                printf("%c", C);
            }
        } 
        else { // No interpretation, just print as-is
            printf("%s", argv[0]);
        }

        argv++;
        argc--;

        if (argc > 0) {
            printf(" ");
        }
    }

    if (!newline) {
        printf("\n");
    }

    exit(0);
}
