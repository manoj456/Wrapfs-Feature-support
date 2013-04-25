#include<linux/xattr.h>
#include "wrapfs.h"
#include<linux/string.h>
#include <linux/linkage.h>
#include <linux/moduleloader.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/scatterlist.h>
#include <linux/crypto.h>
#include <linux/fs.h>
#include<linux/file.h>
#include<linux/fsnotify.h>
int compute_checksum(struct file *,void *);

ssize_t wrapfs_getxattr(struct dentry *dentry, const char *name, void *value,
                         size_t size)
{

        struct dentry *lower_dentry = NULL;
        //struct dentry *parent;
        struct path lower_path;
        int err = -EOPNOTSUPP;
        wrapfs_get_lower_path(dentry,&lower_path);
        lower_dentry=lower_path.dentry;
        //bool valid;
        //wrapfs_read_lock(dentry->d_sb, WRAPFS_SMUTEX_CHILD);
        //parent = lock_parent(lower_dentry);
        //spin_lock(&WRAPFS_D(dentry)->lock);

        //valid = __wrapfs_d_revalidate(dentry, parent, false);
        //if (unlikely(!valid)) {
        //      err = -ESTALE;
        //      goto out;
        //}

        //lower_dentry = wrapfs_lower_dentry(dentry);

        err = vfs_getxattr(lower_dentry, (char *) name, value, size);

        //printk("\nERR VALUE=%d",err);
//out:

        //wrapfs_check_dentry(dentry);
        //spin_unlock(&WRAPFS_D(dentry)->lock);
        //unlock_dir(parent);
        //wrapfs_read_unlock(dentry->d_sb);
        wrapfs_put_lower_path(dentry, &lower_path);
        return err;

}

/*
 * BKL held by caller.
 * dentry->d_inode->i_mutex locked
 */
