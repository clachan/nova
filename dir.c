/*
 * BRIEF DESCRIPTION
 *
 * File operations for directories.
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

#include <linux/fs.h>
#include <linux/pagemap.h>
#include "pmfs.h"

#define DT2IF(dt) (((dt) << 12) & S_IFMT)
#define IF2DT(sif) (((sif) & S_IFMT) >> 12)

/* ========================= RB Tree operations ============================= */

static int pmfs_rbtree_compare_find_by_name(struct super_block *sb,
	struct pmfs_dir_node *curr, unsigned int hash, const char *name,
	int namelen)
{
	struct pmfs_dir_logentry *entry;
	int min_len;

	if (!curr || curr->nvmm == 0)
		BUG();

	/* Use hash code */
	if (hash < curr->hash)
		return -1;
	if (hash > curr->hash)
		return 1;

	/* Double check the name */
	entry = (struct pmfs_dir_logentry *)pmfs_get_block(sb, curr->nvmm);
	if (namelen != entry->name_len)
		pmfs_dbg("%s: name len does not match: %d %d\n",
				__func__, namelen, entry->name_len);

	min_len = namelen < entry->name_len ? namelen : entry->name_len;
	if (strncmp(name, entry->name, min_len) != 0)
		pmfs_dbg("%s: name does not match: %s %s\n",
				__func__, name, entry->name);

	/* FIXME: Ignore hash collision */
	return 0;
}

struct pmfs_dir_node *pmfs_find_dir_node_by_name(struct super_block *sb,
	struct pmfs_inode *pi, struct inode *inode, const char *name,
	unsigned long name_len)
{
	struct pmfs_inode_info *si = PMFS_I(inode);
	struct pmfs_inode_info_header *sih = si->header;
	struct pmfs_dir_node *curr;
	struct rb_node *temp;
	unsigned int hash;
	int compVal;

	hash = BKDRHash(name);
	temp = sih->dir_tree.rb_node;
	while (temp) {
		curr = container_of(temp, struct pmfs_dir_node, node);
		compVal = pmfs_rbtree_compare_find_by_name(sb, curr, hash,
							name, name_len);

		if (compVal == -1) {
			temp = temp->rb_left;
		} else if (compVal == 1) {
			temp = temp->rb_right;
		} else {
			return curr;
		}
	}

	return NULL;
}

static inline struct pmfs_dir_node *pmfs_find_dir_node(struct super_block *sb,
	struct pmfs_inode *pi, struct inode *inode, struct dentry *dentry)
{
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;

	return pmfs_find_dir_node_by_name(sb, pi, inode, name, namelen);
}

static int pmfs_insert_dir_node_by_name(struct super_block *sb,
	struct pmfs_inode *pi, struct pmfs_inode_info_header *sih, u64 ino,
	const char *name, int namelen, u64 dir_entry)
{
	struct pmfs_dir_node *curr, *new;
	struct rb_node **temp, *parent;
	unsigned int hash;
	int compVal;

	hash = BKDRHash(name);
	pmfs_dbg_verbose("%s: insert %s @ 0x%llx\n", __func__, name, dir_entry);

	temp = &(sih->dir_tree.rb_node);
	parent = NULL;

	while (*temp) {
		curr = container_of(*temp, struct pmfs_dir_node, node);
		compVal = pmfs_rbtree_compare_find_by_name(sb, curr, hash,
							name, namelen);
		parent = *temp;

		if (compVal == -1) {
			temp = &((*temp)->rb_left);
		} else if (compVal == 1) {
			temp = &((*temp)->rb_right);
		} else {
			pmfs_dbg("%s: entry %s already exists\n",
				__func__, name);
			return -EINVAL;
		}
	}

	new = pmfs_alloc_dirnode(sb);
	if (!new)
		return -ENOMEM;

	new->nvmm = dir_entry;
	new->ino = ino;
	new->hash = hash;
	rb_link_node(&new->node, parent, temp);
	rb_insert_color(&new->node, &sih->dir_tree);
//	pmfs_print_dir_tree(sb, inode);

	return 0;
}

static inline int pmfs_insert_dir_node(struct super_block *sb,
	struct pmfs_inode *pi, struct inode *inode, u64 ino,
	struct dentry *dentry, u64 dir_entry)
{
	struct pmfs_inode_info *si = PMFS_I(inode);
	struct pmfs_inode_info_header *sih = si->header;
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;

