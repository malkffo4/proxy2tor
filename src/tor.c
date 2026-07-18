#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>

#define TOR_BINARY ".\\tor\\tor.exe"
// #define TOR_BRIDGE_OBFS4_PATH "ClientTransportPlugin obfs4 exec lyrebird"
#else
#include <unistd.h>
#include <sys/wait.h>

#define TOR_BINARY "./tor/tor"
// #define TOR_BRIDGE_OBFS4_PATH "ClientTransportPlugin obfs4 exec /usr/bin/obfs4proxy"
#endif

// #define TOR_USE_BRIDGE "UseBridges 1"
#define LOG_FILENAME "tor_log.txt"

#include "tor.h"


#ifdef _WIN32
    static BOOL WINAPI console_handler(DWORD signal) {
        if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
            printf("[Signal] Sig stop!\n");
            
            kill_existing_tor_processes();
            wipe_file_with_zeros(LOG_FILENAME);
            exit(1);
            return TRUE;
        }
        return FALSE;
    }
#else
    static void signal_handler(int sig) {
        if (sig == SIGINT) {
            printf("[Signal] Sig stop!\n");
            g_stop = 1;
        }
    }
#endif

int file_exists(const char *path) {
    printf("%s\n", path);
#ifdef _WIN32
    DWORD attrib = GetFileAttributesA(path);
    return (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
#else
    return access(path, F_OK) == 0;
#endif
}

void kill_process_pid(DWORD ProcessID) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, ProcessID); // pe32.th32ProcessID
    if (hProcess != NULL) {
        if (TerminateProcess(hProcess, 0)) {
            printf("[+] Terminated (PID: %lu)\n", ProcessID); // pe32.th32ProcessID
        } else {
            fprintf(stderr, "[-] Failed to terminate (PID: %lu), error: %lu\n",
                    ProcessID, GetLastError());
        }
        CloseHandle(hProcess);
    } else {
        fprintf(stderr, "[-] Cannot open (PID: %lu), error: %lu\n",
                ProcessID, GetLastError());
    }
}

DWORD get_processes_pid_by_name(const char *process_name) {
    DWORD pid_proc = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "[-] Failed to create process snapshot\n");
        return -1;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe32)) {
        do {
            if (_stricmp(pe32.szExeFile, process_name) == 0) {
                pid_proc = pe32.th32ProcessID;
            }
        } while (Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);

    return pid_proc;
}

void kill_existing_tor_processes() {
    DWORD pid_tor = get_processes_pid_by_name("tor.exe");
    if (pid_tor == 0) {
        printf("tor.exe not found!\n");
        return;
    }
    kill_process_pid(pid_tor);
}

int wipe_file_with_zeros(const char *filename) {
    FILE *fp = fopen(filename, "r+b");
    if (!fp) {
        printf("[-] File %s not found.\n", filename);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);

    if (filesize <= 0) {
        printf("[*] File is empty.\n");
        fclose(fp);
        goto just_rm;
    }

    printf("[*] Wipe file \"%s\" (%ld byte)...\n", filename, filesize);

    char *zero_buffer = (char *)calloc(1, filesize);
    if (!zero_buffer) {
        perror("[-] Error allocate memory for bzero");
        fclose(fp);
        goto just_rm;
    }

    size_t written = fwrite(zero_buffer, 1, filesize, fp);
    if (written != filesize) {
        fprintf(stderr, "[-] Fewer bytes written than expected (%zu from %ld)\n", written, filesize);
    } else {
        printf("[+] Sucees wipe %zu byte.\n", written);
    }

    free(zero_buffer);
    fclose(fp);

    just_rm:
    remove(filename);

    return 0;
}

int start_tor(const char *tor_addr_bind, int tor_port_bind) {
    // if (file_exists(TOR_BINARY)) {
    //     printf("[+] Tor service doesn't exist.\n");
    //     return -1;
    // }
#ifdef _WIN32
    SetConsoleCtrlHandler(console_handler, TRUE);
#else
    signal(SIGINT, signal_handler);
#endif

    kill_existing_tor_processes();
    
    wipe_file_with_zeros(LOG_FILENAME);

    printf("[*] Run Tor on SOCKS-port: %s:%d\n", tor_addr_bind, tor_port_bind);

#ifdef _WIN32
    SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
    HANDLE readPipe, writePipe;

    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) {
        perror("[-] Failed to create pipe.");
        return -1;
    }

    // SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(writePipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = { 0 };
    PROCESS_INFORMATION pi;
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = writePipe;
    si.hStdError  = writePipe;
    si.wShowWindow = SW_HIDE;

    char cmdline[512];
    snprintf(cmdline, sizeof(cmdline), TOR_BINARY " --SocksPort %s:%d -f torrc Log \"notice file %s\"", tor_addr_bind, tor_port_bind, LOG_FILENAME);

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        perror("[-] Failed runing Tor.");
        CloseHandle(writePipe);
        CloseHandle(readPipe);
        return -1;
    }
    CloseHandle(writePipe);

    FILE* fp;
    while ((fp = fopen("tor_log.txt", "r")) == NULL) Sleep(100);
    
    fseek(fp, 0, SEEK_END);
    long pos = ftell(fp);
    bool bootstap_done = false;
    
    while (!bootstap_done) {
        Sleep(500);
        fseek(fp, pos, SEEK_SET);
        char line[1024];
        while (fgets(line, sizeof(line), fp)) {
            printf("[tor] %s", line);
            if (strstr(line, "Bootstrapped 100% (done): Done")) {
                fclose(fp); // Закрываем файл
                bootstap_done = true;
                break;
            }
        }
        pos = ftell(fp);
    }
    
    fflush(stdout);

    // WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(readPipe);
    
    return (int)pi.dwProcessId;
#else
    pid_t pid = fork();
    if (pid == 0) {
        execlp(TOR_BINARY, "tor", "--SocksPort", tor_port_bind, NULL);
        perror("execlp");
        exit(1);
    } else if (pid < 0) {
        perror("fork");
        return -1;
    }
    
    return (int)pid;
#endif

    return -1;
}

int stop_tor(int pid) {
#ifdef _WIN32
    HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
    if (hProc == NULL) {
        fprintf(stderr, "[-] Failed to open process %d: %lu\n", pid, GetLastError());

        kill_existing_tor_processes();
        wipe_file_with_zeros(LOG_FILENAME);

        return -1;
    }
    
    // Завершить процесс "мягко" в Windows нельзя напрямую — TerminateProcess = принудительно.
    // Альтернатива: отправить CTRL+C — но требует консоли. Иначе просто TerminateProcess.
    if (!TerminateProcess(hProc, 0)) {
        fprintf(stderr, "[-] Failed to terminate process %d: %lu\n", pid, GetLastError());
        CloseHandle(hProc);
        return -1;
    }
    
    CloseHandle(hProc);
    
    wipe_file_with_zeros(LOG_FILENAME);

    return 0;
    
#else
    if (kill((pid_t)pid, SIGTERM) == -1) {
        perror("[-] Failed to send SIGTERM to Tor");
        return -1;
    }
    
    return 0;
#endif
    wipe_file_with_zeros(LOG_FILENAME);
}
    