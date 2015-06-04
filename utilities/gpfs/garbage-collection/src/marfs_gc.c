#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <gpfs.h>
#include <ctype.h>
#include <unistd.h>
#include "marfs_gc.h"

/******************************************************************************
* This program reads gpfs inodes and extended attributes in order to provide
* a total size value to the fsinfo file.  It is meant to run as a regularly
* scheduled batch job.
*
* Features to be added/to do:
* 
*  determine extended attributes that we care about
*     passed in as args or hard coded?
*  determine arguments to main 
*  inode total count
*  block (512 bytes) held by file
*
*
******************************************************************************/
char    *ProgName;
int debug = 0;

/*****************************************************************************
Name: main

*****************************************************************************/

int main(int argc, char **argv) {
   FILE *outfd;
   int ec;
   char *outf = NULL;
   char *rdir = NULL;
   //unsigned int uid = 0;
   int fileset_id = -1;
   int  c;
   unsigned int fileset_count = 1;
   extern char *optarg;
   fileset_stat *fileset_stat_ptr;
//   char * fileset_name = "root,proja,projb";
//   char  fileset_name[] = "root,proja,projb";
//   char  fileset_name[] = "project_a,projb,root";
//   char  fileset_name[] = "project_a,root,projb";
//   char  fileset_name[] = "trash";
   char  fileset_name[] = "project_a";

   if ((ProgName = strrchr(argv[0],'/')) == NULL)
      ProgName = argv[0];
   else
      ProgName++;

   while ((c=getopt(argc,argv,"d:f:ho:u:")) != EOF) {
      switch (c) {
         case 'd': rdir = optarg; break;
         case 'o': outf = optarg; break;
         //case 'u': uid = atoi(optarg); break;
         case 'h': print_usage();
         default:
            exit(0);
      }
   }
   
   if (rdir == NULL || outf == NULL) {
      fprintf(stderr,"%s: no directory (-d) or output file name (-o) specified\n",ProgName);
      exit(1);
   }
   /*
    * Now assuming that the config file has a list of filesets (either name or path or both).
    * I will make a call to get a list of filesets and the count.
    * I will use the count to malloc space for a array of strutures count size.
    * I will malloc an array of ints count size that will map fileset id to array index 
   */
   // Get list of filesets and count

   int *fileset_id_map;
   fileset_id_map = (int *) malloc(sizeof(int)*fileset_count); 
   fileset_stat_ptr = (fileset_stat *) malloc(sizeof(*fileset_stat_ptr)*fileset_count);
   if (fileset_stat_ptr == NULL || fileset_id_map == NULL) {
      fprintf(stderr,"Memory allocation failed\n");
      exit(1);
   }
   init_records(fileset_stat_ptr, fileset_count);
   strcpy(fileset_stat_ptr[0].fileset_name, fileset_name);

   outfd = fopen(outf,"w");
   // Add filsets to structure so that inode scan can update fileset info
   ec = read_inodes(rdir,outfd,fileset_id,fileset_stat_ptr,fileset_count);
   return (0);   
}

/***************************************************************************** 
Name: init_records 

*****************************************************************************/
void init_records(fileset_stat *fileset_stat_buf, unsigned int record_count)
{
   memset(fileset_stat_buf, 0, (size_t)record_count * sizeof(fileset_stat)); 
}

/***************************************************************************** 
Name: print_usage 

*****************************************************************************/
void print_usage()
{
   fprintf(stderr,"Usage: %s -d gpfs_path -o ouput_log_file [-f fileset_id]\n",ProgName);
}



/***************************************************************************** 
Name:  get_xattr_value

This function, given the name of the attribute, returns the associated value.

*****************************************************************************/
int get_xattr_value(struct marfs_xattr *xattr_ptr, const char *desired_xattr, int cnt) {

   int i;
   int ret_value = -1;

   for (i=0; i< cnt; i++) {
      if (!strcmp(xattr_ptr->xattr_name, desired_xattr)) {
         return(i);
      }
      else {
         xattr_ptr++;
      }
   }
   return(ret_value);
}

