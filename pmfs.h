/*
 * BRIEF DESCRIPTION
 *
 * Definitions for the PMFS filesystem.
 *
 * Copyright 2012-2013 Intel Corporation
 * Copyright 2009-2011 Marco Stornelli <marco.stornelli@gmail.com>
 * Copyright 2003 Sony Corporation
 * Copyright 2003 Matsushita Electric Industrial Co., Ltd.
 * 2003-2004 (c) MontaVista Software, Inc. , Steve Longerbeam
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */
#ifndef __PMFS_H
#define __PMFS_H

#include <linux/fs.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/crc16.h>
#include <linux/mutex.h>
#include <linux/pagemap.h>
#include <linux/rcupdate.h>
#include <linux/types.h>
#include <linux/rbtree.h>
#include <linux/version.h>
#include <linux/kthread.h>
#include <linux/uio.h>
#include <asm/tlbflush.h>

#include "pmfs_def.h"
#include "journal.h"
#include "stats.h"

#define PAGE_SHIFT_2M 21
#define PAGE_SHIFT_1G 30

#define PMFS_ASSERT(x)                                                 \
	if (!(x)) {                                                     \
		printk(KERN_WARNING "assertion failed %s:%d: %s\n",     \
	               __FILE__, __LINE__, #x);                         \
	}

/*
 * Debug code
 */
#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#endif

/* #define pmfs_dbg(s, args...)         pr_debug(s, ## args) */
#define pmfs_dbg(s, args ...)           pr_info(s, ## args)
#define pmfs_dbg1(s, args ...)
#define pmfs_err(sb, s, args ...)       pmfs_error_mng(sb, s, ## args)
#define pmfs_warn(s, args ...)          pr_warning(s, ## args)
#define pmfs_info(s, args ...)          pr_info(s, ## args)

extern unsigned int pmfs_dbgmask;
#define PMFS_DBGMASK_MMAPHUGE          (0x00000001)
#define PMFS_DBGMASK_MMAP4K            (0x00000002)
#define PMFS_DBGMASK_MMAPVERBOSE       (0x00000004)
#define PMFS_DBGMASK_MMAPVVERBOSE      (0x00000008)
#define PMFS_DBGMASK_VERBOSE           (0x00000010)
#define PMFS_DBGMASK_TRANSACTION       (0x00000020)

#define pmfs_dbg_mmaphuge(s, args ...)		 \
	((pmfs_dbgmask & PMFS_DBGMASK_MMAPHUGE) ? pmfs_dbg(s, args) : 0)
#define pmfs_dbg_mmap4k(s, args ...)		 \
	((pmfs_dbgmask & PMFS_DBGMASK_MMAP4K) ? pmfs_dbg(s, args) : 0)
#define pmfs_dbg_mmapv(s, args ...)		 \
	((pmfs_dbgmask & PMFS_DBGMASK_MMAPVERBOSE) ? pmfs_dbg(s, args) : 0)
#define pmfs_dbg_mmapvv(s, args ...)		 \
	((pmfs_dbgmask & PMFS_DBGMASK_MMAPVVERBOSE) ? pmfs_dbg(s, args) : 0)

#define pmfs_dbg_verbose(s, args ...)		 \
	((pmfs_dbgmask & PMFS_DBGMASK_VERBOSE) ? pmfs_dbg(s, ##args) : 0)
#define pmfs_dbgv(s, args ...)	pmfs_dbg_verbose(s, ##args)
#define pmfs_dbg_trans(s, args ...)		 \
	((pmfs_dbgmask & PMFS_DBGMASK_TRANSACTION) ? pmfs_dbg(s, ##args) : 0)

#define pmfs_set_bit                   __test_and_set_bit_le
#define pmfs_clear_bit                 __test_and_clear_bit_le
#define pmfs_find_next_zero_bit                find_next_zero_bit_le

#define clear_opt(o, opt)       (o &= ~PMFS_MOUNT_ ## opt)
#define set_opt(o, opt)         (o |= PMFS_MOUNT_ ## opt)
#define test_opt(sb, opt)       (PMFS_SB(sb)->s_mount_opt & PMFS_MOUNT_ ## opt)

#define PMFS_LARGE_INODE_TABLE_SIZE    (0x200000)
/* PMFS size threshold for using 2M blocks for inode table */
#define PMFS_LARGE_INODE_TABLE_THREASHOLD    (0x20000000)
/*
 * pmfs inode flags
 *
 * PMFS_EOFBLOCKS_FL	There are blocks allocated beyond eof
 */
#define PMFS_EOFBLOCKS_FL      0x20000000
/* Flags that should be inherited by new inodes from their parent. */
#define PMFS_FL_INHERITED (FS_SECRM_FL | FS_UNRM_FL | FS_COMPR_FL | \
			    FS_SYNC_FL | FS_NODUMP_FL | FS_NOATIME_FL |	\
			    FS_COMPRBLK_FL | FS_NOCOMP_FL | FS_JOURNAL_DATA_FL | \
			    FS_NOTAIL_FL | FS_DIRSYNC_FL)
/* Flags that are appropriate for regular files (all but dir-specific ones). */
#define PMFS_REG_FLMASK (~(FS_DIRSYNC_FL | FS_TOPDIR_FL))
/* Flags that are appropriate for non-directories/regular files. */
#define PMFS_OTHER_FLMASK (FS_NODUMP_FL | FS_NOATIME_FL)
#define PMFS_FL_USER_VISIBLE (FS_FL_USER_VISIBLE | PMFS_EOFBLOCKS_FL)

/* IOCTLs */
#define	FS_PMFS_FSYNC			0xBCD0000E
#define	PMFS_PRINT_TIMING		0xBCD00010
#define	PMFS_CLEAR_STATS		0xBCD00011
#define	PMFS_COW_WRITE			0xBCD00012
#define	PMFS_PRINT_LOG			0xBCD00013
#define	PMFS_PRINT_LOG_BLOCKNODE	0xBCD00014
#define	PMFS_PRINT_LOG_PAGES		0xBCD00015
#define	PMFS_MALLOC_TEST		0xBCD00016
#define	PMFS_TEST_MULTITHREAD_RECOVERY	0xBCD00017
#define	PMFS_PRINT_FREE_LISTS		0xBCD00018


#define	READDIR_END			0x1
#define	INVALID_CPU			(-1)
#define	SHARED_CPU			(65536)

extern unsigned int blk_type_to_shift[PMFS_BLOCK_TYPE_MAX];
extern unsigned int blk_type_to_size[PMFS_BLOCK_TYPE_MAX];

/* ======================= Log entry ========================= */
/* Inode entry in the log */

#define	INVALID_MASK	4095
#define	BLOCK_OFF(p)	((p) & ~INVALID_MASK)

#define	ENTRY_LOC(p)	((p) & INVALID_MASK)

enum pmfs_entry_type {
	FILE_WRITE = 1,
	DIR_LOG,
	SET_ATTR,
	LINK_CHANGE,
};

static inline u8 pmfs_get_entry_type(void *p)
{
	return *(u8 *)p;
}

static inline void pmfs_set_entry_type(void *p, enum pmfs_entry_type type)
{
	*(u8 *)p |= type;
}

/* Make sure this is 32 bytes */
struct pmfs_file_write_entry {
	/* ret of find_nvmm_block, the lowest byte is entry type */
	__le64	block;
	__le32	pgoff;
	__le32	num_pages;
	__le32	invalid_pages;
	/* For both ctime and mtime */
	__le32	mtime;
	__le64	size;
} __attribute((__packed__));

struct	pmfs_inode_page_tail {
	__le64	padding1;
	__le64	padding2;
	__le64	padding3;
	__le64	next_page;
} __attribute((__packed__));

#define	ENTRIES_PER_PAGE	127

/* Fit in PAGE_SIZE */
struct	pmfs_inode_log_page {
	struct pmfs_file_write_entry entries[ENTRIES_PER_PAGE];
	struct pmfs_inode_page_tail page_tail;
} __attribute((__packed__));

#define	LAST_ENTRY	4064
#define	PAGE_TAIL(p)	(((p) & ~INVALID_MASK) + LAST_ENTRY)

/*
 * Structure of a directory log entry in PMFS.
 * Update DIR_LOG_REC_LEN if modify this struct!
 */
struct pmfs_dir_logentry {
	u8	entry_type;
	u8	name_len;               /* length of the directory entry name */
	u8	file_type;              /* file type */
	u8	new_inode;		/* Followed by a new inode? */
	__le16	de_len;                 /* length of this directory entry */
	__le16	links_count;
	__le32	mtime;			/* For both mtime and ctime */
	__le64	ino;                    /* inode no pointed to by this entry */
	__le64	size;
	char	name[PMFS_NAME_LEN];   /* File name */
} __attribute((__packed__));

#define PMFS_DIR_PAD            4
#define PMFS_DIR_ROUND          (PMFS_DIR_PAD - 1)
#define PMFS_DIR_LOG_REC_LEN(name_len)  (((name_len) + 28 + PMFS_DIR_ROUND) & \
				      ~PMFS_DIR_ROUND)

/*
 * Struct of inode attributes change log (setattr)
 * Make sure it is 32 bytes.
 */
struct pmfs_setattr_logentry {
	u8	entry_type;
	u8	attr;
	__le16	mode;
	__le32	uid;
	__le32	gid;
	__le32	atime;
	__le32	mtime;
	__le32	ctime;
	__le64	size;
} __attribute((__packed__));

/* Do we need this to be 32 bytes? */
struct pmfs_link_change_entry {
	u8	entry_type;
	u8	padding;
	__le16	links;
	__le32	ctime;
	__le32	flags;
	__le32	generation;
	__le64	paddings[2];
} __attribute((__packed__));

struct pmfs_dir_node {
	struct rb_node node;
	unsigned long nvmm;
	u64 ino;
	unsigned int hash;
};

enum alloc_type {
	KMALLOC = 1,
	VMALLOC,
	GETPAGE,
	ALLOCPAGE,
};

/* MALLOC TEST */
#define	TEST_ZERO	1
#define	TEST_NORMAL	2
#define	TEST_VMALLOC	3
#define	TEST_KMALLOC	4
#define	TEST_KZALLOC	5
#define	TEST_PAGEALLOC	6
#define	TEST_PAGEZALLOC	7
#define	TEST_NVMM	8

struct mem_addr {
	unsigned long nvmm_entry;	// NVMM inode entry
	unsigned long nvmm;		// NVMM blocknr
	unsigned long dram;		// DRAM virtual address.
					// Lowest 12 bits contain flag bits.
	unsigned long nvmm_mmap;	// NVMM mmap blocknr
	int nvmm_mmap_write;		// NVMM mmap for write?
	struct page *page;
};



#define _mm_clflush(addr)\
	asm volatile("clflush %0" : "+m" (*(volatile char *)(addr)))
#define _mm_clflushopt(addr)\
	asm volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)(addr)))
