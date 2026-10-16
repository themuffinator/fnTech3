/* Independently authored Raven render fixtures. GPL-2.0-or-later.
 * No retail records or copyrighted assets are embedded in this test. */
#include "../code/qcommon/q_shared.h"
#include "../code/qcommon/qfiles.h"
#include "../code/renderercommon/tr_raven_bsp_format.h"
#include <algorithm>
#include <cassert>
#include <cmath>
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
    // A second lightmap style, an unused vertex style and a second grid style
    // are all served now (Styles() checks what they compose to).
    for(int kind=0;kind<3;++kind) {
        Bytes styled=file;
        int at=kind==2 ? Offset(file,15)+25 : Offset(file,13)+29+kind*4;
        styled[static_cast<size_t>(at)]=static_cast<byte>(kind==1?32:1);
        CHECK(FN_RavenBspRead(styled.data(),static_cast<int>(styled.size()),&view,&error));
        CHECK(FN_RavenBspCheckStyles(&view,&error));
        CHECK(FN_RavenBspProject(styled.data(),static_cast<int>(styled.size()),Allocate,&projection,&error));
        std::free(projection.data);
    }
    Bytes four=file; auto base=static_cast<size_t>(Offset(file,13));
    four[base+29]=38; four[base+30]=39; four[base+31]=40;
    CHECK(FN_RavenBspProject(four.data(),static_cast<int>(four.size()),Allocate,&projection,&error));
    std::free(projection.data);
    four[base+31]=64; CheckFailure(four,"style is out of range");
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

static void SetUV(Bytes &file,int vertex,int slot,float u,float v) {
    size_t at=static_cast<size_t>(Offset(file,10))+static_cast<size_t>(vertex)*80+20+static_cast<size_t>(slot)*8;
    Float(file,at,u/128); Float(file,at+4,v/128);
}
static const byte *Texel(const byte *p,int page,int x,int y) {
    return p+FN_RavenBspI32(p+8+14*8)+page*49152+(y*128+x)*3;
}

