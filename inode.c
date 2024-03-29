#include<linux/xattr.h>
#include "wrapfs.h"

static int wrapfs_create(struct inode *dir, struct dentry *dentry,
                         int mode, struct nameidata *nd)
{
        int err = 0;
        struct dentry *lower_dentry;
        struct dentry *lower_parent_dentry = NULL;
        struct path lower_path, saved_path;
        char *buf= kmalloc(1*sizeof(char),GFP_KERNEL);
        char *buf1=kmalloc(1*sizeof(char),GFP_KERNEL);
        wrapfs_get_lower_path(dentry, &lower_path);
        lower_dentry = lower_path.dentry;
        lower_parent_dentry = lock_parent(lower_dentry);

        err = mnt_want_write(lower_path.mnt);
        if (err)
                goto out_unlock;

        pathcpy(&saved_path, &nd->path);
        pathcpy(&nd->path, &lower_path);
        err = vfs_create(lower_parent_dentry->d_inode, lower_dentry, mode, nd);
        pathcpy(&nd->path, &saved_path);
        if (err)
                goto out;

        err = wrapfs_interpose(dentry, dir->i_sb, &lower_path);
        if (err)
                goto out;
        fsstack_copy_attr_times(dir, wrapfs_lower_inode(dir));
        fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);
        printk("\nout of ur block of code\n");
        if(vfs_getxattr(lower_parent_dentry,"user.has_integrity",buf,1)>0){
                printk("\nIn the block\n");

                if((*(char *)buf)=='1'){
                printk("Nested inside block");
                vfs_setxattr(lower_dentry,"user.has_integrity",buf,1,XATTR_CREATE);
                }
        }
out:
        mnt_drop_write(lower_path.mnt);
out_unlock:
        kfree(buf);
        kfree(buf1);
        unlock_dir(lower_parent_dentry);
        wrapfs_put_lower_path(dentry, &lower_path);
        return err;
}

static int wrapfs_link(struct dentry *old_dentry, struct inode *dir,
                       struct dentry *new_dentry)
{
        struct dentry *lower_old_dentry;
        struct dentry *lower_new_dentry;
        struct dentry *lower_dir_dentry;
        u64 file_size_save;
        int err;
        struct path lower_old_path, lower_new_path;

        file_size_save = i_size_read(old_dentry->d_inode);
        wrapfs_get_lower_path(old_dentry, &lower_old_path);
        wrapfs_get_lower_path(new_dentry, &lower_new_path);
        lower_old_dentry = lower_old_path.dentry;
        lower_new_dentry = lower_new_path.dentry;
        lower_dir_dentry = lock_parent(lower_new_dentry);

        err = mnt_want_write(lower_new_path.mnt);
        if (err)
                goto out_unlock;

        err = vfs_link(lower_old_dentry, lower_dir_dentry->d_inode,
                       lower_new_dentry);
        if (err || !lower_new_dentry->d_inode)
                goto out;

        err = wrapfs_interpose(new_dentry, dir->i_sb, &lower_new_path);
        if (err)
                goto out;
        fsstack_copy_attr_times(dir, lower_new_dentry->d_inode);
        fsstack_copy_inode_size(dir, lower_new_dentry->d_inode);
        set_nlink(old_dentry->d_inode,
                  wrapfs_lower_inode(old_dentry->d_inode)->i_nlink);
        i_size_write(new_dentry->d_inode, file_size_save);
out:
        mnt_drop_write(lower_new_path.mnt);
out_unlock:
        unlock_dir(lower_dir_dentry);
        wrapfs_put_lower_path(old_dentry, &lower_old_path);
        wrapfs_put_lower_path(new_dentry, &lower_new_path);
        return err;
}

static int wrapfs_unlink(struct inode *dir, struct dentry *dentry)
{
        int err;
        struct dentry *lower_dentry;
        struct inode *lower_dir_inode = wrapfs_lower_inode(dir);
        struct dentry *lower_dir_dentry;
        struct path lower_path;

        wrapfs_get_lower_path(dentry, &lower_path);
        lower_dentry = lower_path.dentry;
        dget(lower_dentry);
        lower_dir_dentry = lock_parent(lower_dentry);

        err = mnt_want_write(lower_path.mnt);
        if (err)
                goto out_unlock;
        err = vfs_unlink(lower_dir_inode, lower_dentry);

        /*
         * Note: unlinking on top of NFS can cause silly-renamed files.
         * Trying to delete such files results in EBUSY from NFS
         * below.  Silly-renamed files will get deleted by NFS later on, so
         * we just need to detect them here and treat such EBUSY errors as
         * if the upper file was successfully deleted.
         */
        if (err == -EBUSY && lower_dentry->d_flags & DCACHE_NFSFS_RENAMED)
                err = 0;
        if (err)
                goto out;
        fsstack_copy_attr_times(dir, lower_dir_inode);
        fsstack_copy_inode_size(dir, lower_dir_inode);
        set_nlink(dentry->d_inode,
                  wrapfs_lower_inode(dentry->d_inode)->i_nlink);
        dentry->d_inode->i_ctime = dir->i_ctime;
        d_drop(dentry); /* this is needed, else LTP fails (VFS won't do it) */
out:
        mnt_drop_write(lower_path.mnt);
out_unlock:
        unlock_dir(lower_dir_dentry);
        dput(lower_dentry);
        wrapfs_put_lower_path(dentry, &lower_path);
        return err;
}