#define _mm_clwb(addr)\
	asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *)(addr)))
#define _mm_pcommit()\
	asm volatile(".byte 0x66, 0x0f, 0xae, 0xf8")

/* Provides ordering from all previous clflush too */
static inline void PERSISTENT_MARK(void)
{
	/* TODO: Fix me. */
}

static inline void PERSISTENT_BARRIER(void)
{
	asm volatile ("sfence\n" : : );
	if (support_clwb)
		_mm_pcommit();
}

static inline void pmfs_flush_buffer(void *buf, uint32_t len, bool fence)
{
	uint32_t i;
	len = len + ((unsigned long)(buf) & (CACHELINE_SIZE - 1));
	if (support_clwb) {
		for (i = 0; i < len; i += CACHELINE_SIZE)
			_mm_clwb(buf + i);
	} else {
		for (i = 0; i < len; i += CACHELINE_SIZE)
			_mm_clflush(buf + i);
	}
	/* Do a fence only if asked. We often don't need to do a fence
	 * immediately after clflush because even if we get context switched
	 * between clflush and subsequent fence, the context switch operation
	 * provides implicit fence. */
	if (fence)
		PERSISTENT_BARRIER();
}

static inline void pmfs_update_tail(struct pmfs_inode *pi, u64 new_tail)
{
	PERSISTENT_BARRIER();
	pi->log_tail = new_tail;
	pmfs_flush_buffer(&pi->log_tail, CACHELINE_SIZE, 1);
}

