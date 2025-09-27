/* fanotify_compat.h
 *
 * Robust fanotify compatibility helpers for Android/Bionic.
 * - Prefer existing SYS_fanotify_init / __NR_fanotify_init if available.
 * - If not available, fall back to per-arch syscall numbers (edit if incorrect).
 * - Provide wrappers named fanotify_init_compat / fanotify_mark_compat.
 *
 */

#ifndef FANOTIFY_COMPAT_H
#define FANOTIFY_COMPAT_H

#include <stdint.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <fcntl.h>         /* for O_RDONLY, O_LARGEFILE */
#include <linux/limits.h>
#include <limits.h>
#include <linux/fanotify.h> /* may or may not exist on Android headers */

/* --------- Event mask constants (safeguard if linux/fanotify.h missing) --------- */
#ifndef FAN_ACCESS
#define FAN_ACCESS        0x00000001
#endif
#ifndef FAN_MODIFY
#define FAN_MODIFY        0x00000002
#endif
#ifndef FAN_CLOSE_WRITE
#define FAN_CLOSE_WRITE   0x00000008
#endif
#ifndef FAN_CLOSE_NOWRITE
#define FAN_CLOSE_NOWRITE 0x00000010
#endif
#ifndef FAN_OPEN
#define FAN_OPEN          0x00000020
#endif
#ifndef FAN_Q_OVERFLOW
#define FAN_Q_OVERFLOW    0x00004000
#endif
#ifndef FAN_OPEN_PERM
#define FAN_OPEN_PERM     0x00010000
#endif
#ifndef FAN_ACCESS_PERM
#define FAN_ACCESS_PERM   0x00020000
#endif
#ifndef FAN_ONDIR
#define FAN_ONDIR         0x40000000
#endif
#ifndef FAN_EVENT_ON_CHILD
#define FAN_EVENT_ON_CHILD 0x08000000
#endif
#ifndef FAN_CLOSE
#define FAN_CLOSE         (FAN_CLOSE_WRITE | FAN_CLOSE_NOWRITE)
#endif

/* Extended */
#ifndef FAN_CREATE
#define FAN_CREATE        0x00000100
#endif
#ifndef FAN_DELETE
#define FAN_DELETE        0x00000200
#endif
#ifndef FAN_MOVED_FROM
#define FAN_MOVED_FROM    0x00000040
#endif
#ifndef FAN_MOVED_TO
#define FAN_MOVED_TO      0x00000080
#endif
#ifndef FAN_MOVE
#define FAN_MOVE (FAN_MOVED_FROM | FAN_MOVED_TO)
#endif

/* fanotify_init flags */
#ifndef FAN_CLOEXEC
#define FAN_CLOEXEC       0x00000001
#endif
#ifndef FAN_NONBLOCK
#define FAN_NONBLOCK      0x00000002
#endif
#ifndef FAN_CLASS_NOTIF
#define FAN_CLASS_NOTIF   0x00000000
#endif
#ifndef FAN_CLASS_CONTENT
#define FAN_CLASS_CONTENT 0x00000004
#endif
#ifndef FAN_CLASS_PRE_CONTENT
#define FAN_CLASS_PRE_CONTENT 0x00000008
#endif
#ifndef FAN_UNLIMITED_QUEUE
#define FAN_UNLIMITED_QUEUE 0x00000010
#endif
#ifndef FAN_UNLIMITED_MARKS
#define FAN_UNLIMITED_MARKS 0x00000020
#endif

/* fanotify_mark flags */
#ifndef FAN_MARK_ADD
#define FAN_MARK_ADD      0x00000001
#endif
#ifndef FAN_MARK_REMOVE
#define FAN_MARK_REMOVE   0x00000002
#endif
#ifndef FAN_MARK_DONT_FOLLOW
#define FAN_MARK_DONT_FOLLOW 0x00000004
#endif
#ifndef FAN_MARK_ONLYDIR
#define FAN_MARK_ONLYDIR  0x00000008
#endif
#ifndef FAN_MARK_MOUNT
#define FAN_MARK_MOUNT    0x00000010
#endif
#ifndef FAN_MARK_IGNORED_MASK
#define FAN_MARK_IGNORED_MASK 0x00000020
#endif
#ifndef FAN_MARK_IGNORED_SURV_MODIFY
#define FAN_MARK_IGNORED_SURV_MODIFY 0x00000040
#endif
#ifndef FAN_MARK_FLUSH
#define FAN_MARK_FLUSH    0x00000080
#endif
/*----*/

