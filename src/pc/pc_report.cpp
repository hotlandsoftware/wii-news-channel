// Diagnostics shared by the backend and the generated stubs.

#include <cstdio>

extern "C" void PCUnimplemented(const char* name, unsigned char* once) {
    if (once != nullptr) {
        if (*once) {
            return;
        }
        *once = 1;
    }
    std::fprintf(stderr, "unimplemented: %s\n", name);
}
