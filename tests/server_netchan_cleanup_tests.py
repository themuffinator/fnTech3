"""Execute server netchannel cleanup before a disconnected slot can send again."""
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from compiler_support import compiler_target_args

ROOT=Path(__file__).resolve().parents[1]


class ServerNetchanCleanupTests(unittest.TestCase):
    def test_cleanup_cancels_active_fragments_and_queued_messages(self):
        compiler=shutil.which('clang++') or shutil.which('g++')
        if not compiler:self.skipTest('C++ compiler required')
        source=(ROOT/'code/server/sv_net_chan.cpp').read_text()
        start=source.index('void SV_Netchan_FreeQueue(client_t *client)')
        function=source[start:source.index('\n}',start)+2]
        harness=r'''
#include <cassert>
constexpr bool qfalse=false;
struct netchan_buffer_t {netchan_buffer_t *next;};
struct client_t {
 struct {bool unsentFragments;int unsentFragmentStart,unsentLength;int outgoingSequence;} netchan;
 netchan_buffer_t *netchan_start_queue;netchan_buffer_t **netchan_end_queue;
};
int freed;
void SV_ZFree(netchan_buffer_t *p){++freed;delete p;}
''' + function + r'''
int main(){
 client_t client{};
 client.netchan={true,1300,3000,42};
 client.netchan_start_queue=new netchan_buffer_t{new netchan_buffer_t{nullptr}};
 client.netchan_end_queue=&client.netchan_start_queue->next->next;
 SV_Netchan_FreeQueue(&client);
 assert(freed==2);
 assert(!client.netchan_start_queue);
 assert(client.netchan_end_queue==&client.netchan_start_queue);
 assert(!client.netchan.unsentFragments);
 assert(client.netchan.unsentFragmentStart==0 && client.netchan.unsentLength==0);
 assert(client.netchan.outgoingSequence==42);
 SV_Netchan_FreeQueue(&client);assert(freed==2);
}
'''
        with tempfile.TemporaryDirectory(prefix='fntech3-netchan-cleanup-') as folder:
            cpp=Path(folder)/'probe.cpp';exe=Path(folder)/'probe.exe';cpp.write_text(harness)
            run=subprocess.run([compiler,*compiler_target_args(compiler),'-std=c++17',str(cpp),'-o',str(exe)],capture_output=True,text=True,timeout=60)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            run=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)


if __name__=='__main__':unittest.main()
