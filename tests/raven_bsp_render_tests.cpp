/* Independently authored Raven render fixtures. GPL-2.0-or-later.
 * No retail records or copyrighted assets are embedded in this test. */
#include "../code/qcommon/q_shared.h"
#include "../code/qcommon/qfiles.h"
#include "../code/renderercommon/tr_raven_bsp_format.h"
#include <algorithm>
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::exit(1); } } while (0)
using Bytes = std::vector<unsigned char>;
static int allocations;
static bool allocationFail;
static void *Allocate(int size) { ++allocations; return allocationFail ? nullptr : std::malloc(static_cast<size_t>(size)); }
static void Put(Bytes &file, size_t offset, uint32_t value) { CHECK(offset + 4 <= file.size()); FN_RavenBspPut32(file.data()+offset,value); }
static void Float(Bytes &file, size_t offset, float value) { uint32_t bits; std::memcpy(&bits,&value,4); Put(file,offset,bits); }
static int Offset(const Bytes &file,int lump) { return FN_RavenBspI32(file.data()+8+lump*8); }
static void Lump(Bytes &file,int lump,const Bytes &body) {
    file.push_back(0xaa); // Disk offsets deliberately need not be aligned.
    Put(file,8+static_cast<size_t>(lump)*8,static_cast<uint32_t>(file.size()));
    Put(file,12+static_cast<size_t>(lump)*8,static_cast<uint32_t>(body.size()));
    file.insert(file.end(),body.begin(),body.end());
}
static Bytes Fixture() {
    Bytes file(152); Put(file,0,0x50534252); Put(file,4,1);
    Lump(file,0,Bytes{'{','}',0});
    Bytes shader(72); std::memcpy(shader.data(),"textures/generated",18); Lump(file,1,shader);
    Bytes plane(16); Float(plane,0,1); Float(plane,12,64); Lump(file,2,plane);
    Bytes model(40); Float(model,12,128); Put(model,28,1); Lump(file,7,model);
    Bytes sides(12); Put(sides,8,0x1234); Lump(file,9,sides);
    Bytes vertices(3*80);
    for(int v=0;v<3;++v) {
        size_t base=static_cast<size_t>(v)*80;
        for(int f=0;f<16;++f) Float(vertices,base+static_cast<size_t>(f)*4,static_cast<float>(v*16+f));
        for(int c=0;c<16;++c) vertices[base+64+static_cast<size_t>(c)]=static_cast<unsigned char>(v*16+c);
    }
    Lump(file,10,vertices);
    Bytes indexes(12); Put(indexes,0,2); Put(indexes,4,0); Put(indexes,8,1); Lump(file,11,indexes);
    Bytes surface(148); Put(surface,4,0xffffffff); Put(surface,8,1); Put(surface,16,3); Put(surface,24,3);
    std::memset(surface.data()+28,255,8); surface[28]=surface[32]=0;
    for(int channel=0;channel<4;++channel) {
        Put(surface,36+static_cast<size_t>(channel)*4,static_cast<uint32_t>(channel%2));
        Put(surface,52+static_cast<size_t>(channel)*4,static_cast<uint32_t>(channel+10));
        Put(surface,68+static_cast<size_t>(channel)*4,static_cast<uint32_t>(channel+20));
    }
    Put(surface,84,128); Put(surface,88,128);
    for(int f=0;f<12;++f) Float(surface,92+static_cast<size_t>(f)*4,static_cast<float>(f));
    Lump(file,13,surface); Lump(file,14,Bytes(49152*2,0x5a));
    Bytes pool(60);
    for(int row=0;row<2;++row) {
        size_t base=static_cast<size_t>(row)*30;
        pool[base]=static_cast<unsigned char>(row*10);
        pool[base+1]=static_cast<unsigned char>(row*20); pool[base+2]=static_cast<unsigned char>(row*30);
        pool[base+12]=40; pool[base+13]=50; pool[base+14]=60;
        pool[base+24]=0; pool[base+25]=pool[base+26]=pool[base+27]=255;
        pool[base+28]=static_cast<unsigned char>(8+row); pool[base+29]=static_cast<unsigned char>(9+row);
        for(int c=3;c<12;++c) pool[base+static_cast<size_t>(c)]=static_cast<unsigned char>(100+c);
    }
    Lump(file,15,pool); Lump(file,17,Bytes{1,0,0,0,1,0});
    return file;
}