/* symlink.c */
int pmfs_block_symlink(struct super_block *sb, struct pmfs_inode *pi,
	struct inode *inode, unsigned long blocknr, const char *symname,
	int len);

/* Inline functions start here */

/* Mask out flags that are inappropriate for the given type of inode. */
static inline __le32 pmfs_mask_flags(umode_t mode, __le32 flags)
{
	flags &= cpu_to_le32(PMFS_FL_INHERITED);
	if (S_ISDIR(mode))
		return flags;
	else if (S_ISREG(mode))
		return flags & cpu_to_le32(PMFS_REG_FLMASK);
	else
		return flags & cpu_to_le32(PMFS_OTHER_FLMASK);
}

static inline int pmfs_calc_checksum(u8 *data, int n)
{
	u16 crc = 0;

	crc = crc16(~0, (__u8 *)data + sizeof(__le16), n - sizeof(__le16));
	if (*((__le16 *)data) == cpu_to_le16(crc))
		return 0;
	else
		return 1;
}

struct pmfs_range_node_lowhigh {
	__le64 range_low;
	__le64 range_high;
};

struct pmfs_alive_inode_entry {
	__le64 pi_addr;
};

#define	RANGENODE_PER_PAGE	254

struct pmfs_range_node {
	struct rb_node node;
	unsigned long range_low;
	unsigned long range_high;
};

extern struct kmem_cache *pmfs_header_cachep;

struct pmfs_inode_info_header {
	u64	root;			/* File Btree root */
	u8	height;			/* File Btree height */
	u16	i_mode;			/* Dir or file? */
	u32	log_pages;		/* Num of log pages */
	u64	i_size;
	u64	ino;
	u64	pi_addr;
	struct rb_root	dir_tree;	/* Dir name entry tree root */
};

struct pmfs_inode_info {
	struct pmfs_inode_info_header *header;
	__u32   i_dir_start_lookup;
	struct inode	vfs_inode;
	u64	low_dirty;		/* Dirty low range */
	u64	high_dirty;		/* Dirty high range */
	u64	low_mmap;		/* Mmap low range */
	u64	high_mmap;		/* Mmap high range */
};

enum bm_type {
	BM_4K = 0,
	BM_2M,
	BM_1G,
};

struct multi_set_entry {
	unsigned long bit_low;
	unsigned long bit_high;
	int refcount;
	struct rb_node node;
};

struct single_scan_bm {
	unsigned long bitmap_size;
	unsigned long *bitmap;
	unsigned long multi_set_low;
	unsigned long multi_set_high;
	int num_entries;
	struct rb_root multi_set_tree;	/* Multiple set bit RB tree */
};

struct scan_bitmap {
	struct single_scan_bm scan_bm_4K;
	struct single_scan_bm scan_bm_2M;
	struct single_scan_bm scan_bm_1G;
};

struct free_list {
	spinlock_t s_lock;
	struct rb_root	block_free_tree;
	struct pmfs_range_node *first_node;
	unsigned long	block_start;
	unsigned long	block_end;
	unsigned long	num_free_blocks;
	unsigned long	num_blocknode;

	/* Statistics */
	unsigned long	alloc_count;
	unsigned long	free_count;
	unsigned long	allocated_blocks;
	unsigned long	freed_blocks;
	u64		padding[8];	/* Cache line break */
};

#define	RESERVED_BLOCKS	1

/*
 * PMFS super-block data in memory
 */
