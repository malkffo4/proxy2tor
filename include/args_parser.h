#ifndef ARG_PARSER_H
#define ARG_PARSER_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char* bridge_addr;
    int bridge_port;

    char* tor_addr;
    int tor_port;

    char* c2_file;
    bool daemon;
} ProgramArgs;

// void print_usage(const char* prog);

int parse_args(int argc, char* argv[], ProgramArgs* args);

#endif // ARG_PARSER_H
