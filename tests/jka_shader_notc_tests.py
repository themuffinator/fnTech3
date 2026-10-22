"""Raven noTC through each production shader parser and image wrapper."""
import os
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch
import subprocess

from alice_shader_parse_tests import build_host, host_source, RENDERERS

ROOT = Path(__file__).resolve().parents[1]
SCRATCH = ROOT / '.tmp/jka-compat'

CHECKS = r'''
static int hostFailures;
#define CHECK(test) do { if(!(test)) { fprintf(stderr,"failed line %d: %s\n",__LINE__,#test);++hostFailures; } } while(0)
static qboolean Parse(comGameId_t game,const char *text) {
 HostInit(game);hostFlagCount=0;return HostParse("test/jka",text,text+strlen(text));
}
static int HostChecks(void) {
 comGameId_t game;int i,j;unsigned saved[64];int count;
 const char *plain[]={
  "{\n{\nmap textures/a.tga\n}\n}\n",
  "{\n{\nclampMap textures/a.tga\n}\n}\n",
  "{\n{\nanimMap 2 textures/a.tga textures/b.tga\n}\n}\n",
  "{\nskyParms env/sky 512 -\n}\n"
 };
 const char *uncompressed[]={
  "{\nnoTC\n{\nmap textures/a.tga\n}\n}\n",
  "{\nNoTc\n{\nclampMap textures/a.tga\n}\n}\n",
  "{\nNOTC\n{\nanimMap 2 textures/a.tga textures/b.tga\n}\n}\n",
  "{\nnoTC\nskyParms env/sky 512 -\n}\n"
 };
 for(game=COM_GAME_QUAKE3;game<COM_GAME_COUNT;game=(comGameId_t)(game+1)) {
  /* both Jedi games: Outcast's engines read noTC as Academy's do
     (renderercommon/tr_jka_shader.h) */
  const qboolean raven=game==COM_GAME_JKA_SP||game==COM_GAME_JKA_MP||game==COM_GAME_JK2_SP||game==COM_GAME_JK2_MP;
  for(i=0;i<4;++i) {
   if(!raven) { CHECK(!Parse(game,uncompressed[i]) && !shader.jkaNoTC);continue; }
   CHECK(Parse(game,plain[i]));CHECK(!shader.jkaNoTC);
   count=hostFlagCount;CHECK(count>0 && count<64);memcpy(saved,hostFlags,sizeof(saved));
   CHECK(Parse(game,uncompressed[i]) && shader.jkaNoTC && count==hostFlagCount);
   for(j=0;j<count;++j)CHECK(hostFlags[j]==(saved[j]|IMGFLAG_NO_COMPRESSION));
   CHECK(Parse(game,plain[i]) && !shader.jkaNoTC && count==hostFlagCount);
   for(j=0;j<count;++j)CHECK(hostFlags[j]==saved[j]);
  }
  CHECK(!Parse(game,"{\nnoTCx\n{\nmap a.tga\n}\n}\n"));
  CHECK(!Parse(game,"{\n{\nnoTC\nmap a.tga\n}\n}\n"));
  HostInit(game);shader.jkaNoTC=qtrue;hostFlagCount=0;
  R_FindShaderImage("textures/synthetic.tga",IMGFLAG_MIPMAP|IMGFLAG_PICMIP|IMGFLAG_CLAMPTOEDGE);
  CHECK(hostFlagCount==1);
  CHECK(hostFlags[0]==(unsigned)(IMGFLAG_MIPMAP|IMGFLAG_PICMIP|IMGFLAG_CLAMPTOEDGE|(raven?IMGFLAG_NO_COMPRESSION:0)));
 }
 return hostFailures ? 1 : 0;
}
'''


def linked(path):
    return path.is_symlink() or bool(getattr(path.lstat(), 'st_file_attributes', 0) & 0x400)


class JKAShaderNoTCTests(unittest.TestCase):
    def test_production_parsers_and_image_flags(self):
        SCRATCH.mkdir(parents=True, exist_ok=True)
        self.assertFalse(linked(SCRATCH))
        folder = Path(tempfile.mkdtemp(prefix='shader-notc-', dir=SCRATCH))
        try:
            for renderer in RENDERERS:
                with self.subTest(renderer=renderer):
                    source = (ROOT / 'code' / renderer / 'tr_shader.c').read_text()
                    self.assertEqual(source.count('R_FindImageFile('), 1)
                    # Reuse the existing production-parser host, with a recorded
                    # image flag for every actual image request, including sky.
                    host = host_source(CHECKS, renderer)
                    host = host.replace('image_t *R_FindImageFile( const char *name, imgFlags_t flags ) {',
                        'static unsigned hostFlags[64];static int hostFlagCount;\n'
                        'image_t *R_FindImageFile( const char *name, imgFlags_t flags ) {')
                    host = host.replace('(void)name; (void)flags;',
                        '(void)name;if(hostFlagCount>=64)abort();hostFlags[hostFlagCount++]=(unsigned)flags;', 1)
                    with patch.dict(os.environ, TMP=str(folder), TEMP=str(folder), TMPDIR=str(folder)):
                        # build_host uses host_source internally; substitute only
                        # this generated host within this test process.
                        with patch('alice_shader_parse_tests.host_source', return_value=host):
                            executable = build_host(folder, renderer, CHECKS, renderer=renderer)
                    options = {'creationflags': subprocess.CREATE_NO_WINDOW} if os.name == 'nt' else {}
                    result = subprocess.run([str(executable)], cwd=ROOT, capture_output=True, text=True,
                        timeout=30, **options)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        finally:
            self.assertTrue(folder.resolve().is_relative_to(SCRATCH.resolve()))
            self.assertFalse(linked(folder))
            self.assertFalse(any(linked(path) for path in folder.rglob('*')))
            shutil.rmtree(folder)


if __name__ == '__main__':
    unittest.main()
