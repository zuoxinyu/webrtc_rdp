#ifdef _WIN32

#include <cstdlib>
#include <iostream>
#include <string>

#include <windows.h>
#include <winuser.h>

#pragma comment(lib, "user32.lib")
int main(int argc, char **argv)
{
    if (argc < 3) {
        std::cout << "usage:\n\t" << argv[0] << " <event> x y" << std::endl;
        return 0;
    }

    RECT desktop_rect;
    GetClientRect(GetDesktopWindow(), &desktop_rect);

    int x = 0, y = 0;
    char key;
    std::string action = argv[1];
    if (action == "move") {
        x = std::atoi(argv[2]);
        y = std::atoi(argv[3]);
    } else if (action == "keydown") {
        key = argv[2][0];
    }

    INPUT input = {0};
    if (action == "move") {
        input.type = INPUT_MOUSE;
        input.mi.dwFlags =
            MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK;
    } else if (action == "ldown") {
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    } else if (action == "rdown") {
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
    } else if (action == "keydown") {
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = key;
        input.ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
    } else if (action == "keyup") {
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = key;
        input.ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
    }

    UINT ret = SendInput(1, &input, sizeof(INPUT));
    if (ret != 1) {
        std::cout << "failed to send input: " << GetLastError() << std::endl;
    }

    return 0;
}
#endif
