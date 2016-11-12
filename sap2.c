/*  SAP2
 *  Version 2.2
 *  Copyright (C) 2000-2003 Eric Botcazou
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

/*  version 1.0: programme fonctionnel sous MSDOS et Linux
 *          1.1: retour au format PC à la sortie du programme
 *          1.2: ajout d'un mode de fonctionnement batch
 *          2.0: support des disquettes 5"25 et de deux lecteurs PC
 *          2.1: support de la simple densité
 *          2.2: support de plusieurs langues
 */


#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libintl.h>
#include <locale.h>
#include "libsap.h"
#include "floppy.h"

#define _(String) gettext (String)
#define gettext_noop(String) String
#define N_(String) gettext_noop (String)
#define Q(x) #x
#define QUOTE(x) Q(x)


#define SAP2_VERSION_STR "2.2.0"

#ifdef linux
   #define SAP2_PLATFORM_STR "Linux"
#else
   #define SAP2_PLATFORM_STR "MSDOS"
#endif

#define FILENAME_LENGTH 128



/* pour l'affichage du répertoire */
#define PAGE_HEIGHT 22


static struct floppy_info fi;
static char *drive_type_name[7] = {
  gettext_noop ("not available"),
  gettext_noop ("5.25 - 360 KB"),
  gettext_noop ("5.25 - 1.2 MB"),
  gettext_noop ("3.5 -  720 KB"),
  gettext_noop ("3.5 - 1.44 MB"),
  gettext_noop ("3.5 - 2.88 MB"),
  gettext_noop( "3.5 - 2.88 MB")
};

#define IS_5_INCHES(drive) ((fi.drive_type[drive]>0) && (fi.drive_type[drive]<3))



/* term_puts:
 *  Affiche une chaîne de caractères en respectant la taille du terminal.
 */
static void term_puts(const char buffer[], int lines, int page_height)
{
   const char *start_cur, *end_cur;
   char trash[8];
   int i;

   start_cur = end_cur = buffer;

   while (lines>page_height) {
      for (i=0; i<page_height; i++) {
         while (*end_cur++ != '\n')
            ;
      }

      fwrite(start_cur, sizeof(char), end_cur-start_cur, stdout);

      fgets(trash, 7, stdin);
      printf(_("Press <Return>"));
      getchar();
      printf("\n");
      ungetc('\n', stdin);

      start_cur = end_cur;
      lines -= page_height;
   }

   puts(end_cur);
}



/* ViewDiskDir:
 *  Affiche le répertoire d'une disquette TO.
 */
static int ViewDiskDir(int drive, int density, int page_height)
{
   char data[512*SAP_NSECTS];
   char buffer[4096];
   int lines;

   if (FloppyReadSector(drive, density, 20, 1, SAP_NSECTS, data) != 0)
      return 1;

   /* on réduit la FAT à 80 blocks pour les disquettes 5"25 DD */
   if (IS_5_INCHES(drive) && density == 2)
      memset(data + SAP_SECTSIZE1 + 1 + SAP_NTRACKS1, 0xFE, SAP_NTRACKS1);

   /* fonction bas-niveau de libSAP */
   lines = _ExtractDir(buffer, sizeof(buffer), drive, density, data);

   if ((page_height<0) || (lines<=page_height))
      puts(buffer);
   else
      term_puts(buffer, lines, page_height);

   return 0;
}



/* ViewArchiveDir:
 *  Affiche le répertoire d'une archive SAP.
 */
static int ViewArchiveDir(const char sap_name[], int page_height)
{
   char buffer[4096];
   sapID sap_file;
   int format, lines;

   if ((sap_file=sap_OpenArchive(sap_name, &format)) == SAP_ERROR)
      return 1;

   lines = sap_ListArchive(sap_file, buffer, sizeof(buffer));

   if (lines == 0)
      return 2;

   if ((page_height<0) || (lines<=page_height))
      puts(buffer);
   else
      term_puts(buffer, lines, page_height);

   sap_CloseArchive(sap_file);
   return 0;
}



/* CreateEmptyArchive:
 *  Crée une archive SAP vide (mais formatée).
 */