	return pmfs_insert_dir_node_by_name(sb, pi, sih, ino, name,
						namelen, dir_entry);
}

void pmfs_remove_dir_node_by_name(struct super_block *sb, struct pmfs_inode *pi,
	struct pmfs_inode_info_header *sih, const char *name, int namelen)
{
	struct pmfs_dir_node *curr;
	struct rb_node *temp;
	unsigned int hash;
	int compVal;

	hash = BKDRHash(name);
	temp = sih->dir_tree.rb_node;
	while (temp) {
		curr = container_of(temp, struct pmfs_dir_node, node);
		compVal = pmfs_rbtree_compare_find_by_name(sb, curr, hash,
							name, namelen);

		if (compVal == -1) {
			temp = temp->rb_left;
		} else if (compVal == 1) {
			temp = temp->rb_right;
		} else {
			rb_erase(&curr->node, &sih->dir_tree);
			pmfs_free_dirnode(sb, curr);
			break;
		}
	}

	return;
}

static inline void pmfs_remove_dir_node(struct super_block *sb,
	struct pmfs_inode *pi, struct inode *inode, struct dentry *dentry)
{
	struct pmfs_inode_info *si = PMFS_I(inode);
	struct pmfs_inode_info_header *sih = si->header;
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;

	return pmfs_remove_dir_node_by_name(sb, pi, sih, name, namelen);
}

void pmfs_print_dir_tree(struct super_block *sb,
	struct pmfs_inode_info_header *sih, unsigned long ino)
{
	struct pmfs_dir_node *curr;
	struct pmfs_dir_logentry *entry;
	struct rb_node *temp;

	pmfs_dbg("%s: dir ino %lu\n", __func__, ino);
	temp = rb_first(&sih->dir_tree);
	while (temp) {
		curr = container_of(temp, struct pmfs_dir_node, node);

		if (!curr || curr->nvmm == 0)
			BUG();

		entry = (struct pmfs_dir_logentry *)
				pmfs_get_block(sb, curr->nvmm);
		pmfs_dbg("%.*s\n", entry->name_len, entry->name);
		temp = rb_next(temp);
	}

	return;
}

void pmfs_delete_dir_tree(struct super_block *sb,
	struct pmfs_inode_info_header *sih)
{
	struct pmfs_dir_node *curr;
	struct rb_node *temp;
	timing_t delete_time;

	PMFS_START_TIMING(delete_dir_tree_t, delete_time);
	temp = rb_first(&sih->dir_tree);
	while (temp) {
		curr = container_of(temp, struct pmfs_dir_node, node);
		temp = rb_next(temp);
		rb_erase(&curr->node, &sih->dir_tree);
		pmfs_free_dirnode(sb, curr);
	}
	PMFS_END_TIMING(delete_dir_tree_t, delete_time);
	return;
}

/* ========================= Entry operations ============================= */

/*
 * Append a pmfs_dir_logentry to the current pmfs_inode_log_page.
 * Note unlike append_file_write_entry(), this method returns the tail pointer
 * after append.
 */
