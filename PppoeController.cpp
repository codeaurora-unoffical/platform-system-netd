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

#include <cutils/log.h>
#include <cutils/properties.h>

#include "PppoeController.h"
#include "ResponseCode.h"

#define PPPD_PATH "/system/bin/pppd"
#define LOG_TAG "PppoeController"
#define PPPOE_PID "/data/pppoe.pid"

static const int pppoeExit = 666;
pid_t            PppoeController::mPid;
NetlinkManager*  PppoeController::mNm;

PppoeController *PppoeController::sInstance = NULL;
PppoeController* PppoeController::Instance() {
    if(!sInstance) {
        sInstance = new PppoeController();
    }
    return sInstance;
}

PppoeController::PppoeController() {
    mPid = 0;
    mNm = NetlinkManager::Instance();
}

PppoeController::~PppoeController() {
    mPid = 0;
}

void PppoeController::notifyPppoeExited() {
    char err_code[PROPERTY_VALUE_MAX];
    property_get("pppd.errcode", err_code, "651");
    property_set("pppd.errcode", "");
    ALOGD("notifyPppoeExited err code is %s", err_code);

    char msg[254];
    sprintf(msg, "pppoe exited error code %s", err_code);
    mNm->getBroadcaster()->sendBroadcast(pppoeExit, msg, false);
}

void PppoeController::sigchld_interrupt(int sig) {
    int status = 0;
    if ((mPid != 0) && (waitpid(mPid, &status, WNOHANG)>0)) {
        if (WIFEXITED(status)) {
            status = WEXITSTATUS(status);
            ALOGD("pppd exited (status = %d)", status);
        }else {
            ALOGD("pppd was killed (pid = %d)", mPid);
        }
        (void)signal(SIGCHLD,SIG_DFL);
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

        sprintf(mtuString, "%d", config->mtu);
        sprintf(mruString, "%d", config->mru);
        sprintf(lcp_echo_interval_String, "%d", config->lcp_echo_interval);
        sprintf(lcp_echo_failure_String, "%d", config->lcp_echo_failure);
        sprintf(ptyString, "/system/xbin/pppoe -p %s -I %s -T %d -U -m %d",
                PPPOE_PID, config->interf, config->timeout, config->MSS);
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
        argv[i++] = NULL;

        if(execv(PPPD_PATH, argv)) {
            ALOGE("execv failed (%s)", strerror(errno));
        }
        ALOGE("Should never get here!");
        return -1;
    } else {
        mPid = pid;
        ALOGD("pid is %d", mPid);
    }
    return 0;
}

int PppoeController::kill_pppoe(void) {
    FILE *f;
    int pid = 0; /* pid number from pid file */

    if((f = fopen(PPPOE_PID, "r")) == 0) {
        fprintf(stderr, "Can't open pid file");
        return -1;
    }

    if(fscanf(f, "%d", &pid)!= 1) {
    }
    /* send signal SIGKILL to kill */
    if(pid > 0){
        kill(pid, SIGKILL);
    }

    fclose(f);
    // delete the pid file when killed
    remove(PPPOE_PID);
    return 0;
}

int PppoeController::stopPppoe() {

    if (mPid == 0) {
        ALOGE("PPPOE already stopped");
        return 0;
    }

    ALOGD("Stopping PPPOE services");
    // kill pppoe first
    kill_pppoe();
    kill(mPid, SIGKILL);
    ALOGD("PPPOE services stopped");
    return 0;
}

int PppoeController::setRoute(char* iface, char* gateway) {

    if (mPid == 0) {
        ALOGE("PPPOE already stopped");
        return 0;
    }
    char set_route[254];
    if(gateway != NULL) {
        sprintf(set_route, "ip route add default via %s dev %s", gateway, iface);
    } else {
        sprintf(set_route, "ip route add default dev %s", iface);
    }
    ALOGD("delete default route");
    system("ip route del default");
    ALOGD("add default route %s via %s", iface, gateway);
    system(set_route);

    return 0;
}
