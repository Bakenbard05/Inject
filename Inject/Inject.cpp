// Inject.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <fstream>
#include <string>
#include <Psapi.h>

HANDLE lib = NULL;

DWORD GetProcId(const char* procName)
{
    DWORD procId = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 procEntry;
        procEntry.dwSize = sizeof(procEntry);

        if (Process32First(hSnap, &procEntry))
        {
            do
            {
                if (!_stricmp(procEntry.szExeFile, procName))
                {
                    procId = procEntry.th32ProcessID;
                    break;
                }
            } while (Process32Next(hSnap, &procEntry));
        }
    }
    CloseHandle(hSnap);
    return procId;
}

void ListLoadedDlls(DWORD processId, std::string& dllPath) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess == NULL) {
        std::cerr << "Не удалось открыть процесс: " << GetLastError() << std::endl;
        return;
    }

    HMODULE hMods[1024];
    DWORD cbNeeded;

    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        std::cout << "Загруженные DLL в процессе с ID " << processId << ":\n";
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            char szModName[MAX_PATH];
            if (GetModuleFileNameExA(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(char))) {
                std::cout << szModName << std::endl;
            }
        }
    }
    else {
        std::cerr << "Не удалось перечислить модули: " << GetLastError() << std::endl;
    }

    CloseHandle(hProcess);
}

std::string FindDllPath(const char* procName, const std::string& dllName) {
    DWORD processId = GetProcId(procName);
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (hProcess == NULL) {
        std::cerr << "Не удалось открыть процесс: " << GetLastError() << std::endl;
        return "";
    }

    HMODULE hMods[1024];
    DWORD cbNeeded;
    std::string dllPath;

    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            char szModName[MAX_PATH];
            if (GetModuleFileNameExA(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(char))) {
                // Проверяем имя DLL
                if (dllName == szModName || dllName == std::string(szModName).substr(std::string(szModName).find_last_of("\\") + 1)) {
                    dllPath = szModName; // Сохраняем путь к DLL
                    break; // Выходим из цикла после нахождения первой DLL
                }
            }
        }
    }
    else {
        std::cerr << "Не удалось перечислить модули: " << GetLastError() << std::endl;
    }

    CloseHandle(hProcess);
    return dllPath;
}

void Inject(const char* procName, const char* name)
{
    DWORD procId = 0;
    while (!procId)
    {
        procId = GetProcId(procName);
        Sleep(30);
    }
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, 0, procId);
    if (hProc && hProc != INVALID_HANDLE_VALUE)
    {
        void* loc = VirtualAllocEx(hProc, 0, MAX_PATH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (loc)
        {
            WriteProcessMemory(hProc, loc, name, strlen(name) + 1, 0);
            HANDLE hThread = CreateRemoteThread(hProc, 0, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, loc, 0, 0);
            lib = hThread;
            if (hThread)
            {
                CloseHandle(hThread);
            }
            std::cout << GetLastError() << std::endl;
        }
        else {
            std::cout << "Error! LOC" << std::endl;
        }
    }
    else {
        std::cout << "Error! hProc" << std::endl;
    }
    if (hProc)
    {
        CloseHandle(hProc);
    }
}


void Unload(const char* procName, const char* dllPath) {
    DWORD processId = GetProcId(procName);
    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
    if (hProcess == NULL) {
        std::cerr << "Не удалось открыть процесс: " << GetLastError() << std::endl;
        return;
    }

    LPVOID pDllPath = VirtualAllocEx(hProcess, NULL, strlen(dllPath) + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (pDllPath == NULL) {
        std::cerr << "Не удалось выделить память: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return;
    }

    WriteProcessMemory(hProcess, pDllPath, (LPVOID)dllPath, strlen(dllPath) + 1, NULL);

    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)FreeLibrary, pDllPath, 0, NULL);
    if (hThread == NULL) {
        std::cerr << "Не удалось создать удаленный поток: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return;
    }

    WaitForSingleObject(hThread, INFINITE);
    VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProcess);

    std::cout << "DLL успешно отключена." << std::endl;
}

void Unload2(const char* procName, const char* dllName) {
    DWORD processId = GetProcId(procName);

    if (processId == 0) {
        std::cerr << "Процесс не найден." << std::endl;
        return;
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);

    if (hProcess == NULL) {
        std::cerr << "Не удалось открыть процесс: " << GetLastError() << std::endl;
        return;
    }

    // Получаем хэндл библиотеки из адресного пространства процесса
    HMODULE hModule = NULL;
    HMODULE hMods[1024];
    DWORD cbNeeded;

    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            char szModName[MAX_PATH];
            if (GetModuleFileNameExA(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(char))) {
                // Проверяем имя DLL
                if (dllName == szModName || dllName == std::string(szModName).substr(std::string(szModName).find_last_of("\\") + 1)) {
                    hModule = hMods[i]; // Сохраняем хэндл библиотеки
                    break; // Выходим из цикла после нахождения первой DLL
                }
            }
        }
    }
    else {
        std::cerr << "Не удалось перечислить модули: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return;
    }

    if (hModule == NULL) {
        std::cerr << "DLL не найдена в процессе." << std::endl;
        CloseHandle(hProcess);
        return;
    }

    // Создаем удаленный поток для вызова FreeLibrary
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0,
        (LPTHREAD_START_ROUTINE)FreeLibrary,
        hModule,
        0,
        NULL);

    if (hThread == NULL) {
        std::cerr << "Не удалось создать удаленный поток: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return;
    }

    WaitForSingleObject(hThread, INFINITE); // Ожидаем завершения потока
    CloseHandle(hThread); // Закрываем дескриптор потока
    CloseHandle(hProcess); // Закрываем дескриптор процесса

    std::cout << "DLL успешно отключена." << std::endl;
}

int main(int argc, char* argv[])
{
    setlocale(LC_ALL, "RUS");
    const char* procName = argv[2];
    std::cout << procName << std::endl;
    const char* mod = argv[1];
    std::cout << FindDllPath(procName, argv[3]) << std::endl;
    if (strcmp(mod, "u") == 0)
    {
        //Unload2(procName, FindDllPath(procName, argv[3]).c_str());
        Unload2(procName, argv[3]);
    }
    else if(strcmp(mod, "l") == 0)
    {
        Inject(procName, argv[3]);
    }
    system("pause");
    return 0;
}


// Запуск программы: CTRL+F5 или меню "Отладка" > "Запуск без отладки"
// Отладка программы: F5 или меню "Отладка" > "Запустить отладку"

// Советы по началу работы 
//   1. В окне обозревателя решений можно добавлять файлы и управлять ими.
//   2. В окне Team Explorer можно подключиться к системе управления версиями.
//   3. В окне "Выходные данные" можно просматривать выходные данные сборки и другие сообщения.
//   4. В окне "Список ошибок" можно просматривать ошибки.
//   5. Последовательно выберите пункты меню "Проект" > "Добавить новый элемент", чтобы создать файлы кода, или "Проект" > "Добавить существующий элемент", чтобы добавить в проект существующие файлы кода.
//   6. Чтобы снова открыть этот проект позже, выберите пункты меню "Файл" > "Открыть" > "Проект" и выберите SLN-файл.
