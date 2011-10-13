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

struct usb_target_pid_table
{
    const char *platform;
    const char *baseband;
    const char *pid;
    const char *functions;
};

class UsbController {

public:
    UsbController();
    virtual ~UsbController();

    int startRNDIS();
    int stopRNDIS();
    bool isRNDISStarted();

private:
    static struct usb_target_pid_table disableRNDIS_disableADB_list[];
    static struct usb_target_pid_table disableRNDIS_enableADB_list[];
    static struct usb_target_pid_table enableRNDIS_disableADB_list[];
    static struct usb_target_pid_table enableRNDIS_enableADB_list[];

    void select_pid_funcs(const char **, const char **, bool);
    int enableRNDIS(bool enable);
    void rndis_enable(bool);
    bool function_enabled(const char *);
};

#endif
