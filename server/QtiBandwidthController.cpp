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

#define LOG_NDEBUG 0

#include <stdlib.h>
#include <errno.h>
#include <sys/inotify.h>
#include <map>
#include <string>
#include <sys/types.h>
#define LOG_TAG "QtiBandwidthController"
#include <cutils/log.h>
#include "QtiBandwidthController.h"
#include <sys/socket.h>
#include <sys/stat.h>
#include "NatController.h"

#define MAX_CMD_LEN  1024

 std::map<std::string, IpaTetherStats> prevStatsForPair;
 std::map<std::string, IpaTetherStats> LastSnapShotForPair;
 char pair_name[2*MAX_CMD_LEN+1];



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
    hwiptOutput = popen(cmd3, "r");
    ALOGD("cmd3: %s err=%s", cmd3, strerror(errno));
    if (hwiptOutput != NULL) {
        pclose(hwiptOutput);
    }
    return 1;
}
void QtiBandwidthController::clearPrevStats() {
    /*
     When interface down, LastSnapShotForPair already recorded
     so no need to have prevStatsForPair
     and prevStatsForPair should be cleared off
    */
    prevStatsForPair.clear();
    ALOGD("clearPrevStats prevStatsForPair is cleared now");
}
/*
  will return from prevStatsForPair

*/

IpaTetherStats* QtiBandwidthController::FindSnapShotForPair(std::string intfPair) {
    std::map<std::string, IpaTetherStats>::iterator it;
    ALOGD("FindSnapShotForPair pair_name:%s",intfPair.c_str());
    it = LastSnapShotForPair.find(intfPair);
        if (it != LastSnapShotForPair.end()) {
            return &(it->second);
        }
        return NULL;
}
/*
  will return from LastSnapShotForPair

*/

IpaTetherStats* QtiBandwidthController::FindPrevStatsForPair(std::string intfPair) {
    std::map<std::string, IpaTetherStats>::iterator it;
    ALOGD("FindPrevStatsForPair pair_name:%s",intfPair.c_str());
    it = prevStatsForPair.find(intfPair);
    if (it != prevStatsForPair.end()) {
        return &(it->second);
    }
    return NULL;
}

/*
get the matching stats(from LastSnapShotForPair)
and update it
*/

void QtiBandwidthController::updateSnapShot(IpaTetherStats temp) {
    std::string intfPair;

    intfPair = getPairName(temp);
    std::map<std::string, IpaTetherStats>::iterator it;
    ALOGD("updateSnapShot find key %s",intfPair.c_str());
    dumpCache();
    it = LastSnapShotForPair.find(intfPair);
    if (it != LastSnapShotForPair.end()) {
        it->second = temp;
    } else {
        LastSnapShotForPair.insert(
                std::pair<std::string, IpaTetherStats>(intfPair, temp));
    ALOGD("inserted into updateSnapShot %s",intfPair.c_str());
    }
}

void QtiBandwidthController::updatePrevStats(IpaTetherStats current) {
    std::string intfPair;

    intfPair = getPairName(current);
    ALOGD("updatePrevStats find key %s",intfPair.c_str());
    std::map<std::string, IpaTetherStats>::iterator it;
    dumpCache();
    it = prevStatsForPair.find(intfPair);
    if (it != prevStatsForPair.end()) {
        it->second = current;
    } else {
        prevStatsForPair.insert(
                std::pair<std::string, IpaTetherStats>(intfPair, current));
    ALOGD("inserted into updatePrevStats %s", intfPair.c_str());
    }

}

IpaTetherStats QtiBandwidthController::getModifiedStats(IpaTetherStats* curr, IpaTetherStats* prev, IpaTetherStats* last) {
    IpaTetherStats final;
    /* if prev , last not found , zero returned so no issue */

    final.rxB= curr->rxB-prev->rxB+last->rxB;
    final.txB= curr->txB-prev->txB+last->txB;

    final.rxP= curr->rxP-prev->rxP+last->rxP;
    final.txP= curr->txP-prev->txP+last->txP;

    return final; /* local variable returning TODO */
}

std::string QtiBandwidthController:: getPairName(IpaTetherStats temp) {

        memset(pair_name,0,MAX_FILE_LEN);
        sprintf(pair_name, "%s_%s", (char*)temp.iif, (char*)temp.oif); /* KW error snprintf() ?? */
        std::string tetherPairName=std::string(pair_name);
        ALOGD("getPairName tetherPairName:%s",tetherPairName.c_str());
        return tetherPairName;
}
/*
Final = current-prev+lastsnapshot
*/
IpaTetherStats QtiBandwidthController::calculateTetherStats(IpaTetherStats current) {
        IpaTetherStats *prev;
        IpaTetherStats *lastsnap;
        IpaTetherStats FinalStats;
        IpaTetherStats dummy;
        std::string intfPair;
        intfPair = getPairName(current);
        memset(&dummy,0,sizeof(struct ipaTetherStats));
        prev      = FindPrevStatsForPair(intfPair);
        lastsnap  = FindSnapShotForPair(intfPair);

	if(prev == NULL)
            prev = &dummy;
        if(lastsnap == NULL)
            lastsnap = &dummy;
        FinalStats = getModifiedStats(&current,prev,lastsnap);

	memcpy(FinalStats.oif,current.oif,MAX_FILE_LEN);
        memcpy(FinalStats.iif,current.iif,MAX_FILE_LEN);

        updateSnapShot(FinalStats);
        updatePrevStats(current);

        return FinalStats; /* local variable returning TODO */
}

void QtiBandwidthController::handleInotifchangeEvent() {
    FILE *fp;
    IpaTetherStats stat;
    IpaTetherStats FinalStats;
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
        memset(&FinalStats,0,sizeof(struct ipaTetherStats));
        /*kindly match with /data/misc/ipa/tether_stats file */
        fscanf(fp, STATS_TEMPLATE, (char*)&(stat.iif),(char*)&(stat.oif),&stat.rxB,&stat.rxP,&stat.txB,&stat.txP);
        /*No need to updated when all zeros received for some reason from IPA CM*/
        if (stat.rxB > 0 || stat.rxP > 0 || stat.txB > 0 || stat.txP > 0) {
            FinalStats = calculateTetherStats(stat);
            updateipaTetherStats(FinalStats,flag);
        }
        flag = false;
    }while(!feof(fp));
    ALOGD("out of while loop");
    fclose(fp);
    ALOGD("fclose done in handlenotifchangeEvent");
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

void QtiBandwidthController::dumpCache() {

                for (std::map<std::string, IpaTetherStats>::iterator cacheitr =
                        prevStatsForPair.begin();
                        cacheitr != prevStatsForPair.end(); ++cacheitr) {
                }
                for (std::map<std::string, IpaTetherStats>::iterator cacheitr1 =
                        LastSnapShotForPair.begin();
                                        cacheitr1 != LastSnapShotForPair.end(); ++cacheitr1) {
                                }
      }
