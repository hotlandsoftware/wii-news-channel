#ifndef MSL_STDIO_H
#define MSL_STDIO_H

#include <types.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _FILE {
    unsigned long handle;
    unsigned long mode;
    unsigned long state;
    unsigned char is_dyn_alloc;
    unsigned char char_buf;
    unsigned char char_buf_of;
    unsigned char unget_buffer[2];
    wchar_t ungetwc_buffer[2];
    unsigned long pos;
    unsigned char* buffer;
    unsigned long buffer_size;
    unsigned char* buffer_ptr;
    unsigned long buffer_len;
    unsigned long buffer_alignment;
    unsigned long unk;
    unsigned long buffer_pos;
    void* pos_proc;
    void* read_proc;
    void* write_proc;
    void* close_proc;
    void* ref;
    struct _FILE* next_file;
} FILE;

extern FILE __files[];

#define stdin (&__files[0])
#define stdout (&__files[1])
#define stderr (&__files[2])

size_t __fwrite(const void* ptr, size_t size, size_t count, FILE* file);

int printf(const char* format, ...);
int fprintf(FILE* file, const char* format, ...);
int vprintf(const char* format, va_list arg);
int vsnprintf(char* s, size_t n, const char* format, va_list arg);
int vsprintf(char* s, const char* format, va_list arg);
int snprintf(char* s, size_t n, const char* format, ...);
int sprintf(char* s, const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif
