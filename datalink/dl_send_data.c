/* This software is Copyright 1996 by Karl R. Hakimian
 *
 * datazap: Linux version of Timex/Microsoft SDK for Timex datalink watches.
 *
 * Written by Karl R. Hakimian 10/3/96
 * 
 * Permission is hereby granted to copy, reproduce, redistribute or otherwise
 * use this software as long as: there is no monetary profit gained
 * specifically from the use or reproduction of this software, it is not
 * sold, rented, traded or otherwise marketed, and this copyright notice is
 * included prominently in any copy made. 
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. ANY USE OF THIS
 * SOFTWARE IS AT THE USER'S OWN RISK.
 *
 */

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "datalink.h"
#include "datalink_private.h"

#define MAX_PCKT 38

#define TIME_70 0x30
#define DSTART_70 0x60
#define DATA_70 0x61
#define DEND_70 0x62

static unsigned char start1[] = {7, 0x20, 0, 0, 3, 0, 0};
static unsigned char time[] = {17, 0x32, 0, 0, 0, 0, 0, 0 , 0, 0, 0, 0, 0,
							    1, 0, 0, 0};
static unsigned char dstart[] = {5, 0x93, 0, 0, 0};
static unsigned char dinfo[] = {20, 0x90, 0, 0, 0, 0, 0, 0 , 0, 0, 0, 0, 0,
							     0, 0, 0, 0, 0, 0, 0};
static unsigned char dspace[] = {4, 0x91, 0, 0};
static unsigned char dend[] = {5, 0x92, 0, 0, 0};
static unsigned char alarm[] = {18, 0x50, 0, 0, 0, 0, 0, 0 , 0, 0, 0, 0, 0,
							     0, 0, 0, 0, 0};
static unsigned char sysinfo[] = {6, 0x71, 0, 0, 0, 0};
static unsigned char end1[] = {4, 0x21, 0, 0};

_write_data(fd, buf, data, size, pnum, type, wi)
int fd;
unsigned char *buf;
unsigned char *data;
int size;
int *pnum;
int type;
WatchInfoPtr wi;
{
	int bytes_left;

	while (*buf + size > MAX_PCKT - 2) {
		bytes_left = *buf + size - MAX_PCKT + 2;
		memcpy(&buf[buf[0]], data, size - bytes_left);
		buf[0] = MAX_PCKT;
		dl_docrc(buf);

		if (write(fd, buf, *buf) != *buf)
			return((*dl_error_proc)("Can't write data to tmp file."));

		data += size - bytes_left;
		size = bytes_left;

		memcpy(buf, dspace, *dspace);

		if (wi->dl_device == DATALINK_70)
			buf[1] = DATA_70;

		buf[2] = type;
		buf[3] = (*pnum)++;
	}

	if (!size)
		return(0);

	memcpy(&buf[buf[0]], data, size);
	buf[0] += size;
	return(1);
}

