/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2011, Code Aurora Forum. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>


#include <netinet/in.h>
#include <arpa/inet.h>

#define LOG_TAG "UsbController"
#include <cutils/log.h>
#include <cutils/properties.h>

#include "UsbController.h"

#define USB_FUNCTIONS_PATH      "/sys/class/android_usb/android0/functions"
#define USB_ENABLE_PATH         "/sys/class/android_usb/android0/enable"
#define USB_PID_PATH            "/sys/class/android_usb/android0/idProduct"
#define RNDIS_FUNCTION_ENABLE   "/sys/class/usb_composite/rndis/enable"

//List PIDs with RNDIS enabled; ADB disabled
struct usb_target_pid_table UsbController::enableRNDIS_disableADB_list[] = {
    { NULL,      "csfb",   "0x9041", "rndis,diag", },
    { NULL,      "svlte2", "0x9041", "rndis,diag", },
    { NULL,       NULL,    "0xf00e", "rndis", },      //default PID
};

//List PIDs with both RNDIS and ADB enabled
struct usb_target_pid_table UsbController::enableRNDIS_enableADB_list[] = {
    { NULL,      "csfb",   "0x9042", "rndis,diag,adb" },
    { NULL,      "svlte2", "0x9042", "rndis,diag,adb" },
    { NULL,       NULL,    "0x9024", "rndis,adb" },  //default PID
};

//List PIDs with RNDIS disabled; ADB enabled
struct usb_target_pid_table UsbController::disableRNDIS_enableADB_list[] = {
    { "msm8960",  NULL,    "0x9025", "diag,adb,serial,rmnet,mass_storage" },
    { NULL,      "csfb",   "0x9031", "diag,adb,serial,rmnet_sdio,mass_storage" },
    { NULL,      "svlte2", "0x9037", "diag,adb,serial,rmnet_smd_sdio,mass_storage" },
    { NULL,       NULL,    "0x9025", "diag,adb,serial,rmnet_smd,mass_storage" },
};

//List PIDs with both RNDIS and ADB disabled
struct usb_target_pid_table UsbController::disableRNDIS_disableADB_list[] = {
    { "msm8960",  NULL,    "0x9026", "diag,serial,rmnet,mass_storage" },
    { NULL,      "csfb",   "0x9032", "diag,serial,rmnet_sdio,mass_storage" },
    { NULL,      "svlte2", "0x9038", "diag,serial,rmnet_smd_sdio,mass_storage" },
    { NULL,       NULL,    "0x9026", "diag,serial,rmnet_smd,mass_storage" },
};

UsbController::UsbController() {
}

UsbController::~UsbController() {
}

bool UsbController::function_enabled(const char *match)
{
    int fd, ret, n_read;
    char c[256];

    fd = open(USB_FUNCTIONS_PATH, O_RDONLY);
    if(fd < 0) {
        SLOGE("Error while opening the file %s : %s \n",
             USB_FUNCTIONS_PATH, strerror(errno));
        return false;
    }

    n_read = read(fd, c, sizeof(c) - 1);
    if(n_read < 0) {
        SLOGE("Error while reading the file %s : %s \n",
             USB_FUNCTIONS_PATH, strerror(errno));
        close(fd);
        return false;
    }

    c[n_read] = '\0';
    if(strstr(c, match)) {
        ret = true;
    } else {
        ret = false;
    }

    close(fd);
    return ret;
}

