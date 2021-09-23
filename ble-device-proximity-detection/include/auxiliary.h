/*
 * auxiliary.h
 */

#ifndef AUXILIARY_H
#define AUXILIARY_H

#define DEFAULT_NTP_SERVER "pool.ntp.org"
#define DEFAULT_TZ_GMT_OFFSET +1
#define DEFAULT_TZ_DST 1

void retrieve_and_store_NTP_time(const char *ntp_server, const int tz_gmt_offset,const int tz_dst_offset);
unsigned long get_stored_time();

#endif