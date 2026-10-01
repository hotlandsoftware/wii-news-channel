#ifndef MSL_STDIO_H
#define MSL_STDIO_H

#include <types.h>
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

enum file_kinds { file_closed, file_disk, file_console, file_unavailable };

enum open_modes { must_exist, create_if_needed, create_or_truncate };

enum file_orientation { file_unoriented, file_char_oriented, file_wide_oriented };

enum io_states { neutral, writing, reading, rereading };

typedef struct {
    unsigned int open : 2;
    unsigned int io : 3;
    unsigned int buffer : 2;
    unsigned int file : 3;
    unsigned int file_orientation : 2;
    unsigned int binary : 1;
} file_modes;

typedef struct {
    unsigned int io_state : 3;
    unsigned int free_buffer : 1;
    unsigned char eof;
    unsigned char error;
} file_state;

typedef int (*__pos_proc)(unsigned long handle, long* offset, int mode, void* ref);
typedef int (*__io_proc)(unsigned long handle, unsigned char* buffer, size_t* count, void* ref);
typedef int (*__close_proc)(unsigned long handle);

typedef struct _FILE {
    unsigned long handle;            // 0x00
    file_modes mode;                 // 0x04
    file_state state;                // 0x08
    unsigned char is_dyn_alloc;      // 0x0C
    unsigned char char_buf;          // 0x0D
    unsigned char char_buf_of;       // 0x0E
    unsigned char unget_buffer[2];   // 0x0F
    wchar_t ungetwc_buffer[2];       // 0x12
    unsigned long pos;               // 0x18
    unsigned char* buffer;           // 0x1C
    unsigned long buffer_size;       // 0x20
    unsigned char* buffer_ptr;       // 0x24
    unsigned long buffer_len;        // 0x28
    unsigned long buffer_alignment;  // 0x2C
    unsigned long save_buffer_len;   // 0x30
    unsigned long buffer_pos;        // 0x34
    __pos_proc pos_proc;             // 0x38
    __io_proc read_proc;             // 0x3C
    __io_proc write_proc;            // 0x40
    __close_proc close_proc;         // 0x44
    void* ref;                       // 0x48
    struct _FILE* next_file;         // 0x4C
} FILE;

extern FILE __files[];

#define stdin (&__files[0])
#define stdout (&__files[1])
#define stderr (&__files[2])

int fclose(FILE* file);
int fflush(FILE* file);
long ftell(FILE* file);

size_t __fwrite(const void* ptr, size_t size, size_t count, FILE* file);

int printf(const char* format, ...);
int fprintf(FILE* file, const char* format, ...);
int vprintf(const char* format, va_list arg);
int vsnprintf(char* s, size_t n, const char* format, va_list arg);
int vsprintf(char* s, const char* format, va_list arg);
int snprintf(char* s, size_t n, const char* format, ...);
int sprintf(char* s, const char* format, ...);

// MSL internals
long _ftell(FILE* file);
int _fseek(FILE* file, long offset, int mode);
void __prep_buffer(FILE* file);
int __flush_buffer(FILE* file, size_t* length);
void __close_all(void);
int __flush_all(void);
void __stdio_atexit(void);

int __read_console(unsigned long handle, unsigned char* buffer, size_t* count, void* ref);
int __write_console(unsigned long handle, unsigned char* buffer, size_t* count, void* ref);
int __close_console(unsigned long handle);

#ifdef __cplusplus
}
#endif

#endif
