/*  SAPfs
 *  Version 0.9.6
 *  Copyright (C) 2001-2003 Eric Botcazou
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <libintl.h>
#include <locale.h>
#include "libsap.h"

#define _(String) gettext (String)
#define gettext_noop(String) String
#define N_(String) gettext_noop(String)
#define Q(x) #x
#define QUOTE(x) Q(x)


#define SAPFS_VERSION_STR "0.9.6"


#ifdef linux
   #define SAPFS_PLATFORM_STR "Linux"
#else
   #define SAPFS_PLATFORM_STR "MSDOS"
#endif

#define FILENAME_LENGTH 512


/* PrintErrorMessage:
 *  Prints the error message corresponding to the specified error.
 */
static void PrintErrorMessage(int errno, const char str[])
{
   switch (errno) {

      case SAP_EBADF:
         fprintf(stderr, _("Error: file %s is not a valid SAP archive\n"), str);
         break;

      case SAP_EFBIG:
         fprintf(stderr, _("Error: file %s is too large\n"), str);
         break;

      case SAP_ENFILE:
         fprintf(stderr, _("Error: file %s is empty\n"), str);
         break;

      case SAP_ENOENT:
         fprintf(stderr, _("Error: can't find file %s\n"), str);
         break;

      case SAP_ENOSPC:
         fprintf(stderr, _("Error: the archive directory is full\n"));
         break;

      case SAP_EPERM:
         fprintf(stderr, _("Error: can't create file %s.\n"), str);
         break;
   }
}



/* put_separator:
 *  Helper function to add a missing directory separator.
 */
static void put_separator(char filename[])
{
   int last;

   last = strlen(filename) - 1;

   if (filename[last] != '/')
      filename[last + 1] = '/';
}



/* CreateEmptyArchive:
 *  Creates a new archive using the specified name.
 */
static int CreateEmptyArchive(const char sap_name[], int format, int capacity)
{
   sapID sap_file;

   if ((sap_file=sap_CreateArchive(sap_name, format)) == SAP_ERROR) {
      PrintErrorMessage(sap_errno, sap_name);
      return 1;
   }

   sap_FormatArchive(sap_file, capacity);
   sap_CloseArchive(sap_file);

   return 0;
}



/* FormatArchive:
 *  Formats the specified already existing archive.
 */
static int FormatArchive(const char sap_name[], int capacity)
{
   sapID sap_file;
   int format;

   if ((sap_file=sap_OpenArchive(sap_name, &format)) == SAP_ERROR) {
      PrintErrorMessage(sap_errno, sap_name);
      return 1;
   }

   sap_FormatArchive(sap_file, capacity);
   sap_CloseArchive(sap_file);

   return 0;
}



/* VerifyArchive:
 *  Verifies one or more sectors from the specified archive.
 */
static int VerifyArchive(const char sap_name[], int track, int sect)
{
   sapsector_t sapsector;
   sapID sap_file;
   int format, ntracks, s, t, flag;

   if ((sap_file=sap_OpenArchive(sap_name, &format)) == SAP_ERROR) {
      PrintErrorMessage(sap_errno, sap_name);
      return 1;
   }

   ntracks = (format == SAP_FORMAT1 ? SAP_NTRACKS1 : SAP_NTRACKS2);

   /* check track number */
   if (track<0) {
      t = 0;
      track = ntracks-1;
   }
   else {
      if (track>=ntracks) {
         fprintf(stderr, _("Error: illegal track number\n"));
         sap_CloseArchive(sap_file);
         return 1;
      }

      t = track;
   }

   /* check sector number */
   if (sect<0) {
      s = 1;
      sect = SAP_NSECTS;
   }
   else {
      if ((sect<1) || (sect>SAP_NSECTS)) {
         fprintf(stderr, _("Error: illegal sector number\n"));
         sap_CloseArchive(sap_file);
         return 1;
      }

      s = sect;
   }

   while (t <= track) {
      while (s <= sect) {
         flag = sap_ReadSector(sap_file, t, s, &sapsector);

         if (flag != SAP_OK) {
            printf(_("track %d sector %02d: "), t, s);

            if (flag & SAP_NO_STD_FMT)
               printf(_("<format=%d> "), sapsector.format);

            if (flag & SAP_PROTECTED)
               printf(_("<protection=%d> "), sapsector.protection);

            if (flag & SAP_BAD_SECTOR)
               printf(_("<track=%d sector=%d> "), sapsector.track, sapsector.sector);

            if (flag & SAP_CRC_ERROR)
               printf(_("<CRC error>"));

            printf("\n");
         }

         s++;
      }  /* end of while (s <= sect) */

      s = 1;
      t++;
   }  /* end of while (t <= track ) */

   sap_CloseArchive(sap_file);

   return 0;
}



