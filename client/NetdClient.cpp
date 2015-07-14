/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "NetdClient.h"

#include "Fwmark.h"
#include "FwmarkClient.h"
#include "FwmarkCommand.h"
#include "resolv_netid.h"

#include <atomic>
#include <sys/socket.h>
#include <unistd.h>
#include<dlfcn.h>

#include <sys/system_properties.h>
#include <stdlib.h>
#define LOG_TAG "zerobalance"
#include <cutils/log.h>
#include <android/log.h>

static const char *BackgroundDataProperty = "sys.background.data.disable";
static const char *BackgroundDataWhitelist = "sys.background.exception.app";

namespace {

std::atomic_uint netIdForProcess(NETID_UNSET);
std::atomic_uint netIdForResolv(NETID_UNSET);

typedef int (*Accept4FunctionType)(int, sockaddr*, socklen_t*, int);
typedef int (*ConnectFunctionType)(int, const sockaddr*, socklen_t);
typedef int (*SocketFunctionType)(int, int, int);
typedef unsigned (*NetIdForResolvFunctionType)(unsigned);
typedef void (*SetConnectFunc) (ConnectFunctionType* func);
typedef void (*SetSocketFunc) (SocketFunctionType* func);

// These variables are only modified at startup (when libc.so is loaded) and never afterwards, so
// it's okay that they are read later at runtime without a lock.
Accept4FunctionType libcAccept4 = 0;
ConnectFunctionType libcConnect = 0;
SocketFunctionType libcSocket = 0;
ConnectFunctionType propConnect = 0;
SocketFunctionType propSocket = 0;
SetConnectFunc setConnect = 0;
SetSocketFunc setSocket = 0;


static void *propClientHandle = 0;

int closeFdAndSetErrno(int fd, int error) {
    close(fd);
    errno = -error;
    return -1;
}

int netdClientAccept4(int sockfd, sockaddr* addr, socklen_t* addrlen, int flags) {
    int acceptedSocket = libcAccept4(sockfd, addr, addrlen, flags);
    if (acceptedSocket == -1) {
        return -1;
    }
    int family;
    if (addr) {
        family = addr->sa_family;
    } else {
        socklen_t familyLen = sizeof(family);
        if (getsockopt(acceptedSocket, SOL_SOCKET, SO_DOMAIN, &family, &familyLen) == -1) {
            return closeFdAndSetErrno(acceptedSocket, -errno);
        }
    }
    if (FwmarkClient::shouldSetFwmark(family)) {
        FwmarkCommand command = {FwmarkCommand::ON_ACCEPT, 0, 0};
        if (int error = FwmarkClient().send(&command, sizeof(command), acceptedSocket)) {
            return closeFdAndSetErrno(acceptedSocket, error);
        }
    }
    return acceptedSocket;
}

int netdClientConnect(int sockfd, const sockaddr* addr, socklen_t addrlen) {
    if (sockfd >= 0 && addr && FwmarkClient::shouldSetFwmark(addr->sa_family)) {
        FwmarkCommand command = {FwmarkCommand::ON_CONNECT, 0, 0};
        if (int error = FwmarkClient().send(&command, sizeof(command), sockfd)) {
            errno = -error;
            return -1;
        }
    }

    if ((propConnect) && FwmarkClient::shouldSetFwmark(addr->sa_family)) {
      return propConnect(sockfd, addr, addrlen);
    }

    // fallback to libc
    return libcConnect(sockfd, addr, addrlen);
}

int netdClientSocket(int domain, int type, int protocol) {
    int socketFd = -1;
    if (propSocket) {
      socketFd = propSocket(domain, type, protocol);
    } else if (libcSocket) {
      socketFd = libcSocket(domain, type, protocol);
    }

    if (-1 == socketFd) {
      return -1;
    }

    unsigned netId = netIdForProcess;
    if (netId != NETID_UNSET && FwmarkClient::shouldSetFwmark(domain)) {
        if (int error = setNetworkForSocket(netId, socketFd)) {
            return closeFdAndSetErrno(socketFd, error);
        }
    }

    return socketFd;
}

int checkAppInWhitelist() {
    //Zero balance handling
    uid_t ruid = getuid();

    static char property[PROP_VALUE_MAX];
    if (__system_property_get(BackgroundDataProperty, property) > 0) {
        if (strcmp(property, "true") == 0) {
            ALOGE(":checkAppInWhitelist:Hit zero balance ");

            //if it is in whitelist allow
            static char appUid[PROP_VALUE_MAX];
            if (__system_property_get(BackgroundDataWhitelist, appUid) > 0) {
                short found = 0;
                //Get all the uids for exception, parse
                char *token = strtok (appUid,",");
                while (token != NULL) {
                    if (((uid_t)atoi(token) == ruid)) {
                        ALOGE(":checkAppInWhitelist:in whitelist allow : %u",ruid);
                        found = 1;
                        break;
                    }
                    token = strtok (NULL, ",");
                }

                if(found == 0) {
                    ALOGE(":checkAppInWhitelist:not in whitelist: %u",ruid);
                    return 0;
                }
            }
        }
    }
    return 1;
}

unsigned getNetworkForResolv(unsigned netId) {
    //Check whether app in white list
    //if it is not, then net id unset
    if(0 == checkAppInWhitelist())
        return NETID_UNSET;

    if (netId != NETID_UNSET) {
        return netId;
    }
    netId = netIdForProcess;
    if (netId != NETID_UNSET) {
        return netId;
    }
    return netIdForResolv;
}

int setNetworkForTarget(unsigned netId, std::atomic_uint* target) {
    if (netId == NETID_UNSET) {
        *target = netId;
        return 0;
    }
    // Verify that we are allowed to use |netId|, by creating a socket and trying to have it marked
    // with the netId. Call libcSocket() directly; else the socket creation (via netdClientSocket())
    // might itself cause another check with the fwmark server, which would be wasteful.
    int socketFd;
    if (libcSocket) {
        socketFd = libcSocket(AF_INET6, SOCK_DGRAM, 0);
    } else {
        socketFd = socket(AF_INET6, SOCK_DGRAM, 0);
    }
    if (socketFd < 0) {
        return -errno;
    }
    int error = setNetworkForSocket(netId, socketFd);
    if (!error) {
        *target = netId;
    }
    close(socketFd);
    return error;
}

}  // namespace