#ifndef FAN_ALLOW
#define FAN_ALLOW 0x01
#endif
#ifndef FAN_DENY
#define FAN_DENY  0x02
#endif
#ifndef FAN_NOFD
#define FAN_NOFD  -1
#endif

/* Helper macros for parsing events */
#ifndef FAN_EVENT_METADATA_LEN
#define FAN_EVENT_METADATA_LEN (sizeof(struct fanotify_event_metadata))
#endif

#ifndef FAN_EVENT_NEXT
#define FAN_EVENT_NEXT(meta, len) ((len) -= (meta)->event_len, \
    (struct fanotify_event_metadata*)(((char *)(meta)) + (meta)->event_len))
#endif

#ifndef FAN_EVENT_OK
#define FAN_EVENT_OK(meta, len)  ((long)(len) >= (long)FAN_EVENT_METADATA_LEN && \
    (long)(meta)->event_len >= (long)FAN_EVENT_METADATA_LEN && \
    (long)(meta)->event_len <= (long)(len))
#endif

/* ----------------- Syscall number detection -----------------
 * Prefer SYS_fanotify_init or __NR_fanotify_init if available.
 * If none are present, fall back to per-arch numbers (may need verification).
 *
 * CAUTION: syscall numbers can differ across architectures/ABI/old kernels;
 * if the fallback mapping fails on your device, run the provided probe tool.
 */

#if defined(SYS_fanotify_init)
  #define FANOTIFY_SYSCALL_INIT SYS_fanotify_init
#elif defined(__NR_fanotify_init)
  #define FANOTIFY_SYSCALL_INIT __NR_fanotify_init
#elif defined(__NR_fanotify)
  /* unlikely, keep for completeness */
  #define FANOTIFY_SYSCALL_INIT __NR_fanotify
#else
  /* Fallback: per-arch mapping.
   * These are common values seen on some platforms, but **may be wrong**.
   * If these do not work on your device, run the probe program to discover
   * the correct numbers and edit the values below accordingly.
   */
  #if defined(__aarch64__)
    #define FANOTIFY_SYSCALL_INIT 367   /* verify on your device */
    #define FANOTIFY_SYSCALL_MARK 368
  #elif defined(__arm__)
    #define FANOTIFY_SYSCALL_INIT 337
    #define FANOTIFY_SYSCALL_MARK 338
  #elif defined(__x86_64__)
    #define FANOTIFY_SYSCALL_INIT 262
    #define FANOTIFY_SYSCALL_MARK 263
  #elif defined(__i386__)
    #define FANOTIFY_SYSCALL_INIT 338
    #define FANOTIFY_SYSCALL_MARK 339
  #else
    #error "fanotify syscall number not known for this architecture. Please define FANOTIFY_SYSCALL_INIT/FANOTIFY_SYSCALL_MARK."
  #endif
#endif

/* If FANOTIFY_SYSCALL_MARK still undefined, try same checks for mark */
#if defined(SYS_fanotify_mark)
  #define FANOTIFY_SYSCALL_MARK SYS_fanotify_mark
#elif defined(__NR_fanotify_mark)
  #define FANOTIFY_SYSCALL_MARK __NR_fanotify_mark
#elif !defined(FANOTIFY_SYSCALL_MARK)
  /* if not defined above via architecture fallback, provide a default stub that
     will cause a build error — safer than silently using the wrong number */
  #error "FANOTIFY_SYSCALL_MARK is not defined. Please provide syscall numbers for fanotify_mark."
#endif

/* Inline wrappers that call the raw syscall numbers chosen above.
 * Use these in your code instead of libc's fanotify_init()/fanotify_mark().
 */
static inline int fanotify_init_compat(unsigned int flags, unsigned int event_f_flags) {
    /* Some platforms provide SYS_* value; if we are using the numeric fallback
     * these macros will expand to a number. Use syscall() with the macro name.
     */
    return (int)syscall((long)FANOTIFY_SYSCALL_INIT, (long)flags, (long)event_f_flags);
}

static inline int fanotify_mark_compat(int fan_fd, unsigned int flags,
                                       __u64 mask, int dirfd, const char *pathname) {
    return (int)syscall((long)FANOTIFY_SYSCALL_MARK, (long)fan_fd, (long)flags,
                        (unsigned long)mask, (long)dirfd, (const char *)pathname);
}

#endif /* FANOTIFY_COMPAT_H */

