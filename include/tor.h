#ifndef TOR_H
#define TOR_H

void kill_existing_tor_processes();
int wipe_file_with_zeros(const char *filename);
int start_tor(const char *tor_addr_bind, int tor_port_bind);
int stop_tor(int pid);

#endif