static int wrapfs_symlink(struct inode *dir, struct dentry *dentry,
                          const char *symname)
{
        int err = 0;
        struct dentry *lower_dentry;
        struct dentry *lower_parent_dentry = NULL;
        struct path lower_path;

        wrapfs_get_lower_path(dentry, &lower_path);
        lower_dentry = lower_path.dentry;
        lower_parent_dentry = lock_parent(lower_dentry);

        err = mnt_want_write(lower_path.mnt);
        if (err)
                goto out_unlock;
        err = vfs_symlink(lower_parent_dentry->d_inode, lower_dentry, symname);
        if (err)
                goto out;
        err = wrapfs_interpose(dentry, dir->i_sb, &lower_path);
        if (err)
                goto out;
        fsstack_copy_attr_times(dir, wrapfs_lower_inode(dir));
        fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);

out:
        mnt_drop_write(lower_path.mnt);
out_unlock:
        unlock_dir(lower_parent_dentry);
        wrapfs_put_lower_path(dentry, &lower_path);
        return err;
}

static int wrapfs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
        int err = 0;
        struct dentry *lower_dentry;
        struct dentry *lower_parent_dentry = NULL;
        struct path lower_path;

        wrapfs_get_lower_path(dentry, &lower_path);
        lower_dentry = lower_path.dentry;
        lower_parent_dentry = lock_parent(lower_dentry);

        err = mnt_want_write(lower_path.mnt);
        if (err)
                goto out_unlock;
        err = vfs_mkdir(lower_parent_dentry->d_inode, lower_dentry, mode);
        if (err)
                goto out;

        err = wrapfs_interpose(dentry, dir->i_sb, &lower_path);
        if (err)
                goto out;

        fsstack_copy_attr_times(dir, wrapfs_lower_inode(dir));
        fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);
        /* update number of links on parent directory */
        set_nlink(dir, wrapfs_lower_inode(dir)->i_nlink);

out:
        mnt_drop_write(lower_path.mnt);
out_unlock:
        unlock_dir(lower_parent_dentry);
        wrapfs_put_lower_path(dentry, &lower_path);
        return err;
}
static int wrapfs_rmdir(struct inode *dir, struct dentry *dentry)
{
        struct dentry *lower_dentry;
        struct dentry *lower_dir_dentry;
        int err;
        struct path lower_path;

        wrapfs_get_lower_path(dentry, &lower_path);
        lower_dentry = lower_path.dentry;
        lower_dir_dentry = lock_parent(lower_dentry);

        err = mnt_want_write(lower_path.mnt);
        if (err)
                goto out_unlock;
        err = vfs_rmdir(lower_dir_dentry->d_inode, lower_dentry);
        if (err)
                goto out;

        d_drop(dentry); /* drop our dentry on success (why not VFS's job?) */
        if (dentry->d_inode)
                clear_nlink(dentry->d_inode);
        fsstack_copy_attr_times(dir, lower_dir_dentry->d_inode);
        fsstack_copy_inode_size(dir, lower_dir_dentry->d_inode);
        set_nlink(dir, lower_dir_dentry->d_inode->i_nlink);

out:
        mnt_drop_write(lower_path.mnt);
out_unlock:
        unlock_dir(lower_dir_dentry);
        wrapfs_put_lower_path(dentry, &lower_path);
        return err;
}

