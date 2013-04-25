#include<linux/xattr.h>
#include "wrapfs.h"
extern int compute_checksum(struct file *,void *);
static ssize_t wrapfs_read(struct file *file, char __user *buf,
                           size_t count, loff_t *ppos)
{
        int err;
        struct file *lower_file;
        struct dentry *dentry = file->f_path.dentry;

        lower_file = wrapfs_lower_file(file);
        err = vfs_read(lower_file, buf, count, ppos);
        /* update our inode atime upon a successful lower read */
        if (err >= 0)
                fsstack_copy_attr_atime(dentry->d_inode,
                                        lower_file->f_path.dentry->d_inode);

        return err;
}

static ssize_t wrapfs_write(struct file *file, const char __user *buf,
                            size_t count, loff_t *ppos)
{
        int err = 0,i;
        struct file *lower_file;
        struct dentry *dentry = file->f_path.dentry;

        lower_file = wrapfs_lower_file(file);
        err = vfs_write(lower_file, buf, count, ppos);
        printk("\nI am in write function\n");
        /* update our inode times+sizes upon a successful lower write */
        if (err >= 0) {
                fsstack_copy_inode_size(dentry->d_inode,
                                        lower_file->f_path.dentry->d_inode);
                fsstack_copy_attr_times(dentry->d_inode,
                                        lower_file->f_path.dentry->d_inode);
                i= wrapfs_set_value(dentry->d_inode,1);
                printk("\n Flag is set to %d in write function",i);
        }

        return err;
}

static int wrapfs_readdir(struct file *file, void *dirent, filldir_t filldir)
{
        int err = 0;
        struct file *lower_file = NULL;
        struct dentry *dentry = file->f_path.dentry;

        lower_file = wrapfs_lower_file(file);
        err = vfs_readdir(lower_file, filldir, dirent);
        //file->f_pos = lower_file->f_pos;
        if (err >= 0){          /* copy the atime */
        fsstack_copy_attr_atime(dentry->d_inode,lower_file->f_path.dentry->d_inode);
         file->f_pos = lower_file->f_pos;
        }
        return err;
}

static long wrapfs_unlocked_ioctl(struct file *file, unsigned int cmd,
                                  unsigned long arg)
{
        long err = -ENOTTY;
        struct file *lower_file;

        lower_file = wrapfs_lower_file(file);

        /* XXX: use vfs_ioctl if/when VFS exports it */
        if (!lower_file || !lower_file->f_op)
                goto out;
        if (lower_file->f_op->unlocked_ioctl)
                err = lower_file->f_op->unlocked_ioctl(lower_file, cmd, arg);

out:
        return err;
}
#ifdef CONFIG_COMPAT
static long wrapfs_compat_ioctl(struct file *file, unsigned int cmd,
                                unsigned long arg)
{
        long err = -ENOTTY;
        struct file *lower_file;

        lower_file = wrapfs_lower_file(file);

        /* XXX: use vfs_ioctl if/when VFS exports it */
        if (!lower_file || !lower_file->f_op)
                goto out;
        if (lower_file->f_op->compat_ioctl)
                err = lower_file->f_op->compat_ioctl(lower_file, cmd, arg);

out:
        return err;
}
#endif

