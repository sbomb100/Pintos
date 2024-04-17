#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"
#include <stdio.h>

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

/* direct + indirect/double indirect == 124*/
#define INODE_DIRECT_CNT 123

/*indirect sector will have 128 pointers*/
#define INODE_INDIRECT_SECTOR_CNT 128

#define INODE_DIRECT_BYTES (123 * BLOCK_SECTOR_SIZE)

#define INODE_DOUB_INDIRECT_BYTES (128 * BLOCK_SECTOR_SIZE)

#define INODE_BLOCK_CNT 124

#define MAX_FILE_SIZE (8 * 1024 * 1024)
/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
   //fix to work with our layout
struct inode_disk
  {
    block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    int is_directory;                   /* 1 = directory, 0 = file */
    block_sector_t sectors[INODE_BLOCK_CNT];               
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk *data;             /* Inode ptr. */
    struct lock inode_lock;                   /* Lock for inode. */
  };

/* Returns the block device sector that contains byte offset POS
   within INODE.
   If no sector: allcoate it

   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (struct inode *inode, off_t pos, bool is_directory) //TODO: remove success, remove some redundancies in if/else
{
  struct cache_block *cache_block;
  bool allocated = false;

  ASSERT(inode != NULL);
  ASSERT(pos < MAX_FILE_SIZE);

  if (!is_directory)
    lock_acquire (&inode->inode_lock);
  
  size_t sector_idx = pos / BLOCK_SECTOR_SIZE;
  if (sector_idx >= INODE_DIRECT_CNT)
    sector_idx = INODE_DIRECT_CNT;
  
  //use the idx on the inode sector array
  cache_block = cache_get_block(inode->sector, true);
  inode->data = (struct inode_disk *) cache_block->data;
  block_sector_t next_sector = inode->data->sectors[sector_idx]; //next_sector is the sector we are heading to - may be datablock or doubl indirect
  cache_put_block(cache_block);

  /*if next sector = 0, this block has not been allocated. We will allocate it*/
  if(next_sector == 0){

    if (!free_map_allocate (1, &next_sector)){ //get a free block. this will become the destination block.
      /*Entering this if statement means allocation failed.*/
      if (!is_directory){
        lock_release (&inode->inode_lock);
      }
      return 0; //fail

    }

    /*record in the inode the new block we just allocated*/
    cache_block = cache_get_block (inode->sector, true);
    inode->data = (struct inode_disk *) cache_block->data;       
    inode->data->sectors[sector_idx] = next_sector; //record the info to newly allocated block
    cache_mark_block_dirty(cache_block);
    cache_put_block(cache_block);

    /*zero out the newly allocated block*/
    bool exclusive = sector_idx >= INODE_DIRECT_CNT; //if inode
    cache_block = cache_get_block (next_sector, exclusive);
    cache_zero_block(cache_block);
    cache_mark_block_dirty(cache_block);

    allocated = true;
  }

  /*if the sector we are looking for is direct, process it and return*/
  if(sector_idx < INODE_DIRECT_CNT){ 
    //printf("byte_to_sector direct\n");
    if(allocated){
      cache_put_block(cache_block);
    }
    
    if (!is_directory)
      lock_release (&inode->inode_lock);
    return next_sector;

  }
  else{ //sector is not in direct. Look in the doubly indirect
    block_sector_t dub_indir_sector = next_sector; //doubly indirect sector
    block_sector_t *dub_indir_data;
    size_t dub_indir_sector_idx = (pos - INODE_DIRECT_BYTES) / INODE_DOUB_INDIRECT_BYTES; //change it to indirect bytes 
    // printf("byte_to_sector indirect idx: %d\n", sector_idx);
    if(!allocated){
      cache_block = cache_get_block(dub_indir_sector, true);
    }

    dub_indir_data = (block_sector_t *) cache_block->data;
    next_sector = dub_indir_data[dub_indir_sector_idx]; //get the indir block sector pointed to by the index of doubly indirect block
    cache_put_block(cache_block);

    /*the needed indir block is not present. allocate*/
    if(next_sector == 0){ 

      if (!free_map_allocate (1, &next_sector)){ //get a free block. this will become the destination block.
        /*Entering this if statement means allocation failed.*/
        if (!is_directory){
          lock_release (&inode->inode_lock);
        }
        return 0; //fail
      }

      /*Assign the newly fetched block to the doubly indir list*/
      cache_block = cache_get_block(dub_indir_sector, true);
      dub_indir_data = (block_sector_t *) cache_block->data;
      dub_indir_data[dub_indir_sector_idx] = next_sector;
      cache_mark_block_dirty(cache_block);
      cache_put_block(cache_block);

      /*zero out the newly installed indir sector*/
      cache_block = cache_get_block(next_sector, true);
      cache_zero_block(cache_block);
      cache_mark_block_dirty(cache_block);

    }
    /*needed indir block is present*/
    else{
      cache_block = cache_get_block(next_sector, true);
    }

    block_sector_t indir_sector = next_sector; //indirect sector
    block_sector_t *indir_data = (block_sector_t *) cache_block->data;
    /*calcualte the idx to use in the indir sector*/
    size_t indir_sector_idx = ((pos - INODE_DIRECT_BYTES) % INODE_DOUB_INDIRECT_BYTES ) / BLOCK_SECTOR_SIZE;  //double check this?? I think its right
    next_sector = indir_data[indir_sector_idx];
    cache_put_block(cache_block);

    /*if the direct block we want to get does not exist. allocate*/
    if(next_sector == 0){ 

      if (!free_map_allocate (1, &next_sector)){ //get a free block. this will become the destination block.
        /*Entering this if statement means allocation failed.*/
        if (!is_directory){
          lock_release (&inode->inode_lock);
        }
        return 0; //fail
      }

      /*Assign the newly fetched block to indir block*/
      cache_block = cache_get_block(indir_sector, true);
      indir_data = (block_sector_t *) cache_block->data;
      indir_data[indir_sector_idx] = next_sector;
      cache_mark_block_dirty(cache_block);
      cache_put_block(cache_block);

      /*zero out the newly installed block*/
      cache_block = cache_get_block(next_sector, true);
      cache_zero_block(cache_block);
      cache_mark_block_dirty(cache_block);
      cache_put_block(cache_block);

    }

    if (!is_directory){
      lock_release (&inode->inode_lock);
    }
    block_sector_t direct_sector = next_sector;

    return direct_sector;

  }

}