static int wrapfs_mknod(struct inode *dir, struct dentry *dentry, int mode,
                        dev_t dev)
{
        int err = 0;
        struct dentry *lower_dentry;
        struct dentry *lower_parent_dentry = NULL;
        struct path lower_path;
        wrapfs_get_lower_path(dentry, &lower_path);
        lower_dentry = lower_path.dentry;
        lower_parent_dentry = lock_parent(lower_dentry);

        err = mnt_want_write(lower_path.mnt);
        if (err)
                goto out_unlock;
        err = vfs_mknod(lower_parent_dentry->d_inode, lower_dentry, mode, dev);
        if (err)
                goto out;

        err = wrapfs_interpose(dentry, dir->i_sb, &lower_path);
        if (err)
                goto out;
        fsstack_copy_attr_times(dir, wrapfs_lower_inode(dir));
        fsstack_copy_inode_size(dir, lower_parent_dentry->d_inode);

out:
        mnt_drop_write(lower_path.mnt);
out_unlock:
        unlock_dir(lower_parent_dentry);
        wrapfs_put_lower_path(dentry, &lower_path);
        return err;
}

/*
 * The locking rules in wrapfs_rename are complex.  We could use a simpler
 * superblock-level name-space lock for renames and copy-ups.
 */
static int wrapfs_rename(struct inode *old_dir, struct dentry *old_dentry,
                         struct inode *new_dir, struct dentry *new_dentry)
{
        int err = 0;
        struct dentry *lower_old_dentry = NULL;
        struct dentry *lower_new_dentry = NULL;
        struct dentry *lower_old_dir_dentry = NULL;
        struct dentry *lower_new_dir_dentry = NULL;
        struct dentry *trap = NULL;
        struct path lower_old_path, lower_new_path;

        wrapfs_get_lower_path(old_dentry, &lower_old_path);
    wrapfs_get_lower_path(new_dentry, &lower_new_path);
        lower_old_dentry = lower_old_path.dentry;
        lower_new_dentry = lower_new_path.dentry;
        lower_old_dir_dentry = dget_parent(lower_old_dentry);
        lower_new_dir_dentry = dget_parent(lower_new_dentry);

        trap = lock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
        /* source should not be ancestor of target */
        if (trap == lower_old_dentry) {
                err = -EINVAL;
                goto out;
        }
        /* target should not be ancestor of source */
        if (trap == lower_new_dentry) {
                err = -ENOTEMPTY;
                goto out;
        }

        err = mnt_want_write(lower_old_path.mnt);
        if (err)
                goto out;
        err = mnt_want_write(lower_new_path.mnt);
        if (err)
                goto out_drop_old_write;

        err = vfs_rename(lower_old_dir_dentry->d_inode, lower_old_dentry,
                         lower_new_dir_dentry->d_inode, lower_new_dentry);
        if (err)
                goto out_err;

        fsstack_copy_attr_all(new_dir, lower_new_dir_dentry->d_inode);
        fsstack_copy_inode_size(new_dir, lower_new_dir_dentry->d_inode);
        if (new_dir != old_dir) {
                fsstack_copy_attr_all(old_dir,
                                      lower_old_dir_dentry->d_inode);
                fsstack_copy_inode_size(old_dir,
                                        lower_old_dir_dentry->d_inode);
        }

out_err:
        mnt_drop_write(lower_new_path.mnt);
out_drop_old_write:
        mnt_drop_write(lower_old_path.mnt);
out:
        unlock_rename(lower_old_dir_dentry, lower_new_dir_dentry);
        dput(lower_old_dir_dentry);
        dput(lower_new_dir_dentry);
        wrapfs_put_lower_path(old_dentry, &lower_old_path);
        wrapfs_put_lower_path(new_dentry, &lower_new_path);
        return err;
}

static int wrapfs_readlink(struct dentry *dentry, char __user *buf, int bufsiz)
{
        int err;
        struct dentry *lower_dentry;
        struct path lower_path;

        wrapfs_get_lower_path(dentry, &lower_path);
        lower_dentry = lower_path.dentry;
        if (!lower_dentry->d_inode->i_op ||
            !lower_dentry->d_inode->i_op->readlink) {
                err = -EINVAL;
                goto out;
        }

        err = lower_dentry->d_inode->i_op->readlink(lower_dentry,
                                                    buf, bufsiz);
        if (err < 0)
                goto out;
        fsstack_copy_attr_atime(dentry->d_inode, lower_dentry->d_inode);

out:
        wrapfs_put_lower_path(dentry, &lower_path);
        return err;
}

static void *wrapfs_follow_link(struct dentry *dentry, struct nameidata *nd)
{
        char *buf;
        int len = PAGE_SIZE, err;
        mm_segment_t old_fs;

        /* This is freed by the put_link method assuming a successful call. */
        buf = kmalloc(len, GFP_KERNEL);
        if (!buf) {
                buf = ERR_PTR(-ENOMEM);
                goto out;
        }

        /* read the symlink, and then we will follow it */
        old_fs = get_fs();
        set_fs(KERNEL_DS);
        err = wrapfs_readlink(dentry, buf, len);
        set_fs(old_fs);
        if (err < 0) {
                kfree(buf);
                buf = ERR_PTR(err);
        } else {
                buf[err] = '\0';
        }
out:
        nd_set_link(nd, buf);
        return NULL;
}