static int wrapfs_mmap(struct file *file, struct vm_area_struct *vma)
{
        int err = 0;
        bool willwrite;
        struct file *lower_file;
        const struct vm_operations_struct *saved_vm_ops = NULL;

        /* this might be deferred to mmap's writepage */
        willwrite = ((vma->vm_flags | VM_SHARED | VM_WRITE) == vma->vm_flags);

        /*
         * File systems which do not implement ->writepage may use
         * generic_file_readonly_mmap as their ->mmap op.  If you call
         * generic_file_readonly_mmap with VM_WRITE, you'd get an -EINVAL.
         * But we cannot call the lower ->mmap op, so we can't tell that
         * writeable mappings won't work.  Therefore, our only choice is to
         * check if the lower file system supports the ->writepage, and if
         * not, return EINVAL (the same error that
         * generic_file_readonly_mmap returns in that case).
         */
        lower_file = wrapfs_lower_file(file);
        if (willwrite && !lower_file->f_mapping->a_ops->writepage) {
               err = -EINVAL;
                printk(KERN_ERR "wrapfs: lower file system does not "
                       "support writeable mmap\n");
                goto out;
        }

        /*
         * find and save lower vm_ops.
         *
         * XXX: the VFS should have a cleaner way of finding the lower vm_ops
         */
        if (!WRAPFS_F(file)->lower_vm_ops) {
                err = lower_file->f_op->mmap(lower_file, vma);
                if (err) {
                        printk(KERN_ERR "wrapfs: lower mmap failed %d\n", err);
                        goto out;
                }
                saved_vm_ops = vma->vm_ops; /* save: came from lower ->mmap */
                err = do_munmap(current->mm, vma->vm_start,
                                vma->vm_end - vma->vm_start);
                if (err) {
                        printk(KERN_ERR "wrapfs: do_munmap failed %d\n", err);
                        goto out;
                }
        }

        /*
         * Next 3 lines are all I need from generic_file_mmap.  I definitely
         * don't want its test for ->readpage which returns -ENOEXEC.
         */
        file_accessed(file);
        vma->vm_ops = &wrapfs_vm_ops;
        vma->vm_flags |= VM_CAN_NONLINEAR;

        file->f_mapping->a_ops = &wrapfs_aops; /* set our aops */
        if (!WRAPFS_F(file)->lower_vm_ops) /* save for our ->fault */
                WRAPFS_F(file)->lower_vm_ops = saved_vm_ops;

out:
        return err;
}

static int wrapfs_open(struct inode *inode, struct file *file)
{

        int err = 0;
        struct file *lower_file = NULL;
        struct path lower_path;
        char *output=kmalloc(1*sizeof(char),GFP_KERNEL);
        char *a1=kmalloc(16*sizeof(char),GFP_KERNEL);
        char *a2=kmalloc(16 * sizeof(char),GFP_KERNEL);
        char *a3=kmalloc(16*sizeof(char),GFP_KERNEL);

        if (d_unhashed(file->f_path.dentry)) {

                err = -ENOENT;
                goto out_err;
        }

        file->private_data =
                kzalloc(sizeof(struct wrapfs_file_info), GFP_KERNEL);
        if (!WRAPFS_F(file)) {

                err = -ENOMEM;
                goto out_err;
        }


        wrapfs_get_lower_path(file->f_path.dentry, &lower_path);


        lower_file = dentry_open(lower_path.dentry, lower_path.mnt,
                                 file->f_flags, current_cred());



        if (IS_ERR(lower_file)) {

                err = PTR_ERR(lower_file);
                lower_file = wrapfs_lower_file(file);

                if (lower_file) {
                        wrapfs_set_lower_file(file, NULL);
                        fput(lower_file);
                }
        }
        else {


                 //wrapfs_set_lower_file(file, lower_file);



          if(vfs_getxattr(lower_path.dentry,"user.has_integrity",output,1)>0){


                if(!memcmp(output,"1",1)){
                        printk("\nHas Integrity value of file is 1\n");
                //directory
                if(S_ISDIR(lower_path.dentry->d_inode->i_mode)){
                        printk("\n Directory boss\n");
                        wrapfs_set_lower_file(file, lower_file);
                        goto out_err;

                }
                //end of directory
        if(vfs_getxattr(lower_path.dentry,"user.integrity_val",a1,16)>0){


                wrapfs_set_lower_file(file, lower_file);
                compute_checksum(lower_file,a2);

                  if(memcmp(a1,a2,16)){

                        printk("\nCANT OPEN FILE\n");
                        err= -EPERM;
                        //fput(lower_file);
                        goto out_err;
                }

        }
        //end of if
        else{
                wrapfs_set_lower_file(file, lower_file);
                 compute_checksum(lower_file,a3);

                 vfs_setxattr(lower_path.dentry,"user.integrity_val",a3,16,XATTR_CREATE);

        }
                }

                else if(!memcmp(output,"0",1)){
                        printk("\n Integrity is not set\n");
                         wrapfs_set_lower_file(file, lower_file);
                }
          }
          else{
                 wrapfs_set_lower_file(file, lower_file);
          }
        }
        if (err){
                //printk("\nmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm\n");
                kfree(WRAPFS_F(file));
        }
        else{
                //printk("\nbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\n");
                fsstack_copy_attr_all(inode, wrapfs_lower_inode(inode));
        }


out_err:
        kfree(output);
        kfree(a1);
        kfree(a2);
        return err;

}