static int CreateEmptyArchive(const char sap_name[], int format, int capacity)
{
   sapID sap_file;

   if ((sap_file=sap_CreateArchive(sap_name, format)) == SAP_ERROR)
      return 1;

   sap_FormatArchive(sap_file, capacity);
   sap_CloseArchive(sap_file);

   return 0;
}



/* BuildSectorMap:
 *  Construit la carte des secteurs d'une piste
 *  en fonction du facteur d'entrelacement.
 */
static void BuildSectorMap(int *sector_map, int factor)
{
   int sect, loc=0;

   /* mise à zéro de la table */
   memset(sector_map, 0, sizeof(int)*SAP_NSECTS);

   for (sect=1; sect<=SAP_NSECTS; sect++) {
      while (sector_map[loc] != 0)
         loc=(loc+1)%SAP_NSECTS;

      sector_map[loc]=sect;

      loc=(loc+factor)%SAP_NSECTS;
   }
}



/* FormatDisk:
 *  Formate une disquette 3"5 - 720 ko au format Thomson.
 */
static int FormatDisk(int drive, int density, int factor, int verbose)
{
   int num_tracks, sect_size;
   int track, sect, pos, i;
   int sector_map[SAP_NSECTS];
   char header_table[512];
   char buffer[512*SAP_NSECTS];

   /* détermination du nombre de pistes */
   if (IS_5_INCHES(drive))
      num_tracks = SAP_NTRACKS2;
   else
      num_tracks = SAP_NTRACKS1;

   /* et de la taille du secteur */
   if (density == 1)
      sect_size = SAP_SECTSIZE2;
   else
      sect_size = SAP_SECTSIZE1;

   /* construction de la carte des secteurs pour chaque piste,
      à partir du facteur d'entrelacement */
   BuildSectorMap(sector_map, factor);

   /* formatage des pistes */
   for (track=0; track<num_tracks; track++) {
      if (verbose) {
         printf("\rFormatting track %d...", track);
         fflush(stdout);
      }

      /* construction de la table des headers */
      for (sect=1, pos=0; sect<=SAP_NSECTS; sect++, pos+=4) {
         header_table[pos]   = track;
         header_table[pos+1] = 0;
         header_table[pos+2] = sector_map[sect-1];
         header_table[pos+3] = density - 1;
      }

      if (FloppyFormatTrack(drive, density, track, header_table) != 0)
         return 1;
   }

   if (verbose) {
      printf(_("\nCreating file allocation table (track 20)..."));
      fflush(stdout);
   }

   for (i=0; i<sect_size*SAP_NSECTS; i++)
      buffer[i] = 0xFF;

   /* pour MS-DOS (voir teo/src/dos/ddisk.c) */
   buffer[i]=0;

   if (FloppyWriteSector(drive, density, 20, 1, SAP_NSECTS, buffer) != 0)
      return 2;

   buffer[0] = 0;
   buffer[41] = 0xFE;
   buffer[42] = 0xFE;

   /* pour MS-DOS (voir teo/src/dos/ddisk.c) */
   buffer[sect_size] = 0;

   if (FloppyWriteSector(drive, density, 20, 2, 1, buffer) != 0)
      return 2;

   if (verbose)
      printf("\n");

   return 0;
}



/* shrink_fat_160_to_80:
 *  Réduit la taille de la FAT de 160 à 80 blocks.
 */
static void shrink_fat_160_to_80(char fat_data[])
{
   int i;

   for (i=0; i<SAP_NTRACKS1; i++) {
      if (abs_char(fat_data[1 + SAP_NTRACKS1 + i]) != 0xFF)
         return;
   }

   memset(fat_data + 1 + SAP_NTRACKS1, 0xFE, SAP_NTRACKS1);
}



/* PackArchive:
 *  Transfère le contenu d'une disquette TO dans une archive SAP.
 */