/***************************************************************************** 
Name:  get_xattrs

This function fills the xattr struct with all xattr key value pairs

*****************************************************************************/
int get_xattrs(gpfs_iscan_t *iscanP,
                 const char *xattrP,
                 unsigned int xattrLen,
                 const char * desired_xattr,
                 struct marfs_xattr *xattr_ptr) {
   int rc;
   int i;
   const char *nameP;
   const char *valueP;
   unsigned int valueLen;
   const char *xattrBufP = xattrP;
   unsigned int xattrBufLen = xattrLen;
   int printable;
   int xattr_count =0;

   /*  Loop through attributes */
   while ((xattrBufP != NULL) && (xattrBufLen > 0)) {
      rc = gpfs_next_xattr(iscanP, &xattrBufP, &xattrBufLen,
                          &nameP, &valueLen, &valueP);
      if (rc != 0) {
         rc = errno;
         fprintf(stderr, "gpfs_next_xattr: %s\n", strerror(rc));
         return(-1);
      }
      if (nameP == NULL)
         break;

      // keep track of how many xattrs found 
      //xattr_count++;
//      if (!strcmp(nameP, desired_xattr)) {
          strcpy(xattr_ptr->xattr_name, nameP);
          xattr_count++;
//      }

/******* NOT SURE ABOUT THIS JUST YET
      Eliminate gpfs.dmapi attributes for comparision
        Internal dmapi attributes are filtered from snapshots 
      if (printCompare > 1)
      {
         if (strncmp(nameP, "gpfs.dmapi.", sizeof("gpfs.dmapi.")-1) == 0)
            continue;
      }
***********/
    
      if (valueLen > 0) {
         printable = 0;
         if (valueLen > 1) {
            printable = 1;
            for (i = 0; i < (valueLen-1); i++)
               if (!isprint(valueP[i]))
                  printable = 0;
            if (printable) {
               if (valueP[valueLen-1] == '\0')
                  valueLen -= 1;
               else if (!isprint(valueP[valueLen-1]))
                  printable = 0;
            }
         }

         for (i = 0; i < valueLen; i++) {
            if (printable) {
              xattr_ptr->xattr_value[i] = valueP[i]; 
            }
         }
         xattr_ptr->xattr_value[valueLen] = '\0'; 
      }
      xattr_ptr++;
   } // endwhile
   return(xattr_count);
}

/***************************************************************************** 
Name:  clean_exit

This function closes gpfs-related inode information and file handles

*****************************************************************************/

int clean_exit(FILE *fd, gpfs_iscan_t *iscanP, gpfs_fssnap_handle_t *fsP, int terminate) {
   if (iscanP)
      gpfs_close_inodescan(iscanP); /* close the inode file */
   if (fsP)
      gpfs_free_fssnaphandle(fsP); /* close the filesystem handle */
   fclose(fd);
   if (terminate) 
      exit(0);
   else 
      return(0);
}

