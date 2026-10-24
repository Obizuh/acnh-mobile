#pragma once
#include <sys/stat.h>
#include <errno.h>
// Change only UF_HIDDEN, without renaming, moving or deleting the directory.
static inline int dm_directory_hidden(const char *path,int hidden){
 struct stat st;if(lstat(path,&st))return -1;
 if(!S_ISDIR(st.st_mode)){errno=ENOTDIR;return -1;}
 unsigned flags=hidden?(st.st_flags|UF_HIDDEN):(st.st_flags&~UF_HIDDEN);
 return flags==st.st_flags?0:chflags(path,flags);
}