void UsbController::select_pid_funcs(const char **pid, const char **funcs, bool rndis_enable)
{
    char target[PROPERTY_VALUE_MAX];
    char baseband[PROPERTY_VALUE_MAX];
    bool adb_enable;
    int count, i;
    struct usb_target_pid_table *pid_table;

    property_get("ro.board.platform", target, "");
    property_get("ro.baseband", baseband, "");

    adb_enable = function_enabled("adb");

    if(rndis_enable == true && adb_enable == true) {
        pid_table = enableRNDIS_enableADB_list;
        count = sizeof(enableRNDIS_enableADB_list);
    } else if(rndis_enable == true && adb_enable == false) {
        pid_table = enableRNDIS_disableADB_list;
        count = sizeof(enableRNDIS_disableADB_list);
    } else if(rndis_enable == false && adb_enable == true) {
        pid_table = disableRNDIS_enableADB_list;
        count = sizeof(disableRNDIS_enableADB_list);
    } else {
        pid_table = disableRNDIS_disableADB_list;
        count = sizeof(disableRNDIS_disableADB_list);
    }

    for( i = 0; i < count; i++)
    {
        if((((pid_table[i].platform == NULL)) ||
                (!strncmp(pid_table[i].platform, target, PROPERTY_VALUE_MAX))) &&
           ((pid_table[i].baseband == NULL) ||
                (!strncmp(pid_table[i].baseband, baseband, PROPERTY_VALUE_MAX)))) {
            *pid = pid_table[i].pid;
            *funcs = pid_table[i].functions;
            return;
        }
    }
    //shouldn't reach this point
    LOGE("Error while locating PID for device:%s, basebad:%s\n",
             target, baseband);
    *pid = "";
    *funcs = "";

    return;
}

void UsbController::rndis_enable(bool enable)
{
    int fd, fd_enable;
    const char *pid, *funcs;

    select_pid_funcs(&pid, &funcs, enable);

    SLOGD("Enabling USB funcs:%s, pid:%s\n", funcs, pid);

    fd_enable = open(USB_ENABLE_PATH, O_WRONLY);
    if(fd_enable < 0) {
        SLOGE("Error while opening the file %s : %s \n",
             USB_ENABLE_PATH, strerror(errno));
        return;
    }
    if(write(fd_enable, "0", 2) < 0) {
        SLOGE("Error while writing to the file %s : %s \n",
             USB_ENABLE_PATH, strerror(errno));
        close(fd_enable);
        return;
    }

    fd = open(USB_PID_PATH, O_WRONLY);
    if(fd < 0) {
        SLOGE("Error while opening the file %s : %s \n",
             USB_PID_PATH, strerror(errno));
        close(fd_enable);
        return;
    }
    if(write(fd, pid, strlen(pid) + 1) < 0) {
        SLOGE("Error while writing to the file %s : %s \n",
             USB_PID_PATH, strerror(errno));
        close(fd);
        close(fd_enable);
        return;
    }
    close(fd);

    fd = open(USB_FUNCTIONS_PATH, O_WRONLY);
    if(fd < 0) {
        SLOGE("Error while opening the file %s : %s \n",
             USB_FUNCTIONS_PATH, strerror(errno));
        close(fd_enable);
        return;
    }
    if(write(fd, funcs, strlen(funcs) + 1) < 0) {
        SLOGE("Error while writing to the file %s : %s \n",
             USB_FUNCTIONS_PATH, strerror(errno));
        close(fd);
        close(fd_enable);
        return;
    }
    close(fd);

    if(write(fd_enable, "1", 2) < 0) {
        SLOGE("Error while writing to the file %s : %s \n",
             USB_ENABLE_PATH, strerror(errno));
        close(fd_enable);
        return;
    }

    close(fd_enable);
}


int UsbController::startRNDIS() {
    LOGD("Usb RNDIS start");
    return enableRNDIS(true);
}

int UsbController::stopRNDIS() {
    LOGD("Usb RNDIS stop");
    return enableRNDIS(false);
}

int UsbController::enableRNDIS(bool enable) {
    char value[20];
    int fd = open(RNDIS_FUNCTION_ENABLE, O_RDWR);
    int count = snprintf(value, sizeof(value), "%d\n", (enable ? 1 : 0));
    if(fd < 0) {
        //Check for new ABI
        rndis_enable(enable);
        return 0;
    }

    write(fd, value, count);
    close(fd);
    return 0;
}

bool UsbController::isRNDISStarted() {
    char value=0;
    int fd = open(RNDIS_FUNCTION_ENABLE, O_RDWR);
    if(fd < 0) {
        //Check for new ABI
        return function_enabled("rndis");
    }

    read(fd, &value, 1);
    close(fd);
    return (value == '1' ? true : false);
}