/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;
static struct lock open_inodes_lock;

/* Initializes the inode module. */
void
inode_init (void) //done
{
  list_init (&open_inodes);
  lock_init(&open_inodes_lock);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_directory) //done
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      // size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      disk_inode->is_directory = is_directory;

      struct cache_block *cache_block = cache_get_block(sector, true);
      if (cache_block != NULL) {
            memcpy(cache_block->data, disk_inode, sizeof *disk_inode); // write contents of disk inode into cache
            cache_mark_block_dirty(cache_block);
            cache_put_block(cache_block);
            success = true;
       }
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)//done
{
  struct list_elem *e;
  struct inode *inode;
  // printf("looking to open sector: %d\n", sector);
  

  lock_acquire (&open_inodes_lock);
  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes); e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode->open_cnt++;
          lock_release(&open_inodes_lock);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL){
    lock_release(&open_inodes_lock);
    return NULL;
  }

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  lock_init(&inode->inode_lock);
  // struct cache_block *cache_block = cache_get_block (inode->sector, false);
  // void *cache_data = cache_read_block(cache_block);
  // memcpy(&inode->data, cache_data, BLOCK_SECTOR_SIZE);
  // cache_put_block(cache_block);
  lock_release(&open_inodes_lock);
  return inode;
}

/* Reopens and returns INODE. You should probably lock open_inodes_lock before using this*/
struct inode *
inode_reopen (struct inode *inode) //done
{
  lock_acquire(&open_inodes_lock);
  if (inode != NULL)
    inode->open_cnt++;
  lock_release(&open_inodes_lock);
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode) //done
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) //TODO 
{
  if (inode == NULL)
     return;

  lock_acquire(&inode->inode_lock);

  //lock_acquire(&open_inodes_lock); //needed? test

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
  {
    list_remove (&inode->elem);
    lock_release(&inode->inode_lock);

    /*remove & dealloc the blocks if removed*/
    if (inode->removed){

      struct cache_block *cache_block = cache_get_block(inode->sector, true);
      inode->data = (struct inode_disk *) cache_block->data;
      for(size_t i = 0; i < INODE_DIRECT_CNT; i++){
        block_sector_t direct_sector = inode->data->sectors[i];
        if(direct_sector != 0){
          free_map_release(direct_sector, 1);
        }
      }
      free_map_release(inode->sector, 1);

      block_sector_t dub_indir_sector = inode->data->sectors[INODE_DIRECT_CNT]; //sectors[123] is location that holds the dub indir sector num
      cache_put_block(cache_block);

      if(dub_indir_sector != 0){
        for(size_t dub_indir_idx = 0; dub_indir_idx < INODE_INDIRECT_SECTOR_CNT; dub_indir_idx++){
          cache_block = cache_get_block(dub_indir_sector, true);
          block_sector_t *dub_indir_data = (block_sector_t *) cache_block->data;
          block_sector_t indir_sector = dub_indir_data[dub_indir_idx];
          cache_put_block(cache_block);

          if(indir_sector != 0){
            /*traverse through indirect sector's indexes and free allocated blocks*/
            for(size_t indir_idx = 0; indir_idx < INODE_INDIRECT_SECTOR_CNT; indir_idx++){
              cache_block = cache_get_block(indir_sector, true);
              block_sector_t *indir_data = (block_sector_t *) cache_block->data;
              block_sector_t direct = indir_data[indir_idx];
              cache_put_block(cache_block);
              if(direct != 0){
                free_map_release(direct, 1);
              }

            }
            free_map_release(indir_sector, 1);
          }
        }
        free_map_release(dub_indir_sector, 1);
      }
      
    }

    
    free (inode); 
  }
  else{
    lock_release(&inode->inode_lock);
  }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) //done
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) //done
{
  //printf("inode_read_at size: %d\n", size);
  // printf("inode_read_at offset: %d\n", offset);
  if(size < 0){
    return 0;
  }


  struct cache_block *cache_block;
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  off_t length = inode_length(inode);
  bool is_directory;
  block_sector_t sector;

  if(offset >= length){
    return 0;
  }

  is_directory = inode_is_directory(inode);

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      sector = byte_to_sector (inode, offset, is_directory);

      if(sector == 0){
        break;
      }
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = length - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      cache_block = cache_get_block(sector, false);
      memcpy(buffer + bytes_read, cache_block->data + sector_ofs, chunk_size);
      cache_put_block(cache_block);


      // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
      //   {
      //     /* Read full sector directly into caller's buffer. */
      //     struct cache_block *cache_block = cache_get_block (sector_idx, false);
      //     void *cache_data = cache_read_block(cache_block);
      //     memcpy(buffer + bytes_read, cache_data, BLOCK_SECTOR_SIZE);
      //     cache_mark_block_dirty(cache_block);
      //     cache_put_block(cache_block);
      //   }
      // else 
      //   {
      //     /* Read sector into bounce buffer, then partially copy
      //        into caller's buffer. */
      //     // if (bounce == NULL) 
      //     //   {
      //     //     bounce = malloc (BLOCK_SECTOR_SIZE);
      //     //     if (bounce == NULL)
      //     //       break;
      //     //   }
      //     struct cache_block *cache_block = cache_get_block (sector_idx, false);
      //     void *cache_data = cache_read_block(cache_block);
      //     memcpy(buffer + bytes_read, cache_data + sector_ofs, chunk_size);
      //     cache_mark_block_dirty(cache_block);
      //     cache_put_block(cache_block);
      //   }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  return bytes_read;
}

