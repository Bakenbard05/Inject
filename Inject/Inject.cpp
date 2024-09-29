// Inject.cpp : Этот файл содержит функцию "main". Здесь начинается и заканчивается выполнение программы.
#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <fstream>
#include <string>

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

void Inject(const char* name)
{
    const char* processName = "Rocket Science.exe";
    DWORD procId = 0;
    while (!procId)
    {
        procId = GetProcId(processName);
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

int main(int argc, char* argv[])
{
    const char* name = argv[1];
    std::cout << name << std::endl;
    Inject(name);
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