static int wrapfs_flush(struct file *file, fl_owner_t id){

        int err = 0;
        struct file *lower_file = NULL;

        lower_file = wrapfs_lower_file(file);
        if (lower_file && lower_file->f_op && lower_file->f_op->flush)
                err = lower_file->f_op->flush(lower_file, id);

        return err;
}

/* release all lower object references & free the file info structure */
static int wrapfs_file_release(struct inode *inode, struct file *file)
{
        char *buf=kmalloc(16*sizeof(char),GFP_KERNEL);
        struct file *lower_file;
        int flag,n_bytes,e;
        struct path lower_path;
        wrapfs_get_lower_path(file->f_path.dentry,&lower_path);
        lower_file = wrapfs_lower_file(file);
        printk("\n I am in Release function \n");
        //flag=wrapfs_get_value(inode);
        //printk("\nFlag value is %d",flag);
        /*if(lower_file->f_mode & FMODE_WRITE && flag==1){
                printk("\nUpdatte the integrity\n");
                n_bytes=compute_checksum(lower_file,buf);
        e=vfs_setxattr(file->f_path.dentry,"user.integrity_val",buf,16,XATTR_REPLACE);
        }*/
        if (lower_file){
                printk("\nDirectory cant enter as its file\n");
                flag=wrapfs_get_value(inode);
                //printk("\nFlag value is %d",flag);
                if(lower_file->f_mode & FMODE_WRITE && flag==1){
                printk("\nUpdatte the integrity\n");
                n_bytes=compute_checksum(lower_file,buf);
  e=vfs_setxattr(lower_path.dentry,"user.integrity_val",buf,16,XATTR_REPLACE);

                }
                wrapfs_set_lower_file(file, NULL);
                fput(lower_file);
        }
        wrapfs_put_lower_path(file->f_path.dentry, &lower_path);

        kfree(WRAPFS_F(file));
        return 0;
}

static int wrapfs_fsync(struct file *file, loff_t start, loff_t end,
                        int datasync)
{
        int err;
        struct file *lower_file;
        struct path lower_path;
        struct dentry *dentry = file->f_path.dentry;

        err = generic_file_fsync(file, start, end, datasync);
        if (err)
                goto out;
        lower_file = wrapfs_lower_file(file);
        wrapfs_get_lower_path(dentry, &lower_path);
        err = vfs_fsync_range(lower_file, start, end, datasync);
        wrapfs_put_lower_path(dentry, &lower_path);
out:
        return err;
}

static int wrapfs_fasync(int fd, struct file *file, int flag)
{
        int err = 0;
        struct file *lower_file = NULL;

        lower_file = wrapfs_lower_file(file);
        if (lower_file->f_op && lower_file->f_op->fasync)
                err = lower_file->f_op->fasync(fd, lower_file, flag);

        return err;
}

const struct file_operations wrapfs_main_fops = {
        .llseek         = generic_file_llseek,
        .read           = wrapfs_read,
        .write          = wrapfs_write,
        .unlocked_ioctl = wrapfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
        .compat_ioctl   = wrapfs_compat_ioctl,
#endif
        .mmap           = wrapfs_mmap,
        .open           = wrapfs_open,
        .flush          = wrapfs_flush,
        .release        = wrapfs_file_release,
        .fsync          = wrapfs_fsync,
        .fasync         = wrapfs_fasync,
};

/* trimmed directory options */
const struct file_operations wrapfs_dir_fops = {
        .llseek         = generic_file_llseek,
        .read           = generic_read_dir,
        .readdir        = wrapfs_readdir,
        .unlocked_ioctl = wrapfs_unlocked_ioctl,
#ifdef CONFIG_COMPAT
        .compat_ioctl   = wrapfs_compat_ioctl,
#endif
        .open           = wrapfs_open,
        .release        = wrapfs_file_release,
        .flush          = wrapfs_flush,
        .fsync          = wrapfs_fsync,
        .fasync         = wrapfs_fasync,
};
