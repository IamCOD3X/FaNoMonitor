# 🕵️‍♂️ FaNoMonitor: Android Fanotify File Access Tracker

**FaNoMonitor** is a lightweight C daemon for Android that uses the `fanotify` API to track file system activity in real-time. It logs file access events (like open, read, write, delete, etc.) to:

- Android `logcat`
- A local persistent log file
- A connected abstract Unix domain socket (optional client)

This tool is useful for **privacy auditing**, **malware detection**, **file access tracing**, and more on rooted Android systems.

---

## 🚀 Features

- 📁 Monitors all filesystem activity using `fanotify` (with mount-wide marks)
- 📡 Sends real-time event data via abstract Unix socket to a client app
- 📜 Logs events locally to `fanomonitor.log` in app-private storage
- 🔍 Identifies process name and accessed file path
- 🛑 Gracefully shuts down on `SIGINT`/`SIGTERM`
- 📱 Designed to run in Android environments (requires root)

---

## ⚙️ Requirements

- Rooted Android device
- Native build toolchain (NDK or cross-compile)
- Fanotify support (Linux kernel ≥ 2.6.37)
- SELinux permissive mode or appropriate policies for `/proc`, fanotify, and file access

---

## 🛠️ Build Instructions | 📦 Deployment Tip

A build script is provided to compile `fanomonitor` as a static binary for Android across multiple ABIs.

✅ Prerequisites
  - Android NDK r28 or newer
  - Linux build host
  - Rooted Android device or emulator for testing

🏁 Run ./build.sh
```
chmod +x build.sh && bash build.sh
```
- This will produce:
```
fanomonitor_out/
  ├── arm64-v8a/
  │   └── fanomonitor
  ├── armeabi-v7a/
  │   └── fanomonitor
  ├── x86/
  │   └── fanomonitor
  └── x86_64/
      └── fanomonitor
```
- You can include the resulting binaries in your Android app's assets/bin/ directory for runtime execution.
- Optional: Add a check block to just copy required arch binary instead of dumping all.

---

## 📦 Usage
```
./fanomonitor <targetUid|-1> <abstractSocketName>
```
	
### Arguments

- <targetUid>: Filter by UID of process accessing files, or -1 to log all
- <abstractSocketName>: Name of the abstract Unix socket to connect to (e.g., fanosock)

```
./fanomonitor -1 fanosock
```

- This logs all file access events and sends them to an abstract socket named @fanosock.

---

## 📂 Output

- ✔ Sample Log Entry
```
1695821443000|PID=1523|UID=10166|PROC=com.example.app|PATH=/sdcard/Download/file.txt|TYPE=OPEN
```
- ✔ Log File Location
```
/data/data/com.example.app/files/fanomonitor.log
```
- Note: Ensure this path is writable and exists before starting the binary.

---

## 🧩 Integration

- You can pair this binary with an Android app that:
  - Creates a background socket listener on the abstract socket
  - Receives event data in real time
  - Displays or forwards log entries to a UI
  
---

## ⚠️ Security & Limitations

- Requires root access
- May not work on newer Android versions with stricter SELinux or kernel changes
- Events are reported after access (not preemptive)
- File paths are resolved from /proc/self/fd and may fail if the FD closes quickly

---

## 📃 License
- MIT License — see [LICENSE](LICENSE)

---

## 🙌 Credits

- Developed by [@IamCOD3X](https://www.iamcod3x.com)

- Inspired by fanotify-based filesystem monitors on Linux desktops.

---