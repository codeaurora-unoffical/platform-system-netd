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

#ifndef _USB_CONTROLLER_H
#define _USB_CONTROLLER_H

#include <linux/in.h>
#include <utils/List.h>
#include <sysutils/SocketListener.h>

class NetlinkEvent;

class UsbController {

private :
    static UsbController *sInstance;

private:
    SocketListener        *mBroadcaster;
    bool                   mUsbConnected;

public:
    int start();
    void handleSwitchEvent(NetlinkEvent *evt);

    void notifyUsbConnected(bool connected);

    void setBroadcaster(SocketListener *sl) { mBroadcaster = sl; }
    SocketListener *getBroadcaster() { return mBroadcaster; }

    static UsbController *Instance();

    UsbController();
    virtual ~UsbController();

    int startRNDIS();
    int stopRNDIS();
    bool isRNDISStarted();

private:
    int enableRNDIS(bool enable);
};

#endif