struct bmodelFixture { float bounds[2][3]; };
struct world_t {
    char name[64]; fnRavenBspView_t ravenBsp; int numBmodels; bmodelFixture *bmodels;
    float lightGridSize[3],lightGridOrigin[3],lightGridInverseSize[3];
    int lightGridBounds[3]; byte *lightGridData;
};
static world_t s_worldData;
static byte *fileBase;
static int shifts, fileReleases;
static std::set<void *> hunk;
static byte *Hunk(int size,int) { auto *p=static_cast<byte *>(std::malloc(static_cast<size_t>(size))); CHECK(p); hunk.insert(p); return p; }
static void ReleaseFile(void *p) { ++fileReleases; std::free(p); }
static void RaiseError(const char *message) { throw std::runtime_error(message); }
static void EngineError(errorParm_t,const char *format,...) {
    char message[512]; va_list args; va_start(args,format); std::vsnprintf(message,sizeof(message),format,args); va_end(args);
    void (*volatile raise)(const char *)=RaiseError; raise(message);
}
static void R_ColorShiftLightingBytes(byte *in,byte *out,qboolean flag) {
    CHECK(flag==qfalse); ++shifts;
    for(int c=0;c<3;++c) out[c]=static_cast<byte>(in[c]+1);
}
static struct { decltype(&Allocate) Malloc; decltype(&ReleaseFile) FS_FreeFile;
    decltype(&Hunk) Hunk_Alloc; decltype(&EngineError) Error; } ri{Allocate,ReleaseFile,Hunk,EngineError};
#include "../code/renderercommon/tr_raven_bsp_load.h"

