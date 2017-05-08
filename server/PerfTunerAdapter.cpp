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

#define LOG_TAG "PerfTunerAdapter"

#include <cutils/log.h>
#include <cutils/properties.h>
#include <dlfcn.h>
#include <sysutils/SocketClient.h>
#include <sysutils/SocketListener.h>

#include "CommandListener.h"
#include "NetdCommand.h"
#include "PerfTunerAdapter.h"
#include "ResponseCode.h"

void *_libNetPerfTunerHandle = NULL;
void (*_initPerfTunerExtension) (SocketListener*) = NULL;
int (*_runPerfTunerCmd) (SocketClient*, int, char**) = NULL;

void initPerfTunerLibrary() {
    if (!_libNetPerfTunerHandle) {
        _libNetPerfTunerHandle = dlopen("libnetperftuner.so", RTLD_NOW);
        if (_libNetPerfTunerHandle) {
            *(void **)&_initPerfTunerExtension =
                    dlsym(_libNetPerfTunerHandle, "initExtension");
            *(void **)&_runPerfTunerCmd =
                    dlsym(_libNetPerfTunerHandle, "runPerfTunerCmd");
            ALOGD("Successfully loaded libnetperftuner");
        } else {
            ALOGI("Failed to open libnetperftuner, "
                    "some features may not be present.");
        }
    }
}

NetdCommand* getPerfTunerCmd(CommandListener *broadcaster) {
    initPerfTunerLibrary();
    if (_initPerfTunerExtension) _initPerfTunerExtension(broadcaster);
    return (new PerfTunerCommand)->asNetdCommand();
}

NetdCommand *PerfTunerCommand::asNetdCommand() {
    return static_cast<NetdCommand*>(this);
}

int PerfTunerCommand::runCommand
(
    SocketClient *cli,
    int argc,
    char **argv
) {
    if (_runPerfTunerCmd)
        return _runPerfTunerCmd(cli, argc, argv);
    cli->sendMsg(ResponseCode::OperationFailed, "perftuner extension not loaded", false);
    return 0;
}
