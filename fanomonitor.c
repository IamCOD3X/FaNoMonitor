/*
 * FanoMonitor - Android Fanotify File Access Monitor
 *
 * Built by:     IamCOD3X
 * GitHub:       https://www.github.com/IamCOD3X
 * Website:      https://iamcod3x.com
 * Date:         September 27, 2025
 *
 * Description:
 *   This binary monitors file system access events on Android using fanotify,
 *   logs to logcat, app storage, and sends events to a Unix domain socket.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <limits.h>
#include "fanotify_compat.h"
#include <android/log.h>

#define LOG_TAG "FANOMonitor"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

#define FANOTIFY_BUFFER_SIZE 8192

static int running = 1;

/* Graceful shutdown */
void handle_signal(int sig) {
    LOGI("Received signal %d, shutting down...", sig);
    running = 0;
}

/* Resolve file path from FD */
static char *get_file_path_from_fd(int fd, char *buf, size_t size) {
    if (fd <= 0) return NULL;
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "/proc/self/fd/%d", fd);
    ssize_t len = readlink(tmp, buf, size - 1);
    if (len < 0) return NULL;
    buf[len] = '\0';
    return buf;
}

/* Get process name from PID */
static char *get_process_name_from_pid(pid_t pid, char *buf, size_t size) {
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "/proc/%d/cmdline", pid);
    int fd = open(tmp, O_RDONLY);
    if (fd < 0) return NULL;
    ssize_t len = read(fd, buf, size - 1);
    close(fd);
    if (len <= 0) return NULL;
    buf[len] = '\0';
    return buf;
}

static int get_uid_from_pid(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;

    char line[256];
    int uid = -1;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "Uid:", 4) == 0) {
            sscanf(line, "Uid:\t%d", &uid);
            break;
        }
    }
    fclose(fp);
    return uid;
}

const char* resolve_package_from_uid(int uid, char *out, size_t outSize) {
    FILE *fp = fopen("/data/system/packages.list", "r");
    if (!fp) return NULL;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        int pkgUid;
        char pkgName[256];
        if (sscanf(line, "%255s %d", pkgName, &pkgUid) == 2) {
            if (pkgUid == uid) {
                strncpy(out, pkgName, outSize);
                fclose(fp);
                return out;
            }
        }
    }
    fclose(fp);
    return NULL;
}

static void send_event(int sockfd, struct fanotify_event_metadata *meta) {
    char path[PATH_MAX] = {0};
    char proc[256] = {0};
    char *etype = NULL;

    if (meta->mask & FAN_OPEN) etype = "OPEN";
    else if (meta->mask & FAN_ACCESS) etype = "ACCESS";
    else if (meta->mask & FAN_MODIFY) etype = "MODIFY";
    else if (meta->mask & FAN_CLOSE_WRITE) etype = "CLOSE_WRITE";
    else if (meta->mask & FAN_CLOSE_NOWRITE) etype = "CLOSE_NOWRITE";
    else if (meta->mask & FAN_CREATE) etype = "CREATE";
    else if (meta->mask & FAN_DELETE) etype = "DELETE";
    else etype = "OTHER";

    const char *fpath = get_file_path_from_fd(meta->fd, path, sizeof(path)) ? path : "unknown";
    const char *pname = get_process_name_from_pid(meta->pid, proc, sizeof(proc)) ? proc : "unknown";

    int uid = get_uid_from_pid(meta->pid);
    long ts = (long)time(NULL) * 1000L;
    
    char pkgName[256] = "unknown";
    resolve_package_from_uid(uid, pkgName, sizeof(pkgName));

    char line[2048];
    snprintf(line, sizeof(line), "%ld|PID=%d|UID=%d|PROC=%s|PATH=%s|TYPE=%s|PKG=%s",
             ts, meta->pid, uid, pname, fpath, etype, pkgName);

    // 🔹 Send to logcat
    LOGI("%s", line);

    // 🔹 Send to client socket
    if (sockfd >= 0) {
        write(sockfd, line, strlen(line));
        write(sockfd, "\n", 1);
    }

    // 🔹 Append to local file fanomonitor.log
    const char *logPath = "/data/data/com.iamcod3x.privacypeek/files/fanomonitor.log";
    FILE *fp = fopen(logPath, "a");
    if (fp) {
        fprintf(fp, "%s\n", line);
        fclose(fp);
    } else {
        LOGE("Failed to open log file %s: %s", logPath, strerror(errno));
    }
}