static void CheckFailure(const Bytes &file,const char *reason=nullptr) {
    fnRavenBspProjection_t projection; fnRavenBspError_t error;
    allocations=0;
    CHECK(!FN_RavenBspProject(file.data(),static_cast<int>(file.size()),Allocate,&projection,&error));
    CHECK(!projection.data && !projection.length && !allocations);
    CHECK(error.reason && (!reason || std::strstr(error.reason,reason)));
}
static void Unit() {
    Bytes file=Fixture(); fnRavenBspView_t view; fnRavenBspError_t error; fnRavenBspProjection_t projection;
    CHECK(sizeof(drawVert_t)==44 && sizeof(dsurface_t)==104 && sizeof(dheader_t)==144);
    CHECK(FN_RavenBspRead(file.data(),static_cast<int>(file.size()),&view,&error));
    CHECK(view.vertices==3 && view.surfaces==1 && view.gridRows==2 && view.gridCells==3);
    CHECK(FN_RavenBspProject(file.data(),static_cast<int>(file.size()),Allocate,&projection,&error));
    auto *p=static_cast<byte *>(projection.data);
    CHECK(projection.original.length==static_cast<int>(file.size()));
    CHECK(!std::memcmp(projection.original.bytes,file.data(),file.size()));
    CHECK(FN_RavenBspU32(p)==0x50534249 && FN_RavenBspU32(p+4)==46);
    CHECK(FN_RavenBspI32(p+12+10*8)==3*44 && FN_RavenBspI32(p+12+13*8)==104);
    CHECK(FN_RavenBspI32(p+12+15*8)==24 && FN_RavenBspI32(p+12+14*8)==2*49152);
    const byte *grid=p+FN_RavenBspI32(p+8+15*8);
    CHECK(grid[0]==10 && grid[8]==0 && grid[16]==10 && grid[3]==40 && grid[14]==8);
    CHECK(FN_RavenBspGridCellValid(&projection.original,1)); // Black ambient, real directed light.
    CHECK(!FN_RavenBspGridCellValid(&projection.original,3));
    const byte *vertex=p+FN_RavenBspI32(p+8+10*8);
    CHECK(!std::memcmp(vertex,file.data()+Offset(file,10),28));
    CHECK(!std::memcmp(vertex+28,file.data()+Offset(file,10)+52,12));
    CHECK(!std::memcmp(vertex+40,file.data()+Offset(file,10)+64,4));
    const byte *surface=p+FN_RavenBspI32(p+8+13*8);
    CHECK(!std::memcmp(surface+48,file.data()+Offset(file,13)+92,48));
    CHECK(FN_RavenBspI32(surface+32)==10 && FN_RavenBspI32(surface+36)==20);
    CHECK(!std::memcmp(p+FN_RavenBspI32(p+8+11*8),file.data()+Offset(file,11),12));
    std::free(p);
    for(int kind=0;kind<3;++kind) {
        Bytes bad=file;
        int at=kind==2 ? Offset(file,15)+25 : Offset(file,13)+29+kind*4;
        bad[static_cast<size_t>(at)]=static_cast<byte>(kind==1?32:1);
        CHECK(FN_RavenBspRead(bad.data(),static_cast<int>(bad.size()),&view,&error));
        CHECK(!FN_RavenBspNormalStyles(&view,&error) && error.slot==1);
        CheckFailure(bad,"styles are not served yet");
    }
    Bytes four=file; auto base=static_cast<size_t>(Offset(file,13));
    four[base+29]=38; four[base+30]=39; four[base+31]=40; CheckFailure(four,"styles");
    for(int slot=0;slot<16;++slot) for(uint32_t encoding:{0x7fc00001u,0x7f800000u,0xff800000u}) {
        Bytes bad=file; Put(bad,static_cast<size_t>(Offset(file,10))+static_cast<size_t>(slot)*4,encoding);
        if(slot<7 || slot>12) CheckFailure(bad,"non-finite");
        else {
            CHECK(FN_RavenBspProject(bad.data(),static_cast<int>(bad.size()),Allocate,&projection,&error));
            CHECK(!std::memcmp(projection.original.bytes,bad.data(),bad.size()));
            std::free(projection.data);
        }
    }
    // A vertex-lit surface does not consume channel-zero lightmap UVs. Its
    // original bytes remain intact while the projected coordinates are safe.
    Bytes unused=file; Put(unused,static_cast<size_t>(Offset(file,13))+36,0xfffffffd);
    Put(unused,static_cast<size_t>(Offset(file,10))+20,0x7fc00001);
    Put(unused,static_cast<size_t>(Offset(file,10))+24,0xff800000);
    CHECK(FN_RavenBspProject(unused.data(),static_cast<int>(unused.size()),Allocate,&projection,&error));
    const auto *unusedVertex=static_cast<byte *>(projection.data)+FN_RavenBspI32(static_cast<byte *>(projection.data)+8+10*8);
    CHECK(FN_RavenBspU32(unusedVertex+20)==0 && FN_RavenBspU32(unusedVertex+24)==0);
    CHECK(!std::memcmp(projection.original.bytes,unused.data(),unused.size())); std::free(projection.data);
    // Additional active channels are validated before their material contract
    // is refused; their scalar data is never trusted merely because Fn does
    // not serve that style yet.
    Bytes active=file; active[static_cast<size_t>(Offset(file,13))+29]=38;
    Put(active,static_cast<size_t>(Offset(file,10))+28,0x7fc00001); CheckFailure(active,"non-finite");
    for(int type=1;type<=4;++type) for(int scalar=0;scalar<12;++scalar) {
        Bytes fields=file; Put(fields,static_cast<size_t>(Offset(file,13))+8,static_cast<uint32_t>(type));
        if(type==2) { Put(fields,static_cast<size_t>(Offset(file,13))+140,3); Put(fields,static_cast<size_t>(Offset(file,13))+144,1); }
        Put(fields,static_cast<size_t>(Offset(file,13))+92+static_cast<size_t>(scalar)*4,0x7fc00001);
        bool consumed=(type==1 && scalar>=9) || (type==2 && scalar>=3 && scalar<9) || (type==4 && (scalar<6 || scalar>=9));
        if(consumed) CheckFailure(fields,"non-finite");
        else {
            CHECK(FN_RavenBspProject(fields.data(),static_cast<int>(fields.size()),Allocate,&projection,&error));
            CHECK(!std::memcmp(projection.original.bytes,fields.data(),fields.size()));
            const auto *projectedSurface=static_cast<byte *>(projection.data)+FN_RavenBspI32(static_cast<byte *>(projection.data)+8+13*8);
            CHECK(FN_RavenBspU32(projectedSurface+48+scalar*4)==0); std::free(projection.data);
        }
    }
    for(size_t size:{0u,8u,151u}) { Bytes bad(file.begin(),file.begin()+size); CheckFailure(bad,"header"); }
    for(int lump=0;lump<18;++lump) {
        Bytes bad=file; Put(bad,8+static_cast<size_t>(lump)*8,0xffffffff); CheckFailure(bad,"bounds");
    }
    Bytes bad=file; Put(bad,12+10*8,239); CheckFailure(bad,"record size");
    bad=file; Put(bad,8+17*8,static_cast<uint32_t>(Offset(file,15))); CheckFailure(bad,"overlap");
    bad=file; bad[static_cast<size_t>(Offset(file,17))]=255; bad[static_cast<size_t>(Offset(file,17))+1]=255; CheckFailure(bad,"light-array index");
    bad=file; Put(bad,static_cast<size_t>(Offset(file,13))+16,0xffffffff); CheckFailure(bad,"range");
    bad=file; Put(bad,static_cast<size_t>(Offset(file,13))+24,2); CheckFailure(bad,"triangle count");
    bad=file; Put(bad,static_cast<size_t>(Offset(file,13))+8,2); CheckFailure(bad,"patch dimensions");
    bad=file; Put(bad,static_cast<size_t>(Offset(file,2)),0x7f800000); CheckFailure(bad,"plane contains");
    bad=file; Put(bad,static_cast<size_t>(Offset(file,11)),3); CheckFailure(bad,"vertex index");
    bad=file; Put(bad,static_cast<size_t>(Offset(file,13))+36,2); CheckFailure(bad,"lightmap index");
    bad=file; bad[static_cast<size_t>(Offset(file,0))+2]='x'; CheckFailure(bad,"terminated");
    allocationFail=true; CHECK(!FN_RavenBspProject(file.data(),static_cast<int>(file.size()),Allocate,&projection,&error));
    CHECK(!projection.data && std::strstr(error.reason,"allocation failed")); allocationFail=false;
    CHECK(!FN_RavenBspProject(file.data(),static_cast<int>(file.size()),nullptr,&projection,&error));

    float mins[3]={0,0,0}, maxs[3]={128,0,0}, spacing[3]={64,64,128}, origin[3]; int bounds[3];
    CHECK(FN_RavenBspGridGeometry(mins,maxs,spacing,origin,bounds,3) && bounds[0]==3 && bounds[1]==1);
    CHECK(!FN_RavenBspGridGeometry(mins,maxs,spacing,origin,bounds,2));
    for(uint32_t encoding:{0x7fc00001u,0x7f800000u,0xff800000u}) {
        std::memcpy(&spacing[0],&encoding,4); CHECK(!FN_RavenBspGridGeometry(mins,maxs,spacing,origin,bounds,3));
    }
    spacing[0]=0; CHECK(!FN_RavenBspGridGeometry(mins,maxs,spacing,origin,bounds,3));
    spacing[0]=-1; CHECK(!FN_RavenBspGridGeometry(mins,maxs,spacing,origin,bounds,3));
    spacing[0]=64; maxs[0]=1.0e30f; CHECK(!FN_RavenBspGridGeometry(mins,maxs,spacing,origin,bounds,3));

    // Execute the actual common loader: independent ownership, remapping,
    // normal Fn overbright conversion and strict geometry validation.
    void *source=std::malloc(file.size()); CHECK(source); std::memcpy(source,file.data(),file.size());
    int length=static_cast<int>(file.size()); fnRavenBspView_t original;
    R_RavenPrepareWorldBuffer(&source,&length,&original,"generated"); CHECK(fileReleases==1);
    R_RavenKeepWorldRecords(&original); CHECK(s_worldData.ravenBsp.bytes!=original.bytes);
    fileBase=static_cast<byte *>(source);
    bmodelFixture model{{{0,0,0},{128,0,0}}}; s_worldData.numBmodels=1; s_worldData.bmodels=&model;
    s_worldData.lightGridSize[0]=s_worldData.lightGridSize[1]=64; s_worldData.lightGridSize[2]=128;
    lump_t gridLump{FN_RavenBspI32(fileBase+8+15*8),FN_RavenBspI32(fileBase+12+15*8)};
    R_LoadRavenLightGrid(&gridLump); CHECK(shifts==6 && s_worldData.lightGridBounds[0]==3);
    CHECK(s_worldData.lightGridData[0]==11 && s_worldData.lightGridData[8]==1 && s_worldData.lightGridData[11]==41);
    CHECK(FN_RavenBspGridCellValid(&s_worldData.ravenBsp,1));
    ++gridLump.filelen;
    try { R_LoadRavenLightGrid(&gridLump); CHECK(false); } catch(const std::runtime_error &e) { CHECK(std::strstr(e.what(),"light-array length")); }
    std::free(source); CHECK(!std::memcmp(s_worldData.ravenBsp.bytes,file.data(),file.size()));
    for(void *allocation:hunk) std::free(allocation); hunk.clear(); s_worldData={};

    bad=file; bad[static_cast<size_t>(Offset(file,15))+24]=255;
    CHECK(FN_RavenBspProject(bad.data(),static_cast<int>(bad.size()),Allocate,&projection,&error));
    CHECK(!FN_RavenBspGridCellValid(&projection.original,1)); std::free(projection.data);
    std::cout << "Raven RBSP retained rendering records/grid/style bounds PASS\n";
}

int main(int argc,char **argv) {
    if(argc==2 && !std::strcmp(argv[1],"--stdin")) {
#ifdef _WIN32
        _setmode(_fileno(stdin),_O_BINARY);
#endif
        Bytes input;
        char block[16384];
        while(std::cin) {
            std::cin.read(block,sizeof(block)); auto count=std::cin.gcount();
            if(input.size()+static_cast<size_t>(count)>FN_RAVEN_BSP_MAX_BYTES) return 2;
            input.insert(input.end(),block,block+count);
        }
        fnRavenBspProjection_t out; fnRavenBspError_t error;
        if(!FN_RavenBspProject(input.data(),static_cast<int>(input.size()),Allocate,&out,&error)) {
            std::cout << "REFUSED " << error.reason << " lump=" << error.lump << " record=" << error.record << " slot=" << error.slot << " style=" << error.style << "\n";
            return 3;
        }
        CHECK(!std::memcmp(out.original.bytes,input.data(),input.size()));
        std::cout << "ACCEPTED " << out.original.vertices << " " << out.original.surfaces << " " << out.original.gridRows << " " << out.original.gridCells << "\n";
        std::free(out.data); return 0;
    }
    Unit(); return 0;
}