/* DumpArchive:
 *  Dumps one or more sectors from the specified archive.
 */
static int DumpArchive(const char sap_name[], int track, int sect)
{
   sapsector_t sapsector;
   unsigned char c;
   sapID sap_file;
   int format, ntracks, i,j,s,t;

   if ((sap_file=sap_OpenArchive(sap_name, &format)) == SAP_ERROR) {
      PrintErrorMessage(sap_errno, sap_name);
      return 1;
   }

   ntracks = (format == SAP_FORMAT1 ? SAP_NTRACKS1 : SAP_NTRACKS2);

   /* check track number */
   if (track<0) {
      t = 0;
      track = ntracks-1;
   }
   else {
      if (track>=ntracks) {
         fprintf(stderr, _("Error: illegal track number\n"));
         sap_CloseArchive(sap_file);
         return 1;
      }

      t = track;
   }

   /* check sector number */
   if (sect<0) {
      s = 1;
      sect = SAP_NSECTS;
   }
   else {
      if ((sect<1) || (sect>SAP_NSECTS)) {
         fprintf(stderr, _("Error: illegal sector number\n"));
         sap_CloseArchive(sap_file);
         return 1;
      }

      s = sect;
   }

   while (t <= track) {
      while (s <= sect) {
         sap_ReadSector(sap_file, t, s, &sapsector);

         printf("track id: %02d\n", sapsector.track);
         printf("sector id: %02d\n", sapsector.sector);
         printf("format: %d\n", sapsector.format);
         printf("protection: %d\n", sapsector.protection);
         printf("data:\n");

         for (i=0; i< (format == SAP_FORMAT1 ? 16 : 8); i++) {
            for (j=0; j<16; j++) {
               c = sapsector.data[i*16+j];
               printf("%02X ", c);
            }

            for (j=0; j<16; j++) {
               c = sapsector.data[i*16+j];

               if ((c>=32) && (c<=125))
                  putchar(c);
               else
                  putchar('.');
            }

            printf("\n");
         }

         s++;
      }  /* end of while (s <= sect) */

      s = 1;
      t++;
   }  /* end of while (t <= track ) */

   sap_CloseArchive(sap_file);

   return 0;
}



/* ListArchive:
 *  Lists the files contained in the specified archive.
 */
static int ListArchive(const char sap_name[])
{
   char buffer[4096];
   sapID sap_file;
   int format;

   if ((sap_file=sap_OpenArchive(sap_name, &format)) == SAP_ERROR) {
      PrintErrorMessage(sap_errno, sap_name);
      return 1;
   }

   sap_ListArchive(sap_file, buffer, sizeof(buffer));
   puts(buffer);
   sap_CloseArchive(sap_file);

   return 0;
}



/* PrintFileInfo:
 *  Prints the info for the specified file.
 */
