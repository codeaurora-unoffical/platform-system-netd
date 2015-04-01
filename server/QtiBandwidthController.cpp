/*
   Copyright (c) 2015, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#define IPA_TETHER_STATS_DIR "/data/misc/ipa/"
#define IPA_TETHER_STATS "/data/misc/ipa/tether_stats"
#define IPA_TETHER_STATS_FILE "tether_stats"

#define STATS_TEMPLATE "%s %s %" PRId64" %" PRId64" %" PRId64" %" PRId64""

//#define LOG_NDEBUG 0

#include <stdlib.h>
#include <errno.h>
#include <sys/inotify.h>

#include <string>
#include <sys/types.h>
#define LOG_TAG "QtiBandwidthController"
#include <cutils/log.h>
#include "QtiBandwidthController.h"
#include <sys/socket.h>
#include <sys/stat.h>
#include "NatController.h"

#define MAX_CMD_LEN  1024

QtiBandwidthController::QtiBandwidthController() {
    ALOGD("QtiBandwidthController init'led");
    ipaTetherStatInit();
}

QtiBandwidthController::~QtiBandwidthController() {
}

int QtiBandwidthController::updateipaTetherStats(IpaTetherStats stats, bool clearflag) {
    char cmd1[MAX_CMD_LEN];
    char cmd2[MAX_CMD_LEN];
    char cmd3[MAX_CMD_LEN];
    FILE *hwiptOutput;

    memset(cmd1, 0, MAX_CMD_LEN);
    memset(cmd2, 0, MAX_CMD_LEN);
    memset(cmd3, 0, MAX_CMD_LEN);

    if (clearflag) {
        snprintf(cmd1,
            MAX_CMD_LEN,
            "iptables -w -F %s ",
            NatController::LOCAL_HW_TETHER_COUNTERS_CHAIN);
        hwiptOutput = popen(cmd1, "r");
        ALOGD("cmd1: %s err=%s", cmd1, strerror(errno));
        if (hwiptOutput != NULL) {
            pclose(hwiptOutput);
        }
    }

    snprintf(cmd2,
            MAX_CMD_LEN,
            "iptables -w -A %s -i %s -o %s --set-counters %" PRId64"  %" PRId64"  -j RETURN",
            NatController::LOCAL_HW_TETHER_COUNTERS_CHAIN,
        stats.iif,
        stats.oif,
        stats.txP,
        stats.txB);
    hwiptOutput = popen(cmd2, "r");
    ALOGD("cmd2: %s err=%s", cmd2, strerror(errno));
    if (hwiptOutput != NULL) {
        pclose(hwiptOutput);
    }


    memset(cmd3, 0, MAX_CMD_LEN);
    snprintf(cmd3,
            MAX_CMD_LEN,
            "iptables -w -A %s -o %s -i %s --set-counters %" PRId64"  %" PRId64"  -j RETURN",
            NatController::LOCAL_HW_TETHER_COUNTERS_CHAIN,
        stats.iif,
        stats.oif,
        stats.rxP,
        stats.rxB);
    ALOGD("cmd3: %s err=%s", cmd3, strerror(errno));
    hwiptOutput = popen(cmd3, "r");
    if (hwiptOutput != NULL) {
        pclose(hwiptOutput);
    }
    return 1;
}

void QtiBandwidthController::handleInotifchangeEvent() {
    FILE *fp;
    IpaTetherStats stat;
    bool flag = true;
    char fname[MAX_FILE_LEN];

    memset(fname,0,MAX_FILE_LEN);
    memcpy(fname ,IPA_TETHER_STATS,strlen(IPA_TETHER_STATS));
    fp = fopen(fname, "r");
    if (fp == NULL) {
        ALOGE("FATAL...fp is NULL ");
        return ;
    }
    do{
        memset(&stat,0,sizeof(struct ipaTetherStats));
        /*kindly match with /data/misc/ipa/tether_stats file */
        fscanf(fp, STATS_TEMPLATE, (char*)&(stat.iif),(char*)&(stat.oif),&stat.rxB,&stat.rxP,&stat.txB,&stat.txP);
        /*No need to updated when all zeros received for some reason from IPA CM*/
        if (stat.rxB > 0 || stat.rxP > 0 || stat.txB > 0 || stat.txP > 0) {
            updateipaTetherStats(stat,flag);
        }
        flag = false;
    }while(!feof(fp));
    fclose(fp);
}

