/*
 * This file is derived from Box64.
 *
 * SPDX-FileCopyrightText: 2020 ptitSeb
 *
 * SPDX-License-Identifier: MIT
 */

#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <unistd.h>
#include <limits.h>
#include <stdint.h>

#ifndef MAX_PATH
#define MAX_PATH 4096
#endif

#include "debug.h"
#include "fileutils.h"

static const char* x86lib  = "\x7f" "ELF" "\x01" "\x01" "\x01" "\x03" "\x00" "\x00" "\x00" "\x00" "\x00" "\x00" "\x00" "\x00" "\x02" "\x00" "\x03" "\x00";
static const char* x64lib  = "\x7f" "ELF" "\x02" "\x01" "\x01" "\x03" "\x00" "\x00" "\x00" "\x00" "\x00" "\x00" "\x00" "\x00" "\x02" "\x00" "\x3e" "\x00";
static const char* bashsign= "#!/bin/bash";
static const char* shsign  = "#!/bin/sh";

int FileExist(const char* filename, int flags)
{
    struct stat sb;
    if (stat(filename, &sb) == -1)
        return 0;
    if(flags==-1)
        return 1;
    // check type of file? should be executable, or folder
    if(flags&IS_FILE) {
        if(!S_ISREG(sb.st_mode))
            return 0;
    } else if(!S_ISDIR(sb.st_mode))
            return 0;
    
    if(flags&IS_EXECUTABLE) {
        if((sb.st_mode&S_IXUSR)!=S_IXUSR)
            return 0;   // nope
    }
    return 1;
}
const char* GetTmpDir(void) {
    char *tmpdir;
    if ((tmpdir = getenv ("TMPDIR")) != NULL) return tmpdir;
    if ((tmpdir = getenv ("TEMP")) != NULL)   return tmpdir;
    if ((tmpdir = getenv ("TMP")) != NULL)    return tmpdir;
    if(FileExist("/tmp", 0))                  return "/tmp";
    if(FileExist("/var/tmp", 0))              return "/var/tmp";
    if(FileExist("/usr/tmp", 0))              return "/usr/tmp";

    return "/tmp";  // meh...
}
char* ResolveFile(const char* filename, path_collection_t* paths)
{
    char p[MAX_PATH];
    if(filename[0]=='/')
        return box_strdup(filename);
    for (int i=0; i<paths->size; ++i) {
        if(paths->paths[i][0]!='/') {
            // not an absolute path...
            if(!getcwd(p, sizeof(p))) return NULL;
            if(p[strlen(p)-1]!='/')
                strcat(p, "/");
            strcat(p, paths->paths[i]);
        } else
            strcpy(p, paths->paths[i]);
        strcat(p, filename);
        if(FileExist(p, IS_FILE))
            return realpath(p, NULL);
    }

    return box_strdup(filename); //NULL;
}

int FileIsX64ELF(const char* filename)
{
    FILE *f = fopen(filename, "rb");
    if(!f)
        return 0;
    char head[20] = {0};
    int sz = fread(head, 20, 1, f);
    fclose(f);
    if(sz!=1) {
        return 0;
    }
    head[7] = x64lib[7];   // this one changes
    head[16]&=0xfe;
    if(!memcmp(head, x64lib, 20))
        return 1;
    return 0;
}

int FileIsX86ELF(const char* filename)
{
    FILE *f = fopen(filename, "rb");
    if(!f)
        return 0;
    char head[20] = {0};
    int sz = fread(head, 20, 1, f);
    fclose(f);
    if(sz!=1) {
        return 0;
    }
    head[7] = x64lib[7];
    head[16]&=0xfe;
    if(!memcmp(head, x86lib, 20))
        return 1;
    return 0;
}

int FileIsShell(const char* filename)
{
    FILE *f = fopen(filename, "rb");
    if(!f)
        return 0;
    char head[20] = {0};
    int sz = fread(head, strlen(bashsign), 1, f);
    fclose(f);
    if(sz!=1) {
        return 0;
    if(memcmp(head, bashsign, strlen(bashsign))==0)
        return 1;
    }
    if(memcmp(head, shsign, strlen(shsign))==0)
        return 1;
    return 0;
}