static int PrintFileInfo(const char sap_name[], char filename[])
{
   int format, i, ret = 0;
   sapID sap_file;
   sapfileinfo_t info;
   char tmp[64];

   if ((sap_file=sap_OpenArchive(sap_name, &format)) == SAP_ERROR) {
      PrintErrorMessage(sap_errno, sap_name);
      return 1;
   }

   if (sap_GetFileInfo(sap_file, filename, &info) == SAP_ERROR) {
      PrintErrorMessage(sap_errno, filename);
      ret = 1;
   }
   else {
      printf(_("name: %s\n"), filename);
      printf(_("size: %d bytes\n"), info.size);
      printf(_("file type: %d\n"), info.file_type);
      printf(_("data type: %d\n"), info.data_type);

      if (info.date) {
         strftime(tmp, sizeof(tmp), "%m/%d/%Y %H:%M ", localtime(&info.date));
         printf(_("date: %s\n"), tmp);
      }

      if (info.comment[0])
         printf(_("comment: %s\n"), info.comment);

      printf(_("blocks: "));
      for (i=0; i<info.nblocks; i++)
         printf("%d ", info.block[i]);
      printf("\n");

      free(info.block);
   }

   sap_CloseArchive(sap_file);

   return ret;
}



/* ExtractFile:
 *  Extracts one or more files from the specified archive.
 */
static int ExtractFile(const char sap_name[], char *filename[], int nfiles)
{
   int format, i, len, ret = 0;
   sapID sap_file;

   if ((sap_file=sap_OpenArchive(sap_name, &format)) == SAP_ERROR) {
      PrintErrorMessage(sap_errno, sap_name);
      return 1;
   }

   for (i=0; i<nfiles; i++) {
      len = sap_ExtractFile(sap_file, filename[i]);

      if (len == 0) {
         PrintErrorMessage(sap_errno, filename[i]);
         ret = 1;
         break;
      }
   }

   sap_CloseArchive(sap_file);

   return ret;
}



/* AddFile:
 *  Adds one or more files to the specified archive.
 */
static int AddFile(const char sap_name[], char *filename[], int nfiles)
{
   char pathname[FILENAME_LENGTH];
   int format, i, len = 0, ret = 0;
   struct stat s;
   struct dirent *entry;
   sapID sap_file;
   DIR *dir;

   if ((sap_file=sap_OpenArchive(sap_name, &format)) == SAP_ERROR) {
      PrintErrorMessage(sap_errno, sap_name);
      return 1;
   }

   for (i=0; i<nfiles; i++) {
      /* get file statistics */
      if (stat(filename[i], &s) != 0) {
         fprintf(stderr, _("Error: can't find file %s\n"), filename[i]);
         ret = 1;
         break;
      }

      if (S_ISDIR(s.st_mode)) {  /* directory? */
         if ((dir=opendir(filename[i])) == NULL) {
            fprintf(stderr, _("Error: can't read directory %s\n"), filename[i]);
            ret = 1;
            break;
         }

         /* add every entry in turn */
         while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
               strncpy(pathname, filename[i], FILENAME_LENGTH - 1);
               put_separator(pathname);
               strncat(pathname, entry->d_name, FILENAME_LENGTH - strlen(pathname) - 1);
               len = sap_AddFile(sap_file, pathname);

               if (len == 0) {
                  PrintErrorMessage(sap_errno, pathname);
                  ret = 1;
                  break;
               }
            }
         }

         closedir(dir);
      }
      else {
         len = sap_AddFile(sap_file, filename[i]);
      }

      if (len == 0) {
         if (ret == 0) {
            PrintErrorMessage(sap_errno, filename[i]);
            ret = 1;
         }
         break;
      }
   }

   sap_CloseArchive(sap_file);

   return ret;
}



/* DeleteFile:
 *  Deletes one or more files from the specified archive.
 */