static u64 pmfs_append_dir_inode_entry(struct super_block *sb,
	struct pmfs_inode *pidir, struct inode *dir, u64 *pi_addr,
	u64 ino, struct dentry *dentry, unsigned short de_len, u64 tail,
	int link_change, int new_inode,	u64 *curr_tail)
{
	struct pmfs_inode_info *si = PMFS_I(dir);
	struct pmfs_inode_info_header *sih = si->header;
	struct pmfs_dir_logentry *entry;
	u64 curr_p, inode_start;
	size_t size = de_len;
	unsigned short links_count;
	timing_t append_time;

	PMFS_START_TIMING(append_entry_t, append_time);

	curr_p = pmfs_get_append_head(sb, pidir, sih, tail,
						size, new_inode, 0);
	if (curr_p == 0)
		BUG();

	entry = (struct pmfs_dir_logentry *)pmfs_get_block(sb, curr_p);
	entry->entry_type = DIR_LOG;
	entry->ino = cpu_to_le64(ino);
	entry->name_len = dentry->d_name.len;
	__copy_from_user_inatomic_nocache(entry->name, dentry->d_name.name,
					dentry->d_name.len);
	entry->file_type = 0;
	entry->mtime = cpu_to_le32(dir->i_mtime.tv_sec);
	entry->size = cpu_to_le64(dir->i_size);
	entry->new_inode = new_inode;

	links_count = cpu_to_le16(dir->i_nlink);
	if (links_count == 0 && link_change == -1)
		links_count = 0;
	else
		links_count += link_change;
	entry->links_count = cpu_to_le16(links_count);

	/* Update actual de_len */
	entry->de_len = cpu_to_le16(de_len);
	pmfs_dbg_verbose("dir entry @ 0x%llx: ino %llu, entry len %u, "
			"name len %u, file type %u\n",
			curr_p, entry->ino, entry->de_len,
			entry->name_len, entry->file_type);

	pmfs_flush_buffer(entry, de_len, 0);

	*curr_tail = curr_p + de_len;

	if (new_inode) {
		/* Allocate space for the new inode */
		if (is_last_entry(curr_p, de_len, new_inode))
			inode_start = next_log_page(sb, curr_p);
		else
			inode_start = (*curr_tail & (CACHELINE_SIZE - 1)) == 0
				? *curr_tail : CACHE_ALIGN(*curr_tail) +
						CACHELINE_SIZE;

		if (pi_addr)
			*pi_addr = inode_start;

		*curr_tail = inode_start + PMFS_INODE_SIZE;
	}

	dir->i_blocks = pidir->i_blocks;
	PMFS_END_TIMING(append_entry_t, append_time);
	return curr_p;
}

/* Append . and .. entries */
int pmfs_append_dir_init_entries(struct super_block *sb,
	struct pmfs_inode *pi, u64 self_ino, u64 parent_ino)
{
	int allocated;
	u64 new_block;
	u64 curr_p;
	struct pmfs_dir_logentry *de_entry;

	if (pi->log_head) {
		pmfs_dbg("%s: log head exists @ 0x%llx!\n",
				__func__, pi->log_head);
		return - EINVAL;
	}

	allocated = pmfs_allocate_inode_log_pages(sb, pi, 1, &new_block);
	if (allocated != 1) {
		pmfs_err(sb, "ERROR: no inode log page available\n");
		return - ENOMEM;
	}
	pi->log_tail = pi->log_head = new_block;
	pi->i_blocks = 1;
	pmfs_flush_buffer(&pi->log_head, CACHELINE_SIZE, 1);

	de_entry = (struct pmfs_dir_logentry *)pmfs_get_block(sb, new_block);
	de_entry->entry_type = DIR_LOG;
	de_entry->ino = cpu_to_le64(self_ino);
	de_entry->name_len = 1;
	de_entry->de_len = cpu_to_le16(PMFS_DIR_LOG_REC_LEN(1));
	de_entry->mtime = CURRENT_TIME_SEC.tv_sec;
	de_entry->size = sb->s_blocksize;
	de_entry->links_count = 1;
	strncpy(de_entry->name, ".", 1);
	pmfs_flush_buffer(de_entry, PMFS_DIR_LOG_REC_LEN(1), false);

	curr_p = new_block + PMFS_DIR_LOG_REC_LEN(1);

	de_entry = (struct pmfs_dir_logentry *)((char *)de_entry +
					le16_to_cpu(de_entry->de_len));
	de_entry->entry_type = DIR_LOG;
	de_entry->ino = cpu_to_le64(parent_ino);
	de_entry->name_len = 2;
	de_entry->de_len = cpu_to_le16(PMFS_DIR_LOG_REC_LEN(2));
	de_entry->mtime = CURRENT_TIME_SEC.tv_sec;
	de_entry->size = sb->s_blocksize;
	de_entry->links_count = 2;
	strncpy(de_entry->name, "..", 2);
	pmfs_flush_buffer(de_entry, PMFS_DIR_LOG_REC_LEN(2), true);

	curr_p += PMFS_DIR_LOG_REC_LEN(2);
	pmfs_update_tail(pi, curr_p);

	return 0;
}

/* adds a directory entry pointing to the inode. assumes the inode has
 * already been logged for consistency
 */
