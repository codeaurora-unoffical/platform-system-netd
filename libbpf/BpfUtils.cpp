/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sstream>
#include <string>

#include <android-base/stringprintf.h>
#include <android-base/unique_fd.h>
#include <netdutils/Slice.h>
#include <netdutils/StatusOr.h>
#include "bpf/BpfUtils.h"

using android::base::StringPrintf;
using android::base::unique_fd;
using android::netdutils::Slice;
using android::netdutils::statusFromErrno;
using android::netdutils::StatusOr;

namespace android {
namespace bpf {

int bpf(int cmd, Slice bpfAttr) {
    return syscall(__NR_bpf, cmd, bpfAttr.base(), bpfAttr.size());
}

int createMap(bpf_map_type map_type, uint32_t key_size, uint32_t value_size, uint32_t max_entries,
              uint32_t map_flags) {
    bpf_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.map_type = map_type;
    attr.key_size = key_size;
    attr.value_size = value_size;
    attr.max_entries = max_entries;
    attr.map_flags = map_flags;

    return bpf(BPF_MAP_CREATE, Slice(&attr, sizeof(attr)));
}

int writeToMapEntry(const base::unique_fd& map_fd, void* key, void* value, uint64_t flags) {
    bpf_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.map_fd = map_fd.get();
    attr.key = ptr_to_u64(key);
    attr.value = ptr_to_u64(value);
    attr.flags = flags;

    return bpf(BPF_MAP_UPDATE_ELEM, Slice(&attr, sizeof(attr)));
}

int findMapEntry(const base::unique_fd& map_fd, void* key, void* value) {
    bpf_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.map_fd = map_fd.get();
    attr.key = ptr_to_u64(key);
    attr.value = ptr_to_u64(value);

    return bpf(BPF_MAP_LOOKUP_ELEM, Slice(&attr, sizeof(attr)));
}

int deleteMapEntry(const base::unique_fd& map_fd, void* key) {
    bpf_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.map_fd = map_fd.get();
    attr.key = ptr_to_u64(key);

    return bpf(BPF_MAP_DELETE_ELEM, Slice(&attr, sizeof(attr)));
}

int getNextMapKey(const base::unique_fd& map_fd, void* key, void* next_key) {
    bpf_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.map_fd = map_fd.get();
    attr.key = ptr_to_u64(key);
    attr.next_key = ptr_to_u64(next_key);

    return bpf(BPF_MAP_GET_NEXT_KEY, Slice(&attr, sizeof(attr)));
}

int bpfProgLoad(bpf_prog_type prog_type, Slice bpf_insns, const char* license,
                uint32_t kern_version, Slice bpf_log) {
    bpf_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.prog_type = prog_type;
    attr.insns = ptr_to_u64(bpf_insns.base());
    attr.insn_cnt = bpf_insns.size() / sizeof(struct bpf_insn);
    attr.license = ptr_to_u64((void*)license);
    attr.log_buf = ptr_to_u64(bpf_log.base());
    attr.log_size = bpf_log.size();
    attr.log_level = DEFAULT_LOG_LEVEL;
    attr.kern_version = kern_version;
    int ret = bpf(BPF_PROG_LOAD, Slice(&attr, sizeof(attr)));

    if (ret < 0) {
        std::string prog_log = netdutils::toString(bpf_log);
        std::istringstream iss(prog_log);
        for (std::string line; std::getline(iss, line);) {
            ALOGE("%s", line.c_str());
        }
    }
    return ret;
}

int mapPin(const base::unique_fd& map_fd, const char* pathname) {
    bpf_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.pathname = ptr_to_u64((void*)pathname);
    attr.bpf_fd = map_fd.get();

    return bpf(BPF_OBJ_PIN, Slice(&attr, sizeof(attr)));
}

int mapRetrieve(const char* pathname, uint32_t flag) {
    bpf_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.pathname = ptr_to_u64((void*)pathname);
    attr.file_flags = flag;
    return bpf(BPF_OBJ_GET, Slice(&attr, sizeof(attr)));
}

int attachProgram(bpf_attach_type type, uint32_t prog_fd, uint32_t cg_fd) {
    bpf_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.target_fd = cg_fd;
    attr.attach_bpf_fd = prog_fd;
    attr.attach_type = type;

    return bpf(BPF_PROG_ATTACH, Slice(&attr, sizeof(attr)));
}

int detachProgram(bpf_attach_type type, uint32_t cg_fd) {
    bpf_attr attr;
    memset(&attr, 0, sizeof(attr));
    attr.target_fd = cg_fd;
    attr.attach_type = type;

    return bpf(BPF_PROG_DETACH, Slice(&attr, sizeof(attr)));
}

uint64_t getSocketCookie(int sockFd) {
    uint64_t sock_cookie;
    socklen_t cookie_len = sizeof(sock_cookie);
    int res = getsockopt(sockFd, SOL_SOCKET, SO_COOKIE, &sock_cookie, &cookie_len);
    if (res < 0) {
        res = -errno;
        ALOGE("Failed to get socket cookie: %s\n", strerror(errno));
        errno = -res;
        // 0 is an invalid cookie. See sock_gen_cookie.
        return NONEXISTENT_COOKIE;
    }
    return sock_cookie;
}

StatusOr<unique_fd> setUpBPFMap(uint32_t key_size, uint32_t value_size, uint32_t map_size,
                                const char* path, bpf_map_type map_type) {
    int ret;
    base::unique_fd map_fd;
    ret = access(path, R_OK);
    /* Check the pinned location first to check if the map is already there.
     * otherwise create a new one.
     */
    if (ret == 0) {
        map_fd = base::unique_fd(mapRetrieve(path, 0));
        if (map_fd < 0) {
            return statusFromErrno(errno, StringPrintf("pinned map not accessible or does not "
                                                       "exist: (%s)\n",
                                                       path));
        }
    } else if (ret < 0 && errno == ENOENT) {
        map_fd = base::unique_fd(createMap(map_type, key_size, value_size, map_size, BPF_F_NO_PREALLOC));
        if (map_fd < 0) {
            return statusFromErrno(errno, StringPrintf("map create failed!: %s", path));
        }
        ret = mapPin(map_fd, path);
        if (ret) {
            return statusFromErrno(errno, StringPrintf("bpf map pin(%d, %s)", map_fd.get(), path));
        }
    } else {
        return statusFromErrno(errno, StringPrintf("pinned map not accessible: %s", path));
    }
    return map_fd;
}

bool hasBpfSupport() {
    struct utsname buf;
    int kernel_version_major;
    int kernel_version_minor;

    int ret = uname(&buf);
    if (ret) {
        return false;
    }
    char dummy;
    ret = sscanf(buf.release, "%d.%d%c", &kernel_version_major, &kernel_version_minor, &dummy);
    return (ret >= 2 && ((kernel_version_major > 4) ||
                         (kernel_version_major == 4 && kernel_version_minor >= 9)));
}

}  // namespace bpf
}  // namespace android