static int DeleteFile(const char sap_name[], char *filename[], int nfiles)
{
   int format, i, len, ret = 0;
   sapID sap_file;

   if ((sap_file=sap_OpenArchive(sap_name, &format)) == SAP_ERROR) {
      PrintErrorMessage(sap_errno, sap_name);
      return 1;
   }

   for (i=0; i<nfiles; i++) {
      len = sap_DeleteFile(sap_file, filename[i]);

      if (len == 0) {
         PrintErrorMessage(sap_errno, filename[i]);
         ret = 1;
         break;
      }
   }

   sap_CloseArchive(sap_file);

   return ret;
}



/* CopyArchive:
 *  Copies one or more sectors from the source archive to the dest archive.
 */
static int CopyArchive(const char src_name[], const char dest_name[], int track, int sect)
{
   sapsector_t sapsector;
   sapID src_file, dest_file;
   int src_format, dest_format, ntracks, s, t;

   if ((src_file=sap_OpenArchive(src_name, &src_format)) == SAP_ERROR) {
      PrintErrorMessage(sap_errno, src_name);
      return 1;
   }

   if ((dest_file=sap_OpenArchive(dest_name, &dest_format)) == SAP_ERROR) {
      PrintErrorMessage(sap_errno, dest_name);
      sap_CloseArchive(src_file);
      return 1;
   }

   if (src_format != dest_format) {
      fprintf(stderr, _("Error: different archive formats\n"));
      goto Error;
   }

   ntracks = (src_format == SAP_FORMAT1 ? SAP_NTRACKS1 : SAP_NTRACKS2);

   /* check track number */
   if (track<0) {
      t = 0;
      track = ntracks-1;
   }
   else {
      if (track>=ntracks) {
         fprintf(stderr, _("Error: illegal track number\n"));
         goto Error;
      }

      t = track;
   }

   /* check sector number */
   if (sect<0) {
      s = 1;
      sect = SAP_NSECTS;
   }
   else {
      if ((sect<1) || (sect>SAP_NSECTS)) {
         fprintf(stderr, _("Error: illegal sector number\n"));
         goto Error;
      }

      s = sect;
   }

   while (t <= track) {
      while (s <= sect) {
         sap_ReadSector(src_file, t, s, &sapsector);
         sap_WriteSector(dest_file, t, s, &sapsector);

         s++;
      }  /* end of while (s <= sect) */

      s = 1;
      t++;
   }  /* end of while (t <= track ) */

   sap_CloseArchive(dest_file);
   sap_CloseArchive(src_file);
   return 0;

 Error:
   sap_CloseArchive(src_file);
   sap_CloseArchive(dest_file);
   return 1;
}



/* MoveSector:
 *  Copies with deplacement one sector from the source archive to the dest archive.
 */
static int MoveSector(const char src_name[], int src_track, int src_sect,
                        const char dest_name[], int dest_track, int dest_sect)
{
   sapsector_t sapsector;
   sapID src_file, dest_file;
   int src_format, dest_format, ntracks;

   if ((src_file=sap_OpenArchive(src_name, &src_format)) == SAP_ERROR) {
      PrintErrorMessage(sap_errno, src_name);
      return 1;
   }

   if ((dest_file=sap_OpenArchive(dest_name, &dest_format)) == SAP_ERROR) {
      PrintErrorMessage(sap_errno, dest_name);
      sap_CloseArchive(src_file);
      return 1;
   }

   if (src_format != dest_format) {
      fprintf(stderr, _("Error: different archive formats\n"));
      goto Error;
   }

   ntracks = (src_format == SAP_FORMAT1 ? SAP_NTRACKS1 : SAP_NTRACKS2);

   /* check track number */
   if ((src_track<0)  || (src_track>=ntracks) ||
       (dest_track<0) || (dest_track>=ntracks)) {
      fprintf(stderr, _("Error: illegal track number\n"));
      goto Error;
   }

   /* check sector number */
   if ((src_sect<1)  || (src_sect>SAP_NSECTS) ||
       (dest_sect<1) || (dest_sect>SAP_NSECTS)) {
      fprintf(stderr, _("Error: illegal sector number\n"));
      goto Error;
   }

   sap_ReadSector(src_file, src_track, src_sect, &sapsector);

   sapsector.track = dest_track;
   sapsector.sector = dest_sect;

   sap_WriteSector(dest_file, dest_track, dest_sect, &sapsector);

   sap_CloseArchive(src_file);
   sap_CloseArchive(dest_file);
   return 0;

 Error:
   sap_CloseArchive(src_file);
   sap_CloseArchive(dest_file);
   return 1;
}


