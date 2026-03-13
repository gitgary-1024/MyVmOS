#ifndef CROSS_PLATFORM_H
#define CROSS_PLATFORM_H

// ==========================================
// 1. 平台检测宏
// ==========================================
#ifdef _WIN32
    #define PLATFORM_WINDOWS
#elif defined(__linux__)
    #define PLATFORM_LINUX
#else
    #error "Unsupported platform (only Windows/Linux are supported)"
#endif

// ==========================================
// 2. 网络模块（Socket 封装）
// ==========================================
#ifdef PLATFORM_WINDOWS
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")  // 自动链接 Winsock 库

    // Socket 初始化/清理
    #define NET_INIT() { WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa); }
    #define NET_CLEANUP() WSACleanup()

    // Socket 关闭与错误码
    #define SOCKET_CLOSE closesocket
    #define SOCKET_ERRNO WSAGetLastError()

    // 类型兼容（Linux 下 SOCKET 是 int，Windows 下是 unsigned int）
    using SocketType = SOCKET;
    #define INVALID_SOCKET_FD INVALID_SOCKET

#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <errno.h>

    // Linux 无需显式初始化网络库
    #define NET_INIT()
    #define NET_CLEANUP()

    // Socket 关闭与错误码
    #define SOCKET_CLOSE close
    #define SOCKET_ERRNO errno

    // 类型兼容
    using SocketType = int;
    #define INVALID_SOCKET_FD (-1)
#endif

// ==========================================
// 3. 文件模块（路径、目录操作）
// ==========================================
#ifdef PLATFORM_WINDOWS
    #include <direct.h>
    #include <io.h>

    // 路径分隔符
    #define PATH_SEP "\\"
    #define PATH_SEP_CHAR '\\'

    // 目录创建（Windows _mkdir 仅需路径，Linux mkdir 需权限）
    #define MKDIR(path) _mkdir(path)

    // 文件存在性检查
    #define FILE_EXISTS(path) (_access(path, 0) == 0)

#else
    #include <sys/stat.h>
    #include <unistd.h>

    // 路径分隔符
    #define PATH_SEP "/"
    #define PATH_SEP_CHAR '/'

    // 目录创建（默认权限 0755）
    #define MKDIR(path) mkdir(path, 0755)

    // 文件存在性检查
    #define FILE_EXISTS(path) (access(path, F_OK) == 0)
#endif

// ==========================================
// 4. 终端模块（清屏、颜色、输入）
// ==========================================
#ifdef PLATFORM_WINDOWS
    #include <windows.h>
    #include <conio.h>

    // 清屏（Windows API 实现，避免 system("cls") 的性能问题）
    #define TERM_CLEAR() { \
        HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE); \
        CONSOLE_SCREEN_BUFFER_INFO csbi; \
        GetConsoleScreenBufferInfo(h, &csbi); \
        DWORD dwSize = csbi.dwSize.X * csbi.dwSize.Y; \
        COORD coord = {0, 0}; \
        DWORD dwWritten; \
        FillConsoleOutputCharacter(h, ' ', dwSize, coord, &dwWritten); \
        SetConsoleCursorPosition(h, coord); \
    }

    // 终端颜色（Windows 控制台颜色宏）
    #define TERM_COLOR_RED     (FOREGROUND_RED | FOREGROUND_INTENSITY)
    #define TERM_COLOR_GREEN   (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
    #define TERM_COLOR_BLUE    (FOREGROUND_BLUE | FOREGROUND_INTENSITY)
    #define TERM_COLOR_RESET   (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
    #define TERM_SET_COLOR(color) SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color)

    // 无回显输入（如密码输入）
    #define GETCH() _getch()

#else
    #include <iostream>
    #include <termios.h>
    #include <unistd.h>

    // 清屏（ANSI 转义序列）
    #define TERM_CLEAR() std::cout << "\033[2J\033[H" << std::flush

    // 终端颜色（ANSI 转义序列）
    #define TERM_COLOR_RED     "\033[31;1m"
    #define TERM_COLOR_GREEN   "\033[32;1m"
    #define TERM_COLOR_RESET   "\033[0m"
    #define TERM_SET_COLOR(color) std::cout << color

    // 无回显输入（Linux 下通过 termios 实现）
    inline char getch_linux() {
        struct termios oldt, newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
        char ch = getchar();
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return ch;
    }
    #define GETCH() getch_linux()
#endif

#endif // CROSS_PLATFORM_H