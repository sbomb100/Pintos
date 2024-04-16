#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"

/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry 
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, block_sector_t parent, size_t entry_cnt)
{
  // return inode_create (sector, entry_cnt * sizeof (struct dir_entry), true);
  // entry_cnt += 2;
  bool success = inode_create (sector, entry_cnt * sizeof (struct dir_entry), true);
  if (success) {
    struct dir *dir = dir_open(inode_open(sector));
    struct dir_entry e[2];
    e[0].in_use = true;
    strlcpy(e[0].name, ".", sizeof(e[0].name));
    e[0].inode_sector = sector;
    inode_write_at(dir->inode, &e[0], sizeof(e[0]), 0);

    e[1].in_use = true;
    strlcpy(e[1].name, "..", sizeof(e[1].name));
    e[1].inode_sector = parent;
    inode_write_at(dir->inode, &e[1], sizeof(e[1]), sizeof(e[0]));
    // inode_close(dir->inode);
    dir_close(dir);
  }
  return success;
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  ASSERT(dir != NULL);
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) {

    /* e.name is the full path, so this culls it down to just the file name. */
    char *last_entry = strrchr(e.name, '/');
    if (last_entry != NULL) {
      last_entry++;
    } else {
      last_entry = e.name;
    }

    // does the same to name
    char *name_copy = malloc(strlen(name) + 1);
    if (name_copy == NULL) {
      return false;
    }
    strlcpy(name_copy, name, strlen(name) + 1);
    char *last_name = strrchr(name_copy, '/');
    if (last_name != NULL) {
      last_name++;
    } else {
      last_name = name_copy;
    }

    if (e.in_use && (!strcmp (last_name, last_entry) || !strcmp(name, e.name)))
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        free(name_copy);
        return true;
      } 
      free(name_copy);
       }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  inode_lock(dir->inode);

  // printf("dir_lookup: %s\n", name);

  // if just "/", return root
  if (strcmp(name, "/") == 0) {
    *inode = inode_open(ROOT_DIR_SECTOR);
    inode_unlock(dir->inode);
    return true;
  }

  if (lookup (dir, name, &e, NULL)) {
    // printf("dir_lookup: %s\n", e.name);
    *inode = inode_open (e.inode_sector);
  }
  else {
    // printf("setting inode to NULL\n");
    *inode = NULL;
  }
  inode_unlock(dir->inode);
  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  // printf("removing %s\n", name);
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (strcmp (name, ".") == 0 || strcmp (name, "..") == 0) {
    return false;
  }

  inode_lock(dir->inode);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  if (inode_is_directory(inode) && (inode_get_open_cnt(inode) > 1)) {
    // printf("directory has %d open files\n", inode_get_open_cnt(inode));
    goto done;
  }

  if (inode_is_directory(inode) && !dir_is_empty(dir_open(inode))) {
    // printf("directory is not empty\n");
    goto done;
  }

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) {
    goto done;
  }

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  inode_unlock(dir->inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;
  struct inode *inode = dir->inode;
  inode_lock(inode);
  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
  {
    dir->pos += sizeof e;
    if (e.in_use && strcmp(e.name, ".") && strcmp(e.name, ".."))
      {
        strlcpy (name, e.name, NAME_MAX + 1);
        inode_unlock(inode);
        return true;
      } 
  }
  inode_unlock(inode);
  return false;
}

/* Returns true if the directory is empty, false otherwise. */
bool
dir_is_empty (struct dir *dir)
{
  struct dir_entry e;
  off_t ofs;

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e; ofs += sizeof e) {
    if (strcmp(e.name, ".") == 0 || strcmp(e.name, "..") == 0) {
      continue;
    }
    if (e.in_use) {
      return false;
    }
  }
  return true;
}