#define COMMAND_MAX  14

static char *short_command[] = { "-h", "-v", "-c", "-f", "-w", "-u",
                                 "-t", "-i", "-x", "-y", "-a", "-d", "-k", "-m" };
static char *long_command[] = { "--help", "--version", "--create", "--format", "--verify", "--dump",
                                "--list", "--info", "--extract", "--extract-all", "--add", "--delete",
                                "--copy", "--move" };


/* usage:
 *  Displays the commands and quits.
 */
static void usage(const char prog_name[])
{
   fprintf(stderr, _("Usage: %s -h --help | -v --version | -c --create |  -f --format\n"), prog_name);
   fprintf(stderr, "               -w --verify | -d --dump | -t --list | -i --info\n");
   fprintf(stderr, "               -x --extract[-all] | -a --add | -d --delete\n");
   exit(EXIT_FAILURE);
}



/* main:
 *  Entry point for the program.
 */
int main(int argc, char *argv[])
{
   int i, ret = 0;
   char *star = "*";

   setlocale(LC_ALL, "");
   if (bindtextdomain("sapfs", QUOTE(PREFIX_MO) "/share/locale") == NULL)
      fprintf(stderr, "Error: bindtextdomain() failed\n");
   textdomain("sapfs");

   if (argc < 2)  /* no argument? */
      usage(argv[0]);

   if (argv[1][0] == '-') {

      switch (argv[1][1]) {

         case '-':  /* long commands */
            for (i=0; i<COMMAND_MAX; i++) {
               if (strcmp(argv[1], long_command[i]) == 0) {
                  argv[1] = short_command[i];
                  return main(argc, argv);
               }
            }

            usage(argv[0]);

         case 'h':  /* help */
            printf(_("SAPfs is an SAP archive handling tool to perform common file system\n"
                     "operations on image files\n\n"));
            printf(_("Usage:\n\n"
                   "    %s [-hituvwx] archive.sap [file...] [track] [sector]\n"
                   "    %s -c | -f archive.sap [tracks] [density]\n"
                   "    %s -k archive.sap archive2.sap [track] [sector]\n"
                   "    %s -m archive1.sap track sector archive2.sap track sector\n\n"
                   "  -a, --add           adds one or more files to the SAP archive\n"
                   "  -c, --create        creates an empty SAP archive\n"
                   "  -d, --delete        deletes one or multiple files from the SAP archive\n"
                   "  -f, --format        formats an SAP archive\n"
                   "  -h, --help          shows this help\n"
                   "  -i, --info          displays information about a file\n"
                   "  -k, --copy          copy one or more sectors\n"
                   "  -m, --move          copy a sector with displacement\n"
                   "  -t, --list          show the directory of the SAP archive\n"
                   "  -u, --dump          dump the contents of one or more sectors\n"
                   "  -v, --version       shows the version number of this program\n"
                   "  -w, --verify        verify one or more sectors\n"
                   "  -x, --extract       extracts one or more files from the SAP archive\n"
                   "      --extract-all   extracts all files from the SAP archive\n"),
                     argv[0], argv[0], argv[0], argv[0]);
            break;

         case 'v':  /* version */
            printf(_("SAPfs version %s for %s, Copyright (C) 2001-2003 Eric Botcazou.\n"),
                SAPFS_VERSION_STR, SAPFS_PLATFORM_STR);
            break;

         case 'c':  /* create */
            if (argc < 3)
               usage(argv[0]);

            if (argc < 4) {
               ret = CreateEmptyArchive(argv[2], SAP_FORMAT1, SAP_TRK80);
            }
            else if (argc == 4){
               if (atoi(argv[3]) == 40)
                  ret = CreateEmptyArchive(argv[2], SAP_FORMAT1, SAP_TRK40);
               else if (atoi(argv[3]) == 80)
                  ret = CreateEmptyArchive(argv[2], SAP_FORMAT1, SAP_TRK80);
               else
                  ret = 1;
            }
            else {
               if (atoi(argv[3]) == 40)
                  ret = CreateEmptyArchive(argv[2], (atoi(argv[4]) == 1 ? SAP_FORMAT2 : SAP_FORMAT1), SAP_TRK40);
               else if (atoi(argv[3]) == 80)
                  ret = CreateEmptyArchive(argv[2], (atoi(argv[4]) == 1 ? SAP_FORMAT2 : SAP_FORMAT1), SAP_TRK80);
               else
                  ret = 1;
            }
            break;

         case 'f':  /* format */
            if (argc < 3)
               usage(argv[0]);

            if (argc < 4) {
               ret = FormatArchive(argv[2], SAP_TRK80);
            }
            else {
               if (atoi(argv[3]) == 80)
                  ret = FormatArchive(argv[2], SAP_TRK80);
               else if (atoi(argv[3]) == 40)
                  ret = FormatArchive(argv[2], SAP_TRK40);
               else
                  ret = 1;
            }
            break;

         case 'w':  /* verify */
            if (argc < 3)
               usage(argv[0]);

            if (argc < 4)
               ret = VerifyArchive(argv[2], -1, -1);
            else if (argc < 5)
               ret = VerifyArchive(argv[2], atoi(argv[3]), -1);
            else
               ret = VerifyArchive(argv[2], atoi(argv[3]), atoi(argv[4]));
            break;


         case 'u':  /* dump */
            if (argc < 3)
               usage(argv[0]);

            if (argc < 4)
               ret = DumpArchive(argv[2], -1, -1);
            else if (argc < 5)
               ret = DumpArchive(argv[2], atoi(argv[3]), -1);
            else
               ret = DumpArchive(argv[2], atoi(argv[3]), atoi(argv[4]));
            break;

         case 't':  /* list */
            if (argc < 3)
               usage(argv[0]);

            ret = ListArchive(argv[2]);
            break;

         case 'i':  /* info */
            if (argc < 4)
               usage(argv[0]);

            ret = PrintFileInfo(argv[2], argv[3]);
            break;

         case 'x':  /* extract */
            if (argc < 4)
               usage(argv[0]);

            ret = ExtractFile(argv[2], argv+3, argc-3);
            break;

         case 'y':  /* extract all */
            if (argc < 3)
               usage(argv[0]);

            ret = ExtractFile(argv[2], &star, 1);
            break;

         case 'a':  /* add */
            if (argc < 4)
               usage(argv[0]);

            ret = AddFile(argv[2], argv+3, argc-3);
            break;

         case 'd':  /* delete */
            if (argc < 4)
               usage(argv[0]);

            ret = DeleteFile(argv[2], argv+3, argc-3);
            break;

         case 'k':  /* copy */
            if (argc < 4)
               usage(argv[0]);

            if (argc < 5)
               ret = CopyArchive(argv[2], argv[3], -1, -1);
            else if (argc < 6)
               ret = CopyArchive(argv[2], argv[3], atoi(argv[4]), -1);
            else
               ret = CopyArchive(argv[2], argv[3], atoi(argv[4]), atoi(argv[5]));
            break;

         case 'm':  /* move */
            if (argc != 8)
               usage(argv[0]);

            ret = MoveSector(argv[2], atoi(argv[3]), atoi(argv[4]), argv[5], atoi(argv[6]), atoi(argv[7]));
            break;

         default:
            usage(argv[0]);
      }  /* end of switch */
   }

   return ret;
}

