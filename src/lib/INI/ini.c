/* inih -- simple .INI file parser

inih is released under the New BSD license (see LICENSE.txt). Go to the project
home page for more info:

http://code.google.com/p/inih/

*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "ini.h"

#if !INI_USE_STACK
#include <stdlib.h>
#endif

#define MAX_SECTION 50
#define MAX_NAME 50

int fat_read_line(struct fat_file_struct* fd, uint8_t* buffer, uint8_t buffer_len);

/* Strip whitespace chars off end of given string, in place. Return s. */
static char* rstrip(char* s)
{
    char* p = s + strlen(s);
    while (p > s && isspace(*--p))
        *p = '\0';
    return s;
}

/* Return pointer to first non-whitespace char in given string. */
static char* lskip(const char* s)
{
    while (*s && isspace(*s))
        s++;
    return (char*)s;
}

/* Return pointer to first char c or ';' comment in given string, or pointer to
   null at end of string if neither found. ';' must be prefixed by a whitespace
   character to register as a comment. */
static char* find_char_or_comment(const char* s, char c)
{
    int was_whitespace = 0;
    while (*s && *s != c && !(was_whitespace && *s == ';')) {
        was_whitespace = isspace(*s);
        s++;
    }
    return (char*)s;
}

/* Version of strncpy that ensures dest (size bytes) is null-terminated. */
static char* strncpy0(char* dest, const char* src, size_t size)
{
    strncpy(dest, src, size);
    dest[size - 1] = '\0';
    return dest;
}

/* See documentation in header file. */
int ini_parse_file(struct fat_file_struct* file,
                   int (*handler)(void*, const char*, const char*,
                                  const char*),
                   void* user)
{
    /* Uses a fair bit of stack (use heap instead if you need to) */
#if INI_USE_STACK
    char line[INI_MAX_LINE];
#else
    char* line;
#endif
    char section[MAX_SECTION] = "";
    char prev_name[MAX_NAME] = "";

    char* start;
    char* end;
    char* name;
    char* value;
    int lineno = 0;
    int error = 0;

#if !INI_USE_STACK
    line = (unsigned char*)malloc(INI_MAX_LINE);
    if (!line) {
        return -2;
    }
#endif
#if INI_DEBUG
	printf("reading ini file...\r\n");
#endif
    /* Scan through file line by line */
    //while ( (error = fat_read_file(file, line, sizeof(line))) > 0) {
		//line[error] = '\0';
	while(fat_read_line(file, line, sizeof(line)) > 0) {
#if INI_DEBUG
		printf("read: '%s'\r\n", line);
#endif
        lineno++;

        start = line;
#if INI_ALLOW_BOM
        if (lineno == 1 && (unsigned char)start[0] == 0xEF &&
                           (unsigned char)start[1] == 0xBB &&
                           (unsigned char)start[2] == 0xBF) {
            start += 3;
        }
#endif
        start = lskip(rstrip(start));

        if (*start == ';' || *start == '#') {
            /* Per Python ConfigParser, allow '#' comments at start of line */
        }
#if INI_ALLOW_MULTILINE
        else if (*prev_name && *start && start > line) {
            /* Non-black line with leading whitespace, treat as continuation
               of previous name's value (as per Python ConfigParser). */
            if (!handler(user, section, prev_name, start) && !error) {
                error = lineno;
#if INI_DEBUG
				printf("multiline error\r\n");
#endif
			}
        }
#endif
        else if (*start == '[') {
            /* A "[section]" line */
            end = find_char_or_comment(start + 1, ']');
            if (*end == ']') {
                *end = '\0';
                strncpy0(section, start + 1, sizeof(section));
                *prev_name = '\0';
            }
            else if (!error) {
                /* No ']' found on section line */
                error = lineno;
#if INI_DEBUG				
				printf("No ']' found on section line\r\n");
#endif
            }
        }
        else if (*start && *start != ';') {
            /* Not a comment, must be a name[=:]value pair */
            end = find_char_or_comment(start, '=');
            if (*end != '=') {
                end = find_char_or_comment(start, ':');
            }
            if (*end == '=' || *end == ':') {
                *end = '\0';
                name = rstrip(start);
                value = lskip(end + 1);
                end = find_char_or_comment(value, '\0');
                if (*end == ';')
                    *end = '\0';
                rstrip(value);

                /* Valid name[=:]value pair found, call handler */
                strncpy0(prev_name, name, sizeof(prev_name));
                if (!handler(user, section, name, value) && !error) {
#if INI_DEBUG					
					printf("error handling name/value pair\r\n");
#endif
					error = lineno;
				}
            }
            else if (!error) {
                /* No '=' or ':' found on name[=:]value line */
#if INI_DEBUG				
				printf("No '=' or ':' found on name[=:]value line\r\n");
#endif
                error = lineno;
            }
        }
    }
#if !INI_USE_STACK
    free(line);
#endif

    return error;
}

// reads a file from the SD card one line at a time
int fat_read_line(struct fat_file_struct* fd, uint8_t* buffer, uint8_t buffer_len)
{
	char internal[INI_MAX_LINE];
	int bytesRead, bytesToCopy;
	int32_t offset;
	char* pchar;
	
	bytesRead = fat_read_file(fd, internal, INI_MAX_LINE);
#if INI_DEBUG	
	printf("read_line start: bytesRead=%d\r\n", bytesRead);
#endif
	if(bytesRead > 0) { // 0 on EOF, bytes read for >0
		pchar = strchr(internal, '\n'); // return a pointer to the first newline
		if(pchar) { // if newline found, compute byte number for matching char
			bytesToCopy = pchar - internal + 1;
		}
		else { // if newline not found, copy all the bytes read
			bytesToCopy = bytesRead;
		}
		if(bytesToCopy > buffer_len) {
			// warn user or exit?
			bytesToCopy = buffer_len;
		}
		// copy the bytes to the destination buffer
		strncpy0(buffer, internal, bytesToCopy);

		// seek backwards from current file pointer to end of copied text
		offset = bytesToCopy - bytesRead;
		//offset = bytesToCopy;
#if INI_DEBUG		
		printf(	"offset=%d, bytesRead=%d, bytesToCopy=%d\r\n", 
				(int)offset, bytesRead, bytesToCopy);
#endif
		if(!fat_seek_file(fd, &offset, FAT_SEEK_CUR)) {
		//if(!fat_seek_file(fd, &offset, FAT_SEEK_SET)) {
#if INI_DEBUG			
			printf("fat_seek_file: failure\r\n");
#endif
		}
#if INI_DEBUG		
		printf("after seek: offset=%d\r\n", offset);
#endif
		return bytesToCopy;
	}
	return bytesRead;
}


/* See documentation in header file. */
int ini_parse(const char* filename,
              int (*handler)(void*, const char*, const char*, const char*),
              void* user,
			  struct fat_fs_struct* fs,
			  struct fat_dir_struct* dd)
{
    struct fat_file_struct* file;
    int error;

    file = open_file_in_dir(fs, dd, filename);
    if (!file) {
		printf("error opening file\r\n");
        return -1;
	}
    error = ini_parse_file(file, handler, user);
    fat_close_file(file);
    return error;
}
