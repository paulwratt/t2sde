
/*
 * --- ROCK-COPYRIGHT-NOTE-BEGIN ---
 * 
 * This copyright note is auto-generated by ./scripts/Create-CopyPatch.
 * Please add additional copyright information _after_ the line containing
 * the ROCK-COPYRIGHT-NOTE-END tag. Otherwise it might get removed by
 * the ./scripts/Create-CopyPatch script. Do not edit this copyright text!
 * 
 * ROCK Linux: rock-src/target/bootdisk/linuxrc.c
 * ROCK Linux is Copyright (C) 1998 - 2003 Clifford Wolf
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. A copy of the GNU General Public
 * License can be found at Documentation/COPYING.
 * 
 * Many people helped and are helping developing ROCK Linux. Please
 * have a look at http://www.rocklinux.org/ and the Documentation/TEAM
 * file for details.
 * 
 * --- ROCK-COPYRIGHT-NOTE-END ---
 *
 * linuxrc.c is Copyright (C) 2003 Cliford Wolf and Rene Rebe
 *
 */

#include <sys/mount.h>
#include <sys/swap.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/klog.h>
#include <fcntl.h>
#include <errno.h>

#ifndef STAGE_2_BIG_IMAGE
#  define STAGE_2_BIG_IMAGE "2nd_stage.tar.gz"
#endif

#ifndef STAGE_2_SMALL_IMAGE
#  define STAGE_2_SMALL_IMAGE "2nd_stage_small.tar.gz"
#endif

/* 64 MB should be enought for the tmpfs */
#define TMPFS_OPTIONS "size=67108864"

/* It seams like we need this prototype here ... */
int pivot_root(const char *new_root, const char *put_old);

int exit_linuxrc=0;

void doboot()
{
	if ( mkdir("/mnt_root/old_root", 700) )
		{ perror("Can't create /mnt_root/old_root"); exit_linuxrc=0; }

	if ( access("/mnt_root/linuxrc", R_OK) )
		{ printf("Can't find /mnt_root/linuxrc!\n"); exit_linuxrc=0; }

	if ( mount("/bin", "/mnt_root/usr/local/bin", NULL, MS_BIND, NULL) ) {
		perror("Can't mount /mnt_root/usr/local/bin");
		if (access("/mnt_root/bin/gzip", R_OK)) {
			perror("Can't find /mnt_root/bin/gzip");
			exit_linuxrc=0;
		}
	}

	if ( exit_linuxrc ) {
		if ( pivot_root("/mnt_root", "/mnt_root/old_root") )
			{ perror("Can't call pivot_root"); exit_linuxrc=0; }
		chdir("/");

		if ( mount("none", "/dev", "devfs", 0, NULL) )
			perror("Can't mount /dev");

		if ( mount("none", "/proc", "proc", 0, NULL) )
			perror("Can't mount /proc");
	} else {
		if ( rmdir("/mnt_root/old_root") )
			perror("Can't remove /mnt_root/old_root");

		if ( umount("/mnt_root") ) perror("Can't umount /mnt_root");
		if ( rmdir ("/mnt_root") ) perror("Can't remove /mnt_root");
	}
}

int trymount (const char* source, const char* target)
{
	return mount(source, "/mnt_source", "ext3",    MS_RDONLY, NULL) &&
		mount(source, target, "ext2",    MS_RDONLY, NULL) &&
		mount(source, target, "minix",   MS_RDONLY, NULL) &&
		mount(source, target, "vfat",    MS_RDONLY, NULL) &&
		mount(source, target, "iso9660", MS_RDONLY, NULL);
}