int pmfs_add_entry(struct dentry *dentry, u64 *pi_addr, u64 ino, int inc_link,
	int new_inode, u64 tail, u64 *new_tail)
{
	struct inode *dir = dentry->d_parent->d_inode;
	struct super_block *sb = dir->i_sb;
	struct pmfs_inode *pidir;
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;
	unsigned short loglen;
	u64 curr_entry, curr_tail;
	timing_t add_entry_time;

	pmfs_dbg_verbose("%s: dir %lu new inode %llu\n",
				__func__, dir->i_ino, ino);
	pmfs_dbg_verbose("%s: %s %d\n", __func__, name, namelen);
	PMFS_START_TIMING(add_entry_t, add_entry_time);
	if (namelen == 0)
		return -EINVAL;

	pidir = pmfs_get_inode(sb, dir);

	/*
	 * XXX shouldn't update any times until successful
	 * completion of syscall, but too many callers depend
	 * on this.
	 */
	dir->i_mtime = dir->i_ctime = CURRENT_TIME_SEC;

	loglen = PMFS_DIR_LOG_REC_LEN(namelen);
	curr_entry = pmfs_append_dir_inode_entry(sb, pidir, dir, pi_addr, ino,
				dentry,	loglen, tail, inc_link, new_inode,
				&curr_tail);
	pmfs_insert_dir_node(sb, pidir, dir, ino, dentry, curr_entry);
	*new_tail = curr_tail;
	PMFS_END_TIMING(add_entry_t, add_entry_time);
	return 0;
}

/* removes a directory entry pointing to the inode. assumes the inode has
 * already been logged for consistency
 */
int pmfs_remove_entry(struct dentry *dentry, int dec_link, u64 tail,
	u64 *new_tail)
{
	struct inode *dir = dentry->d_parent->d_inode;
	struct super_block *sb = dir->i_sb;
	struct pmfs_inode *pidir;
	struct qstr *entry = &dentry->d_name;
	unsigned short loglen;
	u64 curr_tail, curr_entry;
	timing_t remove_entry_time;

	PMFS_START_TIMING(remove_entry_t, remove_entry_time);

	if (!dentry->d_name.len)
		return -EINVAL;

	pidir = pmfs_get_inode(sb, dir);

	dir->i_mtime = dir->i_ctime = CURRENT_TIME_SEC;

	loglen = PMFS_DIR_LOG_REC_LEN(entry->len);
	curr_entry = pmfs_append_dir_inode_entry(sb, pidir, dir, NULL, 0,
				dentry, loglen, tail, dec_link, 0, &curr_tail);
	pmfs_remove_dir_node(sb, pidir, dir, dentry);
	*new_tail = curr_tail;

	PMFS_END_TIMING(remove_entry_t, remove_entry_time);
	return 0;
}

inline int pmfs_replay_add_entry(struct super_block *sb, struct pmfs_inode *pi,
	struct pmfs_inode_info_header *sih, struct pmfs_dir_logentry *entry,
	u64 curr_p)
{
	if (!entry->name_len)
		return -EINVAL;

	pmfs_dbg_verbose("%s: add %s\n", __func__, entry->name);
	return pmfs_insert_dir_node_by_name(sb, pi, sih,
			__le64_to_cpu(entry->ino), entry->name,
			entry->name_len, curr_p);
}

inline int pmfs_replay_remove_entry(struct super_block *sb,
	struct pmfs_inode *pi, struct pmfs_inode_info_header *sih,
	struct pmfs_dir_logentry *entry)
{
	pmfs_dbg_verbose("%s: remove %s\n", __func__, entry->name);
	pmfs_remove_dir_node_by_name(sb, pi, sih, entry->name,
					entry->name_len);
	return 0;
}

static inline void pmfs_rebuild_dir_time_and_size(struct super_block *sb,
	struct pmfs_inode *pi, struct pmfs_dir_logentry *entry)
{
	if (!entry || !pi)
		return;

	pi->i_ctime = entry->mtime;
	pi->i_mtime = entry->mtime;
	pi->i_size = entry->size;
	pi->i_links_count = entry->links_count;
}

int pmfs_rebuild_dir_inode_tree(struct super_block *sb,
	struct pmfs_inode *pi, u64 pi_addr,
	struct pmfs_inode_info_header *sih, struct scan_bitmap *bm)
{
	struct pmfs_dir_logentry *entry = NULL;
	struct pmfs_setattr_logentry *attr_entry = NULL;
	struct pmfs_link_change_entry *link_change_entry = NULL;
	struct pmfs_inode_log_page *curr_page;
	u64 ino = pi->pmfs_ino;
	unsigned short de_len;
	void *addr;
	u64 curr_p;
	u64 next;
	u8 type;
	int ret;