int wrapfs_setxattr(struct dentry *dentry, const char *name,
                     const void *value, size_t size, int flags)
{
        int err1,n_bytes,err,err2;
        //err1= -EOPNOTSUPP;
        //err= -EOPNOTSUPP;
        //err2= -EOPNOTSUPP;
        /*if(current_cred()->uid <0){
                printk("dir dir dir");
                return -EOPNOTSUPP;
        }*/
        //int err1,n_bytes;
        //err1 = -EOPNOTSUPP;

        //printk("\n I AM IN SETXATTR asdfadsfsafsad\n");

        struct file *lower_file=NULL;
        struct dentry *lower_dentry = NULL;
        //struct dentry *parent;
        struct path lower_path;
        //struct file *file;

        char *buf=kmalloc(16*sizeof(char),GFP_KERNEL);
        char *buf1=kmalloc(16*sizeof(char),GFP_KERNEL);

         err1= -EOPNOTSUPP;
         err= -EOPNOTSUPP;
         err2= -EOPNOTSUPP;


        //char *finalbuf=kmalloc(sizeof(char)*32,GFP_KERNEL);
         if(current_cred()->uid <0){
                printk("\nonly root access\n");
                return -EOPNOTSUPP;
           }

        if(strcmp(name,"user.integrity_val")==0){
                kfree(buf);
                kfree(buf1);
                printk("\nPERMISSION DENIED\n");
                return err;
        }
        wrapfs_get_lower_path(dentry,&lower_path);
        lower_dentry=lower_path.dentry;

        //spin_lock(&WRAPFS_D(dentry)->lock);
//wrapfs_get_lower_path(file->f_path.dentry, &lower_path);
        lower_file=dentry_open(lower_path.dentry,lower_path.mnt,flags,
                                current_cred());
        //bool valid;

        //wrapfs_read_lock(dentry->d_sb, WRAPFS_SMUTEX_CHIL);
        //parent = lock_parent(lower_dentry);//WRAPFS_DMUTEX_PARENT);
        //wrapfs_lock_dentry(dentry);// WRAPFS_DMUTEX_CHILD);
        //spin_lock(&WRAPFS_D(dentry)->lock);
        //valid = __wrapfs_d_revalidate(dentry, parent, false);
        //if (unlikely(!valid)) {
        //      err = -ESTALE;
        //      goto out;
        //}

        //lower_dentry = wrapfs_lower_dentry(dentry);

        //err = vfs_setxattr(lower_dentry, (char *) name, (void *) value,
        //                 size, flags);


        if(strcmp(name,"user.has_integrity")==0 ){

                if(size >1){



                        printk("\nvalue is greater than 1 byte\n");
                      goto out;
                }

                else{

                if(*((char *)value)=='1'){

                        if (S_ISDIR(dentry->d_inode->i_mode)){
                                //err=-EPERM;

                 //err=vfs_setxattr(lower_dentry, (char *) name, (void *) value,
                   //        size, flags);

                                printk("\nIts a directory block1\n");
                                goto out_dir;
                        }
        //printk("\nUSER.HAS_INTEGRITY has value=%c\n",*((char *)value));
                        n_bytes=compute_checksum(lower_file,buf);
                //bin_to_hex(buf,finalbuf,32);
                        /*printk("\nBUF CHECKSUM\n");
                        for(i=0;i<16;i++){

                                printk("%02x",((char *)buf)[i] & 0x0FF);
                        }*/

        err1=vfs_setxattr(lower_dentry,"user.integrity_val",buf,16,flags);
                }
                else if(*((char *)value)=='0'){

                   err1=vfs_getxattr(lower_dentry,"user.has_integrity",buf1,16);

                printk("\nBUF1 value is %c",*((char *)buf1));
                        if(*((char *)buf1)=='1'){

                                if (S_ISDIR(dentry->d_inode->i_mode)){
                                //err=-EPERM;
                                printk("\nIts a directory block2\n");
                                goto out_dir;
                                }


                        err2=vfs_removexattr(lower_dentry,"user.integrity_val");

                        }
                }

                else{

                        printk("\nvalue is 1 byte but not 0 or 1");


                        goto out;

                }
           }
        }
         //printk("\nERR VALUE=%d\tFILENAME=%s\t",err,lower_dentry->d_name.name);
out_dir:
        err = vfs_setxattr(lower_dentry, (char *) name, (void *) value,
                           size, flags);
//      printk("\nErr value is %d\n",err);
out:
        //spin_unlock(&WRAPFS_D(dentry)->lock);
        //wrapfs_check_dentry(dentry);
        //wrapfs_unlock_dentry(dentry);
        //unlock_dir(parent);
        //wrapfs_read_unlock(dentry->d_sb);
        wrapfs_put_lower_path(dentry, &lower_path);

        kfree(buf);
        kfree(buf1);
        return err;

}
ssize_t wrapfs_listxattr(struct dentry *dentry, char *list, size_t size)
{
        struct dentry *lower_dentry = NULL;
        //struct dentry *parent;
        int err = -EOPNOTSUPP;
        char *encoded_list = NULL;
        //bool valid;
        struct path lower_path;
        wrapfs_get_lower_path(dentry,&lower_path);
        lower_dentry=lower_path.dentry;
        //unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
        //parent = lock_parent(lower_dentry);//, UNIONFS_DMUTEX_PARENT);
        //unionfs_lock_dentry(dentry, UNIONFS_DMUTEX_CHILD);
        //spin_lock(&WRAPFS_D(dentry)->lock);
        //valid = __unionfs_d_revalidate(dentry, parent, false);
        /*if (unlikely(!valid)) {
                err = -ESTALE;
                goto out;
        }
        */
        //lower_dentry = unionfs_lower_dentry(dentry);

        encoded_list = list;
        err = vfs_listxattr(lower_dentry, encoded_list, size);

//out:
        //unionfs_check_dentry(dentry);
        //spin_unlock(&WRAPFS_D(dentry)->lock);
        //unlock_dir( parent);
        wrapfs_put_lower_path(dentry,&lower_path);
        //unionfs_read_unlock(dentry->d_sb);
        return err;
}
int wrapfs_removexattr(struct dentry *dentry, const char *name)
{
        /*if(current_cred()->uid <0){
                return -EOPNOTSUPP;
        }

         if(strcmp(name,"user.integrity_val")==0){

                printk("\nPERMISSION DENIED");
                return -EOPNOTSUPP;
        }*/

        //struct dentry *lower_dentry = NULL;
        //struct dentry *parent;
        int err,e1;
        //err  = -EOPNOTSUPP;
        //e1 = -EOPNOTSUPP;
        //bool valid;
        struct path lower_path;
        struct dentry *lower_dentry = NULL;

         err  = -EOPNOTSUPP;
        e1 = -EOPNOTSUPP;

         if(current_cred()->uid <0){
                return -EOPNOTSUPP;
        }

         if(strcmp(name,"user.integrity_val")==0){

                printk("\nPERMISSION DENIED");
                return -EOPNOTSUPP;
        }




        wrapfs_get_lower_path(dentry,&lower_path);
        lower_dentry=lower_path.dentry;

        //unionfs_read_lock(dentry->d_sb, UNIONFS_SMUTEX_CHILD);
        //parent = lock_parent(lower_dentry);//, UNIONFS_DMUTEX_PARENT);
        //spin_lock(&WRAPFS_D(dentry)->lock);//, UNIONFS_DMUTEX_CHILD);

        /*valid = __unionfs_d_revalidate(dentry, parent, false);
        if (unlikely(!valid)) {
                err = -ESTALE;
                goto out;
        }

        lower_dentry = unionfs_lower_dentry(dentry);
*/
        if(strcmp(name,"user.has_integrity")==0){


        e1=vfs_removexattr(lower_dentry,"user.integrity_val");


        }
        err = vfs_removexattr(lower_dentry, (char *) name);

//out:
        //unionfs_check_dentry(dentry);
        //spin_unlock(&WRAPFS_D(dentry)->lock);
        //unlock_dir(parent);
        wrapfs_put_lower_path(dentry,&lower_path);
        //unionfs_unlock_parent(dentry, parent);
        //unionfs_read_unlock(dentry->d_sb);
        return err;
}


int compute_checksum(struct file *filp,void *buffer){

                int bytes,rc=0,ret=0;
                struct hash_desc desc;
                struct scatterlist sg;
                mm_segment_t oldfs;
                //void *buffer=kmalloc(16,GFP_KERNEL);
                filp->f_pos = 0;                /* start offset */
                oldfs = get_fs();
                set_fs(KERNEL_DS);
                desc.flags = 0;
                desc.tfm = crypto_alloc_hash("md5", 0, CRYPTO_ALG_ASYNC);
                rc = crypto_hash_init(&desc);
                if (rc) {
                         printk(KERN_ERR
                                "%s: Error initializing crypto hash; rc = [%d]\n",
                                __func__, rc);
                        return ret;
                }
 while((bytes = filp->f_op->read(filp, buffer,16, &filp->f_pos))){
                        sg_init_one(&sg, (u8 *)buffer, 16);
                        rc = crypto_hash_update(&desc, &sg, bytes);
                        //printk("DATA: %s  BYTES: %d\n", (char *)buffer, bytes);
                        //ret= ecryptfs_calculate_md5(myarg->ibuf,buffer,20);
                }
                rc = crypto_hash_final(&desc, buffer);
                set_fs(oldfs);
                /*printk("\nIN Fun buffer checksum\n");
                 for(i=0;i<16;i++){
                        printk("%02x",((char *)buffer)[i] & 0x0FF);
                }*/
return bytes;

}