/* Main entry */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <targetUid|-1> <abstractSocketName>\n", argv[0]);
        return 1;
    }

    int targetUid = atoi(argv[1]);
    const char *sockName = argv[2];

    LOGI("Starting fanomonitor for UID=%d socket=%s", targetUid, sockName);

    // setup signals
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    // setup abstract socket
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        LOGE("socket() failed: %s", strerror(errno));
        return 1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path + 1, sizeof(addr.sun_path) - 1, "%s", sockName);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(sa_family_t) + strlen(sockName) + 1) < 0) {
        LOGE("connect() to @%s failed: %s", sockName, strerror(errno));
        close(sockfd);
        return 1;
    }
    LOGI("Connected to abstract socket @%s", sockName);

    // init fanotify
    int fanfd = fanotify_init_compat(FAN_CLOEXEC | FAN_CLASS_NOTIF, O_RDONLY | O_CLOEXEC);
    if (fanfd < 0) {
        LOGE("fanotify_init_compat failed: %s", strerror(errno));
        close(sockfd);
        return 1;
    }

    unsigned long long masks[] = {
	    FAN_ACCESS,
	    FAN_MODIFY,
	    FAN_OPEN,
	    FAN_CLOSE_WRITE,
	    FAN_CREATE,
	    FAN_DELETE,
	    FAN_MOVED_FROM,
	    FAN_MOVED_TO
	};
                    
    const char *mask_names[] = {
	    "FAN_ACCESS",
	    "FAN_MODIFY",
	    "FAN_OPEN",
	    "FAN_CLOSE_WRITE",
	    "FAN_CREATE",
	    "FAN_DELETE",
	    "FAN_MOVED_FROM",
	    "FAN_MOVED_TO"
	};
	
    for (int i = 0; i < (int)(sizeof(masks)/sizeof(masks[0])); i++) {
	    if (fanotify_mark_compat(fanfd, FAN_MARK_ADD | FAN_MARK_MOUNT,
		                     masks[i], AT_FDCWD, "/") == 0) {
		LOGI("Supported mask: %s (0x%llx)", mask_names[i], masks[i]);
	    } else {
		LOGW("Mask %s (0x%llx) failed: %s",
		     mask_names[i], masks[i], strerror(errno));
	    }
	}

    LOGI("Fanotify monitor active.");

    // event loop
    char buffer[FANOTIFY_BUFFER_SIZE];
    while (running) {
        ssize_t len = read(fanfd, buffer, sizeof(buffer));
        if (len <= 0) {
            if (errno == EINTR) continue;
            LOGE("read() failed: %s", strerror(errno));
            break;
        }

        struct fanotify_event_metadata *meta;
        for (meta = (struct fanotify_event_metadata *)buffer;
             FAN_EVENT_OK(meta, len);
             meta = FAN_EVENT_NEXT(meta, len)) {

            if (meta->vers != FANOTIFY_METADATA_VERSION) {
                LOGE("fanotify metadata version mismatch!");
                continue;
            }

            // UID filter
            if (targetUid != -1) {
                struct fanotify_event_info_pidfd *info =
                        (struct fanotify_event_info_pidfd *)(meta + 1);
                // fallback: just compare meta->pid owner
                // (requires extra /proc lookup if strict)
            }

            send_event(sockfd, meta);
            if (meta->fd > 0) close(meta->fd);
        }
    }

    close(fanfd);
    close(sockfd);
    LOGI("Exiting fanomonitor...");
    return 0;
}

