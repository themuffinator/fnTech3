/* Copyright (C) 2026 fnTech3 contributors. GPL-2.0-or-later. */
#include "../code/client/client_demo_profile.hpp"
#include <cstdio>
#define CHECK(test) do { if (!(test)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); return __LINE__; } } while (false)
namespace demo = fntech3::demo;
int main() {
    constexpr unsigned q3 = COM_PROTOCOL_FAMILY_LEGACY_QUAKE3 | COM_PROTOCOL_FAMILY_IOQUAKE3 | COM_PROTOCOL_FAMILY_Q3_POINT_RELEASES;
    struct Record { unsigned family; int protocol; netchanWireProfile_t wire; };
    constexpr Record records[]{
        {COM_PROTOCOL_FAMILY_JKA_MP, 26, NETCHAN_WIRE_JKA_MP},
        {COM_PROTOCOL_FAMILY_IOEF, 26, NETCHAN_WIRE_IOEF},
        {COM_PROTOCOL_FAMILY_JK2_MP, NETCHAN_JK2_MP_PROTOCOL_VERSION, NETCHAN_WIRE_JK2_MP},
        {COM_PROTOCOL_FAMILY_QUAKELIVE, 91, NETCHAN_WIRE_QL_RETAIL},
        {COM_PROTOCOL_FAMILY_RTCW_SP, 49, NETCHAN_WIRE_RTCW_SP},
        {COM_PROTOCOL_FAMILY_RTCW_MP, 60, NETCHAN_WIRE_RTCW_MP},
        {COM_PROTOCOL_FAMILY_WOLFET, 84, NETCHAN_WIRE_WOLFET},
        {COM_PROTOCOL_FAMILY_STVEF_HM, 24, NETCHAN_WIRE_STVEF_HM},
        {COM_PROTOCOL_FAMILY_STVEF_SP, 40, NETCHAN_WIRE_STVEF_SP},
        {COM_PROTOCOL_FAMILY_MOHAA, 6, NETCHAN_WIRE_MOHAA},
        {COM_PROTOCOL_FAMILY_ALICE, 1, NETCHAN_WIRE_ALICE},
        {COM_PROTOCOL_FAMILY_FAKK2, 1, NETCHAN_WIRE_ALICE},
        {q3, 66, NETCHAN_WIRE_Q3_66}, {q3, 67, NETCHAN_WIRE_Q3_67},
        {q3, 68, NETCHAN_WIRE_LEGACY_Q3}, {q3, 71, NETCHAN_WIRE_IOQ3},
        {q3, 43, NETCHAN_WIRE_LEGACY_Q3}, {q3, 45, NETCHAN_WIRE_LEGACY_Q3},
        {q3, 46, NETCHAN_WIRE_LEGACY_Q3}, {q3, 48, NETCHAN_WIRE_LEGACY_Q3},
    };
    for (const auto &record : records) {
        netchanWireProfile_t output = NETCHAN_WIRE_Q3_IHV;
        CHECK(demo::Wire(record.family, record.protocol, output) && output == record.wire);
        CHECK(!demo::RawQuake3(record.family, 26, false));
        if (record.family != q3) {
            CHECK(!demo::RawQuake3(record.family, 43, false));
            CHECK(!demo::RawQuake3(record.family, 68, true));
        }
    }
    for (const int protocol : {-1, 0, 1, 6, 24, 40, 43, 45, 46, 48, 49, 60, 66, 67, 68, 71, 84, 91, 999}) {
        netchanWireProfile_t output = NETCHAN_WIRE_Q3_IHV;
        CHECK(!demo::Wire(COM_PROTOCOL_FAMILY_JKA_MP, protocol, output));
        CHECK(output == NETCHAN_WIRE_Q3_IHV);
    }
    netchanWireProfile_t output = NETCHAN_WIRE_Q3_IHV;
    CHECK(!demo::Wire(q3, 26, output) && output == NETCHAN_WIRE_Q3_IHV);
    CHECK(!demo::Wire(0, 26, output) && output == NETCHAN_WIRE_Q3_IHV);
    CHECK(demo::RawQuake3(q3, 43, false) && demo::RawQuake3(q3, 68, true));
    return 0;
}