void QtiBandwidthController::ipaTetherStatInit() {
     pthread_t tid;
     ALOGD("ipaTetherStatInit STARTED");

     if ((0 !=  pthread_create(&tid, NULL, QtiBandwidthController::ipaStatsMonitorThread, (void*)"Thread started")) != 0) {
         ALOGE(" ipaMonitorThread creation failed :%s",strerror(errno));
     }
     ALOGD("ipaTetherStatInit done tid: %d",(int)tid);
}

void* QtiBandwidthController::ipaStatsMonitorThread(void *obj) {
      int fd=0, wd=0;
      ALOGD("ipaStatsMonitorThread entry");

      fd = inotify_init();
      ALOGD("arg = %s ipaStatsMonitorThread ",(char*)obj);
      if (fd == -1) {
          ALOGE("%s: failed to init inotify:", __func__);
          return NULL;
      }
      wd = inotify_add_watch(fd, IPA_TETHER_STATS_DIR, IN_CLOSE_WRITE );
      if (wd == -1) {
          ALOGE("%s: inotify_add_watch failed:", __func__);
          return NULL;
      }
      handleInotifyEvent(fd,wd);

      if (fd > 0) {
          close(fd);
      }
      if (wd > 0) {
          inotify_rm_watch( fd, wd );
      }
      ALOGD("ipaStatsMonitorThread exit");
      return NULL;
}

bool QtiBandwidthController:: handleInotifyEvent(int fd,int wd) {

     const int inotifyEventSize = (int) ( sizeof (struct inotify_event) );
     const int inotifyNumEvents = 4;
     const int inotifyEventBufferLength =
       ( inotifyNumEvents * ( inotifyEventSize + 16 ) );
     char event_buffer[inotifyEventBufferLength];

     ALOGD("handleInotifyEvent entry fd:%d, wd:%d",fd,wd);
     while(1) {
         memset(event_buffer,0,inotifyEventBufferLength);
         ssize_t len = read(fd, event_buffer, inotifyEventBufferLength);
         if( len < 0 ) {
             if( errno == EINTR) {
                 ALOGE("could not read inotify event data..."
                     "need to reissue 'read' system call");
                 break;
             } else{
                  ALOGE("could not read inotify event data with error = %s",
                     strerror(errno));
                  break;
             }
         } else if( !len ) {
              ALOGE("could not read inotify event data "
                   "because the event buffer was too small");
              break;
         }
         int evt_buf_offset = 0;
         while ( evt_buf_offset < len)
         {
             struct inotify_event *cur_event;
             cur_event = (struct inotify_event *) &event_buffer[evt_buf_offset];
             if( cur_event->wd == wd && (cur_event->mask & IN_CLOSE_WRITE) ) {
                 if( (cur_event->len) &&
                     (strncmp(cur_event->name, IPA_TETHER_STATS_FILE, strlen(IPA_TETHER_STATS_FILE)) == 0) ) {
                         handleInotifchangeEvent();
                 }
             } else {
                 if ( cur_event->wd != wd ) {
                      ALOGE( "inotify event with watch descriptor=%d does not match"
                             " watch descriptor being monitored=%d", cur_event->wd, wd);
                 }
                 if ( (cur_event->mask & IN_CLOSE_WRITE) == 0 ) {
                       ALOGE( "inotify event was not a close or write event, "
                              "its mask= %d", cur_event->mask);
                 }
             }
             evt_buf_offset += inotifyEventSize + cur_event->len;
         }
     }
     return false;
}
