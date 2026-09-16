"""Execute MP dictionary parsing, formatting, overlays and native output boundaries."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest
from compiler_support import compiler_target_args

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    return source[start:source.index('\n}', start) + 2]


PRELUDE = r'''
#include "code/qcommon/vm_local.h"
#include "code/compat/rtcw_translation.hpp"
#include <cassert>
#include <climits>
#include <cstring>
#include <vector>
using namespace fntech3::rtcw::translation;
static Catalog rtcwTranslations;
static bool multiplayer = true, failBounds;
static int language, debug;
static std::vector<std::pair<uintptr_t, size_t>> ranges;
int Com_GameIsRTCWMP() { return multiplayer; }
int Cvar_VariableIntegerValue(const char *name) { return !std::strcmp(name, "cl_language") ? language : debug; }
void QDECL Com_Error(errorParm_t, const char *, ...) { throw 7; }
static void CheckBounds(const vm_t *, intptr_t address, size_t bytes) {
    if (failBounds) throw 8;
    for (const auto &range : ranges) {
        auto start = static_cast<uintptr_t>(address);
        if (start >= range.first && start-range.first <= range.second && bytes <= range.second-(start-range.first)) return;
    }
    throw 9;
}
#undef VM_CHECKBOUNDS
#define VM_CHECKBOUNDS CheckBounds
static std::string Record(std::string_view key, std::string_view french = "oui", std::string_view extra = "") {
    return "{ english \"" + std::string(key) + "\" french \"" + std::string(french) +
        "\" german \"ja\" italian \"si\" spanish \"si\" " + std::string(extra) + " }\n";
}
'''

CHECKS = r'''
template<class F> static void Reject(F f) { bool caught=false; try { f(); } catch(int) { caught=true; } assert(caught); }
int main() {
    Catalog catalog;
    assert(catalog.Load("/* header */\n#version \"1.1\"\n" + Record("Yes", "oui", "offset -1.25 2.5")).accepted);
    assert(catalog.Size()==1 && catalog.Find("YES")->x == -1.25f && catalog.Find("yes")->y == 2.5f);
    for (int lang : {INT_MIN,-1,0,5,INT_MAX}) assert(catalog.Translate("YES\n",lang,3)=="YES\n");
    assert(catalog.Translate("Y\ne\ns\n",1,0)=="oui\n");
    assert(catalog.Translate("Y\ne\ns\n",1,1)=="^1[oui]^7\n");
    assert(catalog.Translate("Yes",2,0)=="ja" && catalog.Translate("Yes",3,0)=="si");
    assert(catalog.Translate("Yes",4,0)=="si" && catalog.Translate("",1,3).empty());
    assert(catalog.Translate("Mi\nss\n",1,0)=="Mi\nss\n");
    assert(catalog.Translate("Mi\nss\n",1,1)=="^1[Mi\nss]^7\n");
    assert(catalog.Load(Record("yes","oui\n")).accepted);
    assert(catalog.Translate("Yes",1,1)=="^1[oui\n]^7");
    assert(catalog.Translate("Yes\n",1,1)=="^1[oui\n]^7\n");
    assert(catalog.Load(Record("YES","dernier")).accepted && catalog.Size()==1);
    assert(catalog.Translate("yes",1,0)=="dernier");
    assert(catalog.Find("yes")->x==-1.25f && catalog.Find("yes")->y==2.5f);
    assert(catalog.Load(Record("empty","")).accepted && catalog.Translate("EMPTY",1,1)=="^1[EMPTY]^7");
    assert(catalog.Load(Record("escape",R"(line\nnext)")).accepted);
    assert(catalog.Translate("escape",1,0)==R"(line\nnext)");
    std::string latin(1,char(0xe9)); assert(catalog.Load(Record("accent",latin)).accepted);
    assert(catalog.Translate("ACCENT",1,0)==latin);
    const auto savedSize=catalog.Size(); const auto saved=catalog.Translate("yes",1,0);
    for (const auto &bad : {std::string("/* unterminated"), std::string("#version"), std::string("unexpected"),
         std::string("{ english \"unfinished"), Record("new")+"{", Record("new","oui","offset nan 1"),
         Record("new","oui","offset x 1"), Record("new","oui","offset 1 2 missing"), Record("new","%n"),
         Record("new",std::string(TokenBytes,'a')), Record(std::string(TokenBytes,'a')),
         Record("new")+std::string(1,'\0')}) {
        assert(!catalog.Load(bad).accepted && catalog.Size()==savedSize && !catalog.Find("new"));
        assert(catalog.Translate("yes",1,0)==saved);
    }
    assert(!catalog.Load(std::string(FileBytes+1,' ')).accepted);
    assert(catalog.Load(Record(std::string(TokenBytes-1,'a'))).accepted);
    std::string a,b;
    assert(FormatSignature("%02d %s %%",a) && FormatSignature("%02f %s %%",b) && a!=b);
    assert(FormatSignature("%%x",a) && FormatSignature("%%y",b) && a!=b);
    assert(!FormatSignature("%n",a) && !FormatSignature("%hn",a) && !FormatSignature("%1$s",a));
    assert(FormatSignature(std::string(500,'%'),a)); // No fixed-size format scratch buffer.
    Catalog many; std::string records;
    for(size_t i=0;i<MaxEntries;++i) records+=Record(std::to_string(i));
    assert(many.Load(records).accepted && many.Size()==MaxEntries);
    assert(!many.Load(Record("one more")).accepted && many.Size()==MaxEntries);
    assert(many.Load(Record("0","replacement")).accepted);
    many.Clear(); assert(!many.Size());
    // Execute the actual service with real VM conversion and guarded module memory.
    assert(rtcwTranslations.Load(Record("Yes")).accepted);
    vm_t vm{}; vm.dllExports=&vm;
    std::array<char,OutputBytes+8> output; output.fill('X');
    const char input[]="YES";
    ranges={{reinterpret_cast<uintptr_t>(output.data()),OutputBytes}, {reinterpret_cast<uintptr_t>(input),sizeof(input)}};
    language=1; debug=0;
    CL_RTCWTranslateString(&vm,(intptr_t)input,(intptr_t)output.data());
    assert(!std::strcmp(output.data(),"oui") && output[4]=='X' && output[OutputBytes]=='X');
    CL_RTCWTranslateString(&vm,0,(intptr_t)output.data()); assert(!std::strcmp(output.data(),"(null)"));
    std::strcpy(output.data(),"YES"); CL_RTCWTranslateString(&vm,(intptr_t)output.data(),(intptr_t)output.data());
    assert(!std::strcmp(output.data(),"oui"));
    const auto outputSaved=output;
    failBounds=true; Reject([&]{CL_RTCWTranslateString(&vm,(intptr_t)input,(intptr_t)output.data());}); failBounds=false;
    assert(output==outputSaved);
    for (bool badMode : {false,true}) {
        multiplayer=!badMode;
        Reject([&]{CL_RTCWTranslateString(badMode?&vm:nullptr,(intptr_t)input,(intptr_t)output.data());});
        assert(output==outputSaved);
    }
    multiplayer=true; Reject([&]{CL_RTCWTranslateString(&vm,(intptr_t)input,0);});
    std::vector<char> longInput(OutputBytes,'x');
    ranges.push_back({reinterpret_cast<uintptr_t>(longInput.data()),longInput.size()});
    Reject([&]{CL_RTCWTranslateString(&vm,(intptr_t)longInput.data(),(intptr_t)output.data());});
    assert(output==outputSaved);
    longInput.back()=0; language=0;
    CL_RTCWTranslateString(&vm,(intptr_t)longInput.data(),(intptr_t)output.data());
    assert(output[OutputBytes-2]=='x' && !output[OutputBytes-1] && output[OutputBytes]=='X');
    auto longSaved=output; language=1; debug=1;
    Reject([&]{CL_RTCWTranslateString(&vm,(intptr_t)longInput.data(),(intptr_t)output.data());}); assert(output==longSaved);
    // A missing NUL may not cross the source's accessible range.
    const char shortBad[]={'x','y'}; ranges.push_back({reinterpret_cast<uintptr_t>(shortBad),sizeof(shortBad)});
    Reject([&]{CL_RTCWTranslateString(&vm,(intptr_t)shortBad,(intptr_t)output.data());}); assert(output==longSaved);
}
'''

IO_PRELUDE = r'''
#include "code/client/client_cpp.h"
#include <algorithm>
#include <map>
#include <memory>
using fnql::ScopedFileHandle;
using fnql::OpenFileRead;
using fnql::FileRead;
static std::map<std::string,std::string> files;
static const std::string *current;
static std::vector<std::string> listed, openedNames;
static std::vector<char *> listPointers;
static int closes, reads, warnings, notices, gets, lists, frees, lengthDelta;
static int countOverride=INT_MAX;
static bool shortRead, throwRead, missingHandle, nullList;
void QDECL Com_Printf(const char *, ...) { ++warnings; }
void QDECL Com_DPrintf(const char *, ...) { ++notices; }
cvar_t *Cvar_Get(const char *name,const char *value,int flags) {
    assert(!std::strcmp(value,"0"));
    assert(!std::strcmp(name,"cl_language") ? flags==CVAR_ARCHIVE : !std::strcmp(name,"cl_debugTranslation") && !flags);
    ++gets; return nullptr;
}
int FS_FOpenFileRead(const char *name,fileHandle_t *handle,qboolean unique) {
    assert(!current && !unique); openedNames.emplace_back(name); *handle=FS_INVALID_HANDLE;
    const auto found=files.find(name); if(found==files.end()) return -1;
    if(!missingHandle) { current=&found->second; *handle=1; }
    return int(found->second.size())+lengthDelta;
}
int FS_Read(void *output,int bytes,fileHandle_t file) {
    assert(current && file==1 && bytes>0); ++reads;
    if(throwRead) throw 6;
    const int count=std::max(0,std::min(bytes,int(current->size()))-(shortRead?1:0));
    std::memcpy(output,current->data(),count); return count;
}
void FS_FCloseFile(fileHandle_t file) { assert(current && file==1); ++closes; current=nullptr; }
char **FS_ListFiles(const char *path,const char *extension,int *count) {
    assert(!std::strcmp(path,"translations") && !std::strcmp(extension,".cfg")); ++lists;
    *count=countOverride==INT_MAX ? int(listed.size()) : countOverride;
    listPointers.clear(); for(auto &name:listed) listPointers.push_back(name.data()); listPointers.push_back(nullptr);
    return nullList ? nullptr : listPointers.data();
}
void FS_FreeFileList(char **list) { assert(list==listPointers.data()); ++frees; }
static void ResetIO() {
    assert(!current); closes=reads=warnings=notices=gets=lists=frees=lengthDelta=0;
    shortRead=throwRead=missingHandle=nullList=false; countOverride=INT_MAX;
    openedNames.clear(); listed.clear(); files.clear();
}
'''

IO_CHECKS = r'''
int main() {
    // Ordered filesystem overlays, including a bad file between valid ones.
    ResetIO(); files["scripts/translation.cfg"]=Record("yes","base");
    files["translations/b.cfg"]=Record("yes","b")+Record("old");
    files["translations/bad.cfg"]=Record("unpublished")+"{";
    files["translations/a.cfg"]=Record("YES","last");
    listed={"b.cfg","bad.cfg","a.cfg","missing.cfg",std::string(MAX_QPATH,'x')};
    CL_RTCWReloadTranslations();
    assert(rtcwTranslations.Size()==2 && rtcwTranslations.Translate("yes",1,0)=="last");
    assert(!rtcwTranslations.Find("unpublished"));
    assert(openedNames==std::vector<std::string>({"scripts/translation.cfg","translations/b.cfg",
        "translations/bad.cfg","translations/a.cfg","translations/missing.cfg"}));
    assert(closes==4 && reads==4 && warnings==1 && notices==3 && gets==2 && lists==1 && frees==1 && !current);
    // Other profiles do not register language settings, touch files or clear state.
    ResetIO(); multiplayer=false; CL_RTCWReloadTranslations();
    assert(!gets && !lists && openedNames.empty() && rtcwTranslations.Size()==2);
    multiplayer=true; files["scripts/translation.cfg"]=Record("new"); CL_RTCWReloadTranslations();
    assert(rtcwTranslations.Size()==1 && rtcwTranslations.Find("new") && !rtcwTranslations.Find("old"));
    assert(closes==1 && reads==1 && lists==1 && frees==1);
    // Every read rejection keeps the previously published dictionary intact.
    for(int scenario=0;scenario<8;++scenario) {
        ResetIO(); files["one.cfg"]=Record("new","changed");
        switch(scenario) {
        case 0: shortRead=true; break;
        case 1: throwRead=true; break;
        case 2: lengthDelta=1; break;
        case 3: lengthDelta=int(FileBytes); break;
        case 4: missingHandle=true; break;
        case 5: files["one.cfg"].clear(); break;
        case 6: lengthDelta=-1000; break;
        case 7: files["one.cfg"]+="{"; break;
        }
        bool caught=false; try { CL_RTCWLoadTranslationFile("one.cfg"); } catch(int value) { assert(value==6); caught=true; }
        assert(caught==throwRead && !current && closes==(missingHandle?0:1));
        assert(rtcwTranslations.Translate("new",1,0)=="oui");
        assert(reads==((scenario<=2 || scenario==7)?1:0));
    }
    // Reload clears stale data when the base file disappears; invalid file-list
    // sizes and null returns cannot be indexed, and every owned list is freed.
    for(int count : {-1,0,2049}) for(bool null : {false,true}) {
        ResetIO(); countOverride=count; nullList=null; listed={"must-not-open.cfg"};
        CL_RTCWReloadTranslations(); assert(!rtcwTranslations.Size());
        assert(openedNames.size()==1 && !closes && !reads && lists==1 && frees==!null);
    }
    ResetIO(); files["translations/throw.cfg"]=Record("unpublished"); listed={"throw.cfg"}; throwRead=true;
    bool caught=false; try { CL_RTCWReloadTranslations(); } catch(int value) { assert(value==6); caught=true; }
    assert(caught && !current && closes==1 && lists==1 && frees==1 && !rtcwTranslations.Size());
}
'''


class RTCWTranslationTests(unittest.TestCase):
    def run_fixture(self, body):
        compiler = shutil.which('clang++') or shutil.which('g++')
        if not compiler:
            self.skipTest('C++ compiler unavailable')
        with tempfile.TemporaryDirectory(prefix='rtcw-translation-', dir=ROOT/'.tmp') as temporary:
            source=Path(temporary)/'probe.cpp'; source.write_text(body)
            executable=Path(temporary)/('probe.exe' if os.name=='nt' else 'probe')
            flags=compiler_target_args(compiler)
            if os.name=='nt' and not flags:
                flags+=['-DFNTECH3_Q3_ONLY']
            build=subprocess.run([compiler,'-std=c++17',*flags,'-I',str(ROOT),str(source),'-o',str(executable)],
                cwd=ROOT,capture_output=True,text=True,timeout=60)
            self.assertEqual(build.returncode,0,build.stdout+build.stderr)
            run=subprocess.run([str(executable)],cwd=ROOT,capture_output=True,text=True,timeout=20)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr)

    def test_dictionary_and_module_output_contract(self):
        body = PRELUDE + function((ROOT/'code/qcommon/vm.c').read_text(), 'void *VM_ExplicitArgPtr(') + '\n'
        body += function((ROOT/'code/client/cl_main.cpp').read_text(), 'void CL_RTCWTranslateString(') + CHECKS
        self.run_fixture(body)

    def test_ordered_reload_and_io_failures(self):
        source = (ROOT/'code/client/cl_main.cpp').read_text()
        body = PRELUDE + IO_PRELUDE + function(source, 'static void CL_RTCWLoadTranslationFile(')
        body += function(source, 'void CL_RTCWReloadTranslations(') + IO_CHECKS
        self.run_fixture(body)


if __name__=='__main__':
    unittest.main()
