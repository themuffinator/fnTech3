"""Run an opt-in Windows probe on an unswitched private desktop.

This helper never injects input or switches the user's desktop. It owns one
child and closes the desktop only after that child has stopped.
"""
import ctypes as C
from ctypes import wintypes as W
import os, subprocess, uuid

if os.name == "nt":
    import msvcrt
    class SI(C.Structure):
        _fields_ = [('cb', W.DWORD), ('lpReserved', W.LPWSTR), ('lpDesktop', W.LPWSTR), ('lpTitle', W.LPWSTR),
            ('dwX',W.DWORD),('dwY',W.DWORD),('dwXSize',W.DWORD),('dwYSize',W.DWORD),('dwXCountChars',W.DWORD),
            ('dwYCountChars',W.DWORD),('dwFillAttribute',W.DWORD),('dwFlags',W.DWORD),('wShowWindow',W.WORD),
            ('cbReserved2',W.WORD),('lpReserved2',C.c_void_p),('hStdInput',W.HANDLE),('hStdOutput',W.HANDLE),('hStdError',W.HANDLE)]

    class PI(C.Structure):
        _fields_ = [('hProcess',W.HANDLE),('hThread',W.HANDLE),('dwProcessId',W.DWORD),('dwThreadId',W.DWORD)]

    kernel = C.WinDLL('kernel32', use_last_error=True)
    user = C.WinDLL('user32', use_last_error=True)
    user.CreateDesktopW.argtypes = (W.LPCWSTR,W.LPCWSTR,C.c_void_p,W.DWORD,W.DWORD,C.c_void_p)
    user.CreateDesktopW.restype = W.HANDLE
    user.CloseDesktop.argtypes = (W.HANDLE,)
    user.CloseDesktop.restype = W.BOOL
    kernel.CreateProcessW.argtypes = (W.LPCWSTR,W.LPWSTR,C.c_void_p,C.c_void_p,W.BOOL,W.DWORD,C.c_void_p,W.LPCWSTR,C.POINTER(SI),C.POINTER(PI))
    kernel.CreateProcessW.restype = W.BOOL
    kernel.GetExitCodeProcess.argtypes = (W.HANDLE,C.POINTER(W.DWORD))
    kernel.GetExitCodeProcess.restype = W.BOOL
    kernel.TerminateProcess.argtypes = (W.HANDLE,W.UINT)
    kernel.WaitForSingleObject.argtypes = (W.HANDLE,W.DWORD)
    kernel.CloseHandle.argtypes = (W.HANDLE,)

class WindowsPrivateProcess:
    def __init__(self, command, cwd, output):
        if os.name != "nt":
            raise OSError("private desktop probes require Windows")
        self.command = command
        self.name = "fntech3_probe_" + uuid.uuid4().hex
        self.desktop = None
        self.handle = None
        self.streams = []
        try:
            self.desktop = user.CreateDesktopW(self.name, None, None, 0, 0x01ff, None)
            if not self.desktop:
                raise C.WinError(C.get_last_error())
            self.streams = [open(os.devnull, "rb")]
            self.streams.append(open(output, "wb"))
            si = SI(); si.cb = C.sizeof(si); si.lpDesktop = self.name
            si.dwFlags = 0x101; si.wShowWindow = 4
            handles = [msvcrt.get_osfhandle(stream.fileno()) for stream in self.streams]
            si.hStdInput = handles[0]; si.hStdOutput = si.hStdError = handles[1]
            pi = PI()
            try:
                for handle in handles: os.set_handle_inheritable(handle, True)
                ok = kernel.CreateProcessW(str(command[0]), C.create_unicode_buffer(subprocess.list2cmdline(command)),
                    None, None, True, 0x08000000, None, str(cwd), C.byref(si), C.byref(pi))
                error = C.get_last_error()
                if ok:
                    self.handle = pi.hProcess
                    kernel.CloseHandle(pi.hThread)
            finally:
                for handle in handles: os.set_handle_inheritable(handle, False)
            if not ok:
                raise C.WinError(error)
        except BaseException:
            self.close()
            raise

    def poll(self):
        # Wait status distinguishes exit code 259 from a still-running child.
        status = kernel.WaitForSingleObject(self.handle, 0)
        if status == 0x102: return None
        if status != 0: raise C.WinError(C.get_last_error())
        code = W.DWORD()
        if not kernel.GetExitCodeProcess(self.handle, C.byref(code)):
            raise C.WinError(C.get_last_error())
        return code.value

    def wait(self, timeout):
        status = kernel.WaitForSingleObject(self.handle, max(0, int(timeout * 1000)))
        if status == 0x102: raise subprocess.TimeoutExpired(self.command, timeout)
        if status != 0: raise C.WinError(C.get_last_error())
        return self.poll()

    def terminate(self):
        if self.poll() is None and not kernel.TerminateProcess(self.handle, 1):
            if self.poll() is None: raise C.WinError(C.get_last_error())

    def close(self):
        if self.handle:
            if self.poll() is None:
                self.terminate()
                self.wait(5)
            kernel.CloseHandle(self.handle)
            self.handle = None
        for stream in self.streams: stream.close()
        self.streams = []
        if self.desktop:
            if not user.CloseDesktop(self.desktop): raise C.WinError(C.get_last_error())
            self.desktop = None