//maybe integrate to above TODO:
static off_t
update_length (struct inode *inode, off_t offset)
{
  struct cache_block *cache_block;
  off_t length;
  
  cache_block = cache_get_block (inode->sector, true);
  inode->data = (struct inode_disk *) cache_block->data;
  length = inode->data->length;
  if (offset > length)
    {
      length = offset;
      inode->data->length = length;
      cache_mark_block_dirty(cache_block);
      cache_put_block (cache_block);
    }
  else
    cache_put_block (cache_block);
  return length;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) //TODO
{
  //printf("inode_write_at size: %d\n", size);
  struct cache_block *cache_block;
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  // uint8_t *bounce = NULL;
  off_t length = inode_length(inode);
  bool is_directory;
  block_sector_t sector;

  if (inode->deny_write_cnt)
    return 0;

  if(size <= 0){
    return 0;
  }

  // check if it can read ahead
  off_t next_sector = offset + BLOCK_SECTOR_SIZE - 1;
  if (size == 0 && next_sector > offset && next_sector < length && (byte_to_sector(inode, next_sector, is_directory) != 0)) {
    // void send_read_ahead_request(block_sector_t sector);
    send_read_ahead_request(next_sector);
  }

  //fetch the is_directory status
  // cache_block = cache_get_block(inode->sector, true);
  // is_directory = ( (struct inode_disk *)cache_block->data)->is_directory;
  // cache_put_block(cache_block);
  is_directory = inode_is_directory(inode);

  while (size > 0) 
    {
      // if(offset %1 == 0)
      //   printf("offset: %d\n", offset);
      
      sector = byte_to_sector (inode, offset, is_directory);

      // if(offset %1 == 0)
      //   printf("sector: %d\n", sector);
      if(sector == 0){
        break;
      }       
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = MAX_FILE_SIZE - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

        

      cache_block = cache_get_block(sector, false);
      // if(offset %1 == 0)
      //   printf("sector_ofs: %d\n", sector_ofs);
      memcpy(cache_block->data + sector_ofs, buffer + bytes_written, chunk_size);
      cache_mark_block_dirty(cache_block);
      cache_put_block(cache_block);
 

      // if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
      //   {
      //     /* Write full sector directly to disk. */
      //     struct cache_block *cache_block = cache_get_block (sector_idx, true);
      //     void *cache_data = cache_zero_block(cache_block);
      //     memcpy(cache_data, buffer + bytes_written, BLOCK_SECTOR_SIZE);
      //     cache_mark_block_dirty(cache_block);
      //     cache_put_block(cache_block);
      //   }
      // else 
      //   {
      //     /* We need a bounce buffer. */
      //     // if (bounce == NULL) 
      //     //   {
      //     //     bounce = malloc (BLOCK_SECTOR_SIZE);
      //     //     if (bounce == NULL)
      //     //       break;
      //     //   }

      //     // /* If the sector contains data before or after the chunk
      //     //    we're writing, then we need to read in the sector
      //     //    first.  Otherwise we start with a sector of all zeros. */
      //     // if (sector_ofs > 0 || chunk_size < sector_left) {
      //     //   struct cache_block *cache_block = cache_get_block (sector_idx, false);
      //     //   void *cache_data = cache_read_block(cache_block);
      //     //   memcpy (bounce, cache_data, BLOCK_SECTOR_SIZE);
      //     //   cache_put_block(cache_block);
      //     // }
      //     // else {
      //     //   memset (bounce, 0, BLOCK_SECTOR_SIZE);
      //     // memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
      //     struct cache_block *cache_block = cache_get_block (sector_idx, true);
      //     // void *cache_data = cache_zero_block(cache_block);
      //     void *cache_data = cache_read_block(cache_block);
      //     // memcpy(cache_data, bounce, BLOCK_SECTOR_SIZE);
      //     memcpy(cache_data + sector_ofs, buffer + bytes_written, chunk_size);
      //     cache_mark_block_dirty(cache_block);
      //     cache_put_block(cache_block);
      //     // }
      //   }
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }

  // free (bounce);
  length = update_length(inode, offset);
  if(length == -1){
    printf("I just put this here to stop compiler warnings. length may be used in future for read ahead.");
  }
  
  //       // try to read ahead
  // off_t next_sector = offset + BLOCK_SECTOR_SIZE - 1;
  // if (size == 0 && next_sector > offset && next_sector < length && (byte_to_sector(inode, next_sector, is_directory) != 0)) {
  //   // void send_read_ahead_request(block_sector_t sector);
  //   send_read_ahead_request(next_sector);
  // }


  return bytes_written;
}



/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) //done
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) //done
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (struct inode *inode)//done? //is it correct to modify inode->data?
{
  struct cache_block *cache_block = cache_get_block(inode->sector, true);
  inode->data = cache_read_block(cache_block);
  off_t length = inode->data->length;
  cache_put_block(cache_block);

  return length;
}

bool
inode_is_directory (struct inode *inode){
  struct cache_block *cache_block = cache_get_block (inode->sector, false);
  void *cache_data = cache_read_block(cache_block);
  struct inode_disk *disk_inode = (struct inode_disk *) cache_data;
  cache_put_block(cache_block);
  return disk_inode->is_directory;

}

int inode_get_open_cnt (struct inode *inode) {
  return inode->open_cnt;
}


void
lock_inode(struct inode *inode) {
  lock_acquire(&inode->inode_lock);
}

void
unlock_inode(struct inode *inode) {
  lock_release(&inode->inode_lock);
}
