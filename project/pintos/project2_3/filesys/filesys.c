#include "threads/thread.h"
#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/malloc.h"

/* Partition that contains the file system. */
struct block *fs_device;

int hit_cnt;
int miss_cnt;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  cache_init ();
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
  cache_close ();
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir) 
{
  block_sector_t inode_sector = 0;

  char directory[strlen (name)], file_name[strlen (name)];
  parse_file_name (name, directory, file_name);
  struct dir *dir = dir_open_path (directory);
  bool success = false;
  if (strcmp(file_name, ".") != 0 && strcmp(file_name, "..") != 0)
  {
    success = (dir != NULL
               && free_map_allocate (1, &inode_sector)
               && inode_create (inode_sector, initial_size, is_dir)
               && dir_add (dir, file_name, inode_sector, is_dir));
  }
  
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

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
  int length = strlen (name);
  if (length == 0)
    return NULL;
  char directory[length + 1], file_name[length + 1];
  parse_file_name (name, directory, file_name);

  struct dir *dir = dir_open_path (directory);
  struct inode *inode = NULL;

  if (dir != NULL)
  {
    if (strcmp (file_name, "..") == 0)
    {
      inode = dir_get_parent (dir);
      if (inode == NULL)
        return NULL;
    }
    else if ((dir_is_root (dir) && strlen (file_name) == 0) || strcmp(file_name, ".") == 0)
      return (struct file *) dir;
    else
      dir_lookup (dir, file_name, &inode);
  }

  dir_close (dir);
  if (inode == NULL)
    return NULL;
  if (inode_is_dir (inode))
    return (struct file *) dir_open(inode);
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  char directory[strlen (name)], file_name[strlen (name)];
  parse_file_name (name, directory, file_name);
  struct dir *dir = dir_open_path (directory);
  
  bool success = dir != NULL && dir_remove (dir, file_name);
  dir_close (dir); 

  return success;
}

unsigned long long
filesys_get_read_cnt (void)
{
  return get_read_cnt (fs_device);
}

unsigned long long
filesys_get_write_cnt (void)
{
  return get_write_cnt (fs_device);
}

void 
filesys_reset_read_cnt (void) 
{
  reset_read_cnt (fs_device);
}



/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

void 
cache_entry_init (struct cache_entry *ent)
{
  ent->sector_num = 0;
  ent->recently_used = false;
  ent->dirty = false;
  ent->valid = false;
  lock_init(&(ent->data_lock));
}

void 
cache_init (void) 
{
  clock_hand = 0;
  lock_init (&metadata_lock);
  hit_cnt = 0;
  miss_cnt = 0;
  int i;
  for (i = 0; i < CACHE_SIZE; i++)
    cache_entry_init(&(cache[i]));
}

void 
cache_write_to_disk (struct cache_entry *ent) 
{
  ASSERT (lock_held_by_current_thread (&metadata_lock));
  ASSERT (ent != NULL && ent->valid == true);

  if (ent->dirty) {
    lock_acquire (&ent->data_lock);
    block_write (fs_device, ent->sector_num, ent->data);
    ent->dirty = false;
    lock_release (&ent->data_lock);
  }
}

int 
cache_find_entry (block_sector_t sector) 
{
  int i; 
  for (i = 0; i < CACHE_SIZE; i++)
    if (cache[i].valid == true)
      if (cache[i].sector_num == sector) {
        hit_cnt += 1;
        return i;
      }
  miss_cnt += 1;
  return -1;
}

int
cache_evict (void) 
{
  ASSERT (lock_held_by_current_thread (&metadata_lock));

  while (cache[clock_hand].recently_used) 
  {
    if (cache[clock_hand].valid == false)
      return clock_hand;
    cache[clock_hand].recently_used = false;
    clock_hand = (clock_hand + 1) % CACHE_SIZE;
  }

  struct cache_entry *entry = &cache[clock_hand];
  if (entry->dirty) 
    cache_write_to_disk (entry);

  entry->valid = false;
  int hand = clock_hand;
  clock_hand = (clock_hand + 1) % CACHE_SIZE;
  return hand;
}

void 
cache_read (block_sector_t sector, void *buffer) 
{
  lock_acquire (&metadata_lock);
  struct cache_entry *entry;
  int hand;
  int ent = cache_find_entry (sector);
  /* Cache miss, look for empty entry or if necessary, evict entry */
  if (ent == -1)
  {
    hand = cache_evict ();
    entry = &cache[hand];

    ASSERT (entry != NULL);
    ASSERT (entry->valid == false);

    lock_acquire (&entry->data_lock);
    entry->sector_num = sector;
    entry->dirty = false;
    entry->valid = true;
    block_read (fs_device, sector, entry->data);
    lock_release (&entry->data_lock);
  }
  else 
  {
    entry = &cache[ent];
  }
  
  lock_acquire (&entry->data_lock);
  entry->recently_used = true;
  memcpy (buffer, entry->data, BLOCK_SECTOR_SIZE);
  lock_release (&entry->data_lock);

  lock_release (&metadata_lock);
}

void 
cache_write (block_sector_t sector, const void *buffer) 
{
  lock_acquire (&metadata_lock);

  struct cache_entry *entry;
  int hand;
  int ent = cache_find_entry (sector);
  /* Cache miss, look for empty entry or if necessary, evict entry */
  if (ent == -1)
  {
    hand = cache_evict ();
    entry = &cache[hand];

    ASSERT (entry != NULL);
    ASSERT (entry->valid == false);

    lock_acquire (&entry->data_lock);
    entry->sector_num = sector;
    entry->dirty = false;
    entry->valid = true;
    block_read (fs_device, sector, entry->data);
    lock_release (&entry->data_lock);
  }
  else 
  {
    entry = &cache[ent];
  }

  lock_acquire (&entry->data_lock);
  entry->recently_used = true;
  memcpy (entry->data, buffer, BLOCK_SECTOR_SIZE);
  entry->dirty = true;
  lock_release (&entry->data_lock);

  lock_release (&metadata_lock);
}

void
cache_close (void)
{
  lock_acquire (&metadata_lock);

  int i;
  for (i = 0; i < CACHE_SIZE; i++)
  {
    if (cache[i].valid == true) {
      cache_write_to_disk (&cache[i]);
    }
  }

  lock_release (&metadata_lock);
}

int 
cache_get_hit_cnt (void) 
{
  return hit_cnt;
}

int 
cache_get_miss_cnt (void)
{
  return miss_cnt;
}

bool filesys_chdir (const char *name)
{
  char directory[strlen (name)], file_name[strlen (name)];
  parse_file_name (name, directory, file_name);
  struct dir *dir = dir_open_path (directory);
  struct inode *inode = NULL;
  
  if(dir == NULL)
    return false;
  else if (strcmp (file_name, "..") == 0)
  {
    inode = dir_get_parent (dir);
    if (inode == NULL)
      return false;
  }
  else if (strcmp (file_name, ".") == 0 || (strlen (file_name) == 0
    && inode_get_inumber (dir_get_inode (dir)) == ROOT_DIR_SECTOR))
  {
    thread_current ()->cwd = dir;
    return true;
  }
  else
    dir_lookup (dir, file_name, &inode);

  dir_close (dir);

  dir = dir_open (inode);
  if (dir == NULL)
    return false;
  else
  {
    dir_close (thread_current ()->cwd);
    thread_current ()->cwd = dir;
    return true;
  }
}