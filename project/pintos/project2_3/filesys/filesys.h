#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "inode.h"

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */
#define CACHE_SIZE 64

/* Block device that contains the file system. */
struct block *fs_device;

struct cache_entry
	{
    block_sector_t sector_num;
    bool recently_used;
    bool valid;
    bool dirty;
    char data[BLOCK_SECTOR_SIZE];
    struct lock data_lock;
  };

int clock_hand;
struct lock metadata_lock;
struct cache_entry cache[CACHE_SIZE];

void filesys_init (bool format);
void filesys_done (void);
bool filesys_create (const char *name, off_t initial_size, bool is_dir);
struct file *filesys_open (const char *name);
bool filesys_remove (const char *name);
bool filesys_chdir (const char *name);
unsigned long long filesys_get_read_cnt (void);
unsigned long long filesys_get_write_cnt (void);
void filesys_reset_read_cnt (void);

void cache_entry_init (struct cache_entry *ent);
void cache_init (void);
void cache_write_to_disk (struct cache_entry *ent);
/* 	
	Returns the index of cache entry given SECTOR. 
	If sector number is not originally in cache, use clock algorithm to find something to evict, 
	place sector in cache, and return the index. 
*/
int cache_find_entry (block_sector_t sector);
int cache_evict (void);
void cache_read (block_sector_t sector, void *);
void cache_write (block_sector_t sector, const void *);
void cache_close (void);
void cache_reset (void);
int cache_get_hit_cnt (void);
int cache_get_miss_cnt (void);


#endif /* filesys/filesys.h */