// accept() just calls accept4(..., 0), so there's no need to handle accept() separately.
extern "C" void netdClientInitAccept4(Accept4FunctionType* function) {
    if (function && *function) {
        libcAccept4 = *function;
        *function = netdClientAccept4;
    }
}

extern "C" void netdClientInitConnect(ConnectFunctionType* function) {
    if (function && *function) {
        libcConnect = *function;
        *function = netdClientConnect;
    }

    if (!propClientHandle) {
        propClientHandle = dlopen("libvendorconn.so", RTLD_LAZY);
    }
    if (!propClientHandle) {
       return;
    }

    propConnect = reinterpret_cast<ConnectFunctionType>(dlsym(propClientHandle, "vendorConnect"));

    setConnect = reinterpret_cast<SetConnectFunc>(dlsym(propClientHandle, "setConnectFunc"));
    // Pass saved libc handle so it can be called from lib
    if (setConnect && libcConnect) {
        setConnect(&libcConnect);
    }
}

extern "C" void netdClientInitSocket(SocketFunctionType* function) {
    if (function && *function) {
        libcSocket = *function;
        *function = netdClientSocket;
    }

    if (!propClientHandle) {
        propClientHandle = dlopen("libvendorconn.so", RTLD_LAZY);
    }
    if (!propClientHandle) {
       return;
    }

    propSocket = reinterpret_cast<SocketFunctionType>(dlsym(propClientHandle, "vendorSocket"));
    setSocket = reinterpret_cast<SetSocketFunc>(dlsym(propClientHandle, "setSocketFunc"));
    // Pass saved libc handle so it can be called from lib
    if (setSocket && libcSocket) {
        setSocket(&libcSocket);
    }

}

extern "C" void netdClientInitNetIdForResolv(NetIdForResolvFunctionType* function) {
    if (function) {
        *function = getNetworkForResolv;
    }
}

extern "C" int getNetworkForSocket(unsigned* netId, int socketFd) {
    if (!netId || socketFd < 0) {
        return -EBADF;
    }
    Fwmark fwmark;
    socklen_t fwmarkLen = sizeof(fwmark.intValue);
    if (getsockopt(socketFd, SOL_SOCKET, SO_MARK, &fwmark.intValue, &fwmarkLen) == -1) {
        return -errno;
    }
    *netId = fwmark.netId;
    return 0;
}

extern "C" unsigned getNetworkForProcess() {
    return netIdForProcess;
}

extern "C" int setNetworkForSocket(unsigned netId, int socketFd) {
    if (socketFd < 0) {
        return -EBADF;
    }
    FwmarkCommand command = {FwmarkCommand::SELECT_NETWORK, netId, 0};
    return FwmarkClient().send(&command, sizeof(command), socketFd);
}

extern "C" int setNetworkForProcess(unsigned netId) {
    return setNetworkForTarget(netId, &netIdForProcess);
}

extern "C" int setNetworkForResolv(unsigned netId) {
    return setNetworkForTarget(netId, &netIdForResolv);
}

extern "C" int protectFromVpn(int socketFd) {
    if (socketFd < 0) {
        return -EBADF;
    }
    FwmarkCommand command = {FwmarkCommand::PROTECT_FROM_VPN, 0, 0};
    return FwmarkClient().send(&command, sizeof(command), socketFd);
}

extern "C" int setNetworkForUser(uid_t uid, int socketFd) {
    if (socketFd < 0) {
        return -EBADF;
    }
    FwmarkCommand command = {FwmarkCommand::SELECT_FOR_USER, 0, uid};
    return FwmarkClient().send(&command, sizeof(command), socketFd);
}