dl_send_data(wi, type)
WatchInfoPtr wi;
int type;
{
	char fname[1024];
	unsigned char buf[64];
	unsigned char data[64];
	unsigned short addr = 0x0236;
	AppointmentPtr ap;
	ToDoPtr tp;
	PhonePtr pp;
	AnniversaryPtr anp;
	WristAppPtr wristapp;
	SystemPtr sys;
	MelodyPtr melody;
	int ofd;
	int i;
	int pnum;
	int pid;
	int status;
	int ret;
	int p;

	if (type == BLINK_FILE)
		strcpy(fname, "DEBUGOUTPUT");
	else if (!tmpnam(fname))
		return((*dl_error_proc)("Can't create tmp file."));

	if ((ofd = open(fname, O_WRONLY|O_CREAT|O_TRUNC, 0644)) < 0) {
		sprintf(buf, "Can't open %s for writing.", fname);
		return((*dl_error_proc)(buf));
	}

	memcpy(buf, start1, *start1);

	if (wi->dl_device == DATALINK_70)
		buf[4] = 1;
	if (wi->dl_device == DATALINK_IRONMAN)
	{
		buf[4] = 9;
		buf[5] = 6;
	}

	dl_docrc(buf);

	if (write(ofd, buf, *buf) != *buf)
		return((*dl_error_proc)("Can't write start to tmp file."));

	for (i = 0; i < dl_download_data.num_times; i++) {
		memcpy(buf, time, *time);
		p = 2;
		buf[p++] = dl_download_data.times[i].tz_num;

		if (wi->dl_device == DATALINK_150)
			buf[p++] = dl_download_data.times[i].seconds;

		buf[p++] = dl_download_data.times[i].hours;
		buf[p++] = dl_download_data.times[i].minutes;
		buf[p++] = dl_download_data.times[i].month;
		buf[p++] = dl_download_data.times[i].day;
		buf[p++] = dl_download_data.times[i].year;
		buf[p++] = dl_pack_char(dl_download_data.times[i].label[0]);
		buf[p++] = dl_pack_char(dl_download_data.times[i].label[1]);
		buf[p++] = dl_pack_char(dl_download_data.times[i].label[2]);
		buf[p++] = dl_download_data.times[i].dow;

		if (wi->dl_device == DATALINK_150) {
			buf[p++] = dl_download_data.times[i].hour_fmt;
			buf[p++] = dl_download_data.times[i].date_fmt&0xFF;
		}

		if (wi->dl_device == DATALINK_70)
			buf[1] = TIME_70;

		dl_docrc(buf);

		if (write(ofd, buf, *buf) != *buf)
			return((*dl_error_proc)("Can't write time to tmp file."));

	}

	if (dl_download_data.memory) {
		memcpy(buf, dstart, *dstart);
		buf[2] = 1;
		dl_docrc(buf);

		if (write(ofd, buf, *buf) != *buf)
			return((*dl_error_proc)("Can't write dstart to tmp file."));

		memcpy(buf, dinfo, *dinfo);
		buf[2] = 1;
		buf[3] = dl_download_data.memory/(MAX_PCKT - 6);

		if (dl_download_data.memory%(MAX_PCKT - 6))
			buf[3]++;

		buf[12] = dl_download_data.num_apps;
		buf[13] = dl_download_data.num_todos;
		buf[14] = dl_download_data.num_phones;
		buf[15] = dl_download_data.num_annivs;
		buf[4] = (addr>>8)&0xff;
		buf[5] = addr&0xff;
		addr += dl_download_data.app_size;
		buf[6] = (addr>>8)&0xff;
		buf[7] = addr&0xff;
		addr += dl_download_data.todo_size;
		buf[8] = (addr>>8)&0xff;
		buf[9] = addr&0xff;
		addr += dl_download_data.phone_size;
		buf[10] = (addr>>8)&0xff;
		buf[11] = addr&0xff;
		buf[16] = 0x62;
		buf[17] = dl_download_data.pre_notification_time/5;
		if (!buf[17]) buf[17] = 0xff;

		if (wi->dl_device == DATALINK_70)
			buf[1] = DATA_70;

		dl_docrc(buf);

		if (write(ofd, buf, *buf) != *buf)
			return((*dl_error_proc)("Can't write dinfo to tmp file."));

		pnum = 1;
		memcpy(buf, dspace, *dspace);

		if (wi->dl_device == DATALINK_70)
			buf[1] = DATA_70;

		buf[2] = 1;
		buf[3] = pnum++;

		for (i = 0; i < dl_download_data.num_apps; i++) {
			ap = &dl_download_data.apps[i];
			data[0] = dl_pack_size(ap->label);
			data[1] = ap->month;
			data[2] = ap->day;
			data[3] = ap->time;

			if (data[0] != dl_pack_ascii(&data[4], ap->label))
				return((*dl_error_proc)("ERROR Bad pack_ascii.\n"));

			*data += 4;

			if (!_write_data(ofd, buf, data, *data, &pnum, 1, wi))
				return((*dl_error_proc)("Can't write to tmp file."));

		}

		for (i = 0; i < dl_download_data.num_todos; i++) {
			tp = &dl_download_data.todos[i];
			data[0] = dl_pack_size(tp->label);
			data[1] = tp->priority;

			if (data[0] != dl_pack_ascii(&data[2], tp->label))
				return((*dl_error_proc)("ERROR Bad pack_ascii.\n"));

			*data += 2;

			if (!_write_data(ofd, buf, data, *data, &pnum, 1, wi))
				return((*dl_error_proc)("Can't write to tmp file."));

		}

		for (i = 0; i < dl_download_data.num_phones; i++) {
			pp = &dl_download_data.phones[i];
			data[0] = dl_pack_size(pp->label);
			dl_pack_phone(&data[1], pp->number, dl_download_data.max_phone_str);

			if (data[0] != dl_pack_ascii(&data[7], pp->label)) {
				printf("ERROR Bad pack_ascii.\n");
				exit(-1);
			}

			*data += dl_download_data.max_phone_str/2;
			(*data)++;

			if (!_write_data(ofd, buf, data, *data, &pnum, 1, wi))
				return((*dl_error_proc)("Can't write to tmp file."));

		}

		for (i = 0; i < dl_download_data.num_annivs; i++) {
			anp = &dl_download_data.annivs[i];
			data[0] = dl_pack_size(anp->label);
			data[1] = anp->month;
			data[2] = anp->day;

			if (data[0] != dl_pack_ascii(&data[3], anp->label)) {
				printf("ERROR Bad pack_ascii.\n");
				exit(-1);
			}

			*data += 4;

			if (!_write_data(ofd, buf, data, *data, &pnum, 1, wi))
				return((*dl_error_proc)("Can't write to tmp file."));

		}

		*buf += 2;
		dl_docrc(buf);

		if (write(ofd, buf, *buf) != *buf)
			return((*dl_error_proc)("Can't write to tmp file."));

		memcpy(buf, dend, *dend);
		buf[2] = 1;
		dl_docrc(buf);

		if (write(ofd, buf, *buf) != *buf)
			return((*dl_error_proc)("Can't write to tmp file."));

	}

	for (i = 0; i < dl_download_data.num_alarms; i++) {
		memcpy(buf, alarm, *alarm);
		buf[2] = dl_download_data.alarms[i].alarm_num;
		buf[3] = dl_download_data.alarms[i].hours;
		buf[4] = dl_download_data.alarms[i].minutes;
		buf[5] = dl_download_data.alarms[i].month;
		buf[6] = dl_download_data.alarms[i].day;
		dl_fill_pack_ascii(&buf[7], dl_download_data.alarms[i].label,
			dl_download_data.max_alarm_str, ' ');
		buf[15] = dl_download_data.alarms[i].audible;
		dl_docrc(buf);

		if (write(ofd, buf, *buf) != *buf)
			return((*dl_error_proc)("Can't write to tmp file."));

	}

	if (dl_download_data.num_wristapp) {
		wristapp = dl_download_data.wristapp;
		memcpy(buf, dstart, *dstart);
		buf[2] = 2;
		dl_docrc(buf);

		if (write(ofd, buf, *buf) != *buf)
			return((*dl_error_proc)("Can't write to tmp file."));

		memcpy(buf, dinfo, *dinfo);
		buf[2] = 2;
		buf[3] = wristapp->len/(MAX_PCKT - 6);

		if (wristapp->len%(MAX_PCKT - 6))
			buf[3]++;

		buf[4] = 1;
		*buf = 7;

		dl_docrc(buf);

		if (write(ofd, buf, *buf) != *buf)
			return((*dl_error_proc)("Can't write to tmp file."));

		memcpy(buf, dspace, *dspace);

		if (wi->dl_device == DATALINK_70)
			buf[1] = DATA_70;

		pnum = 1;
		buf[2] = 2;
		buf[3] = pnum++;

		if (!_write_data(ofd, buf, wristapp->data, wristapp->len, &pnum, 2, wi))
			return((*dl_error_proc)("Can't write to tmp file."));

		if (*buf != 4) {
			dl_docrc(buf);

			if (write(ofd, buf, *buf) != *buf)
				return((*dl_error_proc)("Can't write to tmp file."));

		}

		memcpy(buf, dend, *dend);
		buf[2] = 2;
		dl_docrc(buf);

		if (write(ofd, buf, *buf) != *buf)
			return((*dl_error_proc)("Can't write to tmp file."));

	}

	if (dl_download_data.num_melody) {
		melody = dl_download_data.melody;
		memcpy(buf, dstart, *dstart);
		buf[2] = 3;
		dl_docrc(buf);

		if (write(ofd, buf, *buf) != *buf)
			return((*dl_error_proc)("Can't write to tmp file."));

		memcpy(buf, dinfo, *dinfo);
		buf[2] = 3;
		buf[3] = melody->len/(MAX_PCKT - 6);

		if (melody->len%(MAX_PCKT - 6))
			buf[3]++;

		buf[4] = 0xff - melody->len;
		*buf = 7;

		dl_docrc(buf);

		if (write(ofd, buf, *buf) != *buf)
			return((*dl_error_proc)("Can't write to tmp file."));

		memcpy(buf, dspace, *dspace);

		if (wi->dl_device == DATALINK_70)
			buf[1] = DATA_70;

		pnum = 1;
		buf[2] = 3;
		buf[3] = pnum++;

		if (!_write_data(ofd, buf, melody->data, melody->len, &pnum, 3, wi))
			return((*dl_error_proc)("Can't write to tmp file."));

		if (*buf != 4) {
			dl_docrc(buf);

			if (write(ofd, buf, *buf) != *buf)
				return((*dl_error_proc)("Can't write to tmp file."));

		}

		memcpy(buf, dend, *dend);
		buf[2] = 3;
		dl_docrc(buf);

		if (write(ofd, buf, *buf) != *buf)
			return((*dl_error_proc)("Can't write to tmp file."));

	}

	if (dl_download_data.num_system) {
		sys = dl_download_data.system;
		memcpy(buf, sysinfo, *sysinfo);
		buf[2] = sys->chime;
		buf[3] = sys->beep;
		dl_docrc(buf);

		if (write(ofd, buf, *buf) != *buf)
			return((*dl_error_proc)("Can't write to tmp file."));

	}

	memcpy(buf, end1, *end1);
	dl_docrc(buf);

	if (write(ofd, buf, *buf) != *buf)
		return((*dl_error_proc)("Can't write to tmp file."));

	close(ofd);

	switch (type) {
	case BLINK_FILE:
		ret = 0;
		break;
	case SVGA_BLINK:

		switch ((pid = fork())) {
		case -1:
			return((*dl_error_proc)("Could not execute svga blink engine."));
			return(-1);
		case 0:	/* Child */
			sprintf(buf, "svgablink", BINDIR);

			if (wi->dl_device == DATALINK_150)
			{
				execlp(buf, "svgablink", fname, NULL);
				sprintf(buf, "./svgablink", BINDIR);
				execl(buf, "svgablink", fname, NULL);
			}
			else
			{
				execlp(buf, "svgablink", "-70", fname, NULL);
				sprintf(buf, "./svgablink", BINDIR);
				execl(buf, "svgablink", "-70", fname, NULL);
			}

			(*dl_error_proc)("Could not execute svga blink engine.");
			exit(-1);
		default:

			if (waitpid(pid, &status, 0) < 0)
				perror("waitpid");

			//(void)unlink(fname);

			if (WIFEXITED(status))
				ret = WEXITSTATUS(status);
			else
				ret = -1;

			break;
		}

		break;
	case SER_BLINK:

		switch ((pid = fork())) {
		case -1:
			return((*dl_error_proc)("Could not fork child for serial blink engine."));
			return(-1);
		case 0:	/* Child */
			sprintf(buf, "serblink", BINDIR);
			execlp(buf, "serblink", fname, NULL);
			sprintf(buf, "./serblink", BINDIR);
			execl(buf, "serblink", fname, NULL);
			(*dl_error_proc)("Could not execute serial blink engine.");
			exit(-1);
		default:

			if (waitpid(pid, &status, 0) < 0)
				perror("waitpid");

			//(void)unlink(fname);

			if (WIFEXITED(status))
				ret = WEXITSTATUS(status);
			else
				ret = -1;

			break;
		}

		break;
	}

	return(ret);
}