struct pmfs_sb_info {
	/*
	 * base physical and virtual address of PMFS (which is also
	 * the pointer to the super block)
	 */
	phys_addr_t	phys_addr;
	void		*virt_addr;

	unsigned long	block_start;
	unsigned long	block_end;

	/*
	 * Backing store option:
	 * 1 = no load, 2 = no store,
	 * else do both
	 */
	unsigned int	pmfs_backing_option;

	/* Mount options */
	unsigned long	bpi;
	unsigned long	num_inodes;
	unsigned long	blocksize;
	unsigned long	initsize;
	unsigned long	s_mount_opt;
	kuid_t		uid;    /* Mount uid for root directory */
	kgid_t		gid;    /* Mount gid for root directory */
	umode_t		mode;   /* Mount mode for root directory */
	atomic_t	next_generation;
	/* inode tracking */
	struct mutex inode_table_mutex;
	unsigned long	num_range_node_inode;
	unsigned long	s_inodes_used_count;
	unsigned long	reserved_blocks;

	struct mutex 	s_lock;	/* protects the SB's buffer-head */

	/* Journaling related structures */
	struct mutex lite_journal_mutex;

	/* Header tree */
	unsigned long root;
	unsigned int height;
	u8 btype;

	/* Track inuse inodes */
	struct rb_root	inode_inuse_tree;
	struct pmfs_range_node *first_inode_range;

	/* ZEROED page for cache page initialized */
	unsigned long zeroed_page;

	int cpus;
	/* Per-CPU free block list */
	struct free_list *free_lists;

	/* Shared free block list */
	unsigned long per_list_blocks;
	struct free_list shared_free_list;
};

static inline struct pmfs_sb_info *PMFS_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

static inline struct pmfs_inode_info *PMFS_I(struct inode *inode)
{
	return container_of(inode, struct pmfs_inode_info, vfs_inode);
}

/* If this is part of a read-modify-write of the super block,
 * pmfs_memunlock_super() before calling! */
static inline struct pmfs_super_block *pmfs_get_super(struct super_block *sb)
{
	struct pmfs_sb_info *sbi = PMFS_SB(sb);

	return (struct pmfs_super_block *)sbi->virt_addr;
}

static inline struct pmfs_inode *pmfs_get_inode_table(struct super_block *sb)
{
	struct pmfs_super_block *ps = pmfs_get_super(sb);

	return (struct pmfs_inode *)((char *)ps +
			le64_to_cpu(ps->s_inode_table_offset));
}

static inline struct pmfs_super_block *pmfs_get_redund_super(struct super_block *sb)
{
	struct pmfs_sb_info *sbi = PMFS_SB(sb);

	return (struct pmfs_super_block *)(sbi->virt_addr + PMFS_SB_SIZE);
}

/* If this is part of a read-modify-write of the block,
 * pmfs_memunlock_block() before calling! */
static inline void *pmfs_get_block(struct super_block *sb, u64 block)
{
	struct pmfs_super_block *ps = pmfs_get_super(sb);

	return block ? ((void *)ps + block) : NULL;
}

static inline
struct free_list *pmfs_get_free_list(struct super_block *sb, int cpu)
{
	struct pmfs_sb_info *sbi = PMFS_SB(sb);

	if (cpu < sbi->cpus)
		return &sbi->free_lists[cpu];
	else
		return &sbi->shared_free_list;
}

// BKDR String Hash Function
static inline unsigned int BKDRHash(const char *str)
{
	unsigned int seed = 131; // 31 131 1313 13131 131313 etc..
	unsigned int hash = 0;

	while (*str) {
		hash = hash * seed + (*str++);
	}

	return (hash & 0x7FFFFFFF);
}

/* uses CPU instructions to atomically write up to 8 bytes */
static inline void pmfs_memcpy_atomic (void *dst, const void *src, u8 size)
{
	switch (size) {
		case 1: {
			volatile u8 *daddr = dst;
			const u8 *saddr = src;
			*daddr = *saddr;
			break;
		}
		case 2: {
			volatile __le16 *daddr = dst;
			const u16 *saddr = src;
			*daddr = cpu_to_le16(*saddr);
			break;
		}
		case 4: {
			volatile __le32 *daddr = dst;
			const u32 *saddr = src;
			*daddr = cpu_to_le32(*saddr);
			break;
		}
		case 8: {
			volatile __le64 *daddr = dst;
			const u64 *saddr = src;
			*daddr = cpu_to_le64(*saddr);
			break;
		}
		default:
			pmfs_dbg("error: memcpy_atomic called with %d bytes\n", size);
			//BUG();
	}
}

