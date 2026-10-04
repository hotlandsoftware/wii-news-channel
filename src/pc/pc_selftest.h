// Checks used by the start-up self-test in main.cpp (`newschannel --selftest`).

#ifndef PC_SELFTEST_H
#define PC_SELFTEST_H

void PCSelfTestCheck(bool ok, const char* expression, const char* file, int line);

#define PC_CHECK(expr) PCSelfTestCheck((expr), #expr, __FILE__, __LINE__)

#endif
