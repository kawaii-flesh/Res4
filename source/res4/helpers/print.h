#pragma once

// typedef enum { INFO = 0xFF00DDFF, ERROR = 0xFFE70000, GOOD = 0xFF40FF00 } Res4Colors;
typedef enum { INFO = 0xFF66BBFF, ERROR = 0xFFFF6666, GOOD = 0xFF80FF80, WARNING = 0xFFFF8C00 } Res4Colors;

void print(const Res4Colors color, const char* fmt, ...);
