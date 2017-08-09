/*
   Copyright (c) 2017, The Linux Foundation. All rights reserved.

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

#include <sys/wait.h>
#include <cutils/log.h>
#include <logwrap/logwrap.h>
#include "android-base/stringprintf.h"
#include "android-base/strings.h"
#include "QtiDataController.h"
#include "NetdConstants.h"

using android::base::StringAppendF;
using android::base::StringPrintf;

static int iptablesRestoreFunction(IptablesTarget target, const std::string &commands) {
    return execIptablesRestore(target, commands);
}

bool prepare(std::string name) {
    std::string iptCmd = StringPrintf("*filter\n"
                                          "-D OUTPUT -j %s\n"
                                          ":%s - [0:0]\n"
                                          "COMMIT\n", name.c_str(), name.c_str());
    iptablesRestoreFunction(V4, iptCmd);
    return true;
}

bool enableMms(char *uids) {
    std::string name = "blacklist-mms";
    if (uids == NULL) {
        ALOGE("enableMms NULL point exit!");
        return false;
    }
    prepare(name);

    char *outer_ptr = NULL;
    char *s = strtok_r(uids, "|", &outer_ptr);

    std::string cmd = StringPrintf("*filter\n"
                                       ":%s - [0:0]\n"
                                       "-A OUTPUT -j %s\n",
                                   name.c_str(), name.c_str());
    while (s) {
        ALOGI("current uid is :%s", s);
        StringAppendF(&cmd,
                      "-A %s -d 10.0.0.200/32 -m owner --uid-owner %s -j DROP\n"
                          "-A %s -d 10.0.0.172/32 -m owner --uid-owner %s -j DROP\n",
                      name.c_str(), s, name.c_str(), s);
        s = strtok_r(NULL, "|", &outer_ptr);
    }
    StringAppendF(&cmd, "COMMIT\n");
    int res = iptablesRestoreFunction(V4, cmd);
    if (res) {
        ALOGE("enableMms res: %d, 200 exit!", res);
        return false;
    }
    return true;
}

