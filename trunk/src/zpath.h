/*
Copyright (C) 1997-2006 ZSNES Team ( zsKnight, _Demo_, pagefault, Nach )

http://www.zsnes.com
http://sourceforge.net/projects/zsnes

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef ZPATH_H
#define ZPATH_H

#include <zlib.h>
#include <stdio.h>
#include <sys/stat.h>

#include "zip/zunzip.h"

#ifndef NO_JMA
#include "jma/zsnesjma.h"
#endif

#ifdef _MSC_VER
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
#endif

#if !defined(__cplusplus) && !defined(bool)
//C++ style code in C
#define bool unsigned char
#define true 1
#define false 0
#endif

#ifdef __UNIXSDL__
#define DIR_SLASH "/"
#define DIR_SLASH_C '/'
#define DIR_SLASH_C_OTHER '\\'
#define ROOT_LEN 1 //"/"
#else
#define DIR_SLASH "\\"
#define DIR_SLASH_C '\\'
#define DIR_SLASH_C_OTHER '/'
#define ROOT_LEN 3 //"A:\"
#endif

#ifndef __MSDOS__
#define PATH_SIZE 4096
#define NAME_SIZE 512
#else
#define PATH_SIZE 256
#define NAME_SIZE 13
#endif

extern char ZCfgFile[];
extern char *ZStartPath, *ZCfgPath, *ZSramPath, *ZRomPath;
extern char *ZSnapPath, *ZSpcPath;
extern char *ZCartName, *ZSaveName, *ZStateName;

bool init_paths(char *launch_command);
void init_save_paths();
bool init_rom_path(char *path);

char *strdupcat(const char *str1, const char *str2);

int access_dir(const char *path, const char *file, int mode);
int stat_dir(const char *path, const char *file, struct stat *buf);
FILE *fopen_dir(const char *path, const char *file, const char *mode);
gzFile gzopen_dir(const char *path, const char *file, const char *mode);
unzFile unzopen_dir(const char *path, const char *file);
#ifndef NO_JMA
void load_jma_file_dir(const char *path, const char *file);
#endif
int remove_dir(const char *path, const char *file);
int mkdir_dir(const char *path, const char *dir);
char *realpath_dir(const char *path, const char *file, char *buf);
#ifdef __MSDOS__
char *realpath_sfn_dir(const char *path, const char *file, char *buf);
#endif
int system_dir(const char *path, const char *command);
FILE *popen_dir(const char *path, char *command, const char *type);

void natify_slashes(char *str);
void strcatslash(char *str);
void setextension(char *base, const char *ext);
bool isextension(const char *fname, const char *ext);
void strdirname(char *str);
void strbasename(char *str);

#endif