static int PackArchive(const char sap_name[], int drive, int density, int verbose)
{
   int num_tracks, sect_size;
   int track, sect;
   sapID sap_file;
   sapsector_t sapsector;
   char buffer[512*SAP_NSECTS];

   if ((sap_file=sap_CreateArchive(sap_name, density == 1 ? SAP_FORMAT2 : SAP_FORMAT1)) == SAP_ERROR)
      return 1;

   /* détermination du nombre de pistes */
   if (IS_5_INCHES(drive))
      num_tracks = SAP_NTRACKS2;
   else
      num_tracks = SAP_NTRACKS1;

   /* et de la taille du secteur */
   if (density == 1)
      sect_size = SAP_SECTSIZE2;
   else
      sect_size = SAP_SECTSIZE1;

   for (track=0; track<num_tracks; track++) {
      if (verbose) {
         printf("\r");
         printf(_("Reading track %d..."), track);
         fflush(stdout);
      }

      if (FloppyReadSector(drive, density, track, 1, SAP_NSECTS, buffer) != 0) {
         /* erreur dans la piste, on essaie secteur par secteur */
         for (sect=1; sect<=SAP_NSECTS; sect++) {
            sapsector.format     = 0;
            sapsector.protection = 0;
            sapsector.track      = track;
            sapsector.sector     = sect;

            if (FloppyReadSector(drive, density, track, sect, 1, buffer) != 0) {
               /* erreur de lecture du secteur */
               if (verbose)
                  printf(_("\nSector %d unreadable"), sect);

               sapsector.format = 4;
               memset(sapsector.data, 0xF7, sect_size);
            }
            else {
               memcpy(sapsector.data, buffer, sect_size);
            }

            /* traitement spécial de la FAT pour les disquettes 5"25 DD */
            if ((track == 20) && (sect == 2) && IS_5_INCHES(drive) && (density == 2))
               shrink_fat_160_to_80(sapsector.data);

            sap_FillArchive(sap_file, &sapsector);
         }

         if ((track < num_tracks-1) && verbose)
            printf("\n");
      }
      else {
         for (sect=1; sect<=SAP_NSECTS; sect++) {
            sapsector.format     = 0;
            sapsector.protection = 0;
            sapsector.track      = track;
            sapsector.sector     = sect;

            memcpy(sapsector.data, buffer+(sect-1)*sect_size, sect_size);

            /* traitement spécial de la FAT pour les disquettes 5"25 DD */
            if ((track == 20) && (sect == 2) && IS_5_INCHES(drive) && (density == 2))
               shrink_fat_160_to_80(sapsector.data);

            sap_FillArchive(sap_file, &sapsector);
         }
      }
   }

   if (verbose)
      printf("\n");

   sap_CloseArchive(sap_file);
   return 0;
}



/* UnpackArchive:
 *  Transfère le contenu d'une archive SAP vers une disquette TO.
 */
static int UnpackArchive(const char sap_name[], int drive, int density, int verbose)
{
   int format, num_tracks, sect_size;
   int track, sect;
   sapID sap_file;
   sapsector_t sapsector;
   char buffer[512*SAP_NSECTS];

   if ((sap_file=sap_OpenArchive(sap_name, &format)) == SAP_ERROR)
      return 1;

   /* vérification du format */
   if (((format == SAP_FORMAT1) && (density != 2)) || ((format == SAP_FORMAT2) && (density != 1)))
      return 1;

   /* détermination du nombre de pistes */
   if (IS_5_INCHES(drive))
      num_tracks = SAP_NTRACKS2;
   else
      num_tracks = SAP_NTRACKS1;

   /* et de la taille du secteur */
   if (density == 1)
      sect_size = SAP_SECTSIZE2;
   else
      sect_size = SAP_SECTSIZE1;

   for (track=0; track<num_tracks; track++) {
      if (verbose) {
         printf("\r");
         printf(_("Writing track %d..."), track);
         fflush(stdout);
      }

      for (sect=1; sect<=SAP_NSECTS; sect++) {
         sapsector.format     = 0;
         sapsector.protection = 0;
         sapsector.track      = track;
         sapsector.sector     = sect;

         if (sap_ReadSector(sap_file, track, sect, &sapsector) == SAP_ERROR) {
            sap_CloseArchive(sap_file);
            return 2;
         }
         else {
            memcpy(buffer+(sect-1)*sect_size, sapsector.data, sect_size);
         }
      }

      /* pour MS-DOS (voir teo/src/dos/ddisk.c) */
      buffer[SAP_NSECTS*sect_size] = 0;

      if (FloppyWriteSector(drive, density, track, 1, SAP_NSECTS, buffer) != 0) {
         sap_CloseArchive(sap_file);
         return 3;
      }
   }

   if (verbose)
      printf("\n");

   sap_CloseArchive(sap_file);
   return 0;
}