/* this @nd *IS* still used */
static void wrapfs_put_link(struct dentry *dentry, struct nameidata *nd,
                            void *cookie)
{
        char *buf = nd_get_link(nd);
        if (!IS_ERR(buf))       /* free the char* */
                kfree(buf);
}

static int wrapfs_permission(struct inode *inode, int mask)
{
        struct inode *lower_inode;
        int err;

        lower_inode = wrapfs_lower_inode(inode);
        err = inode_permission(lower_inode, mask);
        return err;
}

static int wrapfs_setattr(struct dentry *dentry, struct iattr *ia)
{
        int err = 0;
        struct dentry *lower_dentry;
        struct inode *inode;
        struct inode *lower_inode;
        struct path lower_path;
        struct iattr lower_ia;

        inode = dentry->d_inode;

        /*
         * Check if user has permission to change inode.  We don't check if
         * this user can change the lower inode: that should happen when
         * calling notify_change on the lower inode.
         */
        err = inode_change_ok(inode, ia);
        if (err)
                goto out_err;

        wrapfs_get_lower_path(dentry, &lower_path);
        lower_dentry = lower_path.dentry;
        lower_inode = wrapfs_lower_inode(inode);

        /* prepare our own lower struct iattr (with the lower file) */
        memcpy(&lower_ia, ia, sizeof(lower_ia));
        if (ia->ia_valid & ATTR_FILE)
                lower_ia.ia_file = wrapfs_lower_file(ia->ia_file);

        /*
         * If shrinking, first truncate upper level to cancel writing dirty
         * pages beyond the new eof; and also if its' maxbytes is more
         * limiting (fail with -EFBIG before making any change to the lower
         * level).  There is no need to vmtruncate the upper level
         * afterwards in the other cases: we fsstack_copy_inode_size from
         * the lower level.
         */
        if (ia->ia_valid & ATTR_SIZE) {
                err = inode_newsize_ok(inode, ia->ia_size);
                if (err)
                        goto out;
                truncate_setsize(inode, ia->ia_size);
        }
         */
        if (lower_ia.ia_valid & (ATTR_KILL_SUID | ATTR_KILL_SGID))
                lower_ia.ia_valid &= ~ATTR_MODE;

        /* notify the (possibly copied-up) lower inode */
        /*
         * Note: we use lower_dentry->d_inode, because lower_inode may be
         * unlinked (no inode->i_sb and i_ino==0.  This happens if someone
         * tries to open(), unlink(), then ftruncate() a file.
         */
        mutex_lock(&lower_dentry->d_inode->i_mutex);
        err = notify_change(lower_dentry, &lower_ia); /* note: lower_ia */
        mutex_unlock(&lower_dentry->d_inode->i_mutex);
        if (err)
                goto out;

        /* get attributes from the lower inode */
        fsstack_copy_attr_all(inode, lower_inode);
        /*
         * Not running fsstack_copy_inode_size(inode, lower_inode), because
         * VFS should update our inode size, and notify_change on
         * lower_inode should update its size.
         */

out:
        wrapfs_put_lower_path(dentry, &lower_path);
out_err:
        return err;
}

const struct inode_operations wrapfs_symlink_iops = {
        .readlink       = wrapfs_readlink,
        .permission     = wrapfs_permission,
        .follow_link    = wrapfs_follow_link,
        .setattr        = wrapfs_setattr,
        .put_link       = wrapfs_put_link,
};

const struct inode_operations wrapfs_dir_iops = {
        .create         = wrapfs_create,
        .lookup         = wrapfs_lookup,
        .link           = wrapfs_link,
        .unlink         = wrapfs_unlink,
        .symlink        = wrapfs_symlink,
        .mkdir          = wrapfs_mkdir,
        .rmdir          = wrapfs_rmdir,
        .mknod          = wrapfs_mknod,
        .rename         = wrapfs_rename,
        .permission     = wrapfs_permission,
        .setattr        = wrapfs_setattr,
         .setxattr       = wrapfs_setxattr,
        .getxattr       = wrapfs_getxattr,
        .listxattr      = wrapfs_listxattr,
        .removexattr    = wrapfs_removexattr,

};

const struct inode_operations wrapfs_main_iops = {
        .permission     = wrapfs_permission,
        .setattr        = wrapfs_setattr,
        .setxattr       = wrapfs_setxattr,
        .getxattr       = wrapfs_getxattr,
        .listxattr      = wrapfs_listxattr,
        .removexattr    = wrapfs_removexattr,
};
