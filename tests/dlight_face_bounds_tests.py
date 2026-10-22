"""Compile each renderer's face loader and light culling against finite faces.

Vulkan and RTX used to test only the infinite plane, admitting extra receivers
and shadow casters in RTCW forest and mp_village. These synthetic faces check
the loaded bounds, touching lights, linear lights and rotated face planes.
"""
import unittest

from alice_patch_collision_tests import build_and_run
from bsp_index_bounds_tests import ROOT, RENDERERS, function, read, renderer_source


CHECKS = r'''
static srfSurfaceFace_t *LoadFace( void ) {
    /* The first and last vertices belong to other surfaces. */
    drawVert_t verts[6] = { 0 };
    int indexes[3] = { 0, 1, 2 };
    dsurface_t ds = { 0 };
    static msurface_t surface;
    static dshader_t shader;
    VectorSet( verts[0].xyz, -999, -999, -999 );
    VectorSet( verts[1].xyz, 20, -30, 10 );
    VectorSet( verts[2].xyz, 40, -30, 10 );
    VectorSet( verts[3].xyz, 40, -10, 10 );
    VectorSet( verts[4].xyz, 20, -10, 10 );
    VectorSet( verts[5].xyz, 999, 999, 999 );
    ds.firstVert = 1; ds.numVerts = 4; ds.numIndexes = 3;
    ds.lightmapVecs[2][2] = 1;
    s_worldData.surfaces = &surface;
    s_worldData.shaders = &shader; s_worldData.numShaders = 1;
    ParseFace( &ds, verts, 6, &surface, indexes, 3 );
    return (srfSurfaceFace_t *)surface.data;
}

int main( void ) {
    srfSurfaceFace_t *face = LoadFace();
    dlight_t light = { 0 };
    int axis, side;
    const vec3_t middle = { 30, -20, 10 };
    assert( face->numPoints == 4 && face->numIndices == 3 );
    assert( face->bounds[0][0] == 20 && face->bounds[1][0] == 40 );
    assert( face->bounds[0][1] == -30 && face->bounds[1][1] == -10 );
    assert( face->bounds[0][2] == 10 && face->bounds[1][2] == 10 );
    assert( face->plane.dist == 10 );
    assert( ((int *)((byte *)face + face->ofsIndices))[2] == 2 );

    light.radius = 5;
    VectorCopy( middle, light.transformed );
    assert( !R_LightCullFace( face, &light ) );
    /* Every face of the radius-expanded AABB: touching is retained, just
     * outside is rejected, including lights in the same infinite plane. */
    for ( axis = 0; axis < 3; axis++ ) {
        for ( side = 0; side < 2; side++ ) {
            float sign = side ? 1 : -1;
            VectorCopy( middle, light.transformed );
            light.transformed[axis] = face->bounds[side][axis] + sign * light.radius;
            assert( !R_LightCullFace( face, &light ) );
            light.transformed[axis] += sign * 0.25f;
            assert( R_LightCullFace( face, &light ) );

            light.linear = qtrue;
            VectorCopy( light.transformed, light.transformed2 );
            assert( R_LightCullFace( face, &light ) );
            VectorCopy( middle, light.transformed2 );
            assert( !R_LightCullFace( face, &light ) );
            light.transformed2[axis] = face->bounds[!side][axis] - sign * 10;
            assert( !R_LightCullFace( face, &light ) );
            light.linear = qfalse;
        }
    }
    /* Bounds do not replace the plane test: a tilted face's AABB contains
     * points too far from the plane. Linear lights crossing it still fit. */
    VectorSet( face->bounds[0], -100, -100, -100 );
    VectorSet( face->bounds[1], 100, 100, 100 );
    VectorSet( face->plane.normal, 0.6f, 0.8f, 0 ); face->plane.dist = 0;
    VectorSet( light.transformed, 30, 40, 0 );
    assert( R_LightCullFace( face, &light ) );
    light.linear = qtrue;
    VectorSet( light.transformed2, -30, -40, 0 );
    assert( !R_LightCullFace( face, &light ) );
    VectorSet( light.transformed2, 60, 80, 0 );
    assert( R_LightCullFace( face, &light ) );
    return 0;
}
'''


class DlightFaceBoundsTests(unittest.TestCase):
    def test_loaded_faces_cull_point_and_linear_lights_consistently(self):
        for renderer in RENDERERS:
            with self.subTest(renderer=renderer):
                world = read(f'code/{renderer}/tr_world.c')
                checks = ('typedef struct { vec3_t transformed, transformed2; '
                          'float radius; qboolean linear; } dlight_t;\n')
                checks += function(world, 'qboolean R_LightCullBounds(')
                checks += function(world, 'static qboolean R_LightCullFace(')
                # This activates the face bounds without unrelated normal generation.
                source = '#define USE_LEGACY_DLIGHTS 1\n' + renderer_source(renderer, checks + CHECKS)
                build_and_run(self, f'facebounds{RENDERERS.index(renderer)}',
                              [source, ROOT / 'code/qcommon/q_math.c'])


if __name__ == '__main__':
    unittest.main()
