"""Exercise Outcast's production UI definition boundary and shared parser."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
import zipfile

from compiler_support import compiler_target_args
from rtcw_server_world_tests import function
from jka_parser_global_tests import parser_fixture, SOURCES

ROOT = Path(__file__).resolve().parents[1]


def source():
    # Reuse the real parser's file/allocation fixture, not its implementation.
    base = parser_fixture().split('int main(int argc,char **argv){')[0]
    base = '#include <map>\n#include <algorithm>\nstruct Drop{};\n' + base.replace('std::exit( 3 );', 'throw Drop{};')
    base = base.replace('static std::size_t importedPosition;', '''static std::size_t importedPosition;
static std::map<std::string,std::string> packaged;
static std::vector<std::pair<const std::string *,std::size_t>> packagedHandles;''')
    base = base.replace('std::string path = root + "/" + qpath;', '''
 if(auto found=packaged.find(qpath);found!=packaged.end()){
  *handle=20000+int(packagedHandles.size());packagedHandles.emplace_back(&found->second,0);return int(found->second.size());
 }
 std::string path = root + "/" + qpath;''')
    base = base.replace('static int HostRead( void *buffer, int len, fileHandle_t handle ) {', '''static int HostRead( void *buffer, int len, fileHandle_t handle ) {
 if(handle>=20000){auto &h=packagedHandles.at(handle-20000);assert(h.first);const auto n=(std::min)(std::size_t(len),h.first->size()-h.second);
 std::memcpy(buffer,h.first->data()+h.second,n);h.second+=n;return int(n);}''')
    base = base.replace('static void HostClose( fileHandle_t handle ) {', '''static void HostClose( fileHandle_t handle ) {
 if(handle>=20000){packagedHandles.at(handle-20000).first=nullptr;return;}''')
    ui = (ROOT / 'code/client/client_jk2_ui.hpp').read_text()
    vm = (ROOT / 'code/qcommon/vm.c').read_text()
    base += r'''
#include "code/qcommon/qcommon.h"
#include "code/qcommon/vm_local.h"
#include "code/ui/ui_public.h"
#include "code/compat/jk2_client_abi.h"
#include <limits>
#include <cstdint>
static vm_t *uivm;
static botlib_export_t *botlib_export;
static int timeCalls;
static bool convertTime=true;
static const qtime_t calendar{59,58,23,31,11,126,4,364,-1};
int Com_RealTime(qtime_t *out){++timeCalls;if(out&&convertTime)*out=calendar;return 0x12345678;}
'''
    base += function(vm, 'void *VM_ExplicitArgPtr(') + '\n'
    base += function(vm, 'void VM_CheckBounds(') + '\n'
    base += function(ui, 'static void *CL_JK2UISpan(') + '\n'
    base += function(ui, 'static bool CL_JK2UIDefinitionCall(') + '\n'
    base += function(ui, 'static bool CL_JK2UIRecordCall(') + '\n'
    base += function(ui, 'static intptr_t CL_JK2UISharedCall(') + '\n'
    return base + r'''
template<class F>void Reject(F call){bool rejected=false;try{call();}catch(Drop){rejected=true;}assert(rejected);}
static int InvokeRaw(int ordinal,intptr_t address=0){
 intptr_t args[]{ordinal,address},result=-777;
 assert(CL_JK2UIDefinitionCall(args,result));return int(result);
}
static int Invoke(int ordinal,const char *text=nullptr){return InvokeRaw(ordinal,reinterpret_cast<intptr_t>(text));}
static int loads,adds,clears,sourceLoads;static const char *external;
static int Load(const char *text){assert(text!=external);++loads;return PC_LoadGlobalDefines(text);}
static int Add(const char *text){assert(text!=external);++adds;return PC_AddGlobalDefine(text);}
static int Source(const char *text){assert(text!=external);++sourceLoads;return PC_LoadJKASourceHandle(text);}
static void Clear(){++clears;PC_RemoveAllGlobalDefines();}
static int reads,diagnostics,badToken,badDiagnostic;
static int ReadToken(int handle,pc_token_t *token){
 ++reads;if(badToken){std::memset(token->string,'x',sizeof(token->string));return 1;}
 return PC_ReadTokenHandle(handle,token);
}
static int Diagnostic(int handle,char *filename,int *line){
 ++diagnostics;
 if(badDiagnostic){
  std::memset(filename,'x',badDiagnostic==1?MAX_STRING_CHARS:128);
  if(badDiagnostic!=1)filename[badDiagnostic==4?127:128]=0;
  *line=badDiagnostic==3?-1:123;return 1;
 }
 return PC_SourceFileAndLine(handle,filename,line);
}
static int Record(int ordinal,intptr_t a,intptr_t b=0,intptr_t c=0){
 intptr_t args[]{ordinal,a,b,c},result=-777;assert(CL_JK2UIRecordCall(args,result));return int(result);
}
static void NameTokens(int handle){
 assert(handle);pc_token_t token{};
 for(const char *name:{"@MENUS_VALUE","@","@Name2","next","@MENUS_INCLUDED","@MENUS_ALIAS"}){
  assert(PC_ReadTokenHandle(handle,&token));assert(token.type==TT_NAME&&token.subtype==int(std::strlen(name)));
  assert(!std::strcmp(token.string,name)&&token.intvalue==0&&token.floatvalue==0);
 }
 assert(!PC_ReadTokenHandle(handle,&token));assert(PC_FreeSourceHandle(handle));
}
static_assert(sizeof(fntech3_jk2_mp_pc_token_t)==1040);
static_assert(offsetof(fntech3_jk2_mp_pc_token_t,type)==0);
static_assert(offsetof(fntech3_jk2_mp_pc_token_t,subtype)==4);
static_assert(offsetof(fntech3_jk2_mp_pc_token_t,intvalue)==8);
static_assert(offsetof(fntech3_jk2_mp_pc_token_t,floatvalue)==12);
static_assert(offsetof(fntech3_jk2_mp_pc_token_t,string)==16);
static_assert(sizeof(fntech3_jk2_mp_qtime_t)==36);
static_assert(offsetof(fntech3_jk2_mp_qtime_t,tm_isdst)==32);
static void Records(bool native,const char *path="tokens.menu",bool retail=false){
 std::array<byte,4128> memory;memory.fill(0xa5);auto *data=memory.data()+16;
 vm_t vm{};vm.dataBase=data;vm.dataMask=4095;vm.exactDataLength=4096;
 if(native)vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 vm_t *previous=uivm;uivm=&vm;
 auto address=[&](int offset)->intptr_t{return native?reinterpret_cast<intptr_t>(data+offset):offset;};
 botlib_export->PC_ReadTokenHandle=ReadToken;botlib_export->PC_SourceFileAndLine=Diagnostic;
 int handle=PC_LoadSourceHandle(path);assert(handle);
 const int expectedHandle=PC_LoadSourceHandle(path);assert(expectedHandle);
 pc_token_t expected{};
 while(PC_ReadTokenHandle(expectedHandle,&expected)){
  memory.fill(0xa5);const auto before=memory;
  assert(Record(66,handle,address(3))==1);
  fntech3_jk2_mp_pc_token_t actual;std::memcpy(&actual,data+3,sizeof(actual));
  assert(actual.type==expected.type&&actual.subtype==expected.subtype&&actual.intvalue==expected.intvalue);
  assert(!std::memcmp(&actual.floatvalue,&expected.floatvalue,4)&&!std::strcmp(actual.string,expected.string));
  auto unchanged=memory;std::memcpy(unchanged.data()+19,before.data()+19,sizeof(actual));assert(unchanged==before);
 }
 auto before=memory;assert(Record(66,handle,address(3))==0&&memory==before);
 assert(PC_FreeSourceHandle(handle)&&PC_FreeSourceHandle(expectedHandle));
 if(retail){uivm=previous;return;}
 for(int h:{-1,0,64,INT_MIN,INT_MAX}){assert(Record(66,h,address(3))==0);assert(Record(67,h,address(1101),address(1233))==0);assert(memory==before);}
 handle=PC_LoadSourceHandle(path);assert(handle);
 memory.fill(0xa5);assert(Record(67,handle,address(1101),address(1233))==1);
 assert(!std::strcmp(reinterpret_cast<char *>(data+1101),path));int line;std::memcpy(&line,data+1233,4);assert(line==1);
 assert(Record(66,handle,address(3))==1);
 before=memory;const int wasReads=reads,wasDiagnostics=diagnostics,wasTime=timeCalls;
 Reject([&]{Record(66,handle,0);});Reject([&]{Record(67,handle,0,address(1233));});
 Reject([&]{Record(67,handle,address(1101),0);});
 if(!native){
  for(intptr_t bad:{intptr_t(-1),intptr_t(vm.dataMask),intptr_t(vm.dataMask+1)}){
   Reject([&]{Record(66,handle,bad);});Reject([&]{Record(67,handle,bad,address(1233));});Reject([&]{Record(72,bad);});
  }
  Reject([&]{Record(66,handle,vm.dataMask-1039);});
  Reject([&]{Record(67,handle,vm.dataMask-127,address(1233));});
  Reject([&]{Record(67,handle,address(1101),vm.dataMask-3);});Reject([&]{Record(72,vm.dataMask-35);});
  if(sizeof(intptr_t)>4)Reject([&]{Record(66,handle,static_cast<intptr_t>(INT64_C(0x100000003)));});
 }
 assert(reads==wasReads&&diagnostics==wasDiagnostics&&timeCalls==wasTime&&memory==before);
 botlib_export->PC_ReadTokenHandle=nullptr;Reject([&]{Record(66,handle,address(3));});botlib_export->PC_ReadTokenHandle=ReadToken;
 botlib_export->PC_SourceFileAndLine=nullptr;Reject([&]{Record(67,handle,address(1101),address(1233));});botlib_export->PC_SourceFileAndLine=Diagnostic;
 auto *api=botlib_export;botlib_export=nullptr;Reject([&]{Record(66,handle,address(3));});botlib_export=api;
 if(sizeof(intptr_t)>4)Reject([&]{Record(66,static_cast<intptr_t>(INT64_C(0x100000001)),address(3));});
 badToken=1;Reject([&]{Record(66,handle,address(3));});badToken=0;
 for(int bad:{1,2,3}){badDiagnostic=bad;Reject([&]{Record(67,handle,address(1101),address(1233));});assert(memory==before);}
 badDiagnostic=4;assert(Record(67,handle,address(1101),address(1233))==1&&data[1101+127]==0);badDiagnostic=0;
 assert(Record(72,address(1251))==0x12345678);
 fntech3_jk2_mp_qtime_t actualTime;std::memcpy(&actualTime,data+1251,sizeof(actualTime));
 assert(actualTime.tm_sec==59&&actualTime.tm_min==58&&actualTime.tm_hour==23&&actualTime.tm_mday==31);
 assert(actualTime.tm_mon==11&&actualTime.tm_year==126&&actualTime.tm_wday==4&&actualTime.tm_yday==364&&actualTime.tm_isdst==-1);
 before=memory;convertTime=false;assert(Record(72,address(1251))==0x12345678&&memory==before);convertTime=true;
 assert(Record(72,0)==0x12345678&&memory==before);
 if(!native){assert(Record(72,vm.dataMask-36)==0x12345678);assert(Record(66,handle,vm.dataMask-1040)==1);}
 for(int i=0;i<16;++i)assert(memory[i]==0xa5&&memory[4112+i]==0xa5);
 assert(PC_FreeSourceHandle(handle));uivm=previous;
}
int main(int argc,char **argv){
 assert(argc>=2);root=argv[1];
 botimport.Print=HostPrint;botimport.GetMemory=HostGetMemory;botimport.FreeMemory=HostFreeMemory;
 botimport.AvailableMemory=HostAvailableMemory;botimport.HunkAlloc=HostHunkAlloc;
 botimport.FS_FOpenFile=HostOpen;botimport.FS_Read=HostRead;botimport.FS_FCloseFile=HostClose;
 botimport.Sys_Milliseconds=HostMilliseconds;
 vm_t vm{};vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);uivm=&vm;
 botlib_export_t api{};api.PC_LoadGlobalDefines=Load;api.PC_AddGlobalDefine=Add;
 api.PC_RemoveAllGlobalDefines=Clear;api.PC_LoadJKASourceHandle=Source;botlib_export=&api;
 assert(CL_JK2UISharedCall(70)==UI_S_STOPBACKGROUNDTRACK);
 assert(CL_JK2UISharedCall(71)==UI_S_STARTBACKGROUNDTRACK);
 for(int ordinal:{62,63,64,66,67,68,69,72})assert(CL_JK2UISharedCall(ordinal)==-1);
 intptr_t unrelated[]{72},unhandled=123;
 assert(!CL_JK2UIDefinitionCall(unrelated,unhandled)&&unhandled==123);
 if(argc>2&&!std::strcmp(argv[2],"--menus")){
  std::string filename,contents;while(std::getline(std::cin,filename,'\0')){assert(std::getline(std::cin,contents,'\0'));packaged.emplace(filename,contents);}
  api.PC_ReadTokenHandle=ReadToken;api.PC_SourceFileAndLine=Diagnostic;
  unsigned references=0,menus=0;
  for(bool native:{true,false}){
   std::array<byte,4096> memory{};vm.entryPoint=native?reinterpret_cast<decltype(vm.entryPoint)>(1):nullptr;
   vm.dataBase=memory.data();vm.dataMask=4095;
   auto address=[&](int offset)->intptr_t{return native?reinterpret_cast<intptr_t>(memory.data()+offset):offset;};
   std::strcpy(reinterpret_cast<char *>(memory.data()+3),"ui/jk2mp/menudef.h");external=reinterpret_cast<const char *>(memory.data()+3);
   assert(InvokeRaw(68,address(3))==1);
   for(const auto &file:packaged)if(file.first.size()>5&&file.first.substr(file.first.size()-5)==".menu"){
    std::strcpy(reinterpret_cast<char *>(memory.data()+3),file.first.c_str());external=reinterpret_cast<const char *>(memory.data()+3);
    const int handle=InvokeRaw(64,address(3));assert(handle);const int beforeErrors=errors;
    while(Record(66,handle,address(256))){
     fntech3_jk2_mp_pc_token_t token;std::memcpy(&token,memory.data()+256,sizeof(token));
     if(token.string[0]=='@'&&token.type==TT_NAME){assert(token.subtype==int(std::strlen(token.string)));assert(token.intvalue==0&&token.floatvalue==0);++references;}
    }
    assert(errors==beforeErrors);assert(PC_FreeSourceHandle(handle));++menus;
   }
   assert(InvokeRaw(69)==0);
  }
  assert(menus>0&&references>1000&&allocations==0);for(const auto &h:packagedHandles)assert(!h.first);
  std::cout<<"Outcast shipped menus passed count="<<menus<<" references="<<references<<"\n";return 0;
 }
 if(argc>2){
  std::getline(std::cin,imported,'\0');external="retail.h";
  assert(Invoke(68,external)==1);Check("retail.menu",{"1","2","3"});Records(true,"retail.menu",true);Records(false,"retail.menu",true);assert(Invoke(69)==0);
  std::array<byte,128> memory{};std::memcpy(memory.data()+3,"retail.h",9);
  vm.entryPoint=nullptr;vm.dataBase=memory.data();vm.dataMask=127;
  external=reinterpret_cast<const char *>(memory.data()+3);const auto saved=memory;
  assert(InvokeRaw(68,3)==1);Check("retail.menu",{"1","2","3"});assert(InvokeRaw(69)==0);
  assert(memory==saved&&allocations==0);std::cout<<"Outcast retail definitions passed native and QVM\n";return 0;
 }
 external="raven.menu";NameTokens(Invoke(64,external));NameTokens(PC_LoadJKASourceHandle("raven.menu"));
 assert(Read(PC_LoadSourceHandle("raven.menu")).empty());assert(Read(PC_LoadWolfETSourceHandle("raven.menu")).empty());
 external="EXISTING 7";assert(Invoke(63,external)==1);
 const int before=PC_LoadSourceHandle("before.menu");assert(before);
 external="valid.h";assert(Invoke(68,external)==1);
 assert(Read(before)==std::vector<std::string>({"VALUE","7"}));
 Check("future.menu",{"42","7","9","[","1","+","2","]"});
 const int retained=PC_LoadSourceHandle("future.menu");assert(retained);
 external="replacement.h";assert(Invoke(68,external)==1);Check("before.menu",{"43","7"});
 for(const char *bad:{"missing.h","bad.h","unterminated.h","../valid.h","/valid.h","C:/valid.h",""}){
  external=bad;assert(Invoke(68,bad)==0);Check("rollback.menu",{"LEAK","43","7"});
 }
 const int wasLoads=loads,wasAdds=adds,wasClears=clears;
 std::array<char,MAX_TOKEN_CHARS+1> text;text.fill('x');text.back()=0;
 Reject([&]{Invoke(63,text.data());});Reject([&]{Invoke(68,text.data());});
 Reject([&]{Invoke(68);});Reject([&]{Invoke(63);});
 Reject([&]{Invoke(68,reinterpret_cast<const char *>(std::uintptr_t(-1)));});
 uivm=nullptr;Reject([&]{Invoke(69);});uivm=&vm;
 vm.entryPoint=nullptr;Reject([&]{InvokeRaw(68,3);});
 vm.entryPoint=reinterpret_cast<decltype(vm.entryPoint)>(1);
 botlib_export=nullptr;Reject([&]{Invoke(68,"valid.h");});Reject([&]{Invoke(69);});botlib_export=&api;
 api.PC_LoadGlobalDefines=nullptr;Reject([&]{Invoke(68,"valid.h");});api.PC_LoadGlobalDefines=Load;
 api.PC_LoadJKASourceHandle=nullptr;Reject([&]{Invoke(64,"raven.menu");});api.PC_LoadJKASourceHandle=Source;
 api.PC_AddGlobalDefine=nullptr;Reject([&]{Invoke(63,"VALUE 1");});api.PC_AddGlobalDefine=Add;
 api.PC_RemoveAllGlobalDefines=nullptr;Reject([&]{Invoke(69);});api.PC_RemoveAllGlobalDefines=Clear;
 assert(loads==wasLoads&&adds==wasAdds&&clears==wasClears);
 Check("rollback.menu",{"LEAK","43","7"});
 intptr_t clearOnly[]{69};intptr_t result=-1;
 assert(CL_JK2UIDefinitionCall(clearOnly,result)&&result==0);Check("before.menu",{"VALUE","EXISTING"});
 assert(Read(retained)==std::vector<std::string>({"42","7","9","[","1","+","2","]"}));
 assert(Invoke(69)==0&&allocations==0);Records(true);Records(false);assert(allocations==0);
 // Outcast's installed UI is a QVM: its arguments are offsets, never host
 // pointers. Execute the real VM translation/bounds functions on guarded data.
 std::array<byte,4128> memory;memory.fill(0xa5);byte *data=memory.data()+16;
 vm.entryPoint=nullptr;vm.dataBase=data;vm.dataMask=4095;vm.exactDataLength=4096;
 auto offset=[&](int ordinal,const char *value,int address=3){
  std::memcpy(data+address,value,std::strlen(value)+1);external=reinterpret_cast<const char *>(data+address);
  const auto saved=memory;const int answer=InvokeRaw(ordinal,address);assert(memory==saved);return answer;
 };
 assert(offset(63,"EXISTING 7")==1);assert(offset(68,"valid.h")==1);
 NameTokens(offset(64,"raven.menu"));
 Check("future.menu",{"42","7","9","[","1","+","2","]"});
 assert(offset(68,"valid.h",int(vm.dataMask)-8)==1); // NUL at final valid byte.
 const int qvmLoads=loads,qvmAdds=adds;const auto saved=memory;
 for(int bad:{-1,0,int(vm.dataMask),int(vm.dataMask)+1,INT_MAX})
  Reject([&]{InvokeRaw(68,bad);});
 if(sizeof(intptr_t)>4)Reject([&]{InvokeRaw(68,static_cast<intptr_t>(INT64_C(0x100000003)));});
 assert(memory==saved);
 std::memset(data+vm.dataMask-3,'x',3);Reject([&]{InvokeRaw(68,vm.dataMask-3);});
 std::memset(data+100,'x',MAX_TOKEN_CHARS);data[100+MAX_TOKEN_CHARS]=0;
 Reject([&]{InvokeRaw(63,100);});Reject([&]{InvokeRaw(68,100);});
 vm.dataBase=nullptr;Reject([&]{InvokeRaw(68,3);});vm.dataBase=data;
 assert(loads==qvmLoads&&adds==qvmAdds);assert(InvokeRaw(69)==0);Check("before.menu",{"VALUE","EXISTING"});
 for(int i=0;i<16;++i)assert(memory[i]==0xa5&&memory[4112+i]==0xa5);
 // The helper follows VM_ExplicitArgPtr's native-pointer convention for
 // dllExports too; module admission remains the loader's responsibility.
 vm.dataBase=nullptr;vm.dllExports=reinterpret_cast<void *>(1);
 external="EXISTING 7";assert(Invoke(63,external)==1);assert(Invoke(69)==0);
 assert(allocations==0);for(auto *file:files)assert(!file);
 std::cout<<"Outcast definition lifetime, native/QVM bounds and audio ordinals passed\n";
}
'''


class JK2UIDefinitionTests(unittest.TestCase):
    def test_production_boundary_parser_and_optional_retail_header(self):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++17 compiler unavailable')
        scratch = ROOT / '.tmp/tests/jk2-ui-definitions'
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as folder:
            directory = Path(folder).resolve()
            cpp = directory / 'host.cpp'
            executable = directory / ('host.exe' if os.name == 'nt' else 'host')
            cpp.write_text(source(), encoding='utf-8')
            files = {
                'valid.h': '#define VALUE 42\n#define F(a,b) [ a + b ]\n#include "extra.h"\n',
                'extra.h': '#define EXTRA 9\n', 'replacement.h': '#define VALUE 43\n',
                'before.menu': 'VALUE EXISTING\n', 'future.menu': 'VALUE EXISTING EXTRA F(1,2)\n',
                'rollback.menu': 'LEAK VALUE EXISTING\n', 'bad.h': '#define LEAK 1\n#invalid x\n',
                'unterminated.h': '#define LEAK 1\n#ifdef VALUE\n',
                'retail.menu': 'ITEM_TYPE_BUTTON ITEM_TYPE_RADIOBUTTON ITEM_TYPE_CHECKBOX\n',
                'tokens.menu': 'name 42 3.25 "quoted text" { - 0xff\nsecond\n',
                'raven.menu': '@MENUS_VALUE @ @Name2 next\n#include "raven-inc.menu"\n#define ALIAS @MENUS_ALIAS\nALIAS\n',
                'raven-inc.menu': '@MENUS_INCLUDED\n',
            }
            for name, contents in files.items():
                (directory / name).write_text(contents)
            target = compiler_target_args(compiler)
            if os.name == 'nt' and not target:
                target = ['-DFNTECH3_Q3_ONLY']
            built = subprocess.run([compiler, *target, '-std=c++17', '-O2',
                                    '-ffast-math', '-Werror', '-D_CRT_SECURE_NO_WARNINGS', '-DBOTLIB',
                                    '-I', str(ROOT), str(cpp), *[str(ROOT / path) for path in SOURCES],
                                    '-o', str(executable)], cwd=directory, capture_output=True, text=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            run = subprocess.run([str(executable), str(directory)], cwd=directory,
                                 capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn('Outcast definition lifetime', run.stdout)
            # Installed proprietary data is read into memory and piped to the
            # probe; no archive entries are copied into the repository.
            archive = Path(os.environ.get('FNTECH3_JK2_BASE', 'E:/Games/Jedi Outcast/base')) / 'assets1.pk3'
            if archive.is_file():
                with zipfile.ZipFile(archive) as package:
                    data = package.read('ui/jk2mp/menudef.h')
                    menus = [(name, package.read(name)) for name in package.namelist()
                             if name.startswith('ui/jk2mp/') and (name.endswith('.menu') or name.endswith('.h'))]
                retail = subprocess.run([str(executable), str(directory), '--retail'], input=data,
                                        cwd=directory, capture_output=True, timeout=30)
                self.assertEqual(retail.returncode, 0, (retail.stdout + retail.stderr).decode(errors='replace'))
                self.assertIn(b'Outcast retail definitions passed', retail.stdout)
                payload = b''.join(name.encode() + b'\0' + contents + b'\0' for name, contents in menus)
                retail = subprocess.run([str(executable), str(directory), '--menus'], input=payload,
                                        cwd=directory, capture_output=True, timeout=60)
                self.assertEqual(retail.returncode, 0, (retail.stdout + retail.stderr).decode(errors='replace'))
                self.assertIn(b'Outcast shipped menus passed', retail.stdout)
                print(retail.stdout.decode().strip())


if __name__ == '__main__':
    unittest.main()
