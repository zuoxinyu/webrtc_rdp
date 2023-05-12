#ifdef _WIN32

#include <cstdlib>
#include <iostream>
#include <string>

#include <windows.h>
#include <winuser.h>

#pragma comment(lib, "user32.lib")
int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cout << "usage: input_test <event> x y" << std::endl;
        return 0;
    }

    RECT desktop_rect;
    GetClientRect(GetDesktopWindow(), &desktop_rect);

    std::string action = argv[1];
    int x = std::atoi(argv[2]);
    int y = std::atoi(argv[3]);

    INPUT input = {0};
    input.type = INPUT_MOUSE;
    input.mi = {
        .dx = x * 65536 / desktop_rect.right,
        .dy = y * 65536 / desktop_rect.bottom,
        .mouseData = 0,
        .dwFlags =
            MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK,
        .time = 0,
        .dwExtraInfo = 100,
    };
    if (action == "move") {
        input.mi.dwFlags =
            MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK;
    } else if (action == "ldown") {
        input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    } else if (action == "rdown") {
        input.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
    }

    UINT ret = SendInput(1, &input, sizeof(INPUT));
    if (ret != 1) {
        std::cout << "failed to send input: " << GetLastError() << std::endl;
    }

    return 0;
}
#endif