/* assumes the length to be 4-byte aligned */
static inline void memset_nt(void *dest, uint32_t dword, size_t length)
{
	uint64_t dummy1, dummy2;
	uint64_t qword = ((uint64_t)dword << 32) | dword;

	asm volatile ("movl %%edx,%%ecx\n"
		"andl $63,%%edx\n"
		"shrl $6,%%ecx\n"
		"jz 9f\n"
		"1:      movnti %%rax,(%%rdi)\n"
		"2:      movnti %%rax,1*8(%%rdi)\n"
		"3:      movnti %%rax,2*8(%%rdi)\n"
		"4:      movnti %%rax,3*8(%%rdi)\n"
		"5:      movnti %%rax,4*8(%%rdi)\n"
		"8:      movnti %%rax,5*8(%%rdi)\n"
		"7:      movnti %%rax,6*8(%%rdi)\n"
		"8:      movnti %%rax,7*8(%%rdi)\n"
		"leaq 64(%%rdi),%%rdi\n"
		"decl %%ecx\n"
		"jnz 1b\n"
		"9:     movl %%edx,%%ecx\n"
		"andl $7,%%edx\n"
		"shrl $3,%%ecx\n"
		"jz 11f\n"
		"10:     movnti %%rax,(%%rdi)\n"
		"leaq 8(%%rdi),%%rdi\n"
		"decl %%ecx\n"
		"jnz 10b\n"
		"11:     movl %%edx,%%ecx\n"
		"shrl $2,%%ecx\n"
		"jz 12f\n"
		"movnti %%eax,(%%rdi)\n"
		"12:\n"
		: "=D"(dummy1), "=d" (dummy2) : "D" (dest), "a" (qword), "d" (length) : "memory", "rcx");
}

#define	DRAM_BIT	0x1UL	// DRAM
#define	KMALLOC_BIT	0x2UL	// Alloc with kmalloc
#define	VMALLOC_BIT	0x4UL	// Alloc with vmalloc
#define	GETPAGE_BIT	0x8UL	// Alloc with get_free_page
#define	DIRTY_BIT	0x10UL	// Dirty
#define	MMAP_WRITE_BIT	0x20UL	// mmaped for write
#define	OUTDATE_BIT	0x40UL	// Outdate with NVMM page
#define	UNINIT_BIT	0x80UL	// Unitialized page

#define	IS_DRAM_ADDR(p)	((p) & (DRAM_BIT))
#define	IS_DIRTY(p)	((p) & (DIRTY_BIT))
#define	IS_MAPPED(p)	((p) & (MMAP_WRITE_BIT))
#define	OUTDATE(p)	((p) & (OUTDATE_BIT))
#define	UNINIT(p)	((p) & (UNINIT_BIT))
#define	DRAM_ADDR(p)	((p) & (PAGE_MASK))

extern struct kmem_cache *pmfs_mempair_cachep;

static inline struct pmfs_inode_info_header *
pmfs_find_info_header(struct super_block *sb, unsigned long ino)
{
	struct pmfs_sb_info *sbi = PMFS_SB(sb);
	__le64 *level_ptr;
	u64 bp = 0;
	u32 height, bit_shift;
	unsigned int idx;

	height = sbi->height;
	bp = sbi->root;
	if (bp == 0)
		return NULL;

	while (height > 0) {
		level_ptr = (__le64 *)DRAM_ADDR(bp);
		bit_shift = (height - 1) * META_BLK_SHIFT;
		idx = ino >> bit_shift;
		bp = le64_to_cpu(level_ptr[idx]);
		if (bp == 0)
			return NULL;
		ino = ino & ((1 << bit_shift) - 1);
		height--;
	}

	return (struct pmfs_inode_info_header *)bp;
}

static inline struct mem_addr *__pmfs_get_mem_pair(struct super_block *sb,
		struct pmfs_inode_info *si, unsigned long blocknr)
{
	struct pmfs_inode_info_header *sih = si->header;
	__le64 *level_ptr;
	u64 bp = 0;
	u32 height, bit_shift;
	unsigned int idx;

	height = sih->height;
	bp = sih->root;
	if (bp == 0)
		return NULL;

	pmfs_dbg_verbose("%s: height %u, root 0x%llx\n", __func__, height, bp);
	while (height > 0) {
		level_ptr = (__le64 *)DRAM_ADDR(bp);
		bit_shift = (height - 1) * META_BLK_SHIFT;
		idx = blocknr >> bit_shift;
		bp = le64_to_cpu(level_ptr[idx]);
		if (bp == 0)
			return NULL;
		blocknr = blocknr & ((1 << bit_shift) - 1);
		height--;
	}

	return (struct mem_addr *)bp;
}

static inline unsigned long pmfs_get_dram_addr(struct mem_addr *pair)
{
	unsigned long addr;

	if (pair->page)
		addr = (unsigned long)kmap_atomic(pair->page);
	else
		addr = pair->dram;

	return addr;
}

static inline u64 __pmfs_find_nvmm_block(struct super_block *sb,
	struct pmfs_inode_info *si, struct mem_addr *mem_pair,
	unsigned long blocknr)
{
	struct mem_addr *pair = NULL;

	if (mem_pair)
		return mem_pair->nvmm << PAGE_SHIFT;

	pair = __pmfs_get_mem_pair(sb, si, blocknr);
	if (!pair)
		return 0;

	return pair->nvmm << PAGE_SHIFT;
}

static inline unsigned int pmfs_inode_blk_shift (struct pmfs_inode *pi)
{
	return blk_type_to_shift[pi->i_blk_type];
}

static inline uint32_t pmfs_inode_blk_size (struct pmfs_inode *pi)
{
	return blk_type_to_size[pi->i_blk_type];
}

/*
 * ROOT_INO: Start from PMFS_SB_SIZE * 2
 * BLOCKNODE_INO: PMFS_SB_SIZE * 2 + PMFS_INODE_SIZE
 */
