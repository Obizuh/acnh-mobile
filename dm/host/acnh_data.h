#pragma once
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
int dm_acnh_data_mount(const char *root,char *why,size_t cap);
const char *dm_acnh_data_root(void);
int dm_acnh_data_stat(const char *path,int *directory,int64_t *size);
int dm_acnh_data_open(const char *path);
int dm_acnh_data_fd_size(int fd,int64_t *size);
ssize_t dm_acnh_data_pread(int fd,void *buf,size_t n,off_t offset,int *handled);
void dm_acnh_data_close(int fd);
typedef int (*dm_acnh_data_emit)(void *,const char *,int,int64_t);
int dm_acnh_data_list(const char *path,dm_acnh_data_emit emit,void *opaque);
