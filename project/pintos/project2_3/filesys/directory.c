#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"

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
dir_create (block_sector_t sector, size_t entry_cnt)
{
  bool ret = inode_create (sector, entry_cnt * sizeof (struct dir_entry), true);
  if (ret == false)
    return false;

  struct dir *dir = dir_open (inode_open (sector));
  if (dir == NULL)
    return false;
  struct dir_entry dir_e;
  dir_e.inode_sector = sector;

  if (inode_write_at (dir->inode, &dir_e, sizeof dir_e, 0) != sizeof dir_e)
    ret = false;
  dir_close (dir);
  return ret;
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
  // TODO
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
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

  if (strcmp (name, "..") == 0)
  {
    inode_read_at (dir->inode, &e, sizeof e, 0);
    *inode = inode_open (e.inode_sector);
  }
  else if (strcmp (name, ".") == 0)
    *inode = inode_reopen (dir->inode);
  else if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector, bool is_dir)
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

  if (is_dir)
  {
    struct dir *child = dir_open (inode_open (inode_sector));
    if (!child)
      goto done;
    e.inode_sector = inode_get_inumber (dir_get_inode (dir));
    if (inode_write_at (child->inode, &e, sizeof e, 0) != sizeof e)
    {
      dir_close (child);
      goto done;
    }
    dir_close(child);
  }


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
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  if (inode_is_dir (inode) && inode_get_open_cnt (inode) > 2)
    goto done;

  // if (inode_is_dir (inode) && !dir_empty (inode))
  //   goto done;

  struct dir *direct = dir_open (inode);
  if (inode_is_dir (inode) && !dir_empty (direct))
    goto done;

  // if (inode_is_dir (inode))
  // {
  //   struct dir *direct = dir_open (inode);
  //   if (!dir_empty (direct))
  //   {
  //     dir_close (direct);
  //     goto done;
  //   }
  //   dir_close (direct);
  // }

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}

void
parse_file_name (const char *name, char *directory, char *file_name)
{
  int length = strlen (name);
  char *name_copy = (char *) malloc (sizeof (char) * (length + 1));
  memcpy (name_copy, name, length + 1);
  char *dir = directory;
  if (length > 0 && name[0] == '/')
    if (dir)
      *dir++ = '/';

  char *token, *pointer, *last = "";
  for (token = strtok_r (name_copy, "/", &pointer); token != NULL; token = strtok_r (NULL, "/", &pointer))
  {
    int token_length = strlen (last);
    if (dir && token_length > 0)
    {
      memcpy (dir, last, token_length);
      dir[token_length] = '/';
      dir += token_length + 1;
    }
    last = token;
  }
  if (dir)
    *dir = '\0';
  memcpy (file_name, last, strlen(last) + 1);
  free (name_copy);
}

struct dir *
dir_open_path (const char *path)
{
  int length = strlen (path);
  char copy[length + 1];
  strlcpy (copy, path, length + 1);
  struct dir *current;
  struct thread *t = thread_current ();
  if (path[0] == '/' || !t->cwd)
    current = dir_open_root ();
  else
  {
    current = dir_reopen (t->cwd);
  }

  char *token, *pointer;
  for (token = strtok_r (copy, "/", &pointer); token != NULL; token = strtok_r (NULL, "/", &pointer))
  {
    struct inode *inode = NULL;
    if (!dir_lookup (current, token, &inode))
    {
      dir_close (current);
      return NULL;
    }
    struct dir *next = dir_open (inode);
    if (!next)
    {
      dir_close (current);
      return NULL;
    }
    dir_close (current);
    current = next;
  }
  if (inode_is_removed (dir_get_inode (current)))
  {
    dir_close (current);
    return NULL;
  }
  return current;
}

struct inode*
dir_get_parent (struct dir *dir)
{
  struct dir_entry e;
  if (dir == NULL)
    return NULL;
  inode_read_at (dir->inode, &e, sizeof e, 0);
  return inode_open (e.inode_sector);
}

bool
dir_is_root (struct dir *dir)
{
  ASSERT (dir != NULL);
  return inode_get_inumber (dir_get_inode (dir)) == ROOT_DIR_SECTOR;
}

bool
dir_empty (const struct dir *dir)
{
  struct dir_entry e;
  size_t ofs;

  ASSERT (dir != NULL);

  for (ofs = sizeof e; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use) 
      return false;
  return true;
}