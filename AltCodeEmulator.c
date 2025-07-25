#define UNICODE
#define _UNICODE

#include <windows.h>
#include <stdbool.h>

char buffer[10];
int bufferLen = 0;
bool shiftPressed = false;

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

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
        DWORD vkCode = p->vkCode;

        if (wParam == WM_KEYDOWN) {
            if (vkCode == VK_RSHIFT && !shiftPressed) {
                shiftPressed = true;
                bufferLen = 0;
                return 1; // Block RShift down
            } else if (shiftPressed && vkCode >= '0' && vkCode <= '9' && bufferLen < 10) {
                buffer[bufferLen++] = (char)vkCode;
                return 1; // Block digit keys
            }
        }

        if (wParam == WM_KEYUP && vkCode == VK_RSHIFT && shiftPressed) {
            shiftPressed = false;
            buffer[bufferLen] = '\0';

            if (bufferLen > 0) {
                int code = atoi(buffer);
                if (code >= 0 && code <= 255) {
                    wchar_t wc = Windows1252ToUnicode((unsigned char)code);
                    InsertUnicodeChar(wc);
                }
            }

            bufferLen = 0;
            return 1; // Block RShift up
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (!hook) return 1;

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnhookWindowsHookEx(hook);
    return 0;
}
