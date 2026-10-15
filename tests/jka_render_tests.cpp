/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "code/compat/render_record_adapter.hpp"
#include <array>
#include <cassert>
#include <climits>
#include <cstdio>
#include <cstring>
#include <utility>

namespace raven = fntech3::jka::render;
template<class T, std::size_t N> void write(std::array<byte,N> &bytes, std::size_t offset, T value) {
    assert(offset + sizeof(value) <= N - 3);
    std::memcpy(bytes.data()+3+offset, &value, sizeof(value));
}
template<class T> bool same(const T &a, const T &b) { return !std::memcmp(&a,&b,sizeof(a)); }
int main() {
    std::array<byte,24+7> poly;poly.fill(0xa5);
    for(int field=0;field<5;++field)write(poly,field*4,float(field+0.25f));
    for(int color=0;color<4;++color)poly[3+20+color]=byte(10+color);
    const auto polyOriginal=poly;polyVert_t vertex{};
    assert(raven::ReadPolyVertex(poly.data()+3,vertex)&&poly==polyOriginal);
    assert(vertex.xyz[0]==0.25f&&vertex.xyz[2]==2.25f&&vertex.st[0]==3.25f&&vertex.st[1]==4.25f);
    assert(vertex.modulate.rgba[0]==10&&vertex.modulate.rgba[3]==13);
    for(int field=0;field<5;++field)for(uint32_t encoding:{0x7f800000u,0xff800000u,0x7fc00001u}){
        poly=polyOriginal;write(poly,field*4,encoding);const auto previous=vertex;
        assert(!raven::ReadPolyVertex(poly.data()+3,vertex)&&same(vertex,previous));
    }
    auto polySaved=vertex;assert(!raven::ReadPolyVertex(nullptr,vertex)&&same(vertex,polySaved));
    assert(fntech3::render::EntityBytes(COM_GAME_JKA_MP)==212);
    assert(fntech3::render::RefdefBytes(COM_GAME_JKA_MP)==384);
    for(auto profile : {COM_GAME_JKA_SP, COM_GAME_JK2_MP, COM_GAME_JK2_SP}) {
        assert(!fntech3::render::EntityBytes(profile)); assert(!fntech3::render::RefdefBytes(profile));
    }
    // Populate physical offsets independently of the frozen declaration. The
    // module record is deliberately unaligned and surrounded by canaries.
    std::array<byte,212+7> entity{}; entity[0]=entity[1]=entity[2]=entity.back()=0xa5;
    write(entity,0,int32_t(0)); write(entity,4,int32_t(0x3cf)); write(entity,8,int32_t(17));
    for(int n=0;n<9;++n) write(entity,12+n*4,float(n+1));
    write(entity,48,int32_t(1));
    for(int n=0;n<3;++n) {write(entity,52+n*4,float(21+n));write(entity,64+n*4,float(31+n));write(entity,108+n*4,float(41+n));}
    write(entity,76,int32_t(22)); write(entity,80,uint32_t(0x44332211));
    write(entity,84,0.125f);write(entity,88,0.875f);write(entity,92,8.0f);write(entity,96,45.0f);
    write(entity,100,1.5f);write(entity,104,int32_t(4));write(entity,120,-17.0f);
    write(entity,124,int32_t(3));write(entity,128,0.25f);write(entity,132,int32_t(2));write(entity,136,int32_t(19));
    // Unused procedural union bits need not encode floats for a static model.
    write(entity,148,uint32_t(0x7fc01234));write(entity,184,uint32_t(0x7fc01234));
    auto original=entity; refEntity_t out{};
    assert(fntech3::render::ReadEntity(COM_GAME_JKA_MP,entity.data()+3,out));assert(entity==original);
    assert(out.reType==RT_MODEL&&out.hModel==17&&out.renderfx==0x3cf&&out.nonNormalizedAxes==qtrue);
    assert(out.axis[2][2]==9&&out.origin[2]==23&&out.oldorigin[2]==33&&out.lightingOrigin[2]==43);
    assert(out.customShader==22&&out.customSkin==19&&out.skinNum==2&&out.frame==4&&out.oldframe==3);
    assert(out.shader.rgba[0]==0x11&&out.shader.rgba[3]==0x44&&out.shaderTime.f==1.5f);
    assert(out.shadowPlane==-17&&out.backlerp==0.25f&&out.radius==8&&out.rotation==45);
    refEntity_t empty{}; assert(same(out.rtcw,empty.rtcw)&&same(out.wolfet,empty.wolfet)&&same(out.alice,empty.alice));
    for(auto pair : {std::pair<int,int>{2,RT_SPRITE},{4,RT_BEAM},{7,RT_PORTALSURFACE}}) {
        write(entity,0,int32_t(pair.first)); assert(raven::ReadEntity(entity.data()+3,out)==raven::Status::Ok);
        assert(int(out.reType)==pair.second);
    }
    auto rejectedEntity=[&](std::size_t offset,uint32_t bad,raven::Status status) {
        entity=original;write(entity,offset,bad);auto before=out;
        assert(raven::ReadEntity(entity.data()+3,out)==status&&same(before,out));
    };
    rejectedEntity(208,1,raven::Status::Ghoul2);rejectedEntity(208,0xffffffff,raven::Status::Ghoul2);
    // The three Raven primitives the client game submits (retail RB_SurfaceEntity
    // 0x4bdcd0): the saber glow from saberLength at 180, the line from its two
    // origins and radius, the oriented line from the union's width at 148 and
    // texture scale at 156. Each carries only its own shape.
    {
        const float glowLength=40.5f,lineWidth=6.25f,lineScale=0.75f;
        entity=original;write(entity,0,int32_t(5));write(entity,180,glowLength);
        assert(raven::ReadEntity(entity.data()+3,out)==raven::Status::Ok);
        assert(out.reType==RT_JKA_SABER_GLOW&&out.jka.saberLength==glowLength&&out.radius==8&&out.origin[2]==23);
        assert(out.jka.lineWidth==0&&out.jka.lineSTScale==0&&out.axis[0][0]==1);
        entity=original;write(entity,0,int32_t(8));write(entity,180,glowLength);
        assert(raven::ReadEntity(entity.data()+3,out)==raven::Status::Ok);
        assert(out.reType==RT_JKA_LINE&&out.jka.saberLength==0&&out.radius==8&&out.oldorigin[2]==33);
        entity=original;write(entity,0,int32_t(9));write(entity,148,lineWidth);write(entity,156,lineScale);
        assert(raven::ReadEntity(entity.data()+3,out)==raven::Status::Ok);
        assert(out.reType==RT_JKA_ORIENTEDLINE&&out.jka.lineWidth==lineWidth&&out.jka.lineSTScale==lineScale);
        assert(out.jka.saberLength==0&&out.axis[1][1]==5);
        // A shape field that does not encode a number refuses its own type, and
        // only its own: the union's other members are the module's leftovers.
        for(uint32_t bits : {0x7fc01234u,0x7f800000u,0xff800000u}) {
            entity=original;write(entity,0,int32_t(5));write(entity,180,bits);
            auto prior=out;assert(raven::ReadEntity(entity.data()+3,out)==raven::Status::InvalidRecord&&same(prior,out));
            write(entity,0,int32_t(9));write(entity,148,lineWidth);write(entity,156,lineScale);
            assert(raven::ReadEntity(entity.data()+3,out)==raven::Status::Ok);
            for(std::size_t offset : {148u,156u}) {
                entity=original;write(entity,0,int32_t(9));write(entity,148,lineWidth);write(entity,156,lineScale);
                write(entity,offset,bits);prior=out;
                assert(raven::ReadEntity(entity.data()+3,out)==raven::Status::InvalidRecord&&same(prior,out));
                write(entity,0,int32_t(5));write(entity,180,glowLength);
                assert(raven::ReadEntity(entity.data()+3,out)==raven::Status::Ok&&out.jka.saberLength==glowLength);
                write(entity,0,int32_t(8));assert(raven::ReadEntity(entity.data()+3,out)==raven::Status::Ok);
            }
        }
        // A blade of any finite length, a negative one included, is retail's to
        // draw: its loop draws no sprite and only the hilt's.
        entity=original;write(entity,0,int32_t(5));write(entity,180,-3.0f);
        assert(raven::ReadEntity(entity.data()+3,out)==raven::Status::Ok&&out.jka.saberLength==-3.0f);
        entity=original;assert(raven::ReadEntity(entity.data()+3,out)==raven::Status::Ok);
    }
    for(uint32_t type : {1u,3u,6u,10u,11u,12u,0xffffffffu}) rejectedEntity(0,type,raven::Status::EntityType);
    for(uint32_t flags : {0x10u,0x20u,0x1000u,0x100000u,0x200000u,0x80000000u}) rejectedEntity(4,flags,raven::Status::EntityFlags);
    // Academy's forced alpha (0x400) and tint (0x800) cross as their own
    // engine fields, never as Quake III flags (jka_entity_override_tests.py).
    assert(same(out.jka,empty.jka));
    for(uint32_t flags : {0x400u,0x800u,0xc00u}) {
        entity=original;write(entity,4,flags|0x3cfu);
        assert(raven::ReadEntity(entity.data()+3,out)==raven::Status::Ok&&out.renderfx==0x3cf);
        assert(out.jka.forceAlpha==((flags&0x400u)?qtrue:qfalse)&&out.jka.rgbTint==((flags&0x800u)?qtrue:qfalse));
        rejectedEntity(4,flags|0x1000u,raven::Status::EntityFlags);
    }
    // A distortion copy (0x2000) crosses as its own field too, and casts no
    // shadow (jka_entity_refraction_tests.py).
    for(uint32_t flags : {0x2000u,0x2400u,0x2c00u}) {
        entity=original;write(entity,4,flags|0x38fu);
        assert(raven::ReadEntity(entity.data()+3,out)==raven::Status::Ok&&out.renderfx==0x3cf);
        assert(out.jka.distortion&&out.jka.forceAlpha==((flags&0x400u)?qtrue:qfalse));
        rejectedEntity(4,flags|0x1000u,raven::Status::EntityFlags);
    }
    entity=original;assert(raven::ReadEntity(entity.data()+3,out)==raven::Status::Ok&&!out.jka.distortion);
    rejectedEntity(48,2,raven::Status::InvalidRecord);
    for(std::size_t offset : {12u,52u,64u,84u,92u,96u,100u,108u,120u,128u})
        for(uint32_t bits : {0x7fc01234u,0x7f800000u,0xff800000u}) rejectedEntity(offset,bits,raven::Status::InvalidRecord);
    uint32_t negative;float minus=-1;std::memcpy(&negative,&minus,4);rejectedEntity(92,negative,raven::Status::InvalidRecord);
    auto before=out;assert(raven::ReadEntity(nullptr,out)==raven::Status::InvalidRecord&&same(before,out));

    std::array<byte,384+7> view{};view[0]=view[1]=view[2]=view.back()=0xa5;
    write(view,0,int32_t(20));write(view,4,int32_t(30));write(view,8,int32_t(640));write(view,12,int32_t(480));
    write(view,16,90.0f);write(view,20,75.0f);
    for(int n=0;n<3;++n)write(view,24+n*4,float(n+10));
    for(int n=0;n<9;++n)write(view,48+n*4,float(n+20));
    write(view,36,uint32_t(0x7fc01234)); // ignored viewangles are not renderer input
    write(view,84,int32_t(123));write(view,88,int32_t(1200));write(view,92,int32_t(5));
    view[3+96]=0x67;view[3+127]=0x89;view[3+128]='a';view[3+383]='z';
    auto sourceView=view;refdef_t fd{};
    assert(fntech3::render::ReadRefdef(COM_GAME_JKA_MP,view.data()+3,fd));assert(view==sourceView);
    assert(fd.x==20&&fd.y==30&&fd.width==640&&fd.height==480&&fd.fov_x==90&&fd.fov_y==75);
    assert(fd.vieworg[2]==12&&fd.viewaxis[2][2]==28&&fd.time==1200&&fd.rdflags==(RDF_NOWORLDMODEL|RDF_HYPERSPACE));
    assert(!(fd.rdflags&(RDF_NOFIRSTPERSON|RDF_NOFOVCORRECTION))&&fd.areamask[0]==0x67&&fd.areamask[31]==0x89);
    assert(fd.text[0][0]=='a'&&fd.text[7][31]=='z');refdef_t emptyView{};assert(same(fd.rtcw,emptyView.rtcw)&&same(fd.alice,emptyView.alice));
    // A terminator must exist within each independent 32-byte text slot.
    // Reject without publishing output; a terminator at byte31 is valid.
    for(int row=0;row<8;++row) {
        view=sourceView;std::memset(view.data()+3+128+32*row,'x',32);
        auto guarded=view;const auto prior=fd;
        assert(raven::ReadView(view.data()+3,fd)==raven::Status::InvalidRecord);
        assert(view==guarded&&same(fd,prior));
        view[3+128+32*row+31]=0;guarded=view;
        assert(raven::ReadView(view.data()+3,fd)==raven::Status::Ok&&view==guarded);
        assert(!std::memcmp(fd.text[row],view.data()+3+128+32*row,32));
    }
    // A normal draw-sky request does not acquire an unrelated Q3 rdflag.
    // Complete copied viewport/view data and canaries remain unchanged.
    for(uint32_t flags:{16u,17u,20u,21u}) {
        view=sourceView;write(view,92,flags);const auto guarded=view;
        assert(raven::ReadView(view.data()+3,fd)==raven::Status::Ok&&view==guarded);
        refdef_t expected{};assert(raven::ReadView(sourceView.data()+3,expected)==raven::Status::Ok);
        expected.rdflags=int(flags&5u);assert(same(fd,expected));
    }
    auto rejectedView=[&](std::size_t offset,uint32_t bad,raven::Status status) {
        view=sourceView;write(view,offset,bad);auto saved=fd;
        assert(raven::ReadView(view.data()+3,fd)==status&&same(saved,fd));
    };
    for(uint32_t flags : {2u,8u,24u,32u,64u,128u,0xffffffffu}) rejectedView(92,flags,raven::Status::ViewFlags);
    for(std::size_t offset : {16u,20u,24u,48u,80u}) rejectedView(offset,0x7fc01234u,raven::Status::InvalidRecord);
    for(std::size_t offset : {8u,12u}) rejectedView(offset,0,raven::Status::InvalidRecord);
    rejectedView(0,uint32_t(INT_MAX),raven::Status::InvalidRecord);rejectedView(4,0xffffffffu,raven::Status::InvalidRecord);
    rejectedView(16,0,raven::Status::InvalidRecord);float wide=180;uint32_t bits;std::memcpy(&bits,&wide,4);rejectedView(20,bits,raven::Status::InvalidRecord);
    auto saved=fd;assert(raven::ReadView(nullptr,fd)==raven::Status::InvalidRecord&&same(saved,fd));
    assert(!fntech3::render::ReadEntity(COM_GAME_JKA_SP,original.data()+3,out));
    assert(!fntech3::render::ReadRefdef(COM_GAME_JKA_SP,sourceView.data()+3,fd));
    std::puts("Jedi Academy static renderer records and refusal checks passed");
}
