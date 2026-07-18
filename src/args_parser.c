#define _GNU_SOURCE

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "args_parser.h"

void print_usage(const char* prog) {
    printf("Usage: %s --bridge-addr <addr> --bridge-port <port> "
           "--tor-addr <addr> --tor-port <port> --file/-f <path> --daemon/-d\n", prog);
}

int parse_args(int argc, char* argv[], ProgramArgs* args) {
    args->bridge_addr = "0.0.0.0";
    args->bridge_port = 8080;
    args->tor_addr = "127.0.0.1";
    args->tor_port = 9050;
    args->c2_file = NULL;
    args->daemon = false;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--bridge-addr") && i + 1 < argc) {
            args->bridge_addr = argv[++i];
        } else if (!strcmp(argv[i], "--bridge-port") && i + 1 < argc) {
            args->bridge_port = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--tor-addr") && i + 1 < argc) {
            args->tor_addr = argv[++i];
        } else if (!strcmp(argv[i], "--tor-port") && i + 1 < argc) {
            args->tor_port = atoi(argv[++i]);
        } else if ((!strcmp(argv[i], "--file") || !strcmp(argv[i], "-f")) && i + 1 < argc) {
            args->c2_file = argv[++i];
        } else if ((!strcmp(argv[i], "--daemon") || !strcmp(argv[i], "-d")) && i < argc) {
            args->daemon = true;
        } else {
            print_usage(argv[0]);
            return -1;
        }
    }

    if (args->c2_file == NULL) {
        fprintf(stderr, "[-] Missing required --file/-f argument\n");
        print_usage(argv[0]);
        return -1;
    }

    return 0;
}
