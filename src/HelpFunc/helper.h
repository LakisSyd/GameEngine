#pragma once
#include <string>


#ifndef DX_CHECK
#define DX_CHECK(call) \
    do { \
        HRESULT hr = (call); \
        if (FAILED(hr)) { \
            char sysMsg[256] = "Неизвестная ошибка"; \
            FormatMessageA( \
                FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, \
                NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), \
                sysMsg, sizeof(sysMsg), NULL); \
            \
            std::string fullLog = "DirectX Error Detected!\n\n"; \
            fullLog += "Function: "; \
            fullLog += #call; \
            fullLog += "\nError Code: 0x"; \
            \
            char hexCode[16]; \
            sprintf_s(hexCode, "%08lX", hr); \
            fullLog += hexCode; \
            \
            fullLog += "\nDescription: "; \
            fullLog += sysMsg; \
            \
            MessageBoxA(NULL, fullLog.c_str(), "DX_CHECK Failed", MB_OK | MB_ICONERROR); \
            return false; \
        } \
    } while(0)
#endif


