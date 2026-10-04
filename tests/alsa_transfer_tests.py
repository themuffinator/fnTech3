"""Execute both ALSA write paths with short-write and no-progress fixtures."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class AlsaTransferTests(unittest.TestCase):
    def test_partial_zero_and_failed_writes(self):
        compiler = shutil.which("clang++") or shutil.which("g++")
        if not compiler:
            self.skipTest("C++ compiler unavailable")
        source = (ROOT / "code/unix/linux_snd.cpp").read_text()
        start = source.index("static void thread_proc_direct( void )\n{")
        functions = source[start:source.index("#else // legacy OSS code path", start)]
        harness = r'''
#include <atomic>
#include <cassert>
#include <cstdio>
#include <vector>
#include <algorithm>
using snd_pcm_uframes_t=unsigned long;using snd_pcm_sframes_t=long;
using snd_pcm_state_t=int;using pid_t=int;struct snd_async_handler_t{};
constexpr int SYS_gettid=1,PRIO_PROCESS=1,SND_PCM_STATE_PREPARED=1;
int syscall(int){return 1;}int setpriority(int,int,int){return 0;}
int mutex=0;void _pthread_mutex_lock(int*){}void _pthread_mutex_unlock(int*){}
void _pthread_exit(int){}
short ring[]{0,1,2,3,4,5,6,7};
struct DMA{int samples=8,channels=1;unsigned char*buffer=reinterpret_cast<unsigned char*>(ring);}dma;
std::atomic_bool snd_loop{true},snd_async{true};
int handle=0,buffer_pos=6,frame_sz=2,waits=0,calls=0,failure=0;
unsigned period_time=20000;unsigned long period_size=2;
std::vector<short> played;
int restore_transfer(){return 0;}int __snd_pcm_state(int){return 2;}
void _snd_pcm_start(int){}long _snd_pcm_avail(int){return 5;}
void _snd_pcm_wait(int,int){if(++waits>1)snd_loop=false;}
long _snd_pcm_avail_update(int){return played.size()<5?5-static_cast<long>(played.size()):0;}
long _snd_pcm_writei(int,const unsigned char*data,long count){
 assert(++calls<20);if(failure)return failure==1?0:-1;
 long actual=(std::min)(count,1L);auto*p=reinterpret_cast<const short*>(data);
 played.insert(played.end(),p,p+actual);return actual;
}
void _snd_pcm_drop(int){}const char*_snd_strerror(int){return "fixture";}
''' + functions + r'''
int main(){
 for(bool asynchronous:{false,true})for(int fault:{0,1,2}){
  snd_loop=true;buffer_pos=6;waits=calls=0;failure=fault;played.clear();
  if(asynchronous)async_proc(nullptr);else thread_proc_direct();
  if(fault){assert(played.empty()&&buffer_pos==6&&calls==1);}
  else if(asynchronous){assert((played==std::vector<short>{6,7,0,1}));assert(buffer_pos==2);}
  else{assert((played==std::vector<short>{6,7,0,1,2}));assert(buffer_pos==3);}
 }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / "alsa.cpp"
            exe = Path(directory) / "alsa.exe"
            cpp.write_text(harness, encoding="utf-8")
            result = subprocess.run(
                [compiler, *compiler_target_args(compiler), "-std=c++17", str(cpp),
                 "-o", str(exe)], capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