void httpload() 
{
	int fd[2];
	char baseurl[200];
	char filename[100];
	char url[500];

	printf("Enter base URL (e.g. http://1.2.3.4/rock): ");
	fflush(stdout);

	baseurl[0]=0; fgets(baseurl, 200, stdin); baseurl[199]=0;
	if (strlen(baseurl) > 0) baseurl[strlen(baseurl)-1]=0;
	if (baseurl[0] == 0) return;

	printf("Select a stage 2 image file:\n\n"
	       "     1. %s\n     2. %s\n\n"
	       "Enter number or image file name (default=1): ",
	       STAGE_2_BIG_IMAGE, STAGE_2_SMALL_IMAGE);

	filename[0]=0; fgets(filename, 100, stdin); filename[99]=0;
	if (strlen(filename) > 0) filename[strlen(filename)-1]=0;
	if (filename[0] == 0) strcpy(filename, STAGE_2_BIG_IMAGE);
	else if (!strcmp(filename, "1")) strcpy(filename, STAGE_2_BIG_IMAGE);
	else if (!strcmp(filename, "2")) strcpy(filename, STAGE_2_SMALL_IMAGE);

	exit_linuxrc=1;
	snprintf(url, 500, "%s/%s", baseurl, filename);

	printf("[ %s ]\n", url);
	setenv("ROCK_INSTALL_SOURCE_URL", baseurl, 1);

	exit_linuxrc=1;
	if ( mkdir("/mnt_root", 700) )
		{ perror("Can't create /mnt_root"); exit_linuxrc=0; }

	if ( mount("none", "/mnt_root", "tmpfs", 0, TMPFS_OPTIONS) )
		{ perror("Can't mount /mnt_root"); exit_linuxrc=0; }

	if ( pipe(fd) < 0 )
		{ perror("Can't create pipe"); exit_linuxrc=0; } 

	if ( fork() == 0 ) {
		dup2(fd[1],1); close(fd[0]); close(fd[1]);
		execlp("wget", "wget", "-O", "-", url, NULL);
		perror("wget");
		_exit(1);
	}

	if ( fork() == 0 ) {
		dup2(fd[0],0); close(fd[0]); close(fd[1]);
		execlp("tar", "tar", "--use-compress-program=gzip",
		       "-C", "/mnt_root", "-xf", "-", NULL);
		perror("tar");
		_exit(1);
	}

	close(fd[0]); close(fd[1]);
	wait(NULL); wait(NULL);
	printf("finished ... now booting 2nd stage\n");
	doboot();
}

void load_modules(char * dir)
{
	struct dirent **namelist;
	char text[100], filename[200];
	char *execargs[100];
	int n, m=0, len;

	n = scandir(dir, &namelist, 0, alphasort);
	if (n > 0) {
		printf("List of available modules:\n\n     ");
		while(n--) {
			strcpy(filename, namelist[n]->d_name);
			free(namelist[n]); len = strlen(filename);

			if (len > 2 && !strcmp(filename+len-2, ".o")) {
				filename[len-2]=0;
				printf("%-15s", filename);
				if (++m % 4 == 0) printf("\n     ");
			}
		}
		if (m % 4 != 0) printf("\n");
		printf("\n");
		free(namelist);
	} else {
		printf("No modules found!\n");
		if (n == 0) free(namelist);
		return;
	}

	printf("Enter module name (and optional parameters): ");
	fflush(stdout);

	while (1) {
		text[0]=0; fgets(text, 100, stdin); text[99]=0;
		if (strlen(text) > 0) text[strlen(text)-1]=0;
		if (text[0] == 0) return;

		snprintf(filename, 200, "%s/%s.o", dir, strtok(text, " "));
		execargs[0] = "insmod"; execargs[1] = "-v";
		execargs[2] = "-f";     execargs[3] = filename;
		for (n=4; (execargs[n] = strtok(NULL, " ")) != NULL; n++) ;

		if ( ! access(filename, R_OK) ) break;
		printf("No such module found. Try again (enter=back): ");
		fflush(stdout);
	}


	if ( fork() == 0 ) {
		execvp(execargs[0], execargs);
		printf("Can't start %s!\n", execargs[0]);
		exit(1);
	}
	wait(NULL);

	return;
}