// Light styles composed into the one channel Fn draws: later lightmap
// channels into the first's texels, vertex channels into a vertex-lit
// surface's colour, grid channels into the grid row; each tinted by its
// style's colour, white unless given.
static void Styles() {
    fnRavenBspProjection_t projection; fnRavenBspError_t error;
    unsigned char colors[FN_RAVEN_BSP_LIGHT_STYLES][4];
    std::memset(colors,255,sizeof(colors));
    colors[1][0]=128; colors[1][1]=0; colors[1][2]=255;
    colors[5][0]=colors[5][1]=colors[5][2]=0;
    Bytes file=Fixture();
    const size_t s=static_cast<size_t>(Offset(file,13)), lightmaps=static_cast<size_t>(Offset(file,14));
    const float corners[3][2]={{0.5f,0.5f},{4.5f,0.5f},{0.5f,4.5f}};
    for(int v=0;v<3;++v) { SetUV(file,v,0,corners[v][0],corners[v][1]); SetUV(file,v,1,corners[v][0]+10,corners[v][1]); }
    for(int y=0;y<128;++y) for(int x=10;x<16;++x) file[lightmaps+49152+static_cast<size_t>(y*128+x)*3]=130;
    file[s+29]=1; // the second lightmap channel: style 1, page 1, ten texels right

    CHECK(FN_RavenBspProject(file.data(),static_cast<int>(file.size()),Allocate,&projection,&error));
    const byte *p=static_cast<byte *>(projection.data);
    for(auto xy:{std::make_pair(0,0),std::make_pair(4,0),std::make_pair(3,2),std::make_pair(0,4)})
        CHECK(Texel(p,0,xy.first,xy.second)[0]==220 && Texel(p,0,xy.first,xy.second)[1]==180);
    for(auto xy:{std::make_pair(5,0),std::make_pair(3,3),std::make_pair(0,5),std::make_pair(127,127)})
        CHECK(Texel(p,0,xy.first,xy.second)[0]==0x5a && Texel(p,0,xy.first,xy.second)[1]==0x5a);
    CHECK(Texel(p,1,10,0)[0]==130 && Texel(p,1,0,0)[0]==0x5a);
    CHECK(!std::memcmp(projection.original.bytes,file.data(),file.size()));
    std::free(projection.data);
    CHECK(FN_RavenBspProjectStyles(file.data(),static_cast<int>(file.size()),Allocate,colors,&projection,&error));
    p=static_cast<byte *>(projection.data);
    CHECK(Texel(p,0,0,0)[0]==0x5a+65 && Texel(p,0,0,0)[1]==0x5a && Texel(p,0,0,0)[2]==180);
    std::free(projection.data);

    // A second surface lighting the same texels alike adds them once.
    {
        Bytes twice=file; Bytes copy(twice.begin()+static_cast<std::ptrdiff_t>(s),twice.begin()+static_cast<std::ptrdiff_t>(s)+148);
        Bytes both(copy); both.insert(both.end(),copy.begin(),copy.end()); Lump(twice,13,both);
        Bytes model(40); Float(model,12,128); Put(model,28,2); Lump(twice,7,model);
        CHECK(FN_RavenBspProject(twice.data(),static_cast<int>(twice.size()),Allocate,&projection,&error));
        CHECK(Texel(static_cast<byte *>(projection.data),0,0,0)[0]==220); std::free(projection.data);
    }
    // A channel that is vertex light adds the style's own colour (a white image).
    Bytes white=file; Put(white,s+40,0xfffffffd);
    CHECK(FN_RavenBspProject(white.data(),static_cast<int>(white.size()),Allocate,&projection,&error));
    CHECK(Texel(static_cast<byte *>(projection.data),0,0,0)[0]==255 && Texel(static_cast<byte *>(projection.data),0,5,0)[0]==0x5a);
    std::free(projection.data);
    // Channels after an unused style are not drawn, as retail stops there.
    Bytes gap=file; gap[s+29]=255; gap[s+30]=1;
    CHECK(FN_RavenBspProject(gap.data(),static_cast<int>(gap.size()),Allocate,&projection,&error));
    CHECK(Texel(static_cast<byte *>(projection.data),0,0,0)[0]==0x5a); std::free(projection.data);
    // A channel with no lightmap stops the map, as retail's FinishShader does,
    // and one that is not the first moved whole is refused, not guessed.
    Bytes none=file; Put(none,s+40,0xffffffff); CheckFailure(none,"no lightmap or vertex light");
    Bytes moved=file; SetUV(moved,2,1,corners[2][0]+11,corners[2][1]); CheckFailure(moved,"not its first channel moved");

    // A vertex-lit surface sums its vertex channels, retail's eight-bit shift.
    Bytes lit=Fixture(); Put(lit,s+36,0xfffffffd); lit[s+33]=5;
    const size_t vertex=static_cast<size_t>(Offset(lit,10));
    const byte channels[8]={100,100,100,200,50,60,70,0};
    std::memcpy(lit.data()+vertex+64,channels,8);
    CHECK(FN_RavenBspProject(lit.data(),static_cast<int>(lit.size()),Allocate,&projection,&error));
    const byte *color=static_cast<byte *>(projection.data)+FN_RavenBspI32(static_cast<byte *>(projection.data)+8+10*8)+40;
    CHECK(color[0]==149 && color[1]==159 && color[2]==169 && color[3]==200); std::free(projection.data);
    CHECK(FN_RavenBspProjectStyles(lit.data(),static_cast<int>(lit.size()),Allocate,colors,&projection,&error));
    color=static_cast<byte *>(projection.data)+FN_RavenBspI32(static_cast<byte *>(projection.data)+8+10*8)+40;
    CHECK(color[0]==99 && color[1]==99 && color[2]==99); std::free(projection.data);
    lit[s+33]=255; // its first channel alone keeps the map's colour
    CHECK(FN_RavenBspProject(lit.data(),static_cast<int>(lit.size()),Allocate,&projection,&error));
    color=static_cast<byte *>(projection.data)+FN_RavenBspI32(static_cast<byte *>(projection.data)+8+10*8)+40;
    CHECK(color[0]==100 && color[3]==200); std::free(projection.data);
    lit[s+33]=64; CheckFailure(lit,"vertex light style is out of range");

    // A grid row sums its channels whose style is not 255.
    Bytes grid=Fixture(); grid[static_cast<size_t>(Offset(grid,15))+25]=1;
    CHECK(FN_RavenBspProject(grid.data(),static_cast<int>(grid.size()),Allocate,&projection,&error));
    const byte *cell=static_cast<byte *>(projection.data)+FN_RavenBspI32(static_cast<byte *>(projection.data)+8+15*8)+8;
    CHECK(cell[0]==103 && cell[1]==104 && cell[2]==105 && cell[3]==40 && cell[4]==50 && cell[5]==60);
    std::free(projection.data);
    CHECK(FN_RavenBspProjectStyles(grid.data(),static_cast<int>(grid.size()),Allocate,colors,&projection,&error));
    cell=static_cast<byte *>(projection.data)+FN_RavenBspI32(static_cast<byte *>(projection.data)+8+15*8)+8;
    CHECK(cell[0]==52 && cell[1]==0 && cell[2]==105 && cell[3]==40 && cell[4]==50 && cell[5]==60);
    std::free(projection.data);
    grid[static_cast<size_t>(Offset(grid,15))+25]=64; CheckFailure(grid,"light-grid style is out of range");

    // A flare in a fog the map lacks is drawn unfogged; another surface is refused.
    Bytes flare=Fixture(); Put(flare,s+8,4); Put(flare,s+4,0);
    CHECK(FN_RavenBspProject(flare.data(),static_cast<int>(flare.size()),Allocate,&projection,&error));
    CHECK(FN_RavenBspI32(static_cast<byte *>(projection.data)+FN_RavenBspI32(static_cast<byte *>(projection.data)+8+13*8)+4)==-1);
    std::free(projection.data);
    Bytes face=Fixture(); Put(face,s+4,0); CheckFailure(face,"surface/index range");
    std::cout << "Raven RBSP light styles composed PASS\n";
}

