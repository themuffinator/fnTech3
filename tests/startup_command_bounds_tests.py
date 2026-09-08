"""Execute the production early parser at its command-array boundary."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


class StartupCommandBoundsTests(unittest.TestCase):
    def test_early_parser_bounds_and_quotes(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        source = (ROOT / 'code/qcommon/common.c').read_text()
        start = source.index('static void Com_ParseCommandLine( char *commandLine )')
        end = source.index('\nchar cl_title', start)
        parser = source[start:end]
        harness = r'''
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#define MAX_CONSOLE_LINES 32
static int com_numConsoleLines;
static char *com_consoleLines[MAX_CONSOLE_LINES];
static char rconPassword2[1024];
static std::string diagnostic;
void Sys_Error(const char *format, ...) {
 char text[2048]; va_list args; va_start(args,format);
 vsnprintf(text,sizeof(text),format,args); va_end(args);
 diagnostic=text; throw 1;
}
PARSER
int main(int argc,char **argv) {
 assert(argc==3);
 int count=std::stoi(argv[1]); char separator=argv[2][0];
 std::string input;
 for(int i=0;i<count;i++) {
  input+=separator;
  input+="set fixture "; input+=std::to_string(i);
  input+=" \"quoted+value\"";
 }
 bool failed=false;
 try { Com_ParseCommandLine(input.data()); } catch(int) { failed=true; }
 assert(failed==(count>MAX_CONSOLE_LINES));
 assert(com_numConsoleLines<=MAX_CONSOLE_LINES);
 if(failed) {
  assert(diagnostic.find("maximum 32")!=std::string::npos);
  assert(diagnostic.find("+exec")!=std::string::npos);
  assert(diagnostic.find("quoted+value")==std::string::npos);
 } else {
  assert(com_numConsoleLines==count);
  for(int i=0;i<count;i++) {
   std::string expected="set fixture "+std::to_string(i)+" \"quoted+value\"";
   assert(expected==com_consoleLines[i]);
  }
  // The early window pass and Com_Init may both call the parser.
  char later[]="+set should_not_reparse 1";
  Com_ParseCommandLine(later);
  assert(com_numConsoleLines==count);
 }
}
'''.replace('PARSER', parser)
        with tempfile.TemporaryDirectory() as directory:
            cpp = Path(directory) / 'startup.cpp'
            exe = Path(directory) / 'startup.exe'
            cpp.write_text(harness)
            built = subprocess.run([compiler, *compiler_target_args(compiler),
                                    '-std=c++17', str(cpp), '-o', str(exe)],
                                   capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            for separator in ['+', '\n', '\r']:
                for count in [0, 1, 31, 32, 33, 64]:
                    with self.subTest(separator=repr(separator), count=count):
                        ran = subprocess.run([str(exe), str(count), separator],
                                             capture_output=True, text=True)
                        self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)


if __name__ == '__main__':
    unittest.main()
