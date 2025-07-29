#define UNICODE
#define _UNICODE

#include <windows.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>

// Глобальные переменные
char buffer[10] = {0};
int bufferLen = 0;
bool altEmulationMode = false;
clock_t firstCtrlPressTime = 0;
bool waitingForSecondCtrl = false;
bool ctrlKeyWasDown = false;

void LogToFile(const char* message) {
    FILE* logFile = fopen("AltCodeEmulator.log", "a");
    if (logFile) {
        time_t now;
        time(&now);
        char timeStr[20];
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", localtime(&now));
        fprintf(logFile, "[%s] %s\n", timeStr, message);
        fclose(logFile);
    }
}

wchar_t Windows1252ToUnicode(unsigned char code) {
    wchar_t wc[2] = { 0 };
    MultiByteToWideChar(1252, 0, (char*)&code, 1, wc, 1);
    return wc[0];
}

void InsertUnicodeChar(wchar_t wc) {
    // Copy to clipboard
    wchar_t str[2] = { wc, 0 };
    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sizeof(str));
        if (hMem) {
            memcpy(GlobalLock(hMem), str, sizeof(str));
            GlobalUnlock(hMem);
            SetClipboardData(CF_UNICODETEXT, hMem);
        }
        CloseClipboard();
    }

    // Simulate Ctrl+V using SendInput
    INPUT inputs[4] = { 0 };

    // Press Ctrl
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;

    // Press V
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';

    // Release V
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;

    // Release Ctrl
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(4, inputs, sizeof(INPUT));
}

bool IsCtrlReallyDown() {
    return (GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0;
}

void CheckForTimeout() {
    if (waitingForSecondCtrl && !altEmulationMode) {
        clock_t currentTime = clock();
        double elapsedTime = (double)(currentTime - firstCtrlPressTime) / CLOCKS_PER_SEC;

        if (elapsedTime >= 0.5) {
            LogToFile("Timeout reached. Single Ctrl press.");
            waitingForSecondCtrl = false;
        }
    }
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
        DWORD vkCode = p->vkCode;

        CheckForTimeout();

        if (vkCode == VK_RCONTROL) {
            if (wParam == WM_KEYDOWN) {
                if (!ctrlKeyWasDown) {
                    ctrlKeyWasDown = true;
                    clock_t currentTime = clock();

                    if (waitingForSecondCtrl) {
                        double elapsedTime = (double)(currentTime - firstCtrlPressTime) / CLOCKS_PER_SEC;
                        if (elapsedTime < 0.5) {
                            LogToFile("Double Ctrl detected! Entering Alt-code mode.");
                            altEmulationMode = true;
                            waitingForSecondCtrl = false;
                            return 1;
                        }
                    }
                    else {
                        LogToFile("First Ctrl pressed. Waiting for second press...");
                        waitingForSecondCtrl = true;
                        firstCtrlPressTime = currentTime;
                    }
                }
            }
            else if (wParam == WM_KEYUP) {
                ctrlKeyWasDown = false;

                if (altEmulationMode) {
                    if (bufferLen > 0) {
                        buffer[bufferLen] = '\0';
                        int code = atoi(buffer);
                        if (code >= 0 && code <= 255) {
                            wchar_t wc = Windows1252ToUnicode((unsigned char)code);
                            InsertUnicodeChar(wc);

                            char logMsg[50];
                            snprintf(logMsg, sizeof(logMsg), "Sent Alt code: %s (%d)", buffer, code);
                            LogToFile(logMsg);
                        }
                    }
                    bufferLen = 0;
                    altEmulationMode = false;
                    LogToFile("Alt-code mode exited.");
                    return 1;
                }
            }
        }
        else if (altEmulationMode && wParam == WM_KEYDOWN) {
            if (vkCode >= '0' && vkCode <= '9' && bufferLen < 9) {
                buffer[bufferLen++] = (char)vkCode;
                char logMsg[50];
                snprintf(logMsg, sizeof(logMsg), "Digit pressed: %c", (char)vkCode);
                LogToFile(logMsg);
                return 1;
            }
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    LogToFile("=== AltCodeEmulator started ===");

    HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (!hook) {
        LogToFile("ERROR: Failed to set keyboard hook!");
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnhookWindowsHookEx(hook);
    LogToFile("=== AltCodeEmulator stopped ===");
    return 0;
}