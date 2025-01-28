/**************************************************************************/
/*  os_unix.cpp                                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "os_unix.h"

#ifdef UNIX_ENABLED

#include "core/config/project_settings.h"
#include "core/debugger/engine_debugger.h"
#include "core/debugger/script_debugger.h"
#include "drivers/unix/dir_access_unix.h"
#include "drivers/unix/file_access_unix.h"
#include "drivers/unix/file_access_unix_pipe.h"
#include "drivers/unix/net_socket_unix.h"
#include "drivers/unix/thread_posix.h"
#include "servers/rendering_server.h"

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#include <mach/host_info.h>
#include <mach/mach_host.h>
#include <mach/mach_time.h>
#include <sys/sysctl.h>
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/param.h>
#include <sys/sysctl.h>
#endif

#if defined(__FreeBSD__)
#include <kvm.h>
#endif

#if defined(__OpenBSD__)
#include <sys/swap.h>
#include <uvm/uvmexp.h>
#endif

#if defined(__NetBSD__)
#include <uvm/uvm_extern.h>
#endif

#include <dlfcn.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef RTLD_DEEPBIND
#define RTLD_DEEPBIND 0
#endif

#ifndef SANITIZERS_ENABLED
#define GODOT_DLOPEN_MODE RTLD_NOW | RTLD_DEEPBIND
#else
#define GODOT_DLOPEN_MODE RTLD_NOW
#endif

#if defined(MACOS_ENABLED) || (defined(__ANDROID_API__) && __ANDROID_API__ >= 28)
// Random location for getentropy. Fitting.
#include <sys/random.h>
#define UNIX_GET_ENTROPY
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || (defined(__GLIBC_MINOR__) && (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 26))
// In <unistd.h>.
// One day... (defined(_XOPEN_SOURCE) && _XOPEN_SOURCE >= 700)
// https://publications.opengroup.org/standards/unix/c211
#define UNIX_GET_ENTROPY
#endif

#if !defined(UNIX_GET_ENTROPY) && !defined(NO_URANDOM)
#include <fcntl.h>
#endif

/// Clock Setup function (used by get_ticks_usec)
static uint64_t _clock_start = 0;
#if defined(__APPLE__)
static double _clock_scale = 0;
static void _setup_clock() {
	mach_timebase_info_data_t info;
	kern_return_t ret = mach_timebase_info(&info);
	ERR_FAIL_COND_MSG(ret != 0, "OS CLOCK IS NOT WORKING!");
	_clock_scale = ((double)info.numer / (double)info.denom) / 1000.0;
	_clock_start = mach_absolute_time() * _clock_scale;
}
#else
#if defined(CLOCK_MONOTONIC_RAW) && !defined(WEB_ENABLED) // This is a better clock on Linux.
#define GODOT_CLOCK CLOCK_MONOTONIC_RAW
#else
#define GODOT_CLOCK CLOCK_MONOTONIC
#endif
static void _setup_clock() {
	struct timespec tv_now = { 0, 0 };
	ERR_FAIL_COND_MSG(clock_gettime(GODOT_CLOCK, &tv_now) != 0, "OS CLOCK IS NOT WORKING!");
	_clock_start = ((uint64_t)tv_now.tv_nsec / 1000L) + (uint64_t)tv_now.tv_sec * 1000000L;
}
#endif

static void handle_interrupt(int sig) {
	if (!EngineDebugger::is_active()) {
		return;
	}

	EngineDebugger::get_script_debugger()->set_depth(-1);
	EngineDebugger::get_script_debugger()->set_lines_left(1);
}

void OS_Unix::initialize_debugging() {
	if (EngineDebugger::is_active()) {
		struct sigaction action;
		memset(&action, 0, sizeof(action));
		action.sa_handler = handle_interrupt;
		sigaction(SIGINT, &action, nullptr);
	}
}

int OS_Unix::unix_initialize_audio(int p_audio_driver) {
	return 0;
}

void OS_Unix::initialize_core() {
#ifdef THREADS_ENABLED
	init_thread_posix();
#endif

	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessUnix>(FileAccess::ACCESS_FILESYSTEM);
	FileAccess::make_default<FileAccessUnixPipe>(FileAccess::ACCESS_PIPE);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessUnix>(DirAccess::ACCESS_FILESYSTEM);

#ifndef UNIX_SOCKET_UNAVAILABLE
	NetSocketUnix::make_default();
	IPUnix::make_default();
#endif
	process_map = memnew((HashMap<ProcessID, ProcessInfo>));

	_setup_clock();
}

void OS_Unix::finalize_core() {
	memdelete(process_map);
#ifndef UNIX_SOCKET_UNAVAILABLE
	NetSocketUnix::cleanup();
#endif
}

Vector<String> OS_Unix::get_video_adapter_driver_info() const {
	return Vector<String>();
}

String OS_Unix::get_stdin_string(int64_t p_buffer_size) {
	Vector<uint8_t> data;
	data.resize(p_buffer_size);
	if (fgets((char *)data.ptrw(), data.size(), stdin)) {
		return String::utf8((char *)data.ptr()).replace("\r\n", "\n").rstrip("\n");
	}
	return String();
}

PackedByteArray OS_Unix::get_stdin_buffer(int64_t p_buffer_size) {
	Vector<uint8_t> data;
	data.resize(p_buffer_size);
	size_t sz = fread((void *)data.ptrw(), 1, data.size(), stdin);
	if (sz > 0) {
		data.resize(sz);
		return data;
	}
	return PackedByteArray();
}

OS_Unix::StdHandleType OS_Unix::get_stdin_type() const {
	int h = fileno(stdin);
	if (h == -1) {
		return STD_HANDLE_INVALID;
	}

	if (isatty(h)) {
		return STD_HANDLE_CONSOLE;
	}
	struct stat statbuf;
	if (fstat(h, &statbuf) < 0) {
		return STD_HANDLE_UNKNOWN;
	}
	if (S_ISFIFO(statbuf.st_mode)) {
		return STD_HANDLE_PIPE;
	} else if (S_ISREG(statbuf.st_mode) || S_ISLNK(statbuf.st_mode)) {
		return STD_HANDLE_FILE;
	}
	return STD_HANDLE_UNKNOWN;
}

OS_Unix::StdHandleType OS_Unix::get_stdout_type() const {
	int h = fileno(stdout);
	if (h == -1) {
		return STD_HANDLE_INVALID;
	}

	if (isatty(h)) {
		return STD_HANDLE_CONSOLE;
	}
	struct stat statbuf;
	if (fstat(h, &statbuf) < 0) {
		return STD_HANDLE_UNKNOWN;
	}
	if (S_ISFIFO(statbuf.st_mode)) {
		return STD_HANDLE_PIPE;
	} else if (S_ISREG(statbuf.st_mode) || S_ISLNK(statbuf.st_mode)) {
		return STD_HANDLE_FILE;
	}
	return STD_HANDLE_UNKNOWN;
}

OS_Unix::StdHandleType OS_Unix::get_stderr_type() const {
	int h = fileno(stderr);
	if (h == -1) {
		return STD_HANDLE_INVALID;
	}

	if (isatty(h)) {
		return STD_HANDLE_CONSOLE;
	}
	struct stat statbuf;
	if (fstat(h, &statbuf) < 0) {
		return STD_HANDLE_UNKNOWN;
	}
	if (S_ISFIFO(statbuf.st_mode)) {
		return STD_HANDLE_PIPE;
	} else if (S_ISREG(statbuf.st_mode) || S_ISLNK(statbuf.st_mode)) {
		return STD_HANDLE_FILE;
	}
	return STD_HANDLE_UNKNOWN;
}

Error OS_Unix::get_entropy(uint8_t *r_buffer, int p_bytes) {
#if defined(UNIX_GET_ENTROPY)
	int left = p_bytes;
	int ofs = 0;
	do {
		int chunk = MIN(left, 256);
		ERR_FAIL_COND_V(getentropy(r_buffer + ofs, chunk), FAILED);
		left -= chunk;
		ofs += chunk;
	} while (left > 0);
// Define this yourself if you don't want to fall back to /dev/urandom.
#elif !defined(NO_URANDOM)
	int r = open("/dev/urandom", O_RDONLY);
	ERR_FAIL_COND_V(r < 0, FAILED);
	int left = p_bytes;
	do {
		ssize_t ret = read(r, r_buffer, p_bytes);
		ERR_FAIL_COND_V(ret <= 0, FAILED);
		left -= ret;
	} while (left > 0);
#else
	return ERR_UNAVAILABLE;
#endif
	return OK;
}

String OS_Unix::get_name() const {
	return "Unix";
}

String OS_Unix::get_distribution_name() const {
	return "";
}

String OS_Unix::get_version() const {
	return "";
}

String OS_Unix::get_temp_path() const {
	return "/tmp";
}

double OS_Unix::get_unix_time() const {
	struct timeval tv_now;
	gettimeofday(&tv_now, nullptr);
	return (double)tv_now.tv_sec + double(tv_now.tv_usec) / 1000000;
}

OS::DateTime OS_Unix::get_datetime(bool p_utc) const {
	time_t t = time(nullptr);
	struct tm lt;
	if (p_utc) {
		gmtime_r(&t, &lt);
	} else {
		localtime_r(&t, &lt);
	}
	DateTime ret;
	ret.year = 1900 + lt.tm_year;
	// Index starting at 1 to match OS_Unix::get_date
	//   and Windows SYSTEMTIME and tm_mon follows the typical structure
	//   of 0-11, noted here: http://www.cplusplus.com/reference/ctime/tm/
	ret.month = (Month)(lt.tm_mon + 1);
	ret.day = lt.tm_mday;
	ret.weekday = (Weekday)lt.tm_wday;
	ret.hour = lt.tm_hour;
	ret.minute = lt.tm_min;
	ret.second = lt.tm_sec;
	ret.dst = lt.tm_isdst;

	return ret;
}

OS::TimeZoneInfo OS_Unix::get_time_zone_info() const {
	time_t t = time(nullptr);
	struct tm lt;
	localtime_r(&t, &lt);
	char name[16];
	strftime(name, 16, "%Z", &lt);
	name[15] = 0;
	TimeZoneInfo ret;
	ret.name = name;

	char bias_buf[16];
	strftime(bias_buf, 16, "%z", &lt);
	int bias;
	bias_buf[15] = 0;
	sscanf(bias_buf, "%d", &bias);

	// convert from ISO 8601 (1 minute=1, 1 hour=100) to minutes
	int hour = (int)bias / 100;
	int minutes = bias % 100;
	if (bias < 0) {
		ret.bias = hour * 60 - minutes;
	} else {
		ret.bias = hour * 60 + minutes;
	}

	return ret;
}

void OS_Unix::delay_usec(uint32_t p_usec) const {
	struct timespec requested = { static_cast<time_t>(p_usec / 1000000), (static_cast<long>(p_usec) % 1000000) * 1000 };
	struct timespec remaining;
	while (nanosleep(&requested, &remaining) == -1 && errno == EINTR) {
		requested.tv_sec = remaining.tv_sec;
		requested.tv_nsec = remaining.tv_nsec;
	}
}

uint64_t OS_Unix::get_ticks_usec() const {
#if defined(__APPLE__)
	uint64_t longtime = mach_absolute_time() * _clock_scale;
#else
	// Unchecked return. Static analyzers might complain.
	// If _setup_clock() succeeded, we assume clock_gettime() works.
	struct timespec tv_now = { 0, 0 };
	clock_gettime(GODOT_CLOCK, &tv_now);
	uint64_t longtime = ((uint64_t)tv_now.tv_nsec / 1000L) + (uint64_t)tv_now.tv_sec * 1000000L;
#endif
	longtime -= _clock_start;

	return longtime;
}

Dictionary OS_Unix::get_memory_info() const {
	Dictionary meminfo;

	meminfo["physical"] = -1;
	meminfo["free"] = -1;
	meminfo["available"] = -1;
	meminfo["stack"] = -1;

#if defined(__APPLE__)
	int pagesize = 0;
	size_t len = sizeof(pagesize);
	if (sysctlbyname("vm.pagesize", &pagesize, &len, nullptr, 0) < 0) {
		ERR_PRINT(vformat("Could not get vm.pagesize, error code: %d - %s", errno, strerror(errno)));
	}

	int64_t phy_mem = 0;
	len = sizeof(phy_mem);
	if (sysctlbyname("hw.memsize", &phy_mem, &len, nullptr, 0) < 0) {
		ERR_PRINT(vformat("Could not get hw.memsize, error code: %d - %s", errno, strerror(errno)));
	}

	mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
	vm_statistics64_data_t vmstat;
	if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vmstat, &count) != KERN_SUCCESS) {
		ERR_PRINT("Could not get host vm statistics.");
	}
	struct xsw_usage swap_used;
	len = sizeof(swap_used);
	if (sysctlbyname("vm.swapusage", &swap_used, &len, nullptr, 0) < 0) {
		ERR_PRINT(vformat("Could not get vm.swapusage, error code: %d - %s", errno, strerror(errno)));
	}

	if (phy_mem != 0) {
		meminfo["physical"] = phy_mem;
	}
	if (vmstat.free_count * (int64_t)pagesize != 0) {
		meminfo["free"] = vmstat.free_count * (int64_t)pagesize;
	}
	if (swap_used.xsu_avail + vmstat.free_count * (int64_t)pagesize != 0) {
		meminfo["available"] = swap_used.xsu_avail + vmstat.free_count * (int64_t)pagesize;
	}
#elif defined(__FreeBSD__)
	int pagesize = 0;
	size_t len = sizeof(pagesize);
	if (sysctlbyname("vm.stats.vm.v_page_size", &pagesize, &len, nullptr, 0) < 0) {
		ERR_PRINT(vformat("Could not get vm.stats.vm.v_page_size, error code: %d - %s", errno, strerror(errno)));
	}

	uint64_t mtotal = 0;
	len = sizeof(mtotal);
	if (sysctlbyname("vm.stats.vm.v_page_count", &mtotal, &len, nullptr, 0) < 0) {
		ERR_PRINT(vformat("Could not get vm.stats.vm.v_page_count, error code: %d - %s", errno, strerror(errno)));
	}
	uint64_t mfree = 0;
	len = sizeof(mfree);
	if (sysctlbyname("vm.stats.vm.v_free_count", &mfree, &len, nullptr, 0) < 0) {
		ERR_PRINT(vformat("Could not get vm.stats.vm.v_free_count, error code: %d - %s", errno, strerror(errno)));
	}

	uint64_t stotal = 0;
	uint64_t sused = 0;
	char errmsg[_POSIX2_LINE_MAX] = {};
	kvm_t *kd = kvm_openfiles(nullptr, "/dev/null", nullptr, 0, errmsg);
	if (kd == nullptr) {
		ERR_PRINT(vformat("kvm_openfiles failed, error: %s", errmsg));
	} else {
		struct kvm_swap swap_info[32];
		int count = kvm_getswapinfo(kd, swap_info, 32, 0);
		for (int i = 0; i < count; i++) {
			stotal += swap_info[i].ksw_total;
			sused += swap_info[i].ksw_used;
		}
		kvm_close(kd);
	}

	if (mtotal * pagesize != 0) {
		meminfo["physical"] = mtotal * pagesize;
	}
	if (mfree * pagesize != 0) {
		meminfo["free"] = mfree * pagesize;
	}
	if ((mfree + stotal - sused) * pagesize != 0) {
		meminfo["available"] = (mfree + stotal - sused) * pagesize;
	}
#elif defined(__OpenBSD__)
	int pagesize = sysconf(_SC_PAGESIZE);

	const int mib[] = { CTL_VM, VM_UVMEXP };
	uvmexp uvmexp_info;
	size_t len = sizeof(uvmexp_info);
	if (sysctl(mib, 2, &uvmexp_info, &len, nullptr, 0) < 0) {
		ERR_PRINT(vformat("Could not get CTL_VM, VM_UVMEXP, error code: %d - %s", errno, strerror(errno)));
	}

	uint64_t stotal = 0;
	uint64_t sused = 0;
	int count = swapctl(SWAP_NSWAP, 0, 0);
	if (count > 0) {
		swapent swap_info[count];
		count = swapctl(SWAP_STATS, swap_info, count);

		for (int i = 0; i < count; i++) {
			if (swap_info[i].se_flags & SWF_ENABLE) {
				sused += swap_info[i].se_inuse;
				stotal += swap_info[i].se_nblks;
			}
		}
	}

	if (uvmexp_info.npages * pagesize != 0) {
		meminfo["physical"] = uvmexp_info.npages * pagesize;
	}
	if (uvmexp_info.free * pagesize != 0) {
		meminfo["free"] = uvmexp_info.free * pagesize;
	}
	if ((uvmexp_info.free * pagesize) + (stotal - sused) * DEV_BSIZE != 0) {
		meminfo["available"] = (uvmexp_info.free * pagesize) + (stotal - sused) * DEV_BSIZE;
	}
#elif defined(__NetBSD__)
	int pagesize = sysconf(_SC_PAGESIZE);

	const int mib[] = { CTL_VM, VM_UVMEXP2 };
	uvmexp_sysctl uvmexp_info;
	size_t len = sizeof(uvmexp_info);
	if (sysctl(mib, 2, &uvmexp_info, &len, nullptr, 0) < 0) {
		ERR_PRINT(vformat("Could not get CTL_VM, VM_UVMEXP2, error code: %d - %s", errno, strerror(errno)));
	}

	if (uvmexp_info.npages * pagesize != 0) {
		meminfo["physical"] = uvmexp_info.npages * pagesize;
	}
	if (uvmexp_info.free * pagesize != 0) {
		meminfo["free"] = uvmexp_info.free * pagesize;
	}
	if ((uvmexp_info.free + uvmexp_info.swpages - uvmexp_info.swpginuse) * pagesize != 0) {
		meminfo["available"] = (uvmexp_info.free + uvmexp_info.swpages - uvmexp_info.swpginuse) * pagesize;
	}
#else
	Error err;
	Ref<FileAccess> f = FileAccess::open("/proc/meminfo", FileAccess::READ, &err);
	uint64_t mtotal = 0;
	uint64_t mfree = 0;
	uint64_t sfree = 0;
	while (f.is_valid() && !f->eof_reached()) {
		String s = f->get_line().strip_edges();
		if (s.begins_with("MemTotal:")) {
			Vector<String> stok = s.replace("MemTotal:", "").strip_edges().split(" ");
			if (stok.size() == 2) {
				mtotal = stok[0].to_int() * 1024;
			}
		}
		if (s.begins_with("MemFree:")) {
			Vector<String> stok = s.replace("MemFree:", "").strip_edges().split(" ");
			if (stok.size() == 2) {
				mfree = stok[0].to_int() * 1024;
			}
		}
		if (s.begins_with("SwapFree:")) {
			Vector<String> stok = s.replace("SwapFree:", "").strip_edges().split(" ");
			if (stok.size() == 2) {
				sfree = stok[0].to_int() * 1024;
			}
		}
	}

	if (mtotal != 0) {
		meminfo["physical"] = mtotal;
	}
	if (mfree != 0) {
		meminfo["free"] = mfree;
	}
	if (mfree + sfree != 0) {
		meminfo["available"] = mfree + sfree;
	}
#endif

	rlimit stackinfo = {};
	getrlimit(RLIMIT_STACK, &stackinfo);

	if (stackinfo.rlim_cur != 0) {
		meminfo["stack"] = (int64_t)stackinfo.rlim_cur;
	}

	return meminfo;
}

Dictionary OS_Unix::execute_with_pipe(const String &p_path, const List<String> &p_arguments, bool p_blocking) {
#define CLEAN_PIPES           \
	if (pipe_in[0] >= 0) {    \
		::close(pipe_in[0]);  \
	}                         \
	if (pipe_in[1] >= 0) {    \
		::close(pipe_in[1]);  \
	}                         \
	if (pipe_out[0] >= 0) {   \
		::close(pipe_out[0]); \
	}                         \
	if (pipe_out[1] >= 0) {   \
		::close(pipe_out[1]); \
	}                         \
	if (pipe_err[0] >= 0) {   \
		::close(pipe_err[0]); \
	}                         \
	if (pipe_err[1] >= 0) {   \
		::close(pipe_err[1]); \
	}

	Dictionary ret;
#ifdef __EMSCRIPTEN__
	// Don't compile this code at all to avoid undefined references.
	// Actual virtual call goes to OS_Web.
	ERR_FAIL_V(ret);
#else
	// Create pipes.
	int pipe_in[2] = { -1, -1 };
	int pipe_out[2] = { -1, -1 };
	int pipe_err[2] = { -1, -1 };

	ERR_FAIL_COND_V(pipe(pipe_in) != 0, ret);
	if (pipe(pipe_out) != 0) {
		CLEAN_PIPES
		ERR_FAIL_V(ret);
	}
	if (pipe(pipe_err) != 0) {
		CLEAN_PIPES
		ERR_FAIL_V(ret);
	}

	// Create process.
	pid_t pid = fork();
	if (pid < 0) {
		CLEAN_PIPES
		ERR_FAIL_V(ret);
	}

	if (pid == 0) {
		// The child process.
		Vector<CharString> cs;
		cs.push_back(p_path.utf8());
		for (const String &arg : p_arguments) {
			cs.push_back(arg.utf8());
		}

		Vector<char *> args;
		for (int i = 0; i < cs.size(); i++) {
			args.push_back((char *)cs[i].get_data());
		}
		args.push_back(0);

		::close(STDIN_FILENO);
		::dup2(pipe_in[0], STDIN_FILENO);

		::close(STDOUT_FILENO);
		::dup2(pipe_out[1], STDOUT_FILENO);

		::close(STDERR_FILENO);
		::dup2(pipe_err[1], STDERR_FILENO);

		CLEAN_PIPES

		execvp(p_path.utf8().get_data(), &args[0]);
		// The execvp() function only returns if an error occurs.
		ERR_PRINT("Could not create child process: " + p_path);
		raise(SIGKILL);
	}
	::close(pipe_in[0]);
	::close(pipe_out[1]);
	::close(pipe_err[1]);

	Ref<FileAccessUnixPipe> main_pipe;
	main_pipe.instantiate();
	main_pipe->open_existing(pipe_out[0], pipe_in[1], p_blocking);

	Ref<FileAccessUnixPipe> err_pipe;
	err_pipe.instantiate();
	err_pipe->open_existing(pipe_err[0], 0, p_blocking);

	ProcessInfo pi;
	process_map_mutex.lock();
	process_map->insert(pid, pi);
	process_map_mutex.unlock();

	ret["stdio"] = main_pipe;
	ret["stderr"] = err_pipe;
	ret["pid"] = pid;

#undef CLEAN_PIPES
	return ret;
#endif
}

Error OS_Unix::execute(const String &p_path, const List<String> &p_arguments, String *r_pipe, int *r_exitcode, bool read_stderr, Mutex *p_pipe_mutex, bool p_open_console) {
#ifdef __EMSCRIPTEN__
	// Don't compile this code at all to avoid undefined references.
	// Actual virtual call goes to OS_Web.
	ERR_FAIL_V(ERR_BUG);
#else
	if (r_pipe) {
		String command = "\"" + p_path + "\"";
		for (const String &arg : p_arguments) {
			command += String(" \"") + arg + "\"";
		}
		if (read_stderr) {
			command += " 2>&1"; // Include stderr
		} else {
			command += " 2>/dev/null"; // Silence stderr
		}

		FILE *f = popen(command.utf8().get_data(), "r");
		ERR_FAIL_NULL_V_MSG(f, ERR_CANT_OPEN, "Cannot create pipe from command: " + command + ".");
		char buf[65535];
		while (fgets(buf, 65535, f)) {
			if (p_pipe_mutex) {
				p_pipe_mutex->lock();
			}
			String pipe_out;
			if (pipe_out.parse_utf8(buf) == OK) {
				(*r_pipe) += pipe_out;
			} else {
				(*r_pipe) += String(buf); // If not valid UTF-8 try decode as Latin-1
			}
			if (p_pipe_mutex) {
				p_pipe_mutex->unlock();
			}
		}
		int rv = pclose(f);

		if (r_exitcode) {
			*r_exitcode = WEXITSTATUS(rv);
		}
		return OK;
	}

	pid_t pid = fork();
	ERR_FAIL_COND_V(pid < 0, ERR_CANT_FORK);

	if (pid == 0) {
		// The child process
		Vector<CharString> cs;
		cs.push_back(p_path.utf8());
		for (const String &arg : p_arguments) {
			cs.push_back(arg.utf8());
		}

		Vector<char *> args;
		for (int i = 0; i < cs.size(); i++) {
			args.push_back((char *)cs[i].get_data());
		}
		args.push_back(0);

		execvp(p_path.utf8().get_data(), &args[0]);
		// The execvp() function only returns if an error occurs.
		ERR_PRINT("Could not create child process: " + p_path);
		raise(SIGKILL);
	}

	int status;
	waitpid(pid, &status, 0);
	if (r_exitcode) {
		*r_exitcode = WIFEXITED(status) ? WEXITSTATUS(status) : status;
	}
	return OK;
#endif
}

Error OS_Unix::create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id, bool p_open_console) {
#ifdef __EMSCRIPTEN__
	// Don't compile this code at all to avoid undefined references.
	// Actual virtual call goes to OS_Web.
	ERR_FAIL_V(ERR_BUG);
#else
	pid_t pid = fork();
	ERR_FAIL_COND_V(pid < 0, ERR_CANT_FORK);

	if (pid == 0) {
		// The new process
		// Create a new session-ID so parent won't wait for it.
		// This ensures the process won't go zombie at the end.
		setsid();

		Vector<CharString> cs;
		cs.push_back(p_path.utf8());
		for (const String &arg : p_arguments) {
			cs.push_back(arg.utf8());
		}

		Vector<char *> args;
		for (int i = 0; i < cs.size(); i++) {
			args.push_back((char *)cs[i].get_data());
		}
		args.push_back(0);

		execvp(p_path.utf8().get_data(), &args[0]);
		// The execvp() function only returns if an error occurs.
		ERR_PRINT("Could not create child process: " + p_path);
		raise(SIGKILL);
	}

	ProcessInfo pi;
	process_map_mutex.lock();
	process_map->insert(pid, pi);
	process_map_mutex.unlock();

	if (r_child_id) {
		*r_child_id = pid;
	}
	return OK;
#endif
}

Error OS_Unix::kill(const ProcessID &p_pid) {
	int ret = ::kill(p_pid, SIGKILL);
	if (!ret) {
		//avoid zombie process
		int st;
		::waitpid(p_pid, &st, 0);
	}
	return ret ? ERR_INVALID_PARAMETER : OK;
}

int OS_Unix::get_process_id() const {
	return getpid();
}

bool OS_Unix::is_process_running(const ProcessID &p_pid) const {
	MutexLock lock(process_map_mutex);
	const ProcessInfo *pi = process_map->getptr(p_pid);

	if (pi && !pi->is_running) {
		return false;
	}

	int status = 0;
	if (waitpid(p_pid, &status, WNOHANG) != 0) {
		if (pi) {
			pi->is_running = false;
			pi->exit_code = status;
		}
		return false;
	}

	return true;
}

int OS_Unix::get_process_exit_code(const ProcessID &p_pid) const {
	MutexLock lock(process_map_mutex);
	const ProcessInfo *pi = process_map->getptr(p_pid);

	if (pi && !pi->is_running) {
		return pi->exit_code;
	}

	int status = 0;
	if (waitpid(p_pid, &status, WNOHANG) != 0) {
		status = WIFEXITED(status) ? WEXITSTATUS(status) : status;
		if (pi) {
			pi->is_running = false;
			pi->exit_code = status;
		}
		return status;
	}
	return -1;
}

String OS_Unix::get_locale() const {
	if (!has_environment("LANG")) {
		return "en";
	}

	String locale = get_environment("LANG");
	int tp = locale.find_char('.');
	if (tp != -1) {
		locale = locale.substr(0, tp);
	}
	return locale;
}

Error OS_Unix::open_dynamic_library(const String &p_path, void *&p_library_handle, GDExtensionData *p_data) {
	String path = p_path;

	if (FileAccess::exists(path) && path.is_relative_path()) {
		// dlopen expects a slash, in this case a leading ./ for it to be interpreted as a relative path,
		//  otherwise it will end up searching various system directories for the lib instead and finally failing.
		path = "./" + path;
	}

	if (!FileAccess::exists(path)) {
		// This code exists so GDExtension can load .so files from within the executable path.
		path = get_executable_path().get_base_dir().path_join(p_path.get_file());
	}

	if (!FileAccess::exists(path)) {
		// This code exists so GDExtension can load .so files from a standard unix location.
		path = get_executable_path().get_base_dir().path_join("../lib").path_join(p_path.get_file());
	}

	ERR_FAIL_COND_V(!FileAccess::exists(path), ERR_FILE_NOT_FOUND);

	p_library_handle = dlopen(path.utf8().get_data(), GODOT_DLOPEN_MODE);
	ERR_FAIL_NULL_V_MSG(p_library_handle, ERR_CANT_OPEN, vformat("Can't open dynamic library: %s. Error: %s.", p_path, dlerror()));

	if (p_data != nullptr && p_data->r_resolved_path != nullptr) {
		*p_data->r_resolved_path = path;
	}

	return OK;
}

Error OS_Unix::close_dynamic_library(void *p_library_handle) {
	if (dlclose(p_library_handle)) {
		return FAILED;
	}
	return OK;
}

Error OS_Unix::get_dynamic_library_symbol_handle(void *p_library_handle, const String &p_name, void *&p_symbol_handle, bool p_optional) {
	const char *error;
	dlerror(); // Clear existing errors

	p_symbol_handle = dlsym(p_library_handle, p_name.utf8().get_data());

	error = dlerror();
	if (error != nullptr) {
		ERR_FAIL_COND_V_MSG(!p_optional, ERR_CANT_RESOLVE, "Can't resolve symbol " + p_name + ". Error: " + error + ".");

		return ERR_CANT_RESOLVE;
	}
	return OK;
}

Error OS_Unix::set_cwd(const String &p_cwd) {
	if (chdir(p_cwd.utf8().get_data()) != 0) {
		return ERR_CANT_OPEN;
	}

	return OK;
}

bool OS_Unix::has_environment(const String &p_var) const {
	return getenv(p_var.utf8().get_data()) != nullptr;
}

String OS_Unix::get_environment(const String &p_var) const {
	const char *val = getenv(p_var.utf8().get_data());
	if (val == nullptr) { // Not set; return empty string
		return "";
	}
	String s;
	if (s.parse_utf8(val) == OK) {
		return s;
	}
	return String(val); // Not valid UTF-8, so return as-is
}

void OS_Unix::set_environment(const String &p_var, const String &p_value) const {
	ERR_FAIL_COND_MSG(p_var.is_empty() || p_var.contains_char('='), vformat("Invalid environment variable name '%s', cannot be empty or include '='.", p_var));
	int err = setenv(p_var.utf8().get_data(), p_value.utf8().get_data(), /* overwrite: */ 1);
	ERR_FAIL_COND_MSG(err != 0, vformat("Failed setting environment variable '%s', the system is out of memory.", p_var));
}

void OS_Unix::unset_environment(const String &p_var) const {
	ERR_FAIL_COND_MSG(p_var.is_empty() || p_var.contains_char('='), vformat("Invalid environment variable name '%s', cannot be empty or include '='.", p_var));
	unsetenv(p_var.utf8().get_data());
}

String OS_Unix::get_user_data_dir(const String &p_user_dir) const {
	return get_data_path().path_join(p_user_dir);
}

String OS_Unix::get_executable_path() const {
#ifdef __linux__
	//fix for running from a symlink
	char buf[256];
	memset(buf, 0, 256);
	ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf));
	String b;
	if (len > 0) {
		b.parse_utf8(buf, len);
	}
	if (b.is_empty()) {
		WARN_PRINT("Couldn't get executable path from /proc/self/exe, using argv[0]");
		return OS::get_executable_path();
	}
	return b;
#elif defined(__OpenBSD__)
	char resolved_path[MAXPATHLEN];

	realpath(OS::get_executable_path().utf8().get_data(), resolved_path);

	return String(resolved_path);
#elif defined(__NetBSD__)
	int mib[4] = { CTL_KERN, KERN_PROC_ARGS, -1, KERN_PROC_PATHNAME };
	char buf[MAXPATHLEN];
	size_t len = sizeof(buf);
	if (sysctl(mib, 4, buf, &len, nullptr, 0) != 0) {
		WARN_PRINT("Couldn't get executable path from sysctl");
		return OS::get_executable_path();
	}

	// NetBSD does not always return a normalized path. For example if argv[0] is "./a.out" then executable path is "/home/netbsd/./a.out". Normalize with realpath:
	char resolved_path[MAXPATHLEN];

	realpath(buf, resolved_path);

	return String(resolved_path);