/* get_drive:
 *  Helper pour obtenir le numéro de lecteur.
 */
static void get_drive(int *drive, int *density)
{
   char trash[8];

   if (fi.num_drives > 1) {
      do {
         printf(_("drive number: "));
         if (!scanf("%d", drive))
            fgets(trash, 7, stdin);
      }
      while ((*drive<0) || (*drive>3) || (fi.drive_type[*drive]==0));
   }
   else {
      for (*drive=0; *drive<4; (*drive)++) {
         if (fi.drive_type[*drive] > 0)
            break;
      }
   }

   if (fi.fm_support && IS_5_INCHES(*drive)) {
      do {
         printf(_("density (1:single, 2:double): "));
         if (!scanf("%d", density))
            fgets(trash, 7, stdin);
      }
      while ((*density<1) || (*density>2));
   }
   else
      *density = 2;
}



/* interactive_main:
 *  Point d'entrée du programme interactif.
 */
static void interactive_main(void)
{
   int c, ret;
   char trash[8];
   int drive, density, format, factor;
   char sap_name[FILENAME_LENGTH];

   while (1) {
      printf(_("SAP2, floppy disk transfer tool for 3.5\" and 5.25\" Thomson disks\n"));
      printf(_("Version %s (%s), "), SAP2_VERSION_STR, SAP2_PLATFORM_STR);
      printf(_("Based on SAP, Copyright (C) 1998 Alexandre Pukall\n"));
      printf(_("Copyright (C) 2000-2003 Eric Botcazou\n\n"));

      if (fi.drive_type[0] > 0) {
         printf(_("Drive A: (%s) --> drive 0"), gettext(drive_type_name[fi.drive_type[0]]));

         if (fi.drive_type[1] > 0)
            printf(_(" + drive 1\n"));
         else
            printf("\n");
      }
      else {
         printf(_("Drive A: (%s)\n"), gettext(drive_type_name[0]));
      }

      if (fi.drive_type[2] > 0) {
         printf(_("Drive B: (%s) --> drive 2"), gettext(drive_type_name[fi.drive_type[2]]));

         if (fi.drive_type[3] > 0)
            printf(_(" + drive 3\n\n"));
         else
            printf("\n\n");
      }
      else {
         printf(_("Drive B: (%s)\n\n"), gettext(drive_type_name[0]));
      }

      printf(_("Transfer TO-->PC:\n"));
      printf(_(" 1. Show the directory of a Thomson disk\n"));
      printf(_(" 2. Create an empty SAP archive\n"));
      printf(_(" 3. Archive a Thomson floppy disk to a SAP archive\n\n"));

      printf(_("Transfer PC-->TO:\n"));
      printf(_(" 4. View the contents of an SAP archive\n"));
      printf(_(" 5. Format a 3\"5 (720 KB) or 5\"25 disk with Thomson format\n"));
      printf(_(" 6. Unpack SAP archive to Thomson disk\n\n"));

      printf(_("Other commands:\n"));
      printf(_(" 7. Quit\n\n"));

      do {
         printf(_("Your choice: "));
         if (!scanf("%d", &c))
            fgets(trash, 7, stdin);
      }
      while ((c<1) || (c>7));

      switch (c) {

         case 1:
            printf(_("Show disk directory:\n"));
            get_drive(&drive, &density);
            printf("\n");

            ret = ViewDiskDir(drive, density, PAGE_HEIGHT);
            if (ret == 1)
               printf(_("*** Error: unreadable directory ***\n"));

            break;

         case 2:
            printf(_("Create an empty SAP archive:\n"));

            printf(_("Name of the archive to create (without file extension): "));
            scanf("%s", sap_name);
            strcat(sap_name, ".sap");

            do {
               printf(_("format (1:5\"25 SD, 2:5\"25 DD, 3:3\"5 DD): "));
               if (!scanf("%d", &format))
                  fgets(trash, 7, stdin);
            }
            while ((format<1) || (format>3));

            ret = CreateEmptyArchive(sap_name, (format == 1 ? SAP_FORMAT2 : SAP_FORMAT1), (format == 3 ? SAP_TRK80 : SAP_TRK40));
            if (ret == 1)
               printf(_("*** Error: unable to create the file %s ***\n"), sap_name);

            break;

         case 3:
            printf(_("Archive a Thomson floppy disk to a SAP archive:\n"));

            printf(_("Filename of the archive to create (without file extension): "));
            scanf("%s", sap_name);
            strcat(sap_name, ".sap");

            get_drive(&drive, &density);
            ret = PackArchive(sap_name, drive, density, 1);
            if (ret == 1)
               printf(_("*** Error: unable to create the file %s ***\n"), sap_name);

            break;

         case 4:
            printf(_("View the contents of an SAP archive\n"));

            printf(_("Filename (without file extension): "));
            scanf("%s", sap_name);
            strcat(sap_name, ".sap");

            printf("\n");

            ret = ViewArchiveDir(sap_name, PAGE_HEIGHT);
            if (ret == 1)
               printf(_("*** Error: unable to open file %s ***\n"), sap_name);
            else if (ret == 2)
               printf(_("*** Error: SAP archive corrupted ***\n"));

            break;

         case 5:
            printf(_("Format a Thomson floppy disk:\n"));
            printf(_("Please note: a double density disk (without a hole opposite the "\
            "write protect switch) is required, a high density disk won't work."));

            get_drive(&drive, &density);

            do {
               printf(_("Interleave factor ([1..%d], %d recommend): "), SAP_NSECTS-1, (SAP_NSECTS-1)/2);
               if (!scanf("%d", &factor))
                  fgets(trash, 7, stdin);
            }
            while ((factor<1) || (factor>(SAP_NSECTS-1)));

            ret = FormatDisk(drive, density, factor, 1);
            if (ret == 1)
               printf(_("\n*** Error: unable to format the disk ***\n"));
            else if (ret == 2)
               printf(_("*** Error: unable to write to disk ***\n"));
            break;

         case 6:
            printf(_("Unpack SAP archive to Thomson disk:\n"));

            get_drive(&drive, &density);

            printf(_("Archive filename (without file extension): "));
            scanf("%s", sap_name);
            strcat(sap_name, ".sap");

            ret = UnpackArchive(sap_name, drive, density, 1);
            if (ret == 1)
               printf(_("*** Error: unable to open file %s ***\n"), sap_name);
            else if (ret == 2)
               printf(_("*** Error: SAP archive corrupted ***\n"));
            else if (ret == 3)
               printf(_("\n*** Error: unable to write disk ***\n"));
            break;

         case 7:
            return;
      }

      fgets(trash, 7, stdin);
      printf(_("Press <Return>"));
      fflush(stdout);
      getchar();
      printf("\n");
   }
}


