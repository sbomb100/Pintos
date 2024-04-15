#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/cache.h"
#include "threads/thread.h"
#include "threads/malloc.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  cache_init();
  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
  cache_shutdown();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir) 
{
  printf("creating file with name %s that is a directory: %d\n", name, is_dir);
  block_sector_t inode_sector = 0;
  char *name_copy = malloc(strlen(name) + 1);
  if (name_copy == NULL) {
    printf("name_copy is null\n");
    return false;
  }
  strlcpy(name_copy, name, strlen(name) + 1);
  struct dir *dir = filesys_get_dir(name);
  bool success = (dir != NULL && free_map_allocate(1, &inode_sector));
  if (success) {
    struct inode *cur_inode;
    if (is_dir) {
      success = dir_create(inode_sector, inode_get_inumber(dir_get_inode(dir)), 16);
    } else {
      success = inode_create(inode_sector, initial_size, is_dir);
    }
    cur_inode = inode_open(inode_sector);
    if (success) {
      success = dir_add(dir, name_copy, inode_sector);
    }
    inode_close(cur_inode);
  }



  dir_close(dir);
  free(name_copy);
  printf("create success: %d\n", success);
  return success;


  // bool success = (dir != NULL
  //                 && free_map_allocate (1, &inode_sector)
  //                 && inode_create (inode_sector, initial_size, is_dir)
  //                 && dir_add (dir, name, inode_sector));
  // if (!success && inode_sector != 0) 
  //   free_map_release (inode_sector, 1);
  // dir_close (dir);
  // free(name_copy);
  
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  if (name == NULL) {
    return NULL;
  }
  char* name_copy = malloc(strlen(name) + 1);
  if (name_copy == NULL) {
    return NULL;
  }
  strlcpy(name_copy, name, strlen(name) + 1);

  struct dir *dir = filesys_get_dir(name);
  struct inode *cur_inode = NULL;


  if (dir != NULL) {
    // printf("dir exists\n");
    dir_lookup(dir, name_copy, &cur_inode);
  }

  dir_close(dir);
  free(name_copy);

  if (cur_inode == NULL) {
    // printf("we really shouldn't be getting here\n");
    return NULL;
  }

  if (inode_is_dir(cur_inode)) {
    // printf("we shouldn't be getting here\n");
    struct dir *dir = dir_open(cur_inode);
    dir_lookup(dir, ".", &cur_inode);
    dir_close(dir);
    return file_open(cur_inode);
  } else {
    // printf("we should be getting here\n");
    return file_open(cur_inode);
  }
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  struct dir *dir = filesys_get_dir(name);
  //char *name_copy = malloc(strlen(name) + 1);
  //if (name_copy == NULL) {
  //  return false;
  //}
  //strlcpy(name_copy, name, strlen(name) + 1);
  
  bool success = dir != NULL && dir_remove(dir, name);
  dir_close(dir);
  //free(name_copy);
  // printf("remove success: %d\n", success);
  return success;

}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

bool filesys_chdir (const char *name) {
  struct dir *cur_dir = filesys_get_dir(name);

  char *name_copy = malloc(strlen(name) + 1);
  if (name_copy == NULL) {
    return false;
  }
  strlcpy(name_copy, name, strlen(name) + 1);

  struct inode *cur_inode = NULL;

  if (cur_dir != NULL) {
    dir_lookup(cur_dir, name, &cur_inode);
  }

  if (cur_inode == NULL || !inode_is_dir(cur_inode)) {
    return false;
  }

  struct thread *cur = thread_current();
  if (cur->cwd != NULL) {
    dir_close(cur->cwd);
  }
  cur->cwd = dir_open(cur_inode);
  dir_close(cur_dir);

  // print the current working directory (for testing, delete later)
  // struct dir *dir = cur->cwd;
  // char name_buf[NAME_MAX + 1];
  // char *token, *save_ptr;
  // token = strtok_r(name_copy, "/", &save_ptr);
  // if (token) {
  //   strlcpy(name_buf, token, strlen(token) + 1);
  // }
  // while (token != NULL) {
  //   token = strtok_r(NULL, "/", &save_ptr);
  //   if (token) {
  //     strlcpy(name_buf, token, strlen(token) + 1);
  //   }
  // }
  // printf("%s\n", name_buf);

  return true;

}




// /* Creates a directory named NAME.
//    Returns true if successful, false on failure.
//    Fails if a directory named NAME already exists,
//    or if an internal memory allocation fails. */
// bool filesys_mkdir (const char *name UNUSED) {
//   return false;
// }

struct dir *filesys_get_dir (const char *name) {
  struct dir *dir;
  char *name_copy = malloc(strlen(name) + 1);
  strlcpy(name_copy, name, strlen(name) + 1);


  if (name_copy[0] == '/' || thread_current()->cwd == NULL) {
    // printf("this is an absolute path\n");
    dir = dir_open_root();
  } else {
    struct thread *cur = thread_current();
    dir = dir_reopen(cur->cwd);
  }
  char *token, *save_ptr;
  char *next_token = NULL;
  token = strtok_r(name_copy, "/", &save_ptr);
  if (token) {
    next_token = strtok_r(NULL, "/", &save_ptr);
  }

  while (next_token != NULL) {
    struct inode *cur_inode;
    if (!dir_lookup(dir, token, &cur_inode)) {
      dir_close(dir);
      return NULL;
    }
    if (!inode_is_dir(cur_inode)) {
      dir_close(dir);
      return NULL;
    }
    dir = dir_open(cur_inode);
    token = next_token;
    next_token = strtok_r(NULL, "/", &save_ptr);
  }




  return dir;
  
}