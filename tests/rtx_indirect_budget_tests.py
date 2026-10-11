"""Exercise phase coverage and the GPU-time controller, including extremes."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class RtxIndirectBudgetTests(unittest.TestCase):
    def test_sampling_and_budget(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if compiler is None:
            self.skipTest('requires a C++ compiler')
        source = '''#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>
#include <cstdio>
#include <initializer_list>
using uint = uint32_t;
'''
        source += (ROOT / 'code/rendererrtx/shaders/rt_indirect_sampling.glsl').read_text()
        source += (ROOT / 'code/rendererrtx/rt/rt_quality.h').read_text()
        source += r'''
int main() {
    for (uint d=1; d<=4; ++d) {
        for (bool checker : {false, true}) {
            uint period=d*d*(checker ? 2 : 1);
            // Include odd image edges and tile boundaries. Every pixel gets
            // exactly one observation per cycle, with an unbiased density.
            for (uint y=0;y<35;y++) for(uint x=0;x<67;x++) {
                uint hits=0; float weight=0;
                for (uint frame=0;frame<period;frame++) {
                    float density=rt_indirect_density(x,y,frame,d,checker);
                    if (density>0) hits++;
                    weight+=density;
                }
                assert(hits==1 && weight==float(period));
            }
        }
    }
    assert(RT_BudgetDivisor(2,1,25,16)==3);
    assert(RT_BudgetDivisor(4,1,100,16)==4);
    assert(RT_BudgetDivisor(3,1,9,16)==2);
    assert(RT_BudgetDivisor(2,2,1,16)==2);
    assert(RT_BudgetDivisor(2,1,16,16)==2);
    assert(RT_BudgetDivisor(4,1,20,0)==1);
    assert(RT_BudgetDivisor(4,2,0,16)==2);
    assert(RT_BudgetDivisor(4,2,std::numeric_limits<float>::quiet_NaN(),16)==2);
    std::puts("phase coverage, inverse density and budget bounds: PASS");
}
'''
        with tempfile.TemporaryDirectory(prefix='rtx-budget-') as temp:
            path = Path(temp)
            cpp = path / 'budget.cpp'
            exe = path / ('budget.exe' if os.name == 'nt' else 'budget')
            cpp.write_text(source, encoding='utf-8')
            subprocess.run([compiler, *compiler_target_args(compiler), '-std=c++17', str(cpp), '-o', str(exe)],
                           cwd=path, check=True, capture_output=True, text=True)
            result = subprocess.run([str(exe)], cwd=path, check=True, capture_output=True, text=True)
            self.assertIn('PASS', result.stdout)


if __name__ == '__main__':
    unittest.main()