// Styles that change: the records name what each style lights, and
// recomposing a page where a changed style lights it gives what composing
// the whole map at the new colours gives.
static const byte *Page(const fnRavenBspProjection_t &projection) {
    return static_cast<const byte *>(projection.data)+FN_RavenBspI32(static_cast<const byte *>(projection.data)+8+14*8);
}
static void ChangingStyles() {
    fnRavenBspProjection_t projection, expected; fnRavenBspError_t error; fnRavenBspView_t view;
    fnRavenBspStyles_t styles;
    unsigned char colors[FN_RAVEN_BSP_LIGHT_STYLES][4];
    std::memset(colors,255,sizeof(colors));
    Bytes file=Fixture();
    const size_t s=static_cast<size_t>(Offset(file,13)), lightmaps=static_cast<size_t>(Offset(file,14));
    const float corners[3][2]={{0.5f,0.5f},{4.5f,0.5f},{0.5f,4.5f}};
    for(int v=0;v<3;++v) { SetUV(file,v,0,corners[v][0],corners[v][1]); SetUV(file,v,1,corners[v][0]+10,corners[v][1]); }
    for(int y=0;y<128;++y) for(int x=10;x<16;++x) file[lightmaps+49152+static_cast<size_t>(y*128+x)*3]=130;
    file[s+29]=1;
    // A second surface on the same page lights an overlapping block with
    // style 2 from page 1's top-left; a third lights it alike (one group).
    Bytes surfaces(file.begin()+static_cast<std::ptrdiff_t>(s),file.begin()+static_cast<std::ptrdiff_t>(s)+148);
    Bytes second=surfaces; second[29]=2; Put(second,12,3);
    Bytes third=second;
    surfaces.insert(surfaces.end(),second.begin(),second.end()); surfaces.insert(surfaces.end(),third.begin(),third.end());
    Bytes vertices(file.begin()+Offset(file,10),file.begin()+Offset(file,10)+3*80);
    vertices.insert(vertices.end(),vertices.begin(),vertices.begin()+3*80);
    Lump(file,13,surfaces); Lump(file,10,vertices);
    Bytes model(40); Float(model,12,128); Put(model,28,3); Lump(file,7,model);
    for(int v=3;v<6;++v) { SetUV(file,v,0,corners[v-3][0]+2,corners[v-3][1]); SetUV(file,v,1,corners[v-3][0]+2,corners[v-3][1]); }
    for(int y=0;y<8;++y) for(int x=0;x<8;++x) file[static_cast<size_t>(Offset(file,14))+49152+static_cast<size_t>(y*128+x)*3+1]=90;

    CHECK(FN_RavenBspRead(file.data(),static_cast<int>(file.size()),&view,&error));
    CHECK(FN_RavenBspBuildStyles(&view,Allocate,&styles,&error));
    CHECK(styles.numPages==2 && styles.numGroups==2 && styles.pageGroups[0]==0 && styles.pageGroups[1]==2 && styles.pageGroups[2]==2);
    CHECK(styles.groups[0].surface==0 && styles.groups[0].styles==(uint64_t(1)<<1));
    CHECK(styles.groups[1].surface==1 && styles.groups[1].styles==(uint64_t(1)<<2));
    CHECK(styles.styles==((uint64_t(1)<<1)|(uint64_t(1)<<2)) && !styles.numSurfaces && !styles.numCells);
    CHECK(FN_RavenBspProject(file.data(),static_cast<int>(file.size()),Allocate,&projection,&error));
    // Every texel a group lists is one its surfaces' light changes at white.
    int changedTexels=0;
    for(int texel=0;texel<128*128;++texel)
        changedTexels+=std::memcmp(Page(projection)+texel*3,file.data()+Offset(file,14)+texel*3,3)!=0;
    std::set<int> listed(styles.texels,styles.texels+styles.numTexels);
    CHECK(static_cast<int>(listed.size())==changedTexels && styles.numTexels>static_cast<int>(listed.size()));
    Bytes pages(Page(projection),Page(projection)+2*49152);
    // A style nothing on the page uses changes nothing.
    colors[7][0]=0;
    CHECK(!FN_RavenBspRecomposePage(&view,&styles,0,uint64_t(1)<<7,colors,pages.data()));
    CHECK(!FN_RavenBspRecomposePage(&view,&styles,1,~uint64_t(0),colors,pages.data()));
    CHECK(!FN_RavenBspRecomposePage(&view,&styles,2,~uint64_t(0),colors,pages.data()));
    CHECK(!std::memcmp(pages.data(),Page(projection),pages.size()));
    // One style, then the other, then both back: each matches the whole map
    // composed at those colours, overlaps included.
    const unsigned char steps[][2][3]={{{128,0,255},{255,255,255}},{{128,0,255},{10,200,30}},{{255,255,255},{0,0,0}},{{255,255,255},{255,255,255}}};
    for(const auto &step:steps) {
        const uint64_t changed=(std::memcmp(colors[1],step[0],3)?uint64_t(1)<<1:0)|(std::memcmp(colors[2],step[1],3)?uint64_t(1)<<2:0);
        std::memcpy(colors[1],step[0],3); std::memcpy(colors[2],step[1],3);
        CHECK(FN_RavenBspRecomposePage(&view,&styles,0,changed,colors,pages.data()));
        CHECK(FN_RavenBspProjectStyles(file.data(),static_cast<int>(file.size()),Allocate,colors,&expected,&error));
        CHECK(!std::memcmp(pages.data(),Page(expected),pages.size()));
        std::free(expected.data);
    }
    CHECK(!std::memcmp(pages.data(),Page(projection),pages.size()));
    std::free(projection.data);

    // Vertex light: a surface whose vertex channels name a style other than 0.
    Bytes lit=Fixture(); Put(lit,s+36,0xfffffffd); lit[s+33]=5;
    CHECK(FN_RavenBspRead(lit.data(),static_cast<int>(lit.size()),&view,&error) && FN_RavenBspBuildStyles(&view,Allocate,&styles,&error));
    CHECK(styles.numSurfaces==1 && styles.surfaces[0].surface==0 && styles.surfaces[0].styles==((uint64_t(1)<<0)|(uint64_t(1)<<5)));
    CHECK(!styles.numGroups && styles.styles==styles.surfaces[0].styles);
    lit[s+33]=255; // the first channel alone, style 0: the map's normal light, not recorded
    CHECK(FN_RavenBspRead(lit.data(),static_cast<int>(lit.size()),&view,&error) && FN_RavenBspBuildStyles(&view,Allocate,&styles,&error));
    CHECK(!styles.numSurfaces && !styles.styles);
    lit[s+32]=9; // a lone switched style: recorded, the map's colour at white, scaled at others
    CHECK(FN_RavenBspRead(lit.data(),static_cast<int>(lit.size()),&view,&error) && FN_RavenBspBuildStyles(&view,Allocate,&styles,&error));
    CHECK(styles.numSurfaces==1 && styles.styles==(uint64_t(1)<<9));
    {
        const byte *vertex=lit.data()+Offset(lit,10)+2*80, *surface=lit.data()+Offset(lit,13);
        byte out[4];
        FN_RavenBspComposeVertexColor(vertex,surface,nullptr,out);
        CHECK(!std::memcmp(out,vertex+64,4));
        colors[9][0]=0; colors[9][1]=128; colors[9][2]=255;
        FN_RavenBspComposeVertexColor(vertex,surface,colors,out);
        CHECK(out[0]==0 && out[1]==(vertex[65]*128+127)/255 && out[2]==vertex[66] && out[3]==vertex[67]);
    }
    // The grid: cells whose row has a style other than 0 (cells 0 and 2 use row 1).
    Bytes grid=Fixture(); grid[static_cast<size_t>(Offset(grid,15))+30+25]=3;
    CHECK(FN_RavenBspRead(grid.data(),static_cast<int>(grid.size()),&view,&error) && FN_RavenBspBuildStyles(&view,Allocate,&styles,&error));
    CHECK(styles.numCells==2 && styles.cells[0]==0 && styles.cells[1]==2 && styles.styles==((uint64_t(1)<<0)|(uint64_t(1)<<3)));
    grid[static_cast<size_t>(Offset(grid,15))+30+24]=255; // a hole is never recorded
    CHECK(FN_RavenBspRead(grid.data(),static_cast<int>(grid.size()),&view,&error) && FN_RavenBspBuildStyles(&view,Allocate,&styles,&error));
    CHECK(!styles.numCells);
    allocationFail=true; CHECK(!FN_RavenBspBuildStyles(&view,Allocate,&styles,&error) && std::strstr(error.reason,"allocation"));
    allocationFail=false;
    CHECK(!FN_RavenBspBuildStyles(nullptr,Allocate,&styles,&error) && !FN_RavenBspBuildStyles(&view,nullptr,&styles,&error));

    // A vertex-lit 3x3 patch with style 5: its control points carry their
    // patch coordinates in the projection, and it is lit where they lie.
    Bytes patch=Fixture();
    Bytes nine(9*80);
    for(int v=0;v<9;++v) {
        size_t base=static_cast<size_t>(v)*80;
        Float(nine,base,static_cast<float>(v%3*64)); Float(nine,base+4,static_cast<float>(v/3*64));
        Float(nine,base+20,static_cast<float>(v%3)*0.25f); Float(nine,base+24,static_cast<float>(v/3)*0.5f);
        for(int c=0;c<4;++c) { nine[base+64+static_cast<size_t>(c)]=static_cast<byte>(20*v+c); nine[base+68+static_cast<size_t>(c)]=static_cast<byte>(200-10*v); }
    }
    Lump(patch,10,nine);
    Put(patch,s+8,2); Put(patch,s+16,9); Put(patch,s+24,0); Put(patch,s+36,0xfffffffd);
    patch[s+33]=5; Put(patch,s+140,3); Put(patch,s+144,3);
    CHECK(FN_RavenBspRead(patch.data(),static_cast<int>(patch.size()),&view,&error));
    const byte *record=view.bytes+view.offsets[13];
    CHECK(FN_RavenBspStyledPatch(record) && FN_RavenBspStyledPatchAt(&view,0));
    CHECK(FN_RavenBspProject(patch.data(),static_cast<int>(patch.size()),Allocate,&projection,&error));
    {
        const byte *projected=static_cast<byte *>(projection.data)+FN_RavenBspI32(static_cast<byte *>(projection.data)+8+10*8);
        for(int v=0;v<9;++v) {
            float at[2]; std::memcpy(at,projected+v*44+20,8);
            CHECK(at[0]==static_cast<float>(v%3)*0.5f && at[1]==static_cast<float>(v/3)*0.5f);
            // At a control point the patch's colour is that point's own.
            byte fromPatch[4], fromVertex[4];
            FN_RavenBspPatchColor(&view,record,at[0],at[1],colors,fromPatch);
            FN_RavenBspComposeVertexColor(view.bytes+view.offsets[10]+v*80,record,colors,fromVertex);
            CHECK(!std::memcmp(fromPatch,fromVertex,4));
            float coords[2]; FN_RavenBspPatchLightmapCoords(&view,record,at[0],at[1],coords);
            CHECK(std::fabs(coords[0]-static_cast<float>(v%3)*0.25f)<1e-6f && std::fabs(coords[1]-static_cast<float>(v/3)*0.5f)<1e-6f);
        }
        // Midway between the first two corners: the curve's own interpolation.
        byte middle[4]; float coords[2];
        FN_RavenBspPatchColor(&view,record,0.25f,0.0f,nullptr,middle);
        FN_RavenBspPatchLightmapCoords(&view,record,0.25f,0.0f,coords);
        CHECK(std::fabs(coords[0]-0.125f)<1e-6f && coords[1]==0.0f);
        const int expectRed=((20*0+20*1)/2 * 255 + (200+190)/2 * 255) >> 8;
        CHECK(std::abs(middle[0]-expectRed)<=2);
    }
    CHECK(!std::memcmp(projection.original.bytes,patch.data(),patch.size())); std::free(projection.data);
    // A patch sharing its control points with another surface keeps them.
    {
        Bytes shared=patch; Bytes two(shared.begin()+Offset(shared,13),shared.begin()+Offset(shared,13)+148);
        Bytes other=two; Put(other,8,1); Put(other,16,3); Put(other,24,3); Put(other,36,0);
        two.insert(two.end(),other.begin(),other.end()); Lump(shared,13,two);
        Bytes model2(40); Float(model2,12,128); Put(model2,28,2); Lump(shared,7,model2);
        CHECK(FN_RavenBspRead(shared.data(),static_cast<int>(shared.size()),&view,&error));
        CHECK(FN_RavenBspStyledPatch(view.bytes+view.offsets[13]) && !FN_RavenBspStyledPatchAt(&view,0));
        CHECK(FN_RavenBspProject(shared.data(),static_cast<int>(shared.size()),Allocate,&projection,&error));
        const byte *projected=static_cast<byte *>(projection.data)+FN_RavenBspI32(static_cast<byte *>(projection.data)+8+10*8);
        float at[2]; std::memcpy(at,projected+44+20,8); CHECK(at[0]==0.25f && at[1]==0.0f);
        std::free(projection.data);
    }
    // Even or too-small patches, and a lightmapped one, are not styled patches.
    Bytes narrow=patch; Put(narrow,s+140,1); Put(narrow,s+144,9);
    CHECK(!FN_RavenBspStyledPatch(narrow.data()+Offset(narrow,13)));
    std::cout << "Raven RBSP changing light styles recomposed PASS\n";
}