void load_ramdisk_file()
{
	char *devn[10], *desc[10];
	char text[100], devicefile[100];
	char filename[100];
	int nr=0;
	int tmp_nr;
	int found;

	printf("Select a device for loading the 2nd stage system from: \n\n");
	
	for (tmp_nr = 0, found = 1; found; ++tmp_nr) {
		sprintf(devicefile, "/dev/cdroms/cdrom%d", tmp_nr);
		sprintf(text, "CD-ROM #%d (IDE/ATAPI or SCSI)", tmp_nr+1);

		if ( ! access (devicefile, R_OK) ) {
			desc[nr] = strdup (text);
			devn[nr++] = strdup (devicefile);
		}
		else {
			found = 0;
		}
	}

	tmp_nr = 0; found = 1;
	for (tmp_nr = 0, found = 1; found; ++tmp_nr) {
		sprintf(devicefile, "/dev/floppy/%d", tmp_nr);
		sprintf(text, "FDD (Floppy Disk Drive) #%d", tmp_nr+1);

		if ( ! access (devicefile, R_OK) ) {
			desc[nr] = strdup (text);
			devn[nr++] = strdup (devicefile);
		}
		else {
			found = 0;
		}
	}
	
	desc[nr] = devn[nr] = NULL;

	for (nr=0; desc[nr]; nr++) {
		printf("     %d. %s\n", nr, desc[nr]);
	}

	printf("\nEnter number or device file name (default=0): ");
	fflush(stdout);

	while (1) {
		text[0]=0; fgets(text, 100, stdin); text[99]=0;
		if (strlen(text) > 0)
			text[strlen(text)-1]=0;

		if (text[0] == 0)
			strcpy (text, "0");

		if ( ! access(text, R_OK) ) {
			strcpy(devicefile, text);
			break;
		}

		if (atoi(text) >= 0 && atoi(text) < nr) {
			strcpy(devicefile, devn[atoi(text)]);
			break;
		}

		printf("No such device found. Try again (enter=back): ");
		fflush(stdout);
	}

	printf("Select a stage 2 image file:\n\n"
	       "     1. %s\n     2. %s\n\n"
	       "Enter number or image file name (default=1): ",
	       STAGE_2_BIG_IMAGE, STAGE_2_SMALL_IMAGE);

	text[0]=0; fgets(text, 100, stdin); text[99]=0;
	if (strlen(text) > 0) text[strlen(text)-1]=0;
	if (text[0] == 0) strcpy(filename, STAGE_2_BIG_IMAGE);
	else if (! strcmp(text, "1")) strcpy(filename, STAGE_2_BIG_IMAGE);
	else if (! strcmp(text, "2")) strcpy(filename, STAGE_2_SMALL_IMAGE);
	else strcpy(filename, text);

	exit_linuxrc=1;
	printf("Using %s:%s.\n", devicefile, filename);

	if ( mkdir("/mnt_source", 700) )
		{ perror("Can't create /mnt_source"); exit_linuxrc=0; }

	if ( trymount (devicefile, "/mnt_source") )
		{ perror("Can't mount /mnt_source"); exit_linuxrc=0; }

	if ( mkdir("/mnt_root", 700) )
		{ perror("Can't create /mnt_root"); exit_linuxrc=0; }

	if ( mount("none", "/mnt_root", "tmpfs", 0, TMPFS_OPTIONS) )
		{ perror("Can't mount /mnt_root"); exit_linuxrc=0; }

	if ( fork() == 0 ) {
		printf("Extracting 2nd stage filesystem to ram ...\n");
		snprintf(text, 100, "/mnt_source/%s", filename);
		execlp( "tar", "tar", "--use-compress-program=gzip",
		               "-C", "/mnt_root", "-xf", text, NULL);
		printf("Can't run tar on %s!\n", filename);
		exit(1);
	}
	wait(NULL);

	if ( umount("/mnt_source") )
		{ perror("Can't umount /mnt_source"); exit_linuxrc=0; }

	if ( rmdir("/mnt_source") )
		{ perror("Can't remove /mnt_source"); exit_linuxrc=0; }

	setenv("ROCK_INSTALL_SOURCE_DEV",  devicefile, 1);
	setenv("ROCK_INSTALL_SOURCE_FILE", filename,   1);
	doboot();
}	

void activate_swap()
{
	char text[100];

	printf("\nEnter file name of swap device: ");
	fflush(stdout);

	text[0]=0; fgets(text, 100, stdin); text[99]=0;
	if ( text[0] ) {
		if ( swapon(text, 0) < 0 )
			perror("Can't activate swap device");
	}
}

void config_net()
{
	char dv[100]="";
	char ip[100]="";
	char gw[100]="";

	if ( fork() == 0 ) {
		execlp("ip", "ip", "addr", NULL);
		perror("ip");
		exit(1);
	}
	wait(NULL);
	printf("\n");

	if ( fork() == 0 ) {
		execlp("ip", "ip", "route", NULL);
		perror("ip");
		exit(1);
	}
	wait(NULL);
	printf("\n");

	printf("Enter interface name (eth0): ");
	fflush(stdout); fgets(dv, 100, stdin); dv[99]=0;
	if (strlen(dv) > 0) dv[strlen(dv)-1]=0;
	if (dv[0] == 0) strcpy(dv, "eth0");

	printf("Enter ip (192.168.0.254/24): ");
	fflush(stdout); fgets(ip, 100, stdin); ip[99]=0;
	if (strlen(ip) > 0) ip[strlen(ip)-1]=0;
	if (ip[0] == 0) strcpy(ip, "192.168.0.254/24");

	if ( fork() == 0 ) {
		execlp("ip", "ip", "addr", "add", ip, "dev", dv, NULL);
		perror("ip");
		exit(1);
	}
	wait(NULL);

	if ( fork() == 0 ) {
		execlp("ip", "ip", "link", "set", dv, "up", NULL);
		perror("ip");
		exit(1);
	}
	wait(NULL);

	printf("Enter default gateway (none): ");
	fflush(stdout); fgets(gw, 100, stdin); gw[99]=0;
	if (strlen(gw) > 0) gw[strlen(gw)-1]=0;

	if (gw[0] != 0) {
		if ( fork() == 0 ) {
			execlp("ip", "ip", "route", "add",
			             "default", "via", gw, NULL);
			perror("ip");
			exit(1);
		}
		wait(NULL);
	}
	printf("\n");

	if ( fork() == 0 ) {
		execlp("ip", "ip", "addr", NULL);
		perror("ip");
		exit(1);
	}
	wait(NULL);
	printf("\n");

	if ( fork() == 0 ) {
		execlp("ip", "ip", "route", NULL);
		perror("ip");
		exit(1);
	}
	wait(NULL);
}

