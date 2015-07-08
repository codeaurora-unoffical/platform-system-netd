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
#ifndef _QTIBANDWIDTH_CONTROLLER_H
#define _QTIBANDWIDTH_CONTROLLER_H

#include <string.h>
#include <string>
#include <errno.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cutils/log.h>
#include <cutils/properties.h>
#include <logwrap/logwrap.h>
#include "BandwidthController.h"
#define MAX_FILE_LEN 200
typedef struct ipaTetherStats{
                  char oif[MAX_FILE_LEN];
                  char iif[MAX_FILE_LEN];
                  int64_t rxB;
                  int64_t rxP;
                  int64_t txB;
                  int64_t txP;
                }IpaTetherStats;

class QtiBandwidthController {
protected:
    static bool tetherRulesExist;

public:
    QtiBandwidthController();
    virtual ~QtiBandwidthController();

void ipaTetherStatInit();
static bool handleInotifyEvent(int fd,int wd);
static void handleInotifchangeEvent();
static int updateipaTetherStats(IpaTetherStats stats,bool flag);
static void *ipaStatsMonitorThread(void *args);

void clearPrevStats();
static std::string getPairName(IpaTetherStats temp);

static IpaTetherStats* FindSnapShotForPair(std::string intfPair);
static IpaTetherStats* FindPrevStatsForPair(std::string intfPair);
static IpaTetherStats calculateTetherStats(IpaTetherStats current);
static void updateSnapShot(IpaTetherStats temp);
static void updatePrevStats(IpaTetherStats current);
static IpaTetherStats getModifiedStats(IpaTetherStats* curr, IpaTetherStats* prev, IpaTetherStats* last);
static void dumpCache();
};

#endif