#elif defined(__FreeBSD__)
	int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1 };
	char buf[MAXPATHLEN];
	size_t len = sizeof(buf);
	if (sysctl(mib, 4, buf, &len, nullptr, 0) != 0) {
		WARN_PRINT("Couldn't get executable path from sysctl");
		return OS::get_executable_path();
	}
	String b;
	b.parse_utf8(buf);
	return b;
#elif defined(__APPLE__)
	char temp_path[1];
	uint32_t buff_size = 1;
	_NSGetExecutablePath(temp_path, &buff_size);

	char *resolved_path = new char[buff_size + 1];

	if (_NSGetExecutablePath(resolved_path, &buff_size) == 1) {
		WARN_PRINT("MAXPATHLEN is too small");
	}

	String path = String::utf8(resolved_path);
	delete[] resolved_path;

	return path;
#else
	ERR_PRINT("Warning, don't know how to obtain executable path on this OS! Please override this function properly.");
	return OS::get_executable_path();
#endif
}

void UnixTerminalLogger::log_error(const char *p_function, const char *p_file, int p_line, const char *p_code, const char *p_rationale, bool p_editor_notify, ErrorType p_type) {
	if (!should_log(true)) {
		return;
	}

	const char *err_details;
	if (p_rationale && p_rationale[0]) {
		err_details = p_rationale;
	} else {
		err_details = p_code;
	}

	// Disable color codes if stdout is not a TTY.
	// This prevents Godot from writing ANSI escape codes when redirecting
	// stdout and stderr to a file.
	const bool tty = isatty(fileno(stdout));
	const char *gray = tty ? "\E[0;90m" : "";
	const char *red = tty ? "\E[0;91m" : "";
	const char *red_bold = tty ? "\E[1;31m" : "";
	const char *yellow = tty ? "\E[0;93m" : "";
	const char *yellow_bold = tty ? "\E[1;33m" : "";
	const char *magenta = tty ? "\E[0;95m" : "";
	const char *magenta_bold = tty ? "\E[1;35m" : "";
	const char *cyan = tty ? "\E[0;96m" : "";
	const char *cyan_bold = tty ? "\E[1;36m" : "";
	const char *reset = tty ? "\E[0m" : "";

	switch (p_type) {
		case ERR_WARNING:
			logf_error("%sWARNING:%s %s\n", yellow_bold, yellow, err_details);
			logf_error("%s     at: %s (%s:%i)%s\n", gray, p_function, p_file, p_line, reset);
			break;
		case ERR_SCRIPT:
			logf_error("%sSCRIPT ERROR:%s %s\n", magenta_bold, magenta, err_details);
			logf_error("%s          at: %s (%s:%i)%s\n", gray, p_function, p_file, p_line, reset);
			break;
		case ERR_SHADER:
			logf_error("%sSHADER ERROR:%s %s\n", cyan_bold, cyan, err_details);
			logf_error("%s          at: %s (%s:%i)%s\n", gray, p_function, p_file, p_line, reset);
			break;
		case ERR_ERROR:
		default:
			logf_error("%sERROR:%s %s\n", red_bold, red, err_details);
			logf_error("%s   at: %s (%s:%i)%s\n", gray, p_function, p_file, p_line, reset);
			break;
	}
}

UnixTerminalLogger::~UnixTerminalLogger() {}

OS_Unix::OS_Unix() {
	Vector<Logger *> loggers;
	loggers.push_back(memnew(UnixTerminalLogger));
	_set_logger(memnew(CompositeLogger(loggers)));
}

#endif
