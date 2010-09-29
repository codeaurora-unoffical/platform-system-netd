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
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <unistd.h>
#include <signal.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#define LOG_TAG "UsbController"
#include <cutils/log.h>
#include <sysutils/NetlinkEvent.h>
#include <cutils/properties.h>

#include "UsbController.h"
#include "ResponseCode.h"

//Add timeout to reset USB event flag
#define WAIT_FOR_EVENT 5
bool ignoreEvent = false;
bool onBootUsbDisConnect = false;

UsbController *UsbController::sInstance = NULL;

UsbController *UsbController::Instance() {
    if (!sInstance)
        sInstance = new UsbController();
    return sInstance;
}


UsbController::UsbController() {
    mUsbConnected = false;
    mBroadcaster = NULL;
}

UsbController::~UsbController() {
}

int UsbController::start() {
    FILE *fp;
    char state[255];
    int ret = 0;

    if ((fp = fopen("/sys/devices/virtual/switch/MSM72K_UDC/state",
                    "r"))) {
        if (fgets(state, sizeof(state), fp)) {
            if (!strncmp(state, "online", 6)) {
                /*
                 * Set system property if the USB is connected on boot
                 * Since network management service would miss the USB
                 * events if the system is booted with USB connected
                 * Tethering makes use of this property to update usb status
                 * on boot completed
                 */
                 property_set("persist.usb.onboot","1");
                 notifyUsbConnected(true);
            } else {
                 property_set("persist.usb.onboot","0");
                 notifyUsbConnected(false);
              }
        } else {
             SLOGE("Failed to read switch state (%s)", strerror(errno));
             ret = -1;
          }
     fclose(fp);
  } else {
       SLOGW("No USB switch available");
    }
    return ret;
}

int UsbController::startRNDIS() {
    LOGD("Usb RNDIS start");
    return enableRNDIS(true);
}

void UsbController::notifyUsbConnected(bool connected) {
    char msg[255];

    if (connected) {
        mUsbConnected = true;
        SLOGD("UsbController: notifyUsbConnected: USB Connected");
    } else {
        mUsbConnected = false;
        SLOGD("UsbController: notifyUsbConnected: USB Not Connected");
    }
    snprintf(msg, sizeof(msg), "Share method usb now %s",
             (connected ? "available" : "unavailable"));

    getBroadcaster()->sendBroadcast(ResponseCode::UsbConnected,
                                    msg, false);
}

void UsbController::handleSwitchEvent(NetlinkEvent *evt) {
    const char *devpath = evt->findParam("DEVPATH");
    const char *name = evt->findParam("SWITCH_NAME");
    const char *state = evt->findParam("SWITCH_STATE");

    if (!name || !state) {
        SLOGW("Switch %s event missing name/state info", devpath);
        return;
    }

    /*
     * When event online or offline is sent, the file product_id is updated
     * with RNDIS or default composition values. Updating the product_id generates
     * USB offline and online events again, to avoid this loop of event generation
     * ignore offline event when composition is changed to RNDIS.
     */
    if (!strcmp(name, "MSM72K_UDC") && !ignoreEvent) {
        SLOGD("UsbController: handleSwitchEvent: Inside USB KERNEL EVENT");

        if (!strcmp(state, "online"))  {
            SLOGD("UsbController: handleSwitchEvent: USB ONLINE");
            notifyUsbConnected(true);
        } else {
            SLOGD("UsbController: handleSwitchEvent: USB OFFLINE");
            /*
             * During bootup, if USB is connected persist.usb.onboot is set
             * to '1', If the user removes the USB after this value is set
             * and before the boot is completed persist.usb.onboot would cause
             * a race condition since tethering service reads the value as '1'.
             * As we receive USB offline event when USB is unplugged reset the
             * value to '0' to avoid race condition
             */
            if (!onBootUsbDisConnect) {
                property_set("persist.usb.onboot","0");
                onBootUsbDisConnect = true;
             }
            notifyUsbConnected(false);
        }
    } else {
        SLOGW("Ignoring unknown switch '%s'", name);
    }
}

int UsbController::stopRNDIS() {
    LOGD("Usb RNDIS stop");
    return enableRNDIS(false);
}

void reset(int signo) {
    SLOGD("Usbcontroller: ALARM is generated");
    ignoreEvent = false;
}

int UsbController::enableRNDIS(bool enable) {
    char value[20];

    int fd = open("/sys/module/g_android/parameters/product_id", O_RDWR);

    /* Switch to RNDIS composition (Product id = F00E) When RNDIS is enabled.
     * Switch back to default composition (Product id = 9017) after RNDIS
     * is disabled.
     */ 
    int count = snprintf(value, sizeof(value), (enable ? "F00E\n" : "9017\n"));
    /*
     * Wait for the alarm to reset the ignoreEvent flag, we need to
     * ignore only the first offline event when composition is changed
     * to RNDIS
     */
    signal(SIGALRM,reset);
    if (enable) {
       /*
        * Ignore the first offline event when composition is changed to RNDIS
        * and set the alarm
        */
       ignoreEvent = true;
       alarm(WAIT_FOR_EVENT);
   }
    write(fd, value, count);
    close(fd);
    return 0;
}

bool UsbController::isRNDISStarted() {
    char value=0;
    int fd = open("/sys/devices/platform/android_usb/functions/rndis", O_RDONLY);
    read(fd, &value, 1);
    close(fd);
    return (value == '1' ? true : false);
}