	pmfs_dbg_verbose("Rebuild dir %llu tree\n", ino);

	sih->dir_tree = RB_ROOT;
	sih->pi_addr = pi_addr;

	curr_p = pi->log_head;
	if (curr_p == 0) {
		pmfs_err(sb, "Dir %llu log is NULL!\n", ino);
		BUG();
	}

	pmfs_dbg_verbose("Log head 0x%llx, tail 0x%llx\n",
				curr_p, pi->log_tail);
	if (bm) {
		BUG_ON(curr_p & (PAGE_SIZE - 1));
		set_bm(curr_p >> PAGE_SHIFT, bm, BM_4K);
	}
	sih->log_pages = 1;
	while (curr_p != pi->log_tail) {
		if (is_last_dir_entry(sb, curr_p)) {
			sih->log_pages++;
			curr_p = next_log_page(sb, curr_p);
			if (bm) {
				BUG_ON(curr_p & (PAGE_SIZE - 1));
				set_bm(curr_p >> PAGE_SHIFT, bm, BM_4K);
			}
		}

		if (curr_p == 0) {
			pmfs_err(sb, "Dir %llu log is NULL!\n", ino);
			BUG();
		}

		addr = (void *)pmfs_get_block(sb, curr_p);
		type = pmfs_get_entry_type(addr);
		switch (type) {
			case SET_ATTR:
				attr_entry =
					(struct pmfs_setattr_logentry *)addr;
				pmfs_apply_setattr_entry(pi, attr_entry);
				curr_p += sizeof(struct pmfs_setattr_logentry);
				continue;
			case LINK_CHANGE:
				link_change_entry =
					(struct pmfs_link_change_entry *)addr;
				pmfs_apply_link_change_entry(pi,
							link_change_entry);
				curr_p += sizeof(struct pmfs_link_change_entry);
				continue;
			case DIR_LOG:
				break;
			default:
				pmfs_dbg("%s: unknown type %d, 0x%llx\n",
							__func__, type, curr_p);
				PMFS_ASSERT(0);
		}

		entry = (struct pmfs_dir_logentry *)pmfs_get_block(sb, curr_p);
		pmfs_dbg_verbose("curr_p: 0x%llx, type %d, ino %llu, "
			"name %*.s, namelen %u, rec len %u\n", curr_p,
			entry->entry_type, le64_to_cpu(entry->ino),
			entry->name_len, entry->name,
			entry->name_len, le16_to_cpu(entry->de_len));

		if (entry->ino > 0) {
			/* A valid entry to add */
			ret = pmfs_replay_add_entry(sb, pi, sih,
							entry, curr_p);
		} else {
			/* Delete the entry */
			ret = pmfs_replay_remove_entry(sb, pi, sih, entry);
		}

		if (ret) {
			pmfs_err(sb, "%s ERROR %d\n", __func__, ret);
			break;
		}

		pmfs_rebuild_dir_time_and_size(sb, pi, entry);

		de_len = le16_to_cpu(entry->de_len);
		curr_p += de_len;

		/*
		 * If following by a new inode, find the inode
		 * and its end first
		 */
		if (entry->new_inode) {
			if (is_last_entry(curr_p - de_len, de_len, 1)) {
				sih->log_pages++;
				curr_p = next_log_page(sb, curr_p);
				if (bm) {
					BUG_ON(curr_p & (PAGE_SIZE - 1));
					set_bm(curr_p >> PAGE_SHIFT,
							bm, BM_4K);
				}
			} else {
				curr_p = (curr_p & (CACHELINE_SIZE - 1)) == 0 ?
					curr_p : CACHE_ALIGN(curr_p) +
							CACHELINE_SIZE;
			}
			/* If power failure, recover the inode in DFS way */
			if (bm)
				pmfs_recover_inode(sb, curr_p,
						bm, smp_processor_id(), 0);

			curr_p += PMFS_INODE_SIZE;
		}
	}

	sih->i_size = le64_to_cpu(pi->i_size);
	sih->i_mode = le64_to_cpu(pi->i_mode);
	pmfs_flush_buffer(pi, sizeof(struct pmfs_inode), 1);