static inline struct pmfs_inode *pmfs_get_basic_inode(struct super_block *sb,
	u64 inode_number)
{
	struct pmfs_sb_info *sbi = PMFS_SB(sb);

	return (struct pmfs_inode *)(sbi->virt_addr + PMFS_SB_SIZE * 2 +
				 (inode_number - 1) * PMFS_INODE_SIZE);
}

/* If this is part of a read-modify-write of the inode metadata,
 * pmfs_memunlock_inode() before calling! */
static inline struct pmfs_inode *pmfs_get_inode_by_ino(struct super_block *sb,
						  u64 ino)
{
	if (ino == 0 || ino >= PMFS_NORMAL_INODE_START)
		return NULL;

	return pmfs_get_basic_inode(sb, ino);
}

static inline struct pmfs_inode *pmfs_get_inode(struct super_block *sb,
	struct inode *inode)
{
	struct pmfs_inode_info *si = PMFS_I(inode);
	struct pmfs_inode_info_header *sih = si->header;

	return (struct pmfs_inode *)pmfs_get_block(sb, sih->pi_addr);
}

static inline u64
pmfs_get_addr_off(struct pmfs_sb_info *sbi, void *addr)
{
	PMFS_ASSERT((addr >= sbi->virt_addr) &&
			(addr < (sbi->virt_addr + sbi->initsize)));
	return (u64)(addr - sbi->virt_addr);
}

static inline u64
pmfs_get_block_off(struct super_block *sb, unsigned long blocknr,
		    unsigned short btype)
{
	return (u64)blocknr << PAGE_SHIFT;
}

static inline unsigned long
pmfs_get_numblocks(unsigned short btype)
{
	unsigned long num_blocks;

	if (btype == PMFS_BLOCK_TYPE_4K) {
		num_blocks = 1;
	} else if (btype == PMFS_BLOCK_TYPE_2M) {
		num_blocks = 512;
	} else {
		//btype == PMFS_BLOCK_TYPE_1G 
		num_blocks = 0x40000;
	}
	return num_blocks;
}

static inline unsigned long
pmfs_get_blocknr(struct super_block *sb, u64 block, unsigned short btype)
{
	return block >> PAGE_SHIFT;
}

static inline unsigned long pmfs_get_pfn(struct super_block *sb, u64 block)
{
	return (PMFS_SB(sb)->phys_addr + block) >> PAGE_SHIFT;
}

static inline int pmfs_is_mounting(struct super_block *sb)
{
	struct pmfs_sb_info *sbi = (struct pmfs_sb_info *)sb->s_fs_info;
	return sbi->s_mount_opt & PMFS_MOUNT_MOUNTING;
}

static inline int pmfs_has_page_cache(struct super_block *sb)
{
	struct pmfs_sb_info *sbi = PMFS_SB(sb);

	return sbi->s_mount_opt & PMFS_MOUNT_PAGECACHE;
}

static inline void check_eof_blocks(struct super_block *sb, 
		struct pmfs_inode *pi, loff_t size)
{
	if ((pi->i_flags & cpu_to_le32(PMFS_EOFBLOCKS_FL)) &&
		(size + sb->s_blocksize) > (le64_to_cpu(pi->i_blocks)
			<< sb->s_blocksize_bits))
		pi->i_flags &= cpu_to_le32(~PMFS_EOFBLOCKS_FL);
}

enum pmfs_new_inode_type {
	TYPE_CREATE = 0,
	TYPE_MKNOD,
	TYPE_SYMLINK,
	TYPE_MKDIR
};

static inline u64 next_log_page(struct super_block *sb, u64 curr_p)
{
	void *curr_addr = pmfs_get_block(sb, curr_p);
	unsigned long page_tail = ((unsigned long)curr_addr & ~INVALID_MASK)
					+ LAST_ENTRY;
	return ((struct pmfs_inode_page_tail *)page_tail)->next_page;
}

#define	CACHE_ALIGN(p)	((p) & ~(CACHELINE_SIZE - 1))

/* Align inode to CACHELINE_SIZE */
static inline bool is_last_entry(u64 curr_p, size_t size, int new_inode)
{
	unsigned int entry_end, inode_start;

	entry_end = ENTRY_LOC(curr_p) + size;

	if (new_inode == 0 || entry_end > LAST_ENTRY)
		return entry_end > LAST_ENTRY;

	inode_start = (entry_end & (CACHELINE_SIZE - 1)) == 0 ?
			entry_end : CACHE_ALIGN(entry_end) + CACHELINE_SIZE;

	return inode_start + PMFS_INODE_SIZE > LAST_ENTRY;
}

static inline bool is_last_dir_entry(struct super_block *sb, u64 curr_p)
{
	struct pmfs_dir_logentry *entry;

	if (ENTRY_LOC(curr_p) + PMFS_DIR_LOG_REC_LEN(0) > LAST_ENTRY)
		return true;

	entry = (struct pmfs_dir_logentry *)pmfs_get_block(sb, curr_p);
	if (entry->name_len == 0)
		return true;
	return false;
}

static inline int is_dir_init_entry(struct super_block *sb,
	struct pmfs_dir_logentry *entry)
{
	if (entry->name_len == 1 && strncmp(entry->name, ".", 1) == 0)
		return 1;
	if (entry->name_len == 2 && strncmp(entry->name, "..", 2) == 0)
		return 1;

	return 0;
}

#include "wprotect.h"

