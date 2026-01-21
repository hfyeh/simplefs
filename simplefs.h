#ifndef SIMPLEFS_H
#define SIMPLEFS_H

/* source: https://en.wikipedia.org/wiki/Hexspeak */
/* Magic number to identify the file system */
#define SIMPLEFS_MAGIC 0xDEADCELL

/* Superblock is always located at block 0 */
#define SIMPLEFS_SB_BLOCK_NR 0

/* Block size is 4KB */
#define SIMPLEFS_BLOCK_SIZE (1 << 12) /* 4 KiB */
#define SIMPLEFS_MAX_EXTENTS \
    ((SIMPLEFS_BLOCK_SIZE - sizeof(uint32_t)) / sizeof(struct simplefs_extent))
#define SIMPLEFS_MAX_BLOCKS_PER_EXTENT 8 /* It can be ~(uint32) 0 */
#define SIMPLEFS_MAX_SIZES_PER_EXTENT \
    (SIMPLEFS_MAX_BLOCKS_PER_EXTENT * SIMPLEFS_BLOCK_SIZE)
#define SIMPLEFS_MAX_FILESIZE                                          \
    ((uint64_t) SIMPLEFS_MAX_BLOCKS_PER_EXTENT * SIMPLEFS_BLOCK_SIZE * \
     SIMPLEFS_MAX_EXTENTS)

#define SIMPLEFS_FILENAME_LEN 255

#define SIMPLEFS_FILES_PER_BLOCK \
    (SIMPLEFS_BLOCK_SIZE / sizeof(struct simplefs_file))
#define SIMPLEFS_FILES_PER_EXT \
    (SIMPLEFS_FILES_PER_BLOCK * SIMPLEFS_MAX_BLOCKS_PER_EXTENT)

#define SIMPLEFS_MAX_SUBFILES (SIMPLEFS_FILES_PER_EXT * SIMPLEFS_MAX_EXTENTS)

/* simplefs partition layout
 * +---------------+
 * |  superblock   |  1 block
 * +---------------+
 * |  inode store  |  sb->nr_istore_blocks blocks
 * +---------------+
 * | ifree bitmap  |  sb->nr_ifree_blocks blocks
 * +---------------+
 * | bfree bitmap  |  sb->nr_bfree_blocks blocks
 * +---------------+
 * |    data       |
 * |      blocks   |  rest of the blocks
 * +---------------+
 */
#ifdef __KERNEL__
#include <linux/jbd2.h>
#endif

/*
 * On-disk inode structure.
 * This structure is stored in the inode store blocks.
 */
struct simplefs_inode {
    uint32_t i_mode;   /* File mode (type and permissions) */
    uint32_t i_uid;    /* Owner ID */
    uint32_t i_gid;    /* Group ID */
    uint32_t i_size;   /* File size in bytes */
    uint32_t i_ctime;  /* Inode change time */
    uint32_t i_atime;  /* Access time */
    uint32_t i_mtime;  /* Modification time */
    uint32_t i_blocks; /* Number of blocks allocated to this file */
    uint32_t i_nlink;  /* Number of hard links */
    uint32_t ei_block; /* Block number containing the extent index for this file */
    char i_data[32];   /* Store symlink content directly if small enough */
};

#define SIMPLEFS_INODES_PER_BLOCK \
    (SIMPLEFS_BLOCK_SIZE / sizeof(struct simplefs_inode))

#ifdef __KERNEL__
#include <linux/version.h>
/* compatibility macros */
#define SIMPLEFS_AT_LEAST(major, minor, rev) \
    LINUX_VERSION_CODE >= KERNEL_VERSION(major, minor, rev)
#define SIMPLEFS_LESS_EQUAL(major, minor, rev) \
    LINUX_VERSION_CODE <= KERNEL_VERSION(major, minor, rev)

/* A 'container' structure that keeps the VFS inode and additional on-disk
 * data. This is the in-memory representation of an inode.
 */
