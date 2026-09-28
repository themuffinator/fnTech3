"""Exercise private-desktop child lifecycle without game or user input."""
import ctypes, os, subprocess, sys, tempfile, unittest
from pathlib import Path
from unittest import mock
import windows_private_process
from windows_private_process import WindowsPrivateProcess
@unittest.skipUnless(os.name=='nt','Windows desktop process API required')
class WindowsPrivateProcessTests(unittest.TestCase):
 def test_private_desktop_and_terminal_exit_259(self):
  script=r'''
import ctypes as C, sys
from ctypes import wintypes as W
u=C.WinDLL('user32');k=C.WinDLL('kernel32')
u.GetThreadDesktop.argtypes=[W.DWORD];u.GetThreadDesktop.restype=W.HANDLE
u.GetUserObjectInformationW.argtypes=[W.HANDLE,C.c_int,C.c_void_p,W.DWORD,C.POINTER(W.DWORD)]
name=C.create_unicode_buffer(256);needed=W.DWORD()
assert u.GetUserObjectInformationW(u.GetThreadDesktop(k.GetCurrentThreadId()),2,name,C.sizeof(name),C.byref(needed))
print(name.value,flush=True)
sys.exit(int(sys.argv[1]))
'''
  with tempfile.TemporaryDirectory() as folder:
   output=Path(folder)/'stdout.log'
   for code in (0,259):
    p=WindowsPrivateProcess([sys.executable,'-c',script,str(code)],folder,output)
    try:
     self.assertEqual(p.wait(10),code)
     self.assertEqual(p.poll(),code)
     self.assertEqual(output.read_text().strip(),p.name)
    finally:p.close()
    p.close()
 def test_timeout_termination_and_failed_launch(self):
  with tempfile.TemporaryDirectory() as folder:
   output=Path(folder)/'stdout.log'
   with self.assertRaises(OSError):
    WindowsPrivateProcess([str(Path(folder)/'missing.exe')],folder,output)
   p=WindowsPrivateProcess([sys.executable,'-c','import time; time.sleep(20)'],folder,output)
   try:
    with self.assertRaises(subprocess.TimeoutExpired):p.wait(.01)
    self.assertIsNone(p.poll())
    p.terminate();self.assertEqual(p.wait(5),1)
   finally:p.close()
 def test_stop_while_the_child_is_already_exiting(self):
  # terminate() then close() at once, as probe_ritual_client stops a client:
  # Windows refuses the second TerminateProcess (ERROR_ACCESS_DENIED) from
  # the moment the first starts the exit, and signals the handle only once
  # the child's teardown ends, milliseconds later.
  with tempfile.TemporaryDirectory() as folder:
   output=Path(folder)/'stdout.log'
   p=WindowsPrivateProcess([sys.executable,'-c','import time; time.sleep(20)'],folder,output)
   try:
    self.assertTrue(p.terminate())
    p.close()
   finally:p.close()
   p=WindowsPrivateProcess([sys.executable,'-c','import time; time.sleep(20)'],folder,output)
   try:
    self.assertTrue(p.terminate())
    p.terminate();self.assertEqual(p.wait(5),1)
    self.assertFalse(p.terminate())
   finally:p.close()
 def test_refused_terminate_fails_only_while_the_child_runs(self):
  # A child exiting on its own is refused the same way.
  real=windows_private_process.kernel
  class Refusing:
   def __getattr__(self,name):return getattr(real,name)
   def TerminateProcess(self,handle,code):ctypes.set_last_error(5);return 0
  with tempfile.TemporaryDirectory() as folder:
   output=Path(folder)/'stdout.log'
   p=WindowsPrivateProcess([sys.executable,'-c','import sys,time; time.sleep(.5); sys.exit(7)'],folder,output)
   try:
    with mock.patch.object(windows_private_process,'kernel',Refusing()):self.assertFalse(p.terminate())
    self.assertEqual(p.poll(),7)
   finally:p.close()
   p=WindowsPrivateProcess([sys.executable,'-c','import time; time.sleep(20)'],folder,output)
   try:
    with mock.patch.object(windows_private_process,'kernel',Refusing()),mock.patch.object(WindowsPrivateProcess,'EXIT_GRACE',.2):
     with self.assertRaises(PermissionError) as caught:p.terminate()
    self.assertEqual(caught.exception.winerror,5)
    self.assertIsNone(p.poll())
   finally:p.close()
if __name__=='__main__':unittest.main()