/* Function Prototypes */
extern void pmfs_error_mng(struct super_block *sb, const char *fmt, ...);

/* balloc.c */
int pmfs_alloc_block_free_lists(struct super_block *sb);
void pmfs_delete_free_lists(struct super_block *sb);
inline struct pmfs_range_node *pmfs_alloc_blocknode(struct super_block *sb);
inline struct pmfs_range_node *pmfs_alloc_inode_node(struct super_block *sb);
inline void pmfs_free_range_node(struct pmfs_range_node *node);
inline void pmfs_free_blocknode(struct super_block *sb,
	struct pmfs_range_node *bnode);
inline void pmfs_free_inode_node(struct super_block *sb,
	struct pmfs_range_node *bnode);
extern void pmfs_init_blockmap(struct super_block *sb, int recovery);
extern void pmfs_free_meta_block(struct super_block *sb, unsigned long blocknr);
extern void pmfs_free_data_blocks(struct super_block *sb,
	unsigned long blocknr, int num, unsigned short btype);
extern void pmfs_free_log_blocks(struct super_block *sb,
	unsigned long blocknr, int num, unsigned short btype);
extern int pmfs_new_data_blocks(struct super_block *sb, struct pmfs_inode *pi,
	unsigned long *blocknr, unsigned int num, unsigned long start_blk,
	unsigned short btype, int zero, int cow);
extern int pmfs_new_log_blocks(struct super_block *sb, unsigned long pmfs_ino,
	unsigned long *blocknr, unsigned int num, unsigned short btype,
	int zero);
extern int pmfs_new_meta_block(struct super_block *sb, unsigned long *blocknr,
	int zero, int nosleep);
extern int pmfs_new_cache_block(struct super_block *sb, struct mem_addr *pair,
	int zero, int nosleep);
extern unsigned long pmfs_count_free_blocks(struct super_block *sb);
inline int pmfs_search_inodetree(struct pmfs_sb_info *sbi,
	unsigned long ino, unsigned long *step,
	struct pmfs_range_node **ret_node);
inline int pmfs_insert_blocktree(struct pmfs_sb_info *sbi,
	struct rb_root *tree, struct pmfs_range_node *new_node);
inline int pmfs_insert_inodetree(struct pmfs_sb_info *sbi,
	struct pmfs_range_node *new_node);
int pmfs_find_free_slot(struct pmfs_sb_info *sbi,
	struct rb_root *tree, unsigned long range_low,
	unsigned long range_high, struct pmfs_range_node **prev,
	struct pmfs_range_node **next);
void pmfs_free_cache_block(struct mem_addr *pair);

/* bbuild.c */
inline void set_bm(unsigned long bit, struct scan_bitmap *bm,
	enum bm_type type);
inline void clear_bm(unsigned long bit, struct scan_bitmap *bm,
	enum bm_type type);
int pmfs_recover_inode(struct super_block *sb, u64 pi_addr,
	struct scan_bitmap *bm, int cpuid, int multithread);
void pmfs_save_blocknode_mappings_to_log(struct super_block *sb);
void pmfs_save_inode_list_to_log(struct super_block *sb);
unsigned int pmfs_free_header_tree(struct super_block *sb);
struct pmfs_inode_info_header *pmfs_alloc_header(struct super_block *sb,
	u16 i_mode);
int pmfs_assign_info_header(struct super_block *sb, unsigned long ino,
	struct pmfs_inode_info_header **sih, u16 i_mode, int need_lock);
int pmfs_inode_log_recovery(struct super_block *sb, int multithread);

/*
 * Inodes and files operations
 */

/* dax.c */
int pmfs_reassign_file_btree(struct super_block *sb,
	struct pmfs_inode *pi, struct pmfs_inode_info_header *sih,
	u64 begin_tail);
ssize_t pmfs_cow_file_write(struct file *filp, const char __user *buf,
          size_t len, loff_t *ppos, bool need_mutex);
ssize_t pmfs_copy_to_nvmm(struct super_block *sb, struct inode *inode,
	struct pmfs_inode *pi, loff_t pos, size_t count, u64 *begin,
	u64 *end);

/* dir.c */
extern const struct file_operations pmfs_dir_operations;
int pmfs_append_dir_init_entries(struct super_block *sb,
	struct pmfs_inode *pi, u64 self_ino, u64 parent_ino);
extern int pmfs_add_entry(struct dentry *dentry, u64 *pi_addr, u64 ino,
	int inc_link, int new_inode, u64 tail, u64 *new_tail);
extern int pmfs_remove_entry(struct dentry *dentry, int dec_link, u64 tail,
	u64 *new_tail);
void pmfs_print_dir_tree(struct super_block *sb,
	struct pmfs_inode_info_header *sih, unsigned long ino);
void pmfs_delete_dir_tree(struct super_block *sb,
	struct pmfs_inode_info_header *sih);
struct pmfs_dir_node *pmfs_find_dir_node_by_name(struct super_block *sb,
	struct pmfs_inode *pi, struct inode *inode, const char *name,
	unsigned long name_len);
int pmfs_rebuild_dir_inode_tree(struct super_block *sb,
	struct pmfs_inode *pi, u64 pi_addr,
	struct pmfs_inode_info_header *sih, struct scan_bitmap *bm);