/***************************************************************************** 
Name: read_inodes 

This function opens an inode scan in order to provide size/block information
as well as file extended attribute information

*****************************************************************************/
int read_inodes(const char *fnameP, FILE *outfd, int fileset_id,fileset_stat *fileset_stat_ptr, size_t rec_count) {
   int rc = 0;
   const gpfs_iattr_t *iattrP;
   const char *xattrBP;
   unsigned int xattr_len; 
   register gpfs_iscan_t *iscanP = NULL;
   gpfs_fssnap_handle_t *fsP = NULL;
   struct marfs_xattr mar_xattrs[MAX_MARFS_XATTR];
   struct marfs_xattr *xattr_ptr = mar_xattrs;
   int xattr_count;
   char fileset_name_buffer[32];

   const char *xattr_objid_name = "user.marfs_objid";
   const char *xattr_post_name = "user.marfs_post";
   MarFS_XattrPost post;
   //const char *xattr_post_name = "user.a";
  
   int early_exit =0;
   int xattr_index;

   //outfd = fopen(onameP,"w");

   /*
    *  Get the unique handle for the filesysteme
   */
   if ((fsP = gpfs_get_fssnaphandle_by_path(fnameP)) == NULL) {
      rc = errno;
      fprintf(stderr, "%s: line %d - gpfs_get_fshandle_by_path: %s\n", 
              ProgName,__LINE__,strerror(rc));
      early_exit = 1;
      clean_exit(outfd, iscanP, fsP, early_exit);
   }

   /*
    *  Open the inode file for an inode scan with xattrs
   */
  //if ((iscanP = gpfs_open_inodescan(fsP, NULL, NULL)) == NULL) {
   if ((iscanP = gpfs_open_inodescan_with_xattrs(fsP, NULL, -1, NULL, NULL)) == NULL) {
      rc = errno;
      fprintf(stderr, "%s: line %d - gpfs_open_inodescan: %s\n", 
      ProgName,__LINE__,strerror(rc));
      early_exit = 1;
      clean_exit(outfd, iscanP, fsP, early_exit);
   }


   while (1) {
      rc = gpfs_next_inode_with_xattrs(iscanP,0x7FFFFFFF,&iattrP,&xattrBP,&xattr_len);
      //rc = gpfs_next_inode(iscanP, 0x7FFFFFFF, &iattrP);
      if (rc != 0) {
         rc = errno;
         fprintf(stderr, "gpfs_next_inode: %s\n", strerror(rc));
         early_exit = 1;
         clean_exit(outfd, iscanP, fsP, early_exit);
      }
      // Are we done?
      if ((iattrP == NULL) || (iattrP->ia_inode > 0x7FFFFFFF))
         break;

      // Determine if invalid inode error 
      if (iattrP->ia_flags & GPFS_IAFLAG_ERROR) {
         fprintf(stderr,"%s: invalid inode %9d (GPFS_IAFLAG_ERROR)\n", ProgName,iattrP->ia_inode);
         continue;
      } 

      // If fileset_id is specified then only look for those inodes and xattrs
      if (fileset_id >= 0) {
         if (fileset_id != iattrP->ia_filesetid){
            continue; 
         }
      }

      // Print out inode values to output file
      // This is handy for debug at the moment
      if (iattrP->ia_inode != 3) {	/* skip the root inode */
         if (debug) { 
            fprintf(outfd,"%u|%lld|%lld|%d|%d|%u|%u|%u|%u|%u|%lld|%d\n",
            iattrP->ia_inode, iattrP->ia_size,iattrP->ia_blocks,iattrP->ia_nlink,iattrP->ia_filesetid,
            iattrP->ia_uid, iattrP->ia_gid, iattrP->ia_mode,
            iattrP->ia_atime.tv_sec,iattrP->ia_mtime.tv_sec, iattrP->ia_blocks, iattrP->ia_xperm );
         }
         gpfs_igetfilesetname(iscanP, iattrP->ia_filesetid, &fileset_name_buffer, 32); 
         if (!strcmp(fileset_name_buffer,fileset_stat_ptr[0].fileset_name)) {



         // Do we have extended attributes?
         // This will be modified as time goes on - what xattrs do we care about
            if (iattrP->ia_xperm == 2 && xattr_len >0 ) {
               xattr_ptr = &mar_xattrs[0];
               if ((xattr_count = get_xattrs(iscanP, xattrBP, xattr_len, xattr_post_name, xattr_ptr)) > 0) {
                  xattr_ptr = &mar_xattrs[0];
                  if ((xattr_index=get_xattr_value(xattr_ptr, xattr_post_name, xattr_count)) != -1 ) { 
                     xattr_ptr = &mar_xattrs[xattr_index];
                     fprintf(outfd,"post xattr name = %s value = %s count = %d\n",xattr_ptr->xattr_name, xattr_ptr->xattr_value, xattr_count);
                     str_2_post(&post, xattr_ptr); 
                  }
                  //str_2_post(&post, xattr_ptr); 
                  // Talk to Jeff about this filespace used not in post xattr
                  if (debug) 
                     printf("found post chunk info bytes %zu\n", post.chunk_info_bytes);
                  if (!strcmp(post.gc_path, "0")){
                     if (debug) 
			// why would this ever happen?  if in trash gc_path should be non-null
                        printf("gc_path is NULL\n");
                  } 
                  else {
                     xattr_ptr = &mar_xattrs[0];
                     if ((xattr_index=get_xattr_value(xattr_ptr, xattr_objid_name, xattr_count)) != -1) { 
                        xattr_ptr = &mar_xattrs[xattr_index];
                        fprintf(outfd,"objid xattr name = %s xattr_value =%s\n",xattr_ptr->xattr_name, xattr_ptr->xattr_value);
                     
                        //call aws delete_object (xattr_ptr->value);
                        //call unlink(gc_path)
                     }


                     // So trash will have two files  associated for the original mds file
		     // The first being inodenumber.datetimestamp.metadata
		     // The second being inodenumber.datetimestamp.path
		     // The *metadata file will have a post xattr with prefix inodenumber.datetimestamp
		     // So read the post xattr and get gc_path and prefix. 
		     // get object name from objid xattr?
		     // Use prefix to form string for removal of *.metadata and *.path
		     // Use gc_path to form string for removal of mdsfile
		     // Use object name to remove oject.
		     //
                     // use gc_path to delete mds stuff.  Or do I open *.path file to get path?
		     //
		     //
                     // Get pre xattr which defines object name
			// xattr_ptr now has name of object to delete
			// Call aws/S3 to delete object
                     // use gc_path to delete mds stuff.  Or do I open *.path file to get path?
		     // delete original mds file and delete trash directory stuff 
                  }
               }
            }
         }
      }
   } // endwhile
   clean_exit(outfd, iscanP, fsP, early_exit);
   return(rc);
}

/***************************************************************************** 
Name: str_2_post 

 parse an xattr-value string into a MarFS_XattrPost

*****************************************************************************/
int str_2_post(MarFS_XattrPost* post, struct marfs_xattr * post_str) {

   int   major;
   int   minor;

   char  obj_type_code;
   if (debug)
      printf("%s\n", post_str->xattr_value);
   // --- extract bucket, and some top-level fields
   int scanf_size = sscanf(post_str->xattr_value, MARFS_POST_FORMAT,
                           &major, &minor,
                           &obj_type_code,
                           &post->obj_offset,
                           &post->num_objects,
                           &post->chunk_info_bytes,
                           &post->correct_info,
                           &post->encrypt_info,
                           (char*)&post->gc_path);

   if (scanf_size == EOF)
      return -1;                // errno is set
   else if (scanf_size != 8) {
      errno = EINVAL;
      return -1;            /* ?? */
   }
   return 0;
}