struct simplefs_inode_info {
    uint32_t ei_block; /* Block with list of extents for this file */
    char i_data[32];   /* Symlink content */
    struct inode vfs_inode; /* Kernel's VFS inode structure */
};

/*
 * Extent structure.
 * An extent represents a contiguous range of physical blocks.
 */
struct simplefs_extent {
    uint32_t ee_block; /* First logical block this extent covers */
    uint32_t ee_len;   /* Number of blocks covered by this extent */
    uint32_t ee_start; /* First physical block this extent covers */
    uint32_t nr_files; /* Number of files in this extent (used for directories) */
};

/*
 * Block containing extent information.
 * Pointed to by inode->ei_block.
 */
struct simplefs_file_ei_block {
    uint32_t nr_files; /* Number of files in directory (if this is a directory) */
    struct simplefs_extent extents[SIMPLEFS_MAX_EXTENTS]; /* List of extents */
};

/*
 * Directory entry structure.
 * Represents a file inside a directory.
 */
struct simplefs_file {
    uint32_t inode; /* Inode number */
    uint32_t nr_blk; /* Number of directory blocks used by this entry (for variable length, simplified here) */
    char filename[SIMPLEFS_FILENAME_LEN]; /* Filename */
};

/*
 * A directory block containing multiple directory entries.
 */
struct simplefs_dir_block {
    uint32_t nr_files; /* Number of files in this block */
    struct simplefs_file files[SIMPLEFS_FILES_PER_BLOCK]; /* Array of file entries */
};

/* superblock functions */
int simplefs_fill_super(struct super_block *sb, void *data, int silent);
void simplefs_kill_sb(struct super_block *sb);

/* inode functions */
int simplefs_init_inode_cache(void);
void simplefs_destroy_inode_cache(void);
struct inode *simplefs_iget(struct super_block *sb, unsigned long ino);

/* dentry function */
struct dentry *simplefs_mount(struct file_system_type *fs_type,
                              int flags,
                              const char *dev_name,
                              void *data);

/* file functions */
extern const struct file_operations simplefs_file_ops;
extern const struct file_operations simplefs_dir_ops;
extern const struct address_space_operations simplefs_aops;

/* extent functions */
extern uint32_t simplefs_ext_search(struct simplefs_file_ei_block *index,
                                    uint32_t iblock);

/* Getters for superblock and inode */
#define SIMPLEFS_SB(sb) (sb->s_fs_info)
/* Extract a simplefs_inode_info object from a VFS inode */
#define SIMPLEFS_INODE(inode) \
    (container_of(inode, struct simplefs_inode_info, vfs_inode))

#endif /* __KERNEL__ */

/*
 * Superblock info.
 * This structure is stored on disk at block 0 and also kept in memory.
 */
struct simplefs_sb_info {
    uint32_t magic; /* Magic number to verify filesystem type */

    uint32_t nr_blocks; /* Total number of blocks (incl sb & inodes) */
    uint32_t nr_inodes; /* Total number of inodes */

    uint32_t nr_istore_blocks; /* Number of inode store blocks */
    uint32_t nr_ifree_blocks;  /* Number of inode free bitmap blocks */
    uint32_t nr_bfree_blocks;  /* Number of block free bitmap blocks */

    uint32_t nr_free_inodes; /* Number of free inodes */
    uint32_t nr_free_blocks; /* Number of free blocks */

    unsigned long *ifree_bitmap; /* In-memory free inodes bitmap */
    unsigned long *bfree_bitmap; /* In-memory free blocks bitmap */
#ifdef __KERNEL__
    journal_t *journal;
    struct block_device *s_journal_bdev; /* v5.10+ external journal device */
#if SIMPLEFS_AT_LEAST(6, 9, 0)
    struct file *s_journal_bdev_file; /* v6.11 external journal device */
#elif SIMPLEFS_AT_LEAST(6, 7, 0)
    struct bdev_handle
        *s_journal_bdev_handle; /* v6.7+ external journal device */
#endif /* SIMPLEFS_AT_LEAST */
#endif /* __KERNEL__ */
};

#endif /* SIMPLEFS_H */
