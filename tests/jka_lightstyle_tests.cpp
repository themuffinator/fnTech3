/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/compat/jka_lightstyle.hpp"
#include <cassert>

namespace styles=fntech3::jka::lightstyle;
int main(){
    styles::Store store;
    styles::Color color{{9,8,7,6}};
    for(int index=0;index<styles::Count;++index){assert(store.Get(index,color));assert((color==styles::Color{{255,255,255,255}}));}
    assert(store.Set(63,UINT32_C(0x817f1305))==styles::Status::Ok);
    assert(store.Get(63,color)&&(color==styles::Color{{5,19,127,129}}));
    assert(store.Set(63,UINT32_C(0x817f1305))==styles::Status::Ok);
    const auto before=color;
    assert(!store.Get(-1,color)&&color==before);assert(!store.Get(64,color)&&color==before);
    assert(store.Set(-1,0)==styles::Status::Index&&store.Set(64,0)==styles::Status::Index);
    assert(store.Set(0,UINT32_C(0x12ffffff))==styles::Status::Ok);
    assert(store.Get(0,color)&&(color==styles::Color{{255,255,255,18}}));
    assert(store.Set(0,UINT32_C(0xffffffff))==styles::Status::Ok);
    // Style zero takes any colour too: the renderer recomposes what it lights.
    assert(store.Set(0,UINT32_C(0x00fffffe))==styles::Status::Ok);
    assert(store.Get(0,color)&&(color==styles::Color{{254,255,255,0}}));
    store.Reset();assert(store.Get(63,color)&&(color==styles::Color{{255,255,255,255}}));
}
