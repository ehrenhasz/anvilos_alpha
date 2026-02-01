 

#include <stdint.h>
#include <stddef.h>

#define likely(x) __builtin_expect((x), 1)

void *memcpy(void *dst, const void *src, size_t n) {
    if (likely(!(((uintptr_t)dst) & 3) && !(((uintptr_t)src) & 3))) {
        
        uint32_t *d = dst;
        const uint32_t *s = src;

        
        for (size_t i = (n >> 2); i; i--) {
            *d++ = *s++;
        }

        if (n & 2) {
            
            *(uint16_t*)d = *(const uint16_t*)s;
            d = (uint32_t*)((uint16_t*)d + 1);
            s = (const uint32_t*)((const uint16_t*)s + 1);
        }

        if (n & 1) {
            
            *((uint8_t*)d) = *((const uint8_t*)s);
        }
    } else {
        
        uint8_t *d = dst;
        const uint8_t *s = src;

        for (; n; n--) {
            *d++ = *s++;
        }
    }

    return dst;
}

void *__memcpy_chk(void *dest, const void *src, size_t len, size_t slen) {
    if (len > slen) {
        return NULL;
    }
    return memcpy(dest, src, len);
}

void *memmove(void *dest, const void *src, size_t n) {
    if (src < dest && (uint8_t*)dest < (const uint8_t*)src + n) {
        
        uint8_t *d = (uint8_t*)dest + n - 1;
        const uint8_t *s = (const uint8_t*)src + n - 1;
        for (; n > 0; n--) {
            *d-- = *s--;
        }
        return dest;
    } else {
        
        return memcpy(dest, src, n);
    }
}

void *memset(void *s, int c, size_t n) {
    if (c == 0 && ((uintptr_t)s & 3) == 0) {
        
        uint32_t *s32 = s;
        for (size_t i = n >> 2; i > 0; i--) {
            *s32++ = 0;
        }
        if (n & 2) {
            *((uint16_t*)s32) = 0;
            s32 = (uint32_t*)((uint16_t*)s32 + 1);
        }
        if (n & 1) {
            *((uint8_t*)s32) = 0;
        }
    } else {
        uint8_t *s2 = s;
        for (; n > 0; n--) {
            *s2++ = c;
        }
    }
    return s;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *s1_8 = s1;
    const uint8_t *s2_8 = s2;
    while (n--) {
        char c1 = *s1_8++;
        char c2 = *s2_8++;
        if (c1 < c2) return -1;
        else if (c1 > c2) return 1;
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n) {
    if (n != 0) {
        const unsigned char *p = s;

        do {
            if (*p++ == c)
                return ((void *)(p - 1));
        } while (--n != 0);
    }
    return 0;
}

size_t strlen(const char *str) {
    int len = 0;
    for (const char *s = str; *s; s++) {
        len += 1;
    }
    return len;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        char c1 = *s1++; 
        char c2 = *s2++; 
        if (c1 < c2) return -1;
        else if (c1 > c2) return 1;
    }
    if (*s2) return -1;
    else if (*s1) return 1;
    else return 0;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && *s1 && *s2) {
        char c1 = *s1++; 
        char c2 = *s2++; 
        n--;
        if (c1 < c2) return -1;
        else if (c1 > c2) return 1;
    }
    if (n == 0) return 0;
    else if (*s2) return -1;
    else if (*s1) return 1;
    else return 0;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while (*src) {
        *d++ = *src++;
    }
    *d = '\0';
    return dest;
}



char *strncpy(char *s1, const char *s2, size_t n) {
     char *dst = s1;
     const char *src = s2;
      
     while (n > 0) {
         n--;
         if ((*dst++ = *src++) == '\0') {
              
             memset(dst, '\0', n);
             break;
         }
     }
     return s1;
 }


char *stpcpy(char *dest, const char *src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) {
        d++;
    }
    while (*src) {
        *d++ = *src++;
    }
    *d = '\0';
    return dest;
}



char *strchr(const char *s, int c)
{
     
    while (*s != '\0' && *s != (char)c)
        s++;
    return ((*s == c) ? (char *) s : 0);
}




char *strstr(const char *haystack, const char *needle)
{
    size_t needlelen;
     
    if (*needle == '\0')
        return (char *) haystack;
    needlelen = strlen(needle);
    for (; (haystack = strchr(haystack, *needle)) != 0; haystack++)
        if (strncmp(haystack, needle, needlelen) == 0)
            return (char *) haystack;
    return 0;
}

size_t strspn(const char *s, const char *accept) {
    const char *ss = s;
    while (*s && strchr(accept, *s) != NULL) {
        ++s;
    }
    return s - ss;
}

size_t strcspn(const char *s, const char *reject) {
    const char *ss = s;
    while (*s && strchr(reject, *s) == NULL) {
        ++s;
    }
    return s - ss;
}
