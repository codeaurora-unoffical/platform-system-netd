/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "UsbController.h"

#define USB_FUNCTIONS_PATH   "/sys/class/android_usb/android0/functions"
#define USB_ENABLE_PATH      "/sys/class/android_usb/android0/enable"
#define USB_PID_PATH         "/sys/class/android_usb/android0/idProduct"

UsbController::UsbController() {
}

UsbController::~UsbController() {
}

bool UsbController::function_enabled(const char *match)
{
    int fd, ret;
    char c[256];

    fd = open(USB_FUNCTIONS_PATH, O_RDONLY);
    if(fd < 0) {
        SLOGE("Error while opening the file %s : %s \n",
             USB_FUNCTIONS_PATH, strerror(errno));
        return false;
    }

    if(read(fd, c, sizeof(c)) < 0) {
        SLOGE("Error while reading the file %s : %s \n",
             USB_FUNCTIONS_PATH, strerror(errno));
        ret = false;
    }

    if(strstr(c, match)) {
        ret = true;
    } else {
        ret = false;
    }

    close(fd);
    return ret;
}

void UsbController::rndis_enable(bool enable)
{
    int fd, fd_enable;
    const char *pid, *funcs;

    if(enable == true) {
        if(function_enabled("adb") == true) {
            pid = "0x9024";
            funcs = "rndis,adb";
        } else {
            pid = "0xf00e";
            funcs = "rndis";
        }
    } else {
        if(function_enabled("adb") == true) {
            pid = "0x9025";
            funcs = "diag,adb,serial,rmnet,mass_storage";
        } else {
            pid = "0x9026";
            funcs = "diag,serial,rmnet,mass_storage";
        }
    }
    SLOGD("Enabling funcs:%s, pid:%s\n", funcs, pid);

    fd_enable = open(USB_ENABLE_PATH, O_WRONLY);
    if(fd_enable < 0) {
        SLOGE("Error while opening the file %s : %s \n",
             USB_ENABLE_PATH, strerror(errno));
        return;
    }
    if(write(fd_enable, "0", 2) < 0) {
        SLOGE("Error while reading the file %s : %s \n",
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
        SLOGE("Error while reading the file %s : %s \n",
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
        SLOGE("Error while reading the file %s : %s \n",
             USB_PID_PATH, strerror(errno));
        close(fd);
        close(fd_enable);
        return;
    }
    close(fd);

    if(write(fd_enable, "1", 2) < 0) {
        SLOGE("Error while reading the file %s : %s \n",
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
    int fd = open("/sys/class/usb_composite/rndis/enable", O_RDWR);
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
    int fd = open("/sys/class/usb_composite/rndis/enable", O_RDWR);
    if(fd < 0) {
        //Check for new ABI
        return function_enabled("rndis");
    }

    read(fd, &value, 1);
    close(fd);
    return (value == '1' ? true : false);
}
