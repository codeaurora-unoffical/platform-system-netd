/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <ctime>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <dirent.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define LOG_TAG "PppoeController"
#include <cutils/log.h>

#include "PppoeController.h"
#include "ResponseCode.h"

#define PPPD_PATH "/system/bin/pppd"

static const int pppoeExit = 666;
pid_t            mPid;
NetlinkManager*  mNm;

PppoeController::PppoeController() {
    mPid = 0;
    mNm = NetlinkManager::Instance();
}

PppoeController::~PppoeController() {
}

static void notifyPppoeExited() {
    int err_code;
    char err_string[254];
    FILE* fp = fopen("/sdcard/pppderror.txt", "r");
    if(fp == NULL) {
        err_code = 651;
    } else {
        int fp_size = 0;
        rewind(fp);
        while(!feof(fp)) {
            err_string[fp_size++] = fgetc(fp);;
        }
        err_string[fp_size] = '\0';
        char* p = err_string;
        if (!strncmp(p, "E=", 2)) {
            p += 2;
        }
        err_code = strtol(p, NULL, 10);
        fclose(fp);
    }
    remove("/sdcard/pppderror.txt");

    ALOGD("notifyPppoeExited err string is %s", err_string);
    ALOGD("notifyPppoeExited err code is %d", err_code);

    char msg[254];
    sprintf(msg, "pppoe exited error code %d", err_code);
    mNm->getBroadcaster()->sendBroadcast(pppoeExit, msg, false);
}

static void sigchld_interrupt(int signal) {
    int status = 0;
    if (signal == SIGCHLD && (waitpid(mPid, &status, WNOHANG)>0)) {
        if (WIFEXITED(status)) {
            status = WEXITSTATUS(status);
            ALOGE("pppd exited (status = %d)", status);
        }else {
            ALOGE("pppd was killed (pid = %d)", mPid);
        }
        mPid = 0;
        notifyPppoeExited();
    }
}

int PppoeController::startPppoe(PppoeConfig* config) {
    pid_t pid;
    int status;

    if (mPid) {
        ALOGE("Multiple PPPD instances not currently supported");
        errno = EBUSY;
        return -1;
    }

    (void)signal(SIGHUP, SIG_IGN);
    (void)signal(SIGINT, SIG_IGN);
    (void)signal(SIGPIPE, SIG_IGN);
    (void)signal(SIGCHLD, sigchld_interrupt);

    if ((pid = fork()) < 0) {
        ALOGE("fork failed (%s)", strerror(errno));
        return -1;
    }

    if (!pid) {
        int i=0;
        char *argv[30];
        char ptyString[254];
        char mtuString[32];
        char mruString[32];
        char lcp_echo_interval_String[32];
        char lcp_echo_failure_String[32];

        time_t now;
        struct tm *fmt;

        time(&now);
        fmt = localtime(&now);

        sprintf(mtuString, "%d", config->mtu);
        sprintf(mruString, "%d", config->mru);
        sprintf(lcp_echo_interval_String, "%d", config->lcp_echo_interval);
        sprintf(lcp_echo_failure_String, "%d", config->lcp_echo_failure);
        char debugfile[254];
        sprintf(debugfile, "/sdcard/ppp/pppoe-%d%d%d.txt", fmt->tm_hour, fmt->tm_min, fmt->tm_sec);
        sprintf(ptyString, "/system/bin/pppoe -p /etc/ppp/pid.pppoe -I %s -T %d -U -m %d -D %s", config->interf, config->timeout, config->MSS, debugfile);
        argv[i++] = PPPD_PATH;
        argv[i++] = "pty";
        argv[i++] = ptyString;
        argv[i++] = "noipdefault";
        argv[i++] = "noauth";
        argv[i++] = "default-asyncmap";
        argv[i++] = "nodefaultroute";
        argv[i++] = "hide-password";
        argv[i++] = "nodetach";
        argv[i++] = "usepeerdns";
        argv[i++] = "mtu";
        argv[i++] = mtuString;
        argv[i++] = "mru";
        argv[i++] = mruString;
        argv[i++] = "noaccomp";
        argv[i++] = "nodeflate";
        argv[i++] = "nopcomp";
        argv[i++] = "novj";
        argv[i++] = "novjccomp";
        argv[i++] = "user";
        argv[i++] = config->user;
        argv[i++] = "password";
        argv[i++] = config->pass;
        argv[i++] = "lcp-echo-interval";
        argv[i++] = lcp_echo_interval_String;
        argv[i++] = "lcp-echo-failure";
        argv[i++] = lcp_echo_failure_String;
        argv[i++] = "debug";
        argv[i++] = NULL;
        for(int j = 0; j < i; j++) {
            ALOGD("execv argv[%d] is %s", j, argv[j]);
        }
        if(execv(PPPD_PATH, argv)) {
            ALOGE("execv failed (%s)", strerror(errno));
        }
        ALOGE("Should never get here!");
        return -1;
    } else {
        mPid = pid;
        ALOGE("pid is %d", mPid);
    }
    return 0;
}

int PppoeController::stopPppoe() {

    if (mPid == 0) {
        ALOGE("PPPOE already stopped");
        return 0;
    }

    ALOGD("Stopping PPPOE services");
    kill(mPid, SIGTERM);
    //waitpid(mPid, NULL, 0);
    //mPid = 0;
    //ALOGD("PPPOE services stopped");
    return 0;
}

int PppoeController::setRoute(char* iface) {

    if (mPid == 0) {
        ALOGE("PPPOE already stopped");
        return 0;
    }
    char set_route[254];
    sprintf(set_route, "ip route add default dev %s", iface);
    ALOGD("delete default route");
    system("ip route del default");
    ALOGD("add default route %s", iface);
    system(set_route);

    return 0;
}