/* file.c */
extern const struct inode_operations pmfs_file_inode_operations;
extern const struct file_operations pmfs_dax_file_operations;
int pmfs_fsync(struct file *file, loff_t start, loff_t end, int datasync);
int pmfs_is_page_dirty(struct mm_struct *mm, unsigned long address,
	int category, int set_clean);

/* inode.c */
extern const struct address_space_operations pmfs_aops_dax;
extern int pmfs_init_inode_table(struct super_block *sb);
extern u64 pmfs_find_nvmm_block(struct inode *inode, 
		unsigned long file_blocknr);
int pmfs_set_blocksize_hint(struct super_block *sb, struct inode *inode,
	struct pmfs_inode *pi, loff_t new_size);
extern struct inode *pmfs_iget(struct super_block *sb, unsigned long ino);
extern void pmfs_evict_inode(struct inode *inode);
extern int pmfs_write_inode(struct inode *inode, struct writeback_control *wbc);
extern void pmfs_dirty_inode(struct inode *inode, int flags);
extern int pmfs_notify_change(struct dentry *dentry, struct iattr *attr);
int pmfs_getattr(struct vfsmount *mnt, struct dentry *dentry, 
		struct kstat *stat);
extern void pmfs_set_inode_flags(struct inode *inode, struct pmfs_inode *pi,
	unsigned int flags);
extern unsigned long pmfs_find_region(struct inode *inode, loff_t *offset,
		int hole);
void pmfs_apply_setattr_entry(struct pmfs_inode *pi,
	struct pmfs_setattr_logentry *entry);
u64 pmfs_extend_inode_log(struct super_block *sb, struct pmfs_inode *pi,
	struct pmfs_inode_info_header *sih, u64 curr_p, int is_file);
void pmfs_free_inode_log(struct super_block *sb, struct pmfs_inode *pi);
int pmfs_allocate_inode_log_pages(struct super_block *sb,
	struct pmfs_inode *pi, unsigned long num_pages,
	u64 *new_block);
u64 pmfs_get_append_head(struct super_block *sb, struct pmfs_inode *pi,
	struct pmfs_inode_info_header *sih, u64 tail, size_t size,
	int new_inode, int is_file);
u64 pmfs_append_file_write_entry(struct super_block *sb, struct pmfs_inode *pi,
	struct inode *inode, struct pmfs_file_write_entry *data, u64 tail);
int pmfs_rebuild_file_inode_tree(struct super_block *sb,
	struct pmfs_inode *pi, u64 pi_addr,
	struct pmfs_inode_info_header *sih, struct scan_bitmap *bm);
u64 pmfs_new_pmfs_inode(struct super_block *sb,
	struct pmfs_inode_info_header **return_sih);
extern struct inode *pmfs_new_vfs_inode(enum pmfs_new_inode_type,
	struct inode *dir, u64 pi_addr,
	struct pmfs_inode_info_header *sih, u64 ino, umode_t mode,
	size_t size, dev_t rdev, const struct qstr *qstr);
struct mem_addr *pmfs_get_mem_pair(struct super_block *sb,
	struct pmfs_inode *pi, struct pmfs_inode_info *si,
	unsigned long file_blocknr);
extern int pmfs_assign_blocks(struct super_block *sb, struct pmfs_inode *pi,
	struct pmfs_inode_info_header *sih, struct pmfs_file_write_entry *data,
	struct scan_bitmap *bm,	u64 address, bool nvmm, bool free,
	bool alloc_dram);
int pmfs_free_dram_resource(struct super_block *sb,
	struct pmfs_inode_info_header *sih);

/* ioctl.c */
extern long pmfs_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#ifdef CONFIG_COMPAT
extern long pmfs_compat_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg);
#endif

/* namei.c */
extern const struct inode_operations pmfs_dir_inode_operations;
extern const struct inode_operations pmfs_special_inode_operations;
extern struct dentry *pmfs_get_parent(struct dentry *child);
int pmfs_append_link_change_entry(struct super_block *sb,
	struct pmfs_inode *pi, struct inode *inode, u64 tail, u64 *new_tail);
void pmfs_apply_link_change_entry(struct pmfs_inode *pi,
	struct pmfs_link_change_entry *entry);

/* super.c */
extern struct super_block *pmfs_read_super(struct super_block *sb, void *data,
	int silent);
extern int pmfs_statfs(struct dentry *d, struct kstatfs *buf);
extern int pmfs_remount(struct super_block *sb, int *flags, char *data);
struct pmfs_dir_node *pmfs_alloc_dirnode(struct super_block *sb);
void pmfs_free_dirnode(struct super_block *sb, struct pmfs_dir_node *node);
int pmfs_check_integrity(struct super_block *sb,
	struct pmfs_super_block *super);
void *pmfs_ioremap(struct super_block *sb, phys_addr_t phys_addr,
	ssize_t size);

/* symlink.c */
extern const struct inode_operations pmfs_symlink_inode_operations;

/* pmfs_stats.c */
void pmfs_print_timing_stats(struct super_block *sb);
void pmfs_clear_stats(void);
void pmfs_print_inode_log(struct super_block *sb, struct inode *inode);
void pmfs_print_inode_log_pages(struct super_block *sb, struct inode *inode);
void pmfs_print_free_lists(struct super_block *sb);
void pmfs_detect_memory_leak(struct super_block *sb);

#endif /* __PMFS_H */