// Shader records reach the Fn view in Quake III's bits (surfaceflags.h); the
// Raven side is every retail Jedi engine's infoparm table.
static void ShaderFlags() {
    const struct { uint32_t raven, fn; } surface[] = {
        {0x00002000u,SURF_SKY},{0x00004000u,SURF_SLICK},{0x00008000u,SURF_METALSTEPS},
        {0x00040000u,SURF_NODAMAGE},{0x00080000u,SURF_NOIMPACT},{0x00100000u,SURF_NOMARKS},
        {0x00200000u,SURF_NODRAW},{0x00400000u,SURF_NOSTEPS},{0x00800000u,SURF_NODLIGHT},
    }, content[] = {
        {0x00000001u,CONTENTS_SOLID},{0x00000002u,CONTENTS_LAVA},{0x00000004u,CONTENTS_WATER},
        {0x00000008u,CONTENTS_FOG},{0x00000010u,CONTENTS_PLAYERCLIP},{0x00000020u,CONTENTS_MONSTERCLIP},
        {0x00000040u,CONTENTS_BOTCLIP},{0x00000400u,CONTENTS_TRIGGER},{0x00000800u,static_cast<uint32_t>(CONTENTS_NODROP)},
        {0x00020000u,CONTENTS_SLIME},{0x08000000u,CONTENTS_DETAIL},{0x80000000u,CONTENTS_TRANSLUCENT},
    };
    uint32_t mapped=0;
    for(const auto &bit:surface) { CHECK(FN_RavenBspFlagsToFn(bit.raven,0)==bit.fn && !FN_RavenBspFlagsToFn(bit.raven,1)); mapped|=bit.raven; }
    for(uint32_t bit=1;bit;bit<<=1) if(!(bit&mapped)) CHECK(!FN_RavenBspFlagsToFn(bit,0));
    mapped=0;
    for(const auto &bit:content) { CHECK(FN_RavenBspFlagsToFn(bit.raven,1)==bit.fn && !FN_RavenBspFlagsToFn(bit.raven,0)); mapped|=bit.raven; }
    for(uint32_t bit=1;bit;bit<<=1) if(!(bit&mapped)) CHECK(!FN_RavenBspFlagsToFn(bit,1));
    // Material numbers are not flags: mp/duel9's snow (14) is not sky, nor
    // is any other material that has Quake III's sky, nodraw or slick bit.
    for(uint32_t material=0;material<32;++material) CHECK(!FN_RavenBspFlagsToFn(material,0));
    // Raven's default opaque content (0x8000) is not Quake III's areaportal.
    CHECK(!FN_RavenBspFlagsToFn(0x00008000u,1));

    // Flag words of retail mp/duel9's shader records: rock (material 23,
    // 0x17, which has Quake III's sky, slick, nodamage and noimpact bits),
    // snow (14), its sky, caulk, its pit fog and its clip.
    const struct { const char *name; uint32_t surface, content, fnSurface, fnContent; } records[] = {
        {"textures/hoth/h_outground_3_clip",0x00000017u,0x00008001u,0,CONTENTS_SOLID},
        {"textures/hoth/snowbiga",0x0000000eu,0x00008001u,0,CONTENTS_SOLID},
        {"textures/mp/s_hothsky",0x00182000u,0x00008001u,SURF_SKY|SURF_NOIMPACT|SURF_NOMARKS,CONTENTS_SOLID},
        {"textures/system/caulk",0x00300000u,0x00008001u,SURF_NODRAW|SURF_NOMARKS,CONTENTS_SOLID},
        {"textures/mp/s_hothpit_fog",0x00000000u,0x80000008u,0,CONTENTS_FOG|CONTENTS_TRANSLUCENT},
        {"textures/system/clip",0x00380000u,0x80000030u,SURF_NODRAW|SURF_NOMARKS|SURF_NOIMPACT,
            CONTENTS_TRANSLUCENT|CONTENTS_PLAYERCLIP|CONTENTS_MONSTERCLIP},
    };
    Bytes file=Fixture(), lump;
    for(const auto &record:records) {
        Bytes shader(72); std::memcpy(shader.data(),record.name,std::strlen(record.name));
        Put(shader,64,record.surface); Put(shader,68,record.content); lump.insert(lump.end(),shader.begin(),shader.end());
    }
    Lump(file,1,lump);
    fnRavenBspProjection_t projection; fnRavenBspError_t error;
    CHECK(FN_RavenBspProject(file.data(),static_cast<int>(file.size()),Allocate,&projection,&error));
    const auto *p=static_cast<byte *>(projection.data);
    CHECK(FN_RavenBspI32(p+12+1*8)==static_cast<int>(lump.size()));
    const auto *shaders=reinterpret_cast<const dshader_t *>(p+FN_RavenBspI32(p+8+1*8));
    for(size_t i=0;i<sizeof(records)/sizeof(records[0]);++i) {
        CHECK(!std::strcmp(shaders[i].shader,records[i].name));
        CHECK(static_cast<uint32_t>(shaders[i].surfaceFlags)==records[i].fnSurface);
        CHECK(static_cast<uint32_t>(shaders[i].contentFlags)==records[i].fnContent);
    }
    // The world-owned original keeps Raven's bits.
    CHECK(!std::memcmp(projection.original.bytes,file.data(),file.size()));
    CHECK(FN_RavenBspU32(projection.original.bytes+projection.original.offsets[1]+2*72+64)==0x00182000u);
    std::free(projection.data);
    std::cout << "Raven RBSP shader flags in the Fn view PASS\n";
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
    Unit(); Styles(); ChangingStyles(); ShaderFlags(); return 0;
}