#define COMMAND_MAX  8

static char *short_command[] = {"-h", "-v", "-d", "-c", "-p", "-t", "-f", "-u"};
static char *long_command[] = {"--help", "--version", "--dir", "--create", "--pack", "--list", "--format", "--unpack"};



/* usage:
 *  Affiche les commandes d'utilisation du programme et quitte.
 */
static void usage(const char prog_name[])
{
   fprintf(stderr, _("Usage: %s  -h --help | -v --version | -t --list | -p --pack | -u --unpack\n"
                     "               -c --create | -d --dir | -f --format\n"), prog_name);
   exit(EXIT_FAILURE);
}



/* main:
 *  Point d'entrée du programme.
 */
int main(int argc, char *argv[])
{
   int ret = 0;
   int i;

   setlocale(LC_ALL, "");
   if(bindtextdomain("sapfs", QUOTE(PREFIX_MO) "/share/locale") == NULL)
      fprintf(stderr, "Error: bindtextdomain() failed\n");
   textdomain("sap2");

   if (argc < 2) { /* no argument? */
      if (FloppyInit(&fi, 1) > 0) {
         interactive_main();
         fprintf(stderr, _("*** Error: no floppy disk drives found, aborting ***\n"));
         FloppyExit();
      }
      else {
         fprintf(stderr, _("*** Error: no floppy disk drives found, aborting ***\n"));
         ret = 1;
      }
   }
   else {
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
               printf(_("SAP2 is a tool to read and write Thomson 3\"5 and 5\"25 floppy disks\n\n"));
               printf(_("Usage:\n\n"
               "%s without parameters enters the interactive mode\n\n"

               "    %s -hv\n"
               "    %s -tup archiv.sap [drive] [density]\n"
               "    %s -c archive.sap [tracks] [density]\n"
               "    %s -df drive [density] [interleave]\n\n"

               "  -c, --create        create an empty SAP archiv\n"
               "  -d, --dir           show the directory of a Thomson floppy disk\n"
               "  -f, --format        format a disk with Thomson format\n"
               "  -h, --help          Show this help text\n"
               "  -p, --pack          Archive a Thomson floppy disk to a SAP archiv\n"
               "  -t, --list          View the contents of an SAP archiv\n"
               "  -u, --unpack        Unpack a SAP archive to a Thomson disk\n"
               "  -v, --version       Print the version of the program plus a copyright and "
                     "a list of  authors\n"),
                       argv[0], argv[0], argv[0], argv[0], argv[0]);
               break;

            case 'v':  /* version */
               /// 1st %s: version string (e.g. 2.0.96), 2nd %s: name of * operating system
               printf(_("SAP2 version %s for %s, copyright (C) 2000-2003 Eric Botcazou.\n"),
                   SAP2_VERSION_STR, SAP2_PLATFORM_STR);
               break;

            case 'd':  /* dir */
               if (argc < 3)
                  usage(argv[0]);

               if (FloppyInit(&fi, 0) > 0) {
                  ret = ViewDiskDir(atoi(argv[2]), argc > 3 ? atoi(argv[3]) : 2, -1);
                  FloppyExit();
               }
               else {
                  fprintf(stderr, _("*** Error: no floppy disk drives found, aborting ***\n"));
                  ret = 1;
               }
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

            case 'p':  /* pack */
               if (argc < 4)
                  usage(argv[0]);

               if (FloppyInit(&fi, 0) > 0) {
                  ret = PackArchive(argv[2], atoi(argv[3]), argc > 4 ? atoi(argv[4]) : 2, 0);
                  FloppyExit();
               }
               else {
                  fprintf(stderr, _("*** Error: no floppy disk drives found, aborting ***\n"));
                  ret = 1;
               }
               break;

            case 't':  /* list */
               if (argc < 3)
                  usage(argv[0]);

               ret = ViewArchiveDir(argv[2], -1);
               break;

            case 'f':  /* format */
               if (argc < 3)
                  usage(argv[0]);

               if (FloppyInit(&fi, 1) > 0) {
                  if (argc == 3)
                     ret = FormatDisk(atoi(argv[2]), 2, (SAP_NSECTS-1)/2, 0);
                  else if (argc == 4)
                     ret = FormatDisk(atoi(argv[2]), atoi(argv[3]), (SAP_NSECTS-1)/2, 0);
                  else
                     ret = FormatDisk(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]), 0);

                  FloppyExit();
               }
               else {
                  fprintf(stderr, _("*** Error: no floppy disk drives found, aborting ***\n"));
                  ret = 1;
               }
               break;

            case 'u':  /* unpack */
               if (argc < 4)
                  usage(argv[0]);

               if (FloppyInit(&fi, 1) > 0) {
                  ret = UnpackArchive(argv[2], atoi(argv[3]), argc > 4 ? atoi(argv[4]) : 2, 0);
                  FloppyExit();
               }
               else {
                  fprintf(stderr, _("*** Error: no floppy disk drives found, aborting ***\n"));
                  ret = 1;
               }
               break;

            default:
               usage(argv[0]);
         }  /* end of switch */
      }
      else {
         usage(argv[0]);
      }
   }

   return ret;
}

