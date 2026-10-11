"""Frame pacing is optional, early, and never changes game/network clocks."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args
from mesh_tag_tests import function

ROOT = Path(__file__).resolve().parents[1]


class RtxLatencyTests(unittest.TestCase):
    def test_pacing_targets_and_percentiles(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if compiler is None:
            self.skipTest('requires a C++ compiler')
        source = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <limits>
#define Com_Memset std::memset
#define qtrue 1
#define qfalse 0
#define VK_NULL_HANDLE 0
#define VK_TRUE 1
#define VK_SUCCESS 0
#define PRINT_WARNING 1
#define NUM_COMMAND_BUFFERS 2
using qboolean=int;
using VkResult=int;
enum refFrameEvent_t {REF_FRAME_PREPARE_INPUT,REF_FRAME_INPUT_POLLED,REF_FRAME_INPUT_SAMPLE,
                     REF_FRAME_SIMULATION_BEGIN,REF_FRAME_SIMULATION_END,REF_FRAME_EVENT_COUNT};
struct {int64_t events[REF_FRAME_EVENT_COUNT];float earlyFence,lateFence,earlyAcquire,lateAcquire,submitMs,presentMs;
        qboolean preparingInput;int lastSubmittedSlot;} s_vkRtLatency;
struct Cvar {int integer;}; Cvar policy={1}; Cvar *rtx_rt_low_latency=&policy;
struct Slot {int waitForFence,rendering_finished_fence,swapchain_image_acquired;};
struct {int device,cmd_index;Slot tess[2],*cmd;struct {int activeRtMode;} caps;} vk;
static bool minimized=false;
static int waits=0,acquires=0,waited=-1,fail=0;
static qboolean Minimized(){return minimized;}
static void Print(int,const char*,...){}
struct {qboolean (*CL_IsMinimized)();void (*Printf)(int,const char*,...);} ri={Minimized,Print};
static int64_t vk_rt_now_microseconds(){return 100;}
static const char *vk_result_string(int){return "failure";}
static void vk_rt_latency_wait(int64_t,qboolean){}
static VkResult qvkWaitForFences(int,int,int *f,int,uint64_t){
    assert(s_vkRtLatency.preparingInput);assert(!acquires);waits++;waited=*f;return fail;
}
static void vk_acquire_frame_image(){assert(s_vkRtLatency.preparingInput);acquires++;}
'''
        source += function((ROOT / 'code/rendererrtx/vk.c').read_text(), 'void vk_rt_frame_event(')
        source += (ROOT / 'code/rendererrtx/rt/rt_frame_stats.h').read_text()
        source += r'''
static void Clear(){waits=acquires=0;waited=-1;}
int main(){
    vk.device=1;vk.caps.activeRtMode=2;vk.cmd_index=0;
    vk.tess[0]={1,10,0};vk.tess[1]={1,11,0};s_vkRtLatency.lastSubmittedSlot=1;
    vk_rt_frame_event(REF_FRAME_PREPARE_INPUT,90);
    assert(waits==1 && waited==11 && acquires==1);
    assert(!s_vkRtLatency.preparingInput);
    // Reset/command recording remain at the normal begin-frame boundary.
    assert(vk.tess[0].waitForFence && vk.tess[1].waitForFence);
    Clear();policy.integer=2;vk_rt_frame_event(REF_FRAME_PREPARE_INPUT,91);
    assert(waits==1 && waited==10 && acquires==1);
    Clear();policy.integer=0;vk_rt_frame_event(REF_FRAME_PREPARE_INPUT,92);
    assert(waits==0 && acquires==0);
    Clear();policy.integer=1;minimized=true;vk_rt_frame_event(REF_FRAME_PREPARE_INPUT,93);
    assert(waits==0 && acquires==0);minimized=false;
    Clear();fail=-1;vk_rt_frame_event(REF_FRAME_PREPARE_INPUT,94);
    assert(waits==1 && acquires==0 && !s_vkRtLatency.preparingInput);fail=0;
    Clear();vk.device=0;vk_rt_frame_event(REF_FRAME_PREPARE_INPUT,95);
    assert(waits==0 && acquires==0);vk.device=1;
    Clear();vk.caps.activeRtMode=0;vk_rt_frame_event(REF_FRAME_PREPARE_INPUT,96);
    assert(waits==0 && acquires==0);
    vk_rt_frame_event(REF_FRAME_INPUT_SAMPLE,97);
    assert(s_vkRtLatency.events[REF_FRAME_INPUT_SAMPLE]==97 && waits==0);
    rtFrameStats_t stats={};float a,b,c;
    RT_FrameStatsPercentiles(&stats,&a,&b,&c);assert(a==0 && b==0 && c==0);
    for(int i=1;i<=256;i++) RT_FrameStatsAdd(&stats,float(i));
    RT_FrameStatsPercentiles(&stats,&a,&b,&c);assert(a==128 && b==244 && c==254);
    for(int i=257;i<=512;i++) RT_FrameStatsAdd(&stats,float(i));
    RT_FrameStatsAdd(&stats,-1);RT_FrameStatsAdd(&stats,std::numeric_limits<float>::infinity());
    RT_FrameStatsAdd(&stats,std::numeric_limits<float>::quiet_NaN());
    RT_FrameStatsPercentiles(&stats,&a,&b,&c);assert(a==384 && b==500 && c==510);
    assert(stats.count==256);
    std::puts("early pacing, optional policy, failure/minimize and percentile ring: PASS");
}
'''
        with tempfile.TemporaryDirectory(prefix='rtx-latency-') as temp:
            path = Path(temp)
            cpp = path / 'latency.cpp'
            exe = path / ('latency.exe' if os.name == 'nt' else 'latency')
            cpp.write_text(source, encoding='utf-8')
            subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', str(cpp), '-o', str(exe)],
                           cwd=path, check=True, capture_output=True, text=True)
            result = subprocess.run([str(exe)], cwd=path, check=True, capture_output=True, text=True)
            self.assertIn('PASS', result.stdout)

    def test_platform_order_and_unchanged_clock_arguments(self):
        for relative in ('code/win32/win_main.cpp', 'code/unix/unix_main.cpp'):
            source = (ROOT / relative).read_text()
            start = source.index('CL_RendererPrepareInput();')
            poll = source.index('IN_Frame();', start)
            self.assertLess(start, poll)
            self.assertLess(poll, source.index('Com_Frame( CL_NoDelay() );', poll))
        source = (ROOT / 'code/qcommon/common.c').read_text()
        simulation = source.index('SV_Frame( gameMsec, realMsec );')
        self.assertLess(source.index('CL_RendererSimulation( qtrue );'), simulation)
        self.assertGreater(source.index('CL_RendererSimulation( qfalse );'), simulation)
        self.assertIn('CL_Frame( gameMsec, realMsec );', source)
        paced = function((ROOT / 'code/rendererrtx/vk.c').read_text(), 'void vk_rt_frame_event(')
        for forbidden in ('QueueWaitIdle', 'DeviceWaitIdle', 'com_frameTime', 'Com_EventLoop', 'qvkResetFences'):
            self.assertNotIn(forbidden, paced)


if __name__ == '__main__':
    unittest.main()
