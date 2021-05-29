/* See LICENSE file for copyright and license details.
 *
 * This header, temp.h, provides the function get_temp,
 * to get the current CPU temperature.
 *
 * Made by Salonia Matteo <saloniamatteo@pm.me>
 *
 */

#ifndef TEMP_H
#define TEMP_H

#include <stdio.h>
#include <stdlib.h>

/* Function prototypes */
int get_temp(void);

int
get_temp(void)
{
	FILE *fd;
	char temp_str[6], ch;
	int temp = 0, i = 0;

	fd = fopen("/sys/class/thermal/thermal_zone0/temp", "r");

	/* Cannot read temperature */
	if (fd == NULL) {
		fprintf(stderr, "Cannot read temperature! Exiting.\n");
		return -1;
	}

	/* Read temperature */
	ch = fgetc(fd);

	while (ch != EOF) {
		temp_str[i] = ch;
		i++;
		ch = fgetc(fd);
	}

	temp_str[i - 1] = '\0';

	/* Convert string to integer */
	temp = atoi(temp_str) / 1000;

	/* Close file descriptor */
	fclose(fd);

	return temp;
}

#endif	/* TEMP_H */
