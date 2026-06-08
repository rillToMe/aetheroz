#ifndef STRING_H
#define STRING_H

#include <stddef.h> /* size_t */
#include <stdint.h>

/* Memory operations */
void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
void* memset(void* s, int c, size_t n);
int   memcmp(const void* s1, const void* s2, size_t n);

/* String operations */
size_t strlen(const char* s);
int    strcmp(const char* s1, const char* s2);
int    strncmp(const char* s1, const char* s2, size_t n);
char*  strncpy(char* dest, const char* src, size_t n);

#endif /* STRING_H */