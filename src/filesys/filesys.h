#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */

/* Block device that contains the file system. */
extern struct block *fs_device;

void filesys_init (bool format);
void filesys_done (void);
bool filesys_create (const char *name, off_t initial_size, bool is_dir);
struct file *filesys_open (const char *name);
bool filesys_remove (const char *name);

bool filesys_chdir (const char *name);
bool filesys_readdir (int fd, char *name);
bool filesys_isdir (int fd);
int filesys_inumber (int fd);

struct dir *filesys_get_dir (const char *name);
char *get_name(const char *path);
#endif /* filesys/filesys.h */