	/* Keep traversing until log ends */
	curr_p &= PAGE_MASK;
	curr_page = (struct pmfs_inode_log_page *)pmfs_get_block(sb, curr_p);
	while ((next = curr_page->page_tail.next_page) != 0) {
		sih->log_pages++;
		curr_p = next;
		if (bm) {
			BUG_ON(curr_p & (PAGE_SIZE - 1));
			set_bm(curr_p >> PAGE_SHIFT, bm, BM_4K);
		}
		curr_page = (struct pmfs_inode_log_page *)
			pmfs_get_block(sb, curr_p);
	}

	if (bm)
		pi->i_blocks += sih->log_pages;

//	pmfs_print_dir_tree(sb, sih, ino);
	return 0;
}

static int pmfs_readdir(struct file *file, struct dir_context *ctx)
{
	struct inode *inode = file_inode(file);
	struct super_block *sb = inode->i_sb;
	struct pmfs_inode *pidir;
	struct pmfs_inode_info *si = PMFS_I(inode);
	struct pmfs_inode_info_header *sih = si->header;
	struct pmfs_inode_info_header *child_sih;
	struct pmfs_dir_node *curr;
	struct pmfs_dir_logentry *entry;
	struct rb_node *temp;
	ino_t ino;
	timing_t readdir_time;

	PMFS_START_TIMING(readdir_t, readdir_time);
	pidir = pmfs_get_inode(sb, inode);
	pmfs_dbgv("%s: ino %llu, size %llu, pos %llu\n",
			__func__, (u64)inode->i_ino,
			pidir->i_size, ctx->pos);

	if (!sih) {
		pmfs_dbg("%s: inode %lu sih does not exist!\n",
				__func__, inode->i_ino);
		ctx->pos = READDIR_END;
		return 0;
	}

	if (ctx->pos == 0) {
		temp = rb_first(&sih->dir_tree);
	} else if (ctx->pos == READDIR_END) {
		goto out;
	} else if (ctx->pos) {
		entry = (struct pmfs_dir_logentry *)
				pmfs_get_block(sb, ctx->pos);
		pmfs_dbgv("ctx: ino %llu, name %*.s, "
			"name_len %u, de_len %u\n",
			(u64)entry->ino, entry->name_len, entry->name,
			entry->name_len, entry->de_len);
		curr = pmfs_find_dir_node_by_name(sb, NULL, inode,
					entry->name, entry->name_len);
		temp = &curr->node;
	}

	while (temp) {
		curr = container_of(temp, struct pmfs_dir_node, node);

		if (!curr || curr->nvmm == 0)
			BUG();

		entry = (struct pmfs_dir_logentry *)
				pmfs_get_block(sb, curr->nvmm);

		if (__le64_to_cpu(entry->ino) != curr->ino)
			pmfs_dbg("%s: ino does not match: %llu, %llu\n",
				__func__, entry->ino, curr->ino);

		if (curr->ino) {
			ino = curr->ino;
			child_sih = pmfs_find_info_header(sb, ino);
			pmfs_dbgv("ctx: ino %llu, name %*.s, "
				"name_len %u, de_len %u\n",
				(u64)ino, entry->name_len, entry->name,
				entry->name_len, entry->de_len);
			if (!child_sih) {
				pmfs_dbg("%s: child inode %lu sih "
					"does not exist!\n", __func__, ino);
				ctx->pos = READDIR_END;
				return 0;
			}
			if (!dir_emit(ctx, entry->name, entry->name_len,
				ino, IF2DT(le16_to_cpu(child_sih->i_mode)))) {
				pmfs_dbgv("Here: pos %llu\n", ctx->pos);
				ctx->pos = curr->nvmm;
				return 0;
			}
		}
		temp = rb_next(temp);
	}

	/*
	 * We have reach the end. To let readdir be aware of that, we assign
	 * a bogus end offset to ctx.
	 */
	ctx->pos = READDIR_END;
out:
	PMFS_END_TIMING(readdir_t, readdir_time);
	return 0;
}

const struct file_operations pmfs_dir_operations = {
	.read		= generic_read_dir,
	.iterate	= pmfs_readdir,
	.fsync		= noop_fsync,
	.unlocked_ioctl = pmfs_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= pmfs_compat_ioctl,
#endif
};