void autoload_modules()
{
	char line[200], cmd[200], module[200];
	int fd[2], rc;
	FILE *f;

	if (pipe(fd) <0)
		{ perror("Can't create pipe"); return; } 

	if ( fork() == 0 ) {
		dup2(fd[1],1); close(fd[0]); close(fd[1]);
		execlp("gawk", "gawk", "-f", "/bin/hwscan", NULL);
		printf("Can't start >>hwscan<< program with gawk!\n");
		exit(1);
	}

	close(fd[1]);
	f = fdopen(fd[0], "r");
	while ( fgets(line, 200, f) != NULL ) {
		if ( sscanf(line, "%s %s", cmd, module) < 2 ) continue;
		if ( !strcmp(cmd, "modprobe") || !strcmp(cmd, "insmod") ) {
			printf("%s %s\n", cmd, module);
			if ( (rc = fork()) == 0 ) {
				execlp(cmd, cmd, module, NULL);
				perror("Cant run modprobe/insmod");
				exit(1);
			}
			waitpid(rc, NULL, 0);
		}
	}
	fclose(f);
	wait(NULL);
}

void exec_sh()
{
	printf ("Quit the shell to return to the stage 1 loader!\n");
	if ( fork() == 0 ) {
		execl("/bin/kiss", "kiss", NULL);
		perror("kiss");
		_exit(1);
	}
	wait(NULL);
}

int main()
{
	char text[100];
	int input=1;

	if ( mount("none", "/dev", "devfs", 0, NULL) && errno != EBUSY )
		perror("Can't mount /dev");

	if ( mount("none", "/proc", "proc", 0, NULL) )
		perror("Can't mount /proc");

	/* Only print important stuff to console */
	klogctl(8, NULL, 3);

	autoload_modules();

	printf("\n\
     ============================================\n\
     ===   ROCK Linux 1st stage boot system   ===\n\
     ============================================\n\
\n\
The ROCK Linux install / rescue system boots up in two stages. You\n\
are now in the first of this two stages and if everything goes right\n\
you will not spend much time here. Just load your SCSI and networking\n\
drivers (if needed) and configure the installation source so the\n\
2nd stage boot system can be loaded and you can start the installation.\n");

	while (exit_linuxrc == 0)
	{
		printf("\n\
     0. Load 2nd stage system from local device\n\
     1. Load 2nd stage system from network\n\
     2. Configure network interfaces (IPv4 only)\n\
     3. Load kernel networking modules from this disk\n\
     4. Load kernel SCSI modules from this disk\n\
     5. Load kernel modules from another disk\n\
     6. Activate already formatted swap device\n\
     7. Execute a (kiss) shell if present (for experts!)\n\
\n\
What do you want to do [0-7] (default=0)? ");
		fflush(stdout);

		text[0]=0; fgets(text, 100, stdin); text[99]=0;
		if (strlen(text) > 0)
			 text[strlen(text)-1]=0;
		
		input=atoi(text);
		
		switch (input) {
		case 0:
		  load_ramdisk_file();
		  break;
		
		case 1:
		  httpload();
		  break;

		case 2:
		  config_net();
		  break;
		  
		case 3:
		  load_modules("/lib/modules/net");
		  break;

		case 4:
		  load_modules("/lib/modules/scsi");
		  break;

		case 5:
		  if ( mkdir("/mnt_floppy", 700) )
		    perror("Can't create /mnt_floppy");
		  
		  if ( trymount("/dev/floppy/0", "/mnt_floppy") )
		    load_modules("/mnt_floppy");
		  
		  if ( umount("/mnt_floppy") )
		    perror("Can't umount /mnt_floppy");
		  
		  if ( rmdir("/mnt_floppy") )
		    perror("Can't remove /mnt_floppy");
		  break;
		  
		case 6:
		  activate_swap();
		  break;
		  
		case 7:
		  exec_sh();
		  break;
		  
		default:
		  perror ("No such option present!");
		}
	}
	
	sleep(1);
	execl("/linuxrc", "/linuxrc", NULL);
	printf("\nCan't start /linuxrc!! Life sucks.\n\n");
	return 0;
}
