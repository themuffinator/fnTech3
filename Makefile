
# Quake3 Unix Makefile
#
# Nov '98 by Zoid <zoid@idsoftware.com>
#
# Loki Hacking by Bernd Kreimeier
#  and a little more by Ryan C. Gordon.
#  and a little more by Rafael Barrero
#  and a little more by the ioq3 cr3w
#
# GNU Make required
#
COMPILE_PLATFORM=$(shell uname | sed -e 's/_.*//' | tr '[:upper:]' '[:lower:]' | sed -e 's/\//_/g')
COMPILE_ARCH=$(shell uname -m | sed -e 's/i.86/x86/' | sed -e 's/^arm.*/arm/')

ifeq ($(shell uname -m),arm64)
  COMPILE_ARCH=aarch64
else
ifeq ($(COMPILE_ARCH),aarch64)
  LONG_BIT=$(shell [ -x /usr/bin/getconf ] && getconf LONG_BIT)
  ifeq ($(LONG_BIT),32)
    COMPILE_ARCH=arm
  endif
endif
endif

ifeq ($(COMPILE_PLATFORM),mingw32)
  ifeq ($(COMPILE_ARCH),i386)
    COMPILE_ARCH=x86
  endif
endif

BUILD_CLIENT     = 1
BUILD_SERVER     = 1

USE_SDL          = 1
USE_CURL         = 1
USE_LOCAL_HEADERS= 0
USE_SYSTEM_JPEG  = 1

USE_OGG_VORBIS    = 1
USE_SYSTEM_OGG    = 1
USE_SYSTEM_VORBIS = 1

USE_VK           = 1
USE_RTX          = 1
USE_GLX          = 1
USE_OPENGL_API   = 1
USE_VULKAN_API   = 1
USE_RENDERER_DLOPEN = 1

# valid options: glx, vk, rtx
RENDERER_DEFAULT = glx

CNAME            = fntech3
DNAME            = fntech3.ded

# Renderer names remain bound to the existing loader contract.
RENDERER_PREFIX  = fnql
VERSION_HEADER   = version/fntech3_version.h


ifeq ($(V),1)
echo_cmd=@:
Q=
else
echo_cmd=@echo
Q=@
endif

#############################################################################
#
# If you require a different configuration from the defaults below, create a
# new file named "Makefile.local" in the same directory as this file and define
# your parameters there. This allows you to change configuration without
# causing problems with keeping up to date with the repository.
#
#############################################################################
-include Makefile.local

ifeq ($(COMPILE_PLATFORM),darwin)
  USE_SDL=1
  # The old in-tree SDL copy was intentionally removed.  Consume an installed
  # SDL3 framework/pkg-config target and keep renderer dylibs beside the app
  # executable, matching the maintained Meson package layout.
  USE_LOCAL_HEADERS=0
  USE_RENDERER_DLOPEN=1
endif

ifeq ($(COMPILE_PLATFORM),cygwin)
  PLATFORM=mingw32
endif

ifndef PLATFORM
PLATFORM=$(COMPILE_PLATFORM)
endif
export PLATFORM

ifeq ($(PLATFORM),mingw32)
  MINGW=1
endif
ifeq ($(PLATFORM),mingw64)
  MINGW=1
endif

ifeq ($(COMPILE_ARCH),i86pc)
  COMPILE_ARCH=x86
endif

ifeq ($(COMPILE_ARCH),amd64)
  COMPILE_ARCH=x86_64
endif
ifeq ($(COMPILE_ARCH),x64)
  COMPILE_ARCH=x86_64
endif

ifndef ARCH
ARCH=$(COMPILE_ARCH)
endif
export ARCH

# MAKECMDGOALS is GNU Make-owned policy input. Reject command-line attempts to
# disguise an output-producing goal as cleanup.
ifeq ($(filter undefined default,$(origin MAKECMDGOALS)),)
  $(error MAKECMDGOALS is managed by GNU Make and must not be overridden)
endif

# Cleanup does not emit binaries, so it remains usable from any host without
# forcing callers to repeat a target architecture. Every goal that can build,
# copy, install, or package output still fails during Makefile evaluation.
override FNQL_ARCH_NEUTRAL_GOALS := clean clean2 clean-debug clean-release distclean toolsclean
override FNQL_ARCH_SENSITIVE_GOALS := $(filter-out $(FNQL_ARCH_NEUTRAL_GOALS),$(MAKECMDGOALS))
override FNQL_ENFORCE_X86 :=
ifeq ($(strip $(MAKECMDGOALS)),)
  override FNQL_ENFORCE_X86 := 1
else ifneq ($(strip $(FNQL_ARCH_SENSITIVE_GOALS)),)
  override FNQL_ENFORCE_X86 := 1
endif

# The same goal split governs build inputs. A fresh clone can run the cleanup
# goals before any Meson subproject has been downloaded or any compiler has
# been installed, so those prerequisites are only demanded when a goal can
# actually produce output.
override FNQL_REQUIRE_BUILD_INPUTS := $(FNQL_ENFORCE_X86)

ifeq ($(FNQL_ENFORCE_X86),1)
  ifneq ($(strip $(ARCH)),x86)
    $(error fnTech3 shared Q3/QL builds support only 32-bit x86 targets; set ARCH=x86 and select a 32-bit toolchain (got ARCH=$(ARCH)))
  endif
endif

# These flags are appended with GNU Make's override directive after platform
# setup as well as to recursive compile/link variables. Command-line variable
# assignments therefore cannot silently turn an x86-labelled build into x64.
override FNQL_REQUIRED_ARCH_CFLAGS := -m32
override FNQL_REQUIRED_ARCH_LDFLAGS := -m32
ifeq ($(COMPILE_PLATFORM),darwin)
  override FNQL_REQUIRED_ARCH_CFLAGS := -arch i386
  override FNQL_REQUIRED_ARCH_LDFLAGS := -arch i386
endif

ifneq ($(PLATFORM),$(COMPILE_PLATFORM))
  CROSS_COMPILING=1
else
  CROSS_COMPILING=0

  ifneq ($(ARCH),$(COMPILE_ARCH))
    CROSS_COMPILING=1
  endif
endif
export CROSS_COMPILING

DESTDIR ?=
ifneq ($(filter install,$(MAKECMDGOALS)),)
  ifeq ($(strip $(DESTDIR)),)
    $(error DESTDIR is required; choose an explicit fnTech3 staging directory)
  endif
endif

ifndef MOUNT_DIR
MOUNT_DIR=code
endif

ifndef BUILD_DIR
BUILD_DIR=build
endif

ifndef PYTHON
PYTHON=python3
endif

ifndef GENERATE_DEPENDENCIES
GENERATE_DEPENDENCIES=1
endif

ifndef USE_CCACHE
USE_CCACHE=0
endif
export USE_CCACHE

ifndef USE_LOCAL_HEADERS
USE_LOCAL_HEADERS=1
endif

ifndef USE_CURL
USE_CURL=1
endif

ifndef USE_CURL_DLOPEN
  ifdef MINGW
    USE_CURL_DLOPEN=0
  else
    USE_CURL_DLOPEN=1
  endif
endif

ifndef USE_OGG_VORBIS
  USE_OGG_VORBIS=1
endif

ifndef USE_SYSTEM_OGG
  USE_SYSTEM_OGG=1
endif

ifndef USE_SYSTEM_VORBIS
  USE_SYSTEM_VORBIS=1
endif

ifeq ($(RENDERER_DEFAULT),glx)
  USE_GLX=1
endif

ifeq ($(RENDERER_DEFAULT),vk)
  USE_VK=1
endif

ifeq ($(RENDERER_DEFAULT),rtx)
  USE_RTX=1
endif

ifeq ($(USE_RENDERER_DLOPEN),0)
  ifeq ($(RENDERER_DEFAULT),vk)
    USE_GLX=0
    USE_VK=1
    USE_RTX=0
    USE_OPENGL_API=0
    USE_VULKAN_API=1
  endif
  ifeq ($(RENDERER_DEFAULT),rtx)
    USE_GLX=0
    USE_VK=0
    USE_RTX=1
    USE_OPENGL_API=0
    USE_VULKAN_API=1
  endif
  ifeq ($(RENDERER_DEFAULT),glx)
    USE_GLX=1
    USE_VK=0
    USE_RTX=0
    USE_OPENGL_API=1
    USE_VULKAN_API=0
  endif
endif

ifneq ($(USE_VK),0)
  USE_VULKAN_API=1
endif

ifneq ($(USE_RTX),0)
  USE_VULKAN_API=1
endif

ifneq ($(USE_GLX),0)
  USE_OPENGL_API=1
endif


#############################################################################

BD=$(BUILD_DIR)/debug-$(PLATFORM)-$(ARCH)
BR=$(BUILD_DIR)/release-$(PLATFORM)-$(ARCH)
ADIR=$(MOUNT_DIR)/asm
CDIR=$(MOUNT_DIR)/client
SDIR=$(MOUNT_DIR)/server
PDIR=$(MOUNT_DIR)/platform
RCDIR=$(MOUNT_DIR)/renderercommon
R1DIR=$(MOUNT_DIR)/renderer
RXDIR=$(MOUNT_DIR)/rendererglx
RVDIR=$(MOUNT_DIR)/renderervk
RTXDIR=$(MOUNT_DIR)/rendererrtx
SDLDIR=$(MOUNT_DIR)/sdl
FONTSTASHDIR ?= subprojects/fontstash/src
MINIMP3DIR ?= subprojects/minimp3

CMDIR=$(MOUNT_DIR)/qcommon
COMPATDIR=$(MOUNT_DIR)/compat
MOHAADIR=$(MOUNT_DIR)/mohaa
UDIR=$(MOUNT_DIR)/unix
W32DIR=$(MOUNT_DIR)/win32
BLIBDIR=$(MOUNT_DIR)/botlib
CLIENTAUDIODIR=$(CDIR)/audio
CLIENTAUDIOCODECSDIR=$(CLIENTAUDIODIR)/codecs
CLIENTAUDIOLEGACYDIR=$(CLIENTAUDIODIR)/legacy
CLIENTAUDIOOPENALDIR=$(CLIENTAUDIODIR)/openal

bin_path=$(shell which $(1) 2> /dev/null)

STRIP ?= strip
PKG_CONFIG ?= pkg-config
INSTALL=install
MKDIR=mkdir -p

ifneq ($(call bin_path, $(PKG_CONFIG)),)
  ifneq ($(USE_SDL),0)
    SDL_INCLUDE ?= $(shell $(PKG_CONFIG) --silence-errors --cflags-only-I sdl3)
    SDL_LIBS ?= $(shell $(PKG_CONFIG) --silence-errors --libs sdl3)
  else
    X11_INCLUDE ?= $(shell $(PKG_CONFIG) --silence-errors --cflags-only-I x11)
    X11_LIBS ?= $(shell $(PKG_CONFIG) --silence-errors --libs x11)
  endif
  ifeq ($(USE_SYSTEM_OGG),1)
    OGG_CFLAGS ?= $(shell $(PKG_CONFIG) --silence-errors --cflags ogg || true)
    OGG_LIBS ?= $(shell $(PKG_CONFIG) --silence-errors --libs ogg || echo -logg)
  endif
  ifeq ($(USE_SYSTEM_VORBIS),1)
    VORBIS_CFLAGS ?= $(shell $(PKG_CONFIG) --silence-errors --cflags vorbisfile || true)
    VORBIS_LIBS ?= $(shell $(PKG_CONFIG) --silence-errors --libs vorbisfile || echo -lvorbisfile)
  endif
  OPENAL_CFLAGS ?= $(shell $(PKG_CONFIG) --silence-errors --cflags openal || true)
  ifeq ($(USE_CURL),1)
    CURL_CFLAGS ?= $(shell $(PKG_CONFIG) --silence-errors --cflags libcurl || true)
    CURL_LIBS ?= $(shell $(PKG_CONFIG) --silence-errors --libs libcurl || echo -lcurl)
  endif
  ifeq ($(USE_SYSTEM_JPEG),1)
    JPEG_CFLAGS ?= $(shell $(PKG_CONFIG) --silence-errors --cflags libjpeg || true)
    JPEG_LIBS ?= $(shell $(PKG_CONFIG) --silence-errors --libs libjpeg || echo -ljpeg)
  endif
endif

# supply some reasonable defaults for SDL/X11
ifeq ($(X11_INCLUDE),)
  X11_INCLUDE = -I/usr/X11R6/include
endif
ifeq ($(X11_LIBS),)
  X11_LIBS = -lX11
endif
ifeq ($(SDL_LIBS),)
  SDL_LIBS = -lSDL3
endif
ifeq ($(CURL_LIBS),)
  CURL_LIBS = -lcurl
endif
ifeq ($(JPEG_LIBS),)
  JPEG_LIBS = -ljpeg
endif

ifeq ($(USE_SYSTEM_OGG),1)
  ifeq ($(OGG_FLAGS),)
    OGG_FLAGS = $(OGG_CFLAGS)
  endif
  ifeq ($(OGG_LIBS),)
    OGG_LIBS = -logg
  endif
endif
ifeq ($(USE_SYSTEM_VORBIS),1)
  ifeq ($(VORBIS_FLAGS),)
    VORBIS_FLAGS = $(VORBIS_CFLAGS)
  endif
  ifeq ($(VORBIS_LIBS),)
    VORBIS_LIBS = -lvorbisfile
  endif
endif

# extract version info
VERSION=$(shell grep -m1 FNTECH3_VERSION_STRING $(VERSION_HEADER) | cut -d\" -f2)

# common qvm definition
ifeq ($(ARCH),x86_64)
  HAVE_VM_COMPILED = true
else
ifeq ($(ARCH),x86)
  HAVE_VM_COMPILED = true
else
  HAVE_VM_COMPILED = false
endif
endif

ifeq ($(ARCH),arm)
  HAVE_VM_COMPILED = true
endif
ifeq ($(ARCH),aarch64)
  HAVE_VM_COMPILED = true
endif
ifeq ($(ARCH),ppc64le)
  HAVE_VM_COMPILED = true
endif
ifeq ($(ARCH),ppc64)
  HAVE_VM_COMPILED = true
endif

BASE_CFLAGS =

ifeq ($(USE_SYSTEM_JPEG),1)
  BASE_CFLAGS += -DUSE_SYSTEM_JPEG $(JPEG_CFLAGS)
endif

ifneq ($(HAVE_VM_COMPILED),true)
  BASE_CFLAGS += -DNO_VM_COMPILED
endif

ifneq ($(USE_RENDERER_DLOPEN),0)
  BASE_CFLAGS += -DUSE_RENDERER_DLOPEN
  BASE_CFLAGS += -DRENDERER_PREFIX=\\\"$(RENDERER_PREFIX)\\\"
  BASE_CFLAGS += -DRENDERER_DEFAULT=$(RENDERER_DEFAULT)
endif

ifdef DEFAULT_BASEDIR
  BASE_CFLAGS += -DDEFAULT_BASEDIR=\\\"$(DEFAULT_BASEDIR)\\\"
endif

ifeq ($(USE_LOCAL_HEADERS),1)
  BASE_CFLAGS += -DUSE_LOCAL_HEADERS=1
endif

ifneq ($(wildcard $(MOUNT_DIR)/openal/include/AL/al.h),)
  BASE_CFLAGS += -I$(MOUNT_DIR)/openal/include
endif
BASE_CFLAGS += $(OPENAL_CFLAGS)

ifeq ($(USE_SDL),1)
  BASE_CFLAGS += -DSDL_FUNCTION_POINTER_IS_VOID_POINTER=1
  BASE_CFLAGS += -DUSE_SDL_SYSCON=1
endif

ifeq ($(USE_CURL),1)
  BASE_CFLAGS += -DUSE_CURL $(CURL_CFLAGS)
  ifeq ($(USE_CURL_DLOPEN),1)
    BASE_CFLAGS += -DUSE_CURL_DLOPEN
  endif
endif

ifeq ($(BUILD_CLIENT),1)
  ifneq ($(wildcard $(FONTSTASHDIR)/fontstash.h),)
    BASE_CFLAGS += -DBUILD_FONTSTASH -isystem $(FONTSTASHDIR)
  else ifeq ($(FNQL_REQUIRE_BUILD_INPUTS),1)
    $(error Retail host fonts require $(FONTSTASHDIR)/fontstash.h; run 'meson subprojects download fontstash' before using Make)
  endif
  # Elite Force's sounds and music and American McGee's Alice's music are
  # MP3; minimp3 is header-only and compiled once in snd_codec_mp3.cpp.
  ifneq ($(wildcard $(MINIMP3DIR)/minimp3.h),)
    BASE_CFLAGS += -DUSE_CODEC_MP3 -isystem $(MINIMP3DIR)
    USE_CODEC_MP3 = 1
  else ifeq ($(FNQL_REQUIRE_BUILD_INPUTS),1)
    $(error Elite Force and Alice audio require $(MINIMP3DIR)/minimp3.h; run 'meson subprojects download minimp3' before using Make)
  endif
  ifeq ($(FNQL_REQUIRE_BUILD_INPUTS),1)
    ifneq ($(USE_SYSTEM_JPEG),1)
      $(error USE_SYSTEM_JPEG=0 requires the removed in-tree libjpeg sources; use Meson for the libjpeg-turbo subproject fallback or install system libjpeg)
    endif
    ifeq ($(USE_OGG_VORBIS),1)
      ifneq ($(USE_SYSTEM_OGG),1)
        $(error USE_SYSTEM_OGG=0 requires the removed in-tree libogg sources; use Meson for the Ogg subproject fallback or install system libogg)
      endif
      ifneq ($(USE_SYSTEM_VORBIS),1)
        $(error USE_SYSTEM_VORBIS=0 requires the removed in-tree libvorbis sources; use Meson for the Vorbis subproject fallback or install system libvorbis)
      endif
    endif
  endif
endif

ifeq ($(USE_VULKAN_API),1)
  BASE_CFLAGS += -DUSE_VULKAN_API
endif

ifeq ($(USE_OPENGL_API),1)
  BASE_CFLAGS += -DUSE_OPENGL_API
endif

ifeq ($(GENERATE_DEPENDENCIES),1)
  BASE_CFLAGS += -MMD
endif


ARCHEXT=

CLIENT_EXTRA_FILES=


#############################################################################
# SETUP AND BUILD -- MINGW32
#############################################################################

ifdef MINGW

  ifeq ($(CROSS_COMPILING),1)
    # If CC is already set to something generic, we probably want to use
    # something more specific
    ifneq ($(findstring $(strip $(CC)),cc gcc),)
      override CC=
    endif

    ifneq ($(findstring $(notdir $(strip $(CXX))),c++ g++),)
      override CXX=
    endif

    ifneq ($(findstring $(strip $(STRIP)),strip),)
      override STRIP=
    endif

    # We need to figure out the correct gcc and windres
    ifeq ($(ARCH),x86_64)
      MINGW_PREFIXES=x86_64-w64-mingw32 amd64-mingw32msvc
    endif
    ifeq ($(ARCH),x86)
      MINGW_PREFIXES=i686-w64-mingw32 i586-mingw32msvc i686-pc-mingw32
    endif

    ifndef CC
      override CC=$(firstword $(strip $(foreach MINGW_PREFIX, $(MINGW_PREFIXES), \
         $(call bin_path, $(MINGW_PREFIX)-gcc))))
    endif

    ifndef CXX
      override CXX=$(firstword $(strip $(foreach MINGW_PREFIX, $(MINGW_PREFIXES), \
         $(call bin_path, $(MINGW_PREFIX)-g++) \
         $(call bin_path, $(MINGW_PREFIX)-c++))))
    endif

    ifndef STRIP
      override STRIP=$(firstword $(strip $(foreach MINGW_PREFIX, $(MINGW_PREFIXES), \
         $(call bin_path, $(MINGW_PREFIX)-strip))))
    endif

    ifndef WINDRES
      WINDRES=$(firstword $(strip $(foreach MINGW_PREFIX, $(MINGW_PREFIXES), \
         $(call bin_path, $(MINGW_PREFIX)-windres))))
    endif
  else
    # Some MinGW installations define CC to cc, but don't actually provide cc,
    # so check that CC points to a real binary and use gcc if it doesn't
    ifeq ($(call bin_path, $(CC)),)
      override CC=gcc
    endif

    ifeq ($(call bin_path, $(CXX)),)
      override CXX=g++
    endif

  endif

  # using generic windres if specific one is not present
  ifeq ($(WINDRES),)
    WINDRES=windres
  endif

  ifeq ($(FNQL_REQUIRE_BUILD_INPUTS),1)
    ifeq ($(CC),)
      $(error Cannot find a suitable cross compiler for $(PLATFORM))
    endif
    ifeq ($(CXX),)
      $(error Cannot find a suitable C++ compiler for $(PLATFORM))
    endif
  endif

  BASE_CFLAGS += -Wall -Wimplicit -Wstrict-prototypes -DUSE_ICON -DMINGW=1

  BASE_CFLAGS += -Wno-unused-result -fvisibility=hidden
  BASE_CFLAGS += -ffunction-sections -flto

  ifeq ($(ARCH),x86_64)
    ARCHEXT = .x64
    BASE_CFLAGS += -m64
    OPTIMIZE = -O2 -ffast-math
  endif
  ifeq ($(ARCH),x86)
    OPTIMIZE = -O2 -march=i586 -mtune=i686 -ffast-math
  endif

  SHLIBEXT = dll
  SHLIBCFLAGS = -fPIC -fvisibility=hidden
  SHLIBLDFLAGS = -shared $(LDFLAGS)

  BINEXT = .exe

  LDFLAGS += -mwindows -Wl,--dynamicbase -Wl,--nxcompat
  LDFLAGS += -Wl,--gc-sections -fvisibility=hidden
  LDFLAGS += -lwsock32 -lgdi32 -lwinmm -lole32 -lws2_32 -lpsapi -lcomctl32
  LDFLAGS += -flto

  CLIENT_LDFLAGS=$(LDFLAGS)

  ifeq ($(USE_SDL),1)
    # SDL3 comes from the toolchain (MSYS2's mingw-w64-i686-sdl3, or pkg-config
    # sdl3 for a cross compiler), as on every other platform: the in-tree copy
    # under code/libsdl was removed. Its DLL is copied beside the client when
    # pkg-config names where it is installed.
    BASE_CFLAGS += $(SDL_INCLUDE)
    CLIENT_LDFLAGS += $(SDL_LIBS)
    SDL_DLL ?= $(wildcard $(shell $(PKG_CONFIG) --silence-errors --variable=prefix sdl3)/bin/SDL3.dll)
    CLIENT_EXTRA_FILES += $(SDL_DLL)
  endif

  ifeq ($(USE_CURL),1)
    CLIENT_LDFLAGS += $(CURL_LIBS)
  endif

  ifeq ($(USE_SYSTEM_JPEG),1)
    CLIENT_LDFLAGS += $(JPEG_LIBS)
  endif

  ifeq ($(USE_OGG_VORBIS),1)
    BASE_CFLAGS += -DUSE_OGG_VORBIS $(OGG_FLAGS) $(VORBIS_FLAGS)
    CLIENT_LDFLAGS += $(OGG_LIBS) $(VORBIS_LIBS)
  endif

  DEBUG_CFLAGS = $(BASE_CFLAGS) -DDEBUG -D_DEBUG -g -O0
  RELEASE_CFLAGS = $(BASE_CFLAGS) -DNDEBUG $(OPTIMIZE)

else # !MINGW

ifeq ($(COMPILE_PLATFORM),darwin)

#############################################################################
# SETUP AND BUILD -- MACOS
#############################################################################

  BASE_CFLAGS += -Wall -Wimplicit -Wstrict-prototypes -pipe -pthread

  BASE_CFLAGS += -Wno-unused-result

  BASE_CFLAGS += -DMACOS_X

  OPTIMIZE = -O2 -fvisibility=hidden

  SHLIBEXT = dylib
  SHLIBCFLAGS = -fPIC -fvisibility=hidden
  SHLIBLDFLAGS = -dynamiclib $(LDFLAGS)

  ARCHEXT = .$(ARCH)

  LDFLAGS += -pthread

  ifeq ($(ARCH),x86_64)
    BASE_CFLAGS += -arch x86_64
    LDFLAGS += -arch x86_64
  endif
  ifeq ($(ARCH),aarch64)
    BASE_CFLAGS += -arch arm64
    LDFLAGS += -arch arm64
  endif

  ifneq ($(SDL_INCLUDE),)
    BASE_CFLAGS += $(SDL_INCLUDE)
    CLIENT_LDFLAGS = $(SDL_LIBS)
  else
    BASE_CFLAGS += -F/Library/Frameworks
    CLIENT_LDFLAGS += -F/Library/Frameworks -framework SDL3
  endif
  CLIENT_LDFLAGS += -framework AppKit

  ifeq ($(USE_SYSTEM_JPEG),1)
    CLIENT_LDFLAGS += $(JPEG_LIBS)
  endif

  ifeq ($(USE_OGG_VORBIS),1)
    BASE_CFLAGS += -DUSE_OGG_VORBIS $(OGG_FLAGS) $(VORBIS_FLAGS)
    CLIENT_LDFLAGS += $(OGG_LIBS) $(VORBIS_LIBS)
  endif

  DEBUG_CFLAGS = $(BASE_CFLAGS) -DDEBUG -D_DEBUG -g -O0
  RELEASE_CFLAGS = $(BASE_CFLAGS) -DNDEBUG $(OPTIMIZE)

else

#############################################################################
# SETUP AND BUILD -- *NIX PLATFORMS
#############################################################################

  BASE_CFLAGS += -Wall -Wimplicit -Wstrict-prototypes -pipe -pthread

  BASE_CFLAGS += -Wno-unused-result

  BASE_CFLAGS += -DUSE_ICON

  BASE_CFLAGS += -I/usr/include -I/usr/local/include

  OPTIMIZE = -O2 -fvisibility=hidden

  ifeq ($(ARCH),x86_64)
    ARCHEXT = .x64
  else
  ifeq ($(ARCH),x86)
    OPTIMIZE += -march=i586 -mtune=i686
  endif
  endif

  ifeq ($(ARCH),arm)
    ifeq ($(LONG_BIT),32)
      OPTIMIZE += -march=armv7-a+fp
    else
      OPTIMIZE += -march=armv7-a
    endif
    ARCHEXT = .arm
  endif

  ifeq ($(ARCH),aarch64)
    ARCHEXT = .aarch64
  endif

  ifeq ($(ARCH),ppc64le)
    ARCHEXT = .ppc64le
  endif

  ifeq ($(ARCH),ppc64)
    ARCHEXT = .ppc64
  endif

  SHLIBEXT = so
  SHLIBCFLAGS = -fPIC -fvisibility=hidden
  SHLIBLDFLAGS = -shared $(LDFLAGS)

  LDFLAGS += -lm -pthread
  LDFLAGS += -Wl,--gc-sections -fvisibility=hidden

  ifeq ($(USE_SDL),1)
    BASE_CFLAGS += $(SDL_INCLUDE)
    CLIENT_LDFLAGS = $(SDL_LIBS)
  else
    BASE_CFLAGS += $(X11_INCLUDE)
    CLIENT_LDFLAGS = $(X11_LIBS)
  endif

  ifeq ($(USE_SYSTEM_JPEG),1)
    CLIENT_LDFLAGS += $(JPEG_LIBS)
  endif

  ifeq ($(USE_CURL),1)
    ifeq ($(USE_CURL_DLOPEN),0)
      CLIENT_LDFLAGS += $(CURL_LIBS)
    endif
  endif

  ifeq ($(USE_OGG_VORBIS),1)
    BASE_CFLAGS += -DUSE_OGG_VORBIS $(OGG_FLAGS) $(VORBIS_FLAGS)
    CLIENT_LDFLAGS += $(OGG_LIBS) $(VORBIS_LIBS)
  endif

  ifeq ($(PLATFORM),linux)
    LDFLAGS += -ldl -Wl,--hash-style=both
  endif

  DEBUG_CFLAGS = $(BASE_CFLAGS) -DDEBUG -D_DEBUG -g -O0
  RELEASE_CFLAGS = $(BASE_CFLAGS) -DNDEBUG $(OPTIMIZE)

  DEBUG_LDFLAGS = -rdynamic

endif # *NIX platforms

endif # !MINGW

# Keep the target-selecting flags present even when callers set the customary
# build variables on GNU Make's command line. The central q_platform.h guard is
# the final compiler-level check if a wrapper ignores these arguments.
ZLIB_CFLAGS ?= $(shell pkg-config --cflags zlib 2>/dev/null)
ZLIB_LIBS ?= -lz
override BASE_CFLAGS += $(FNQL_REQUIRED_ARCH_CFLAGS) $(ZLIB_CFLAGS)

# FreeType rasterizes TrueType fonts and Quake Live's host text; Meson builds
# require it for clients. USE_FREETYPE=1 requires it here too, and
# USE_FREETYPE=0 builds without it, when fonts fall back to their pre-rendered
# glyph pages. Left unset, a client build uses FreeType when pkg-config's
# freetype2 (or plain -lfreetype) links a probe for this target, since a host
# library of another width is found as readily as the target's. The probe's
# result is exported, so the recursive build does not repeat it.
RENDERER_LIBS =
ifeq ($(BUILD_CLIENT),1)
  ifneq ($(USE_FREETYPE),0)
    ifneq ($(call bin_path, $(PKG_CONFIG)),)
      FREETYPE_CFLAGS ?= $(shell $(PKG_CONFIG) --silence-errors --cflags freetype2)
      FREETYPE_LIBS ?= $(shell $(PKG_CONFIG) --silence-errors --libs freetype2)
    endif
    ifeq ($(strip $(FREETYPE_LIBS)),)
      FREETYPE_LIBS = -lfreetype
    endif
    ifeq ($(origin USE_FREETYPE),undefined)
      ifeq ($(FNQL_REQUIRE_BUILD_INPUTS),1)
        FREETYPE_PROBE = $(BUILD_DIR)/freetype-probe$(BINEXT)
        USE_FREETYPE := $(shell $(MKDIR) $(BUILD_DIR) && printf '#include <ft2build.h>\n#include FT_FREETYPE_H\nint main(void) { FT_Library library; return FT_Init_FreeType(&library); }\n' | $(CC) $(FNQL_REQUIRED_ARCH_CFLAGS) $(FREETYPE_CFLAGS) -x c - -x none -o $(FREETYPE_PROBE) $(FNQL_REQUIRED_ARCH_LDFLAGS) $(FREETYPE_LIBS) >/dev/null 2>&1 && echo 1 || echo 0; rm -f $(FREETYPE_PROBE))
        export USE_FREETYPE
      endif
    endif
  endif
endif
ifeq ($(USE_FREETYPE),1)
  override BASE_CFLAGS += -DBUILD_FREETYPE $(FREETYPE_CFLAGS)
  RENDERER_LIBS += $(FREETYPE_LIBS)
endif
LDFLAGS += $(ZLIB_LIBS)
override CFLAGS += $(FNQL_REQUIRED_ARCH_CFLAGS)
override CXXFLAGS += $(FNQL_REQUIRED_ARCH_CFLAGS)
override LDFLAGS += $(FNQL_REQUIRED_ARCH_LDFLAGS)
override SHLIBLDFLAGS += $(FNQL_REQUIRED_ARCH_LDFLAGS)

DEBUG_CXXFLAGS = $(filter-out -Wimplicit -Wstrict-prototypes,$(DEBUG_CFLAGS)) -std=c++17
RELEASE_CXXFLAGS = $(filter-out -Wimplicit -Wstrict-prototypes,$(RELEASE_CFLAGS)) -std=c++17


TARGET_CLIENT = $(CNAME)$(ARCHEXT)$(BINEXT)

TARGET_RENDX = $(RENDERER_PREFIX)_glx_$(SHLIBNAME)
TARGET_RENDV = $(RENDERER_PREFIX)_vk_$(SHLIBNAME)
TARGET_RENDRTX = $(RENDERER_PREFIX)_rtx_$(SHLIBNAME)

TARGET_SERVER = $(DNAME)$(ARCHEXT)$(BINEXT)

PKG_ROOT = pkg
PKG_FILES = $(shell find $(PKG_ROOT) -type f 2>/dev/null)
ROOT_ARCHIVE = fntech3-pkg.fnz
FNQL_WEBPAK = fnql-web.pak
FNQL_WEBPAK_ROOT = code/client/webui
FNQL_WEBPAK_SOURCES = $(shell find $(FNQL_WEBPAK_ROOT) -type f 2>/dev/null)

TARGETS =

ifneq ($(BUILD_SERVER),0)
  TARGETS += $(B)/$(TARGET_SERVER)
endif

ifneq ($(BUILD_CLIENT),0)
  TARGETS += $(B)/$(TARGET_CLIENT)
  ifneq ($(USE_RENDERER_DLOPEN),0)
    ifeq ($(USE_GLX),1)
      TARGETS += $(B)/$(TARGET_RENDX)
    endif
    ifeq ($(USE_VK),1)
      TARGETS += $(B)/$(TARGET_RENDV)
    endif
    ifeq ($(USE_RTX),1)
      TARGETS += $(B)/$(TARGET_RENDRTX)
    endif
  endif
endif

RECURSIVE_MAKE = $(MAKE)
ifdef MINGW
  MAKE_PATH := $(subst ",,$(subst \,/,$(MAKE)))
  ifneq ($(findstring :,$(MAKE_PATH)),)
    # Old MSYS make launched from cmd.exe may record $(MAKE) as a Windows path
    # that /bin/sh can't recurse into reliably. Fall back to the PATH-resolved
    # executable name for recursive invocations.
    RECURSIVE_MAKE = $(notdir $(MAKE_PATH))
  endif
endif

ifeq ($(USE_CCACHE),1)
  CC := ccache $(CC)
  CXX := ccache $(CXX)
endif

ifneq ($(USE_RENDERER_DLOPEN),0)
    RENDCFLAGS=$(SHLIBCFLAGS)
else
    RENDCFLAGS=$(NOTSHLIBCFLAGS)
endif

define DO_CC
$(echo_cmd) "CC $<"
$(Q)$(CC) $(CFLAGS) -o $@ -c $<
endef

define DO_CC_QVM
$(echo_cmd) "CC_QVM $<"
$(Q)$(CC) $(CFLAGS) -fno-fast-math -o $@ -c $<
endef

define DO_CXX
$(echo_cmd) "CXX $<"
$(Q)$(CXX) $(CXXFLAGS) $(AUDIO_FP_CFLAGS) -o $@ -c $<
endef

# RTCW module fade values and PCM gain conversion require IEEE semantics.
AUDIO_FP_CFLAGS = $(if $(filter snd_main.cpp snd_dma.cpp snd_mix.cpp AudioSystem.cpp,$(notdir $<)),-fno-fast-math)

define DO_DED_CXX
$(echo_cmd) "DED_CXX $<"
$(Q)$(CXX) $(CXXFLAGS) -DDEDICATED -o $@ -c $<
endef

# Medal of Honor's TIKI, skeletor and alias runtime keeps OpenMoHAA's upstream
# names and headers (see code/mohaa/README.md). Its sources include "q_shared.h"
# and "qcommon.h" by bare name, so code/mohaa/qcommon leads the include path.
# The client and the dedicated server link the same objects, as meson does.
MOHAA_WFLAGS = -Wno-unknown-warning-option -Wno-sign-compare -Wno-unused-variable \
  -Wno-unused-but-set-variable -Wno-unused-function -Wno-unused-value \
  -Wno-maybe-uninitialized -Wno-parentheses -Wno-format-truncation \
  -Wno-stringop-truncation -Wno-char-subscripts
# Upstream type-puns floats (SkelQuat's Slerp), so it is built without strict
# aliasing, as meson builds every source.
MOHAA_CFLAGS = -I$(MOHAADIR)/qcommon -I$(MOUNT_DIR) -fno-strict-aliasing $(MOHAA_WFLAGS)
MOHAA_CXX_WFLAGS = -Wno-class-memaccess -Wno-reorder -Wno-misleading-indentation

define DO_MOHAA_CC
$(echo_cmd) "MOHAA_CC $<"
$(Q)$(CC) $(CFLAGS) $(MOHAA_CFLAGS) -Wno-strict-prototypes -Wno-pointer-sign -o $@ -c $<
endef

define DO_MOHAA_CXX
$(echo_cmd) "MOHAA_CXX $<"
$(Q)$(CXX) $(CXXFLAGS) $(MOHAA_CFLAGS) $(MOHAA_CXX_WFLAGS) -o $@ -c $<
endef

# RTCW module light parameters and projected vertices may be non-finite.
# These boundary files require IEEE arithmetic even in legacy fast-math builds.
RENDERER_FP_CFLAGS = $(if $(filter tr_scene.c tr_shade.c,$(notdir $<)),-fno-fast-math)

define DO_REND_CC
$(echo_cmd) "REND_CC $<"
$(Q)$(CC) $(CFLAGS) $(RENDCFLAGS) $(RENDERER_FP_CFLAGS) -o $@ -c $<
endef

define DO_GLX_REND_CC
$(echo_cmd) "GLX_REND_CC $<"
$(Q)$(CC) $(CFLAGS) $(RENDCFLAGS) $(GLX_RENDCFLAGS) $(RENDERER_FP_CFLAGS) -o $@ -c $<
endef

define DO_GLX_REND_CXX
$(echo_cmd) "GLX_REND_CXX $<"
$(Q)$(CXX) $(CXXFLAGS) $(RENDCFLAGS) $(GLX_RENDCFLAGS) -o $@ -c $<
endef

define DO_BOT_CC
$(echo_cmd) "BOT_CC $<"
$(Q)$(CC) $(CFLAGS) $(BOTCFLAGS) -DBOTLIB -o $@ -c $<
endef

define DO_RENDV_CC
$(echo_cmd) "RENDV_CC $<"
$(Q)$(CC) $(CFLAGS) $(RENDCFLAGS) $(VK_RENDCFLAGS) $(RENDERER_FP_CFLAGS) -o $@ -c $<
endef

GLX_RENDCFLAGS = -DRENDERER_GLX
VK_RENDCFLAGS = -DRENDERER_VULKAN
RTX_RENDCFLAGS = -DRENDERER_RTX
ifeq ($(USE_RENDERER_DLOPEN),0)
  # Static clients also own fallback host-font wrappers under these RE_* names.
  # Keep the renderer implementations private; GetRefAPI remains the ABI.
  GLX_RENDCFLAGS += \
    -DRE_DrawScaledText=R_GLX_DrawScaledText \
    -DRE_MeasureScaledText=R_GLX_MeasureScaledText
  VK_RENDCFLAGS += \
    -DRE_DrawScaledText=R_VK_DrawScaledText \
    -DRE_MeasureScaledText=R_VK_MeasureScaledText
  RTX_RENDCFLAGS += \
    -DRE_DrawScaledText=R_RTX_DrawScaledText \
    -DRE_MeasureScaledText=R_RTX_MeasureScaledText
endif

define DO_RENDRTX_CC
$(echo_cmd) "RENDRTX_CC $<"
$(Q)$(CC) $(CFLAGS) $(RENDCFLAGS) $(RTX_RENDCFLAGS) $(RENDERER_FP_CFLAGS) -o $@ -c $<
endef

define DO_RENDV_CXX
$(echo_cmd) "RENDV_CXX $<"
$(Q)$(CXX) $(CXXFLAGS) $(RENDCFLAGS) $(VK_RENDCFLAGS) -o $@ -c $<
endef

define DO_RENDRTX_CXX
$(echo_cmd) "RENDRTX_CXX $<"
$(Q)$(CXX) $(CXXFLAGS) $(RENDCFLAGS) $(RTX_RENDCFLAGS) -o $@ -c $<
endef

define DO_BOT_CXX
$(echo_cmd) "BOT_CXX $<"
$(Q)$(CXX) $(CXXFLAGS) $(BOTCFLAGS) -DBOTLIB -o $@ -c $<
endef

define DO_AS
$(echo_cmd) "AS $<"
$(Q)$(CC) $(CFLAGS) -DELF -x assembler-with-cpp -o $@ -c $<
endef

define DO_DED_CC
$(echo_cmd) "DED_CC $<"
$(Q)$(CC) $(CFLAGS) -DDEDICATED -o $@ -c $<
endef

define DO_DED_CC_QVM
$(echo_cmd) "DED_CC_QVM $<"
$(Q)$(CC) $(CFLAGS) -fno-fast-math -DDEDICATED -o $@ -c $<
endef

define DO_WINDRES
$(echo_cmd) "WINDRES $<"
$(Q)$(WINDRES) -i $< -o $@
endef

ifndef SHLIBNAME
  SHLIBNAME=$(ARCH).$(SHLIBEXT)
endif

#############################################################################
# MAIN TARGETS
#############################################################################

default: release
all: debug release

debug:
	+@B="$(BD)" CFLAGS="$(CFLAGS) $(DEBUG_CFLAGS)" CXXFLAGS="$(CXXFLAGS) $(DEBUG_CXXFLAGS)" LDFLAGS="$(LDFLAGS) $(DEBUG_LDFLAGS)" $(RECURSIVE_MAKE) targets V=$(V)

release:
	+@B="$(BR)" CFLAGS="$(CFLAGS) $(RELEASE_CFLAGS)" CXXFLAGS="$(CXXFLAGS) $(RELEASE_CXXFLAGS)" $(RECURSIVE_MAKE) targets V=$(V)

define ADD_COPY_TARGET
TARGETS += $2
$2: $1
	$(echo_cmd) "CP $$<"
	@cp $1 $2
endef

# These functions allow us to generate rules for copying a list of files
# into the base directory of the build; this is useful for bundling libs,
# README files or whatever else
define GENERATE_COPY_TARGETS
$(foreach FILE,$1, \
  $(eval $(call ADD_COPY_TARGET, \
    $(FILE), \
    $(addprefix $(B)/,$(notdir $(FILE))))))
endef

ifneq ($(BUILD_CLIENT),0)
  $(call GENERATE_COPY_TARGETS,$(CLIENT_EXTRA_FILES))
endif

# Create the build directories and tools, print out
# an informational message, then start building
targets: makedirs tools
	@echo ""
	@echo "Building quake3 in $(B):"
	@echo ""
	@echo "  VERSION: $(VERSION)"
	@echo "  PLATFORM: $(PLATFORM)"
	@echo "  ARCH: $(ARCH)"
	@echo "  COMPILE_PLATFORM: $(COMPILE_PLATFORM)"
	@echo "  COMPILE_ARCH: $(COMPILE_ARCH)"
ifdef MINGW
	@echo "  WINDRES: $(WINDRES)"
	@echo "  STRIP: $(STRIP)"
endif
	@echo "  CC: $(CC)"
	@echo "  CXX: $(CXX)"
	@echo ""
	@echo "  CFLAGS:"
	@for i in $(CFLAGS); \
	do \
		echo "    $$i"; \
	done
	@echo ""
	@echo "  CXXFLAGS:"
	@for i in $(CXXFLAGS); \
	do \
		echo "    $$i"; \
	done
	@echo ""
	@echo "  Output:"
	@for i in $(TARGETS); \
	do \
		echo "    $$i"; \
	done
	@echo "    $(B)/$(ROOT_ARCHIVE)"
	@echo "    $(B)/$(FNQL_WEBPAK)"
	@echo ""
	+@$(RECURSIVE_MAKE) $(TARGETS) $(B)/$(ROOT_ARCHIVE) $(B)/$(FNQL_WEBPAK) V=$(V)

makedirs:
	@if [ ! -d $(BUILD_DIR) ];then $(MKDIR) $(BUILD_DIR);fi
	@if [ ! -d $(B) ];then $(MKDIR) $(B);fi
	@if [ ! -d $(B)/client ];then $(MKDIR) $(B)/client/qvm;fi
	@if [ ! -d $(B)/rendx ];then $(MKDIR) $(B)/rendx;fi
	@if [ ! -d $(B)/rendv ];then $(MKDIR) $(B)/rendv;fi
	@if [ ! -d $(B)/rendrtx ];then $(MKDIR) $(B)/rendrtx;fi
	@if [ ! -d $(B)/mohaa ];then $(MKDIR) $(B)/mohaa;fi
ifneq ($(BUILD_SERVER),0)
	@if [ ! -d $(B)/ded ];then $(MKDIR) $(B)/ded/qvm;fi
endif

#############################################################################
# CLIENT/SERVER
#############################################################################

Q3RENDXOBJ = \
  $(B)/rendx/tr_animation.o \
  $(B)/rendx/tr_alice.o \
  $(B)/rendx/tr_alice_tiki.o \
  $(B)/rendx/tr_mohaa.o \
  $(B)/rendx/tr_mohaa_world.o \
  $(B)/rendx/tr_arb.o \
  $(B)/rendx/tr_backend.o \
  $(B)/rendx/tr_bsp.o \
  $(B)/rendx/tr_cmds.o \
  $(B)/rendx/tr_curve.o \
  $(B)/rendx/tr_flares.o \
  $(B)/rendx/tr_font.o \
  $(B)/rendx/tr_font_stash.o \
  $(B)/rendx/tr_image.o \
  $(B)/rendx/tr_image_png.o \
  $(B)/rendx/tr_image_jpg.o \
  $(B)/rendx/tr_image_bmp.o \
  $(B)/rendx/tr_image_tga.o \
  $(B)/rendx/tr_image_pcx.o \
  $(B)/rendx/tr_image_ftx.o \
  $(B)/rendx/tr_image_wal.o \
  $(B)/rendx/tr_init.o \
  $(B)/rendx/tr_light.o \
  $(B)/rendx/tr_main.o \
  $(B)/rendx/tr_marks.o \
  $(B)/rendx/tr_mesh.o \
  $(B)/rendx/tr_model.o \
  $(B)/rendx/tr_model_iqm.o \
  $(B)/rendx/tr_noise.o \
  $(B)/rendx/tr_scene.o \
  $(B)/rendx/tr_shade.o \
  $(B)/rendx/tr_shade_calc.o \
  $(B)/rendx/tr_shader.o \
  $(B)/rendx/tr_shadows.o \
  $(B)/rendx/tr_sky.o \
  $(B)/rendx/tr_surface.o \
  $(B)/rendx/tr_vbo.o \
  $(B)/rendx/tr_world.o \
  $(B)/rendx/glx_caps.o \
  $(B)/rendx/glx_debug.o \
  $(B)/rendx/glx_draw.o \
  $(B)/rendx/glx_executor.o \
  $(B)/rendx/glx_material.o \
  $(B)/rendx/glx_module.o \
  $(B)/rendx/glx_post_shader.o \
  $(B)/rendx/glx_postprocess.o \
  $(B)/rendx/glx_profiler.o \
  $(B)/rendx/glx_static_world.o \
  $(B)/rendx/glx_stream.o

ifneq ($(USE_RENDERER_DLOPEN), 0)
  Q3RENDXOBJ += \
    $(B)/rendx/q_shared.o \
    $(B)/rendx/puff.o \
    $(B)/rendx/q_math.o
endif

Q3RENDVOBJ = \
  $(B)/rendv/tr_animation.o \
  $(B)/rendv/tr_alice.o \
  $(B)/rendv/tr_alice_tiki.o \
  $(B)/rendv/tr_mohaa.o \
  $(B)/rendv/tr_mohaa_world.o \
  $(B)/rendv/tr_backend.o \
  $(B)/rendv/tr_bsp.o \
  $(B)/rendv/tr_cmds.o \
  $(B)/rendv/tr_curve.o \
  $(B)/rendv/tr_font.o \
  $(B)/rendv/tr_font_stash.o \
  $(B)/rendv/tr_image.o \
  $(B)/rendv/tr_image_png.o \
  $(B)/rendv/tr_image_jpg.o \
  $(B)/rendv/tr_image_bmp.o \
  $(B)/rendv/tr_image_tga.o \
  $(B)/rendv/tr_image_pcx.o \
  $(B)/rendv/tr_image_ftx.o \
  $(B)/rendv/tr_image_wal.o \
  $(B)/rendv/tr_init.o \
  $(B)/rendv/tr_light.o \
  $(B)/rendv/tr_main.o \
  $(B)/rendv/tr_marks.o \
  $(B)/rendv/tr_mesh.o \
  $(B)/rendv/tr_model.o \
  $(B)/rendv/tr_model_iqm.o \
  $(B)/rendv/tr_noise.o \
  $(B)/rendv/tr_scene.o \
  $(B)/rendv/tr_shade.o \
  $(B)/rendv/tr_shade_calc.o \
  $(B)/rendv/tr_shader.o \
  $(B)/rendv/tr_shadows.o \
  $(B)/rendv/tr_sky.o \
  $(B)/rendv/tr_surface.o \
  $(B)/rendv/tr_world.o \
  $(B)/rendv/vk.o \
  $(B)/rendv/vk_flares.o \
  $(B)/rendv/vk_vbo.o \

ifneq ($(USE_RENDERER_DLOPEN), 0)
  Q3RENDVOBJ += \
    $(B)/rendv/q_shared.o \
    $(B)/rendv/puff.o \
    $(B)/rendv/q_math.o
endif

Q3RENDRTXOBJ = $(patsubst $(B)/rendv/%,$(B)/rendrtx/%,$(Q3RENDVOBJ))

MOHAAOBJ = \
  $(B)/mohaa/con_set.o \
  $(B)/mohaa/mem_blockalloc.o \
  $(B)/mohaa/mem_tempalloc.o \
  $(B)/mohaa/str.o \
  $(B)/mohaa/tiki_main.o \
  $(B)/mohaa/tiki_script.o \
  $(B)/mohaa/alias.o \
  $(B)/mohaa/mohaa_q_shared.o \
  $(B)/mohaa/bonetable.o \
  $(B)/mohaa/skeletor.o \
  $(B)/mohaa/skeletor_imports.o \
  $(B)/mohaa/skeletor_loadanimation.o \
  $(B)/mohaa/skeletor_model_files.o \
  $(B)/mohaa/skeletor_utilities.o \
  $(B)/mohaa/skeletorbones.o \
  $(B)/mohaa/tokenizer.o \
  $(B)/mohaa/tiki_anim.o \
  $(B)/mohaa/tiki_cache.o \
  $(B)/mohaa/tiki_commands.o \
  $(B)/mohaa/tiki_files.o \
  $(B)/mohaa/tiki_frame.o \
  $(B)/mohaa/tiki_imports.o \
  $(B)/mohaa/tiki_mesh.o \
  $(B)/mohaa/tiki_parse.o \
  $(B)/mohaa/tiki_skel.o \
  $(B)/mohaa/tiki_surface.o \
  $(B)/mohaa/tiki_tag.o \
  $(B)/mohaa/tiki_utility.o \
  $(B)/mohaa/mohaa_engine.o \
  $(B)/mohaa/mohaa_system86.o \
  $(B)/mohaa/mohaa_render_api.o \
  $(B)/mohaa/mohaa_tiki_api.o

Q3OBJ = \
  $(B)/client/cl_cgame.o \
  $(B)/client/cl_hud.o \
  $(B)/client/cl_cin.o \
  $(B)/client/cl_console.o \
  $(B)/client/cl_input.o \
  $(B)/client/cl_keys.o \
  $(B)/client/cl_main.o \
	$(B)/client/cl_workshop.o \
  $(B)/client/cl_net_chan.o \
  $(B)/client/cl_parse.o \
  $(B)/client/cl_scrn.o \
  $(B)/client/cl_ui.o \
  $(B)/client/cl_webpak.o \
  $(B)/client/cl_webui.o \
  $(B)/client/awesomium_backend_win32.o \
  $(B)/client/cl_avi.o \
  $(B)/client/cl_jpeg.o \
  $(B)/client/cl_alice.o \
  $(B)/client/cl_alice_font.o \
  $(B)/client/cl_alice_hud.o \
  $(B)/client/cl_alice_loading.o \
  $(B)/client/cl_alice_saveshot.o \
  $(B)/client/cl_alice_screen.o \
  $(B)/client/cl_alice_ui.o \
  $(B)/client/cl_fakk2_inventory.o \
  $(B)/client/cl_mohaa.o \
  $(B)/client/cl_mohaa_cgame.o \
  $(B)/client/cl_mohaa_decals.o \
  $(B)/client/cl_mohaa_sound.o \
  $(B)/client/cl_mohaa_ui.o \
  $(B)/client/cl_q3test.o \
  $(B)/client/cl_q3test_cgame.o \
  $(B)/client/cl_q3test_menus.o \
  $(B)/client/cl_q3test_ui.o \
  \
  $(B)/client/cm_load.o \
  $(B)/client/cm_mohaa_terrain.o \
  $(B)/client/cm_mohaa_lights.o \
  $(B)/client/cm_patch.o \
  $(B)/client/cm_polylib.o \
  $(B)/client/cm_test.o \
  $(B)/client/cm_trace.o \
  \
	$(B)/client/fnql_steam.o \
	$(B)/client/fnql_workshop.o \
	$(B)/client/process_relaunch.o \
	\
  $(B)/client/cmd.o \
  $(B)/client/common.o \
	$(B)/client/game_abi_adapter.o \
  $(B)/client/cvar.o \
  $(B)/client/files.o \
	$(B)/client/game_profile.o \
  $(B)/client/alice_alias.o \
  $(B)/client/alice_lip.o \
  $(B)/client/alice_runtime.o \
  $(B)/client/alice_tiki.o \
  $(B)/client/com_alice_ui.o \
  $(B)/client/native_seh.o \
  $(B)/client/history.o \
  $(B)/client/keys.o \
  $(B)/client/md4.o \
  $(B)/client/md5.o \
  $(B)/client/msg.o \
  $(B)/client/mohaa_msg.o \
  $(MOHAAOBJ) \
  $(B)/client/net_chan.o \
  $(B)/client/net_ip.o \
  $(B)/client/huffman.o \
  $(B)/client/huffman_static.o \
  \
  $(B)/client/snd_adpcm.o \
  $(B)/client/AudioSystem.o \
  $(B)/client/snd_dma.o \
  $(B)/client/snd_mem.o \
  $(B)/client/snd_mix.o \
  $(B)/client/snd_wavelet.o \
  \
  $(B)/client/snd_main.o \
  $(B)/client/snd_codec.o \
  $(B)/client/snd_codec_wav.o \
  $(B)/client/snd_alice.o \
  \
  $(B)/client/factory_catalog.o \
  $(B)/client/factory_rotation.o \
	$(B)/client/server_game_data.o \
  $(B)/client/sv_bot.o \
  $(B)/client/sv_ccmds.o \
  $(B)/client/sv_client.o \
  $(B)/client/sv_filter.o \
  $(B)/client/sv_game.o \
  $(B)/client/sv_init.o \
  $(B)/client/sv_main.o \
  $(B)/client/sv_net_chan.o \
  $(B)/client/sv_platform.o \
  $(B)/client/sv_factory.o \
  $(B)/client/sv_snapshot.o \
  $(B)/client/sv_world.o \
  $(B)/client/sv_zmq.o \
  \
  $(B)/client/q_math.o \
  $(B)/client/q_shared.o \
  \
  $(B)/client/unzip.o \
  $(B)/client/puff.o \
  \
  $(B)/client/be_aas_bspq3.o \
  $(B)/client/be_aas_cluster.o \
  $(B)/client/be_aas_debug.o \
  $(B)/client/be_aas_entity.o \
  $(B)/client/be_aas_file.o \
  $(B)/client/be_aas_main.o \
  $(B)/client/be_aas_move.o \
  $(B)/client/be_aas_optimize.o \
  $(B)/client/be_aas_reach.o \
  $(B)/client/be_aas_route.o \
  $(B)/client/be_aas_routealt.o \
  $(B)/client/be_aas_sample.o \
  $(B)/client/be_ai_char.o \
  $(B)/client/be_ai_chat.o \
  $(B)/client/be_ai_gen.o \
  $(B)/client/be_ai_goal.o \
  $(B)/client/be_ai_move.o \
  $(B)/client/be_ai_weap.o \
  $(B)/client/be_ai_weight.o \
  $(B)/client/be_ea.o \
  $(B)/client/be_interface.o \
  $(B)/client/l_crc.o \
  $(B)/client/l_libvar.o \
  $(B)/client/l_log.o \
  $(B)/client/l_memory.o \
  $(B)/client/l_precomp.o \
  $(B)/client/l_script.o \
  $(B)/client/l_struct.o

ifeq ($(USE_OGG_VORBIS),1)
  Q3OBJ += $(B)/client/snd_codec_ogg.o
endif

ifeq ($(USE_CODEC_MP3),1)
  Q3OBJ += $(B)/client/snd_codec_mp3.o
endif

ifneq ($(USE_RENDERER_DLOPEN),1)
  ifeq ($(USE_GLX),1)
    Q3OBJ += $(Q3RENDXOBJ)
  else
    ifeq ($(USE_RTX),1)
      Q3OBJ += $(Q3RENDRTXOBJ)
    else
      ifeq ($(USE_VK),1)
        Q3OBJ += $(Q3RENDVOBJ)
      endif
    endif
  endif
endif

ifeq ($(ARCH),x86)
ifndef MINGW
  Q3OBJ += \
    $(B)/client/snd_mix_mmx.o \
    $(B)/client/snd_mix_sse.o
endif
endif

ifeq ($(ARCH),x86_64)
  Q3OBJ += \
    $(B)/client/snd_mix_x86_64.o
endif

Q3OBJ += \
  $(B)/client/qvm/vm.o \
  $(B)/client/qvm/vm_interpreted.o

ifeq ($(HAVE_VM_COMPILED),true)
  ifeq ($(ARCH),x86)
    Q3OBJ += $(B)/client/qvm/vm_x86.o
  endif
  ifeq ($(ARCH),x86_64)
    Q3OBJ += $(B)/client/qvm/vm_x86.o
  endif
  ifeq ($(ARCH),arm)
    Q3OBJ += $(B)/client/qvm/vm_armv7l.o
  endif
  ifeq ($(ARCH),aarch64)
    Q3OBJ += $(B)/client/qvm/vm_aarch64.o
  endif
  ifeq ($(ARCH),ppc64le)
    Q3OBJ += $(B)/client/qvm/vm_powerpc.o
  endif
  ifeq ($(ARCH),ppc64)
    Q3OBJ += $(B)/client/qvm/vm_powerpc.o
  endif
endif

ifeq ($(USE_CURL),1)
  Q3OBJ += $(B)/client/cl_curl.o
endif

ifdef MINGW

  Q3OBJ += \
    $(B)/client/win_main.o \
    $(B)/client/win_shared.o \
    $(B)/client/win_syscon.o \
    $(B)/client/win_client_resource.o

ifeq ($(USE_SDL),1)
    Q3OBJ += \
        $(B)/client/sdl_glimp.o \
        $(B)/client/sdl_gamma.o \
        $(B)/client/sdl_input.o \
        $(B)/client/sdl_snd.o
else # !USE_SDL
    Q3OBJ += \
        $(B)/client/win_gamma.o \
        $(B)/client/win_glimp.o \
        $(B)/client/win_input.o \
        $(B)/client/win_minimize.o \
        $(B)/client/win_snd.o \
        $(B)/client/win_wndproc.o

ifeq ($(USE_OPENGL_API),1)
    Q3OBJ += \
        $(B)/client/win_qgl.o
endif

ifeq ($(USE_VULKAN_API),1)
    Q3OBJ += \
        $(B)/client/win_qvk.o
endif
endif # !USE_SDL

else # !MINGW

  Q3OBJ += \
    $(B)/client/unix_syscon.o \
    $(B)/client/unix_main.o \
    $(B)/client/unix_shared.o \
    $(B)/client/linux_signals.o \
    $(B)/client/game_icon_data.o

ifeq ($(USE_SDL),1)
    Q3OBJ += \
        $(B)/client/sdl_glimp.o \
        $(B)/client/sdl_gamma.o \
        $(B)/client/sdl_input.o \
        $(B)/client/sdl_snd.o
ifeq ($(COMPILE_PLATFORM),darwin)
    Q3OBJ += $(B)/client/sdl_macos_window.o
endif
else # !USE_SDL
    Q3OBJ += \
        $(B)/client/linux_glimp.o \
        $(B)/client/linux_snd.o \
        $(B)/client/x11_dga.o \
        $(B)/client/x11_randr.o \
        $(B)/client/x11_vidmode.o
ifeq ($(PLATFORM),linux)
    Q3OBJ += $(B)/client/linux_joystick.o
    $(B)/client/linux_glimp.o $(B)/client/linux_joystick.o: CXXFLAGS += -DUSE_JOYSTICK
endif
ifeq ($(USE_OPENGL_API),1)
    Q3OBJ += \
        $(B)/client/linux_qgl.o
endif
ifeq ($(USE_VULKAN_API),1)
    Q3OBJ += \
        $(B)/client/linux_qvk.o
endif
endif # !USE_SDL

endif # !MINGW

# client binary

ifneq ($(USE_RENDERER_DLOPEN),1)
  # The renderer's objects are linked into the client.
  CLIENT_RENDERER_LIBS = $(RENDERER_LIBS)
endif

$(B)/$(TARGET_CLIENT): $(Q3OBJ)
	$(echo_cmd) "LD $@"
	$(Q)$(CXX) -o $@ $(Q3OBJ) $(CLIENT_LDFLAGS) $(CLIENT_RENDERER_LIBS) $(LDFLAGS)

# modular renderers

$(B)/$(TARGET_RENDX): $(Q3RENDXOBJ)
	$(echo_cmd) "LD $@"
	$(Q)$(CXX) -o $@ $(Q3RENDXOBJ) $(SHLIBCFLAGS) $(SHLIBLDFLAGS) $(RENDERER_LIBS)

$(B)/$(TARGET_RENDV): $(Q3RENDVOBJ)
	$(echo_cmd) "LD $@"
	$(Q)$(CXX) -o $@ $(Q3RENDVOBJ) $(SHLIBCFLAGS) $(SHLIBLDFLAGS) $(RENDERER_LIBS)

$(B)/$(TARGET_RENDRTX): $(Q3RENDRTXOBJ)
	$(echo_cmd) "LD $@"
	$(Q)$(CXX) -o $@ $(Q3RENDRTXOBJ) $(SHLIBCFLAGS) $(SHLIBLDFLAGS) $(RENDERER_LIBS)

#############################################################################
# DEDICATED SERVER
#############################################################################

Q3DOBJ = \
  $(B)/ded/factory_catalog.o \
  $(B)/ded/factory_rotation.o \
	$(B)/ded/server_game_data.o \
  $(B)/ded/sv_bot.o \
  $(B)/ded/sv_client.o \
  $(B)/ded/sv_ccmds.o \
  $(B)/ded/sv_filter.o \
  $(B)/ded/sv_game.o \
  $(B)/ded/sv_init.o \
  $(B)/ded/sv_main.o \
  $(B)/ded/sv_net_chan.o \
  $(B)/ded/sv_platform.o \
  $(B)/ded/sv_factory.o \
  $(B)/ded/sv_snapshot.o \
  $(B)/ded/sv_world.o \
  $(B)/ded/sv_zmq.o \
  \
  $(B)/ded/cm_load.o \
  $(B)/ded/cm_mohaa_terrain.o \
  $(B)/ded/cm_mohaa_lights.o \
  $(B)/ded/cm_patch.o \
  $(B)/ded/cm_polylib.o \
  $(B)/ded/cm_test.o \
  $(B)/ded/cm_trace.o \
	$(B)/ded/fnql_steam.o \
	$(B)/ded/fnql_workshop.o \
	$(B)/ded/process_relaunch.o \
  $(B)/ded/cmd.o \
  $(B)/ded/common.o \
	$(B)/ded/game_abi_adapter.o \
  $(B)/ded/cvar.o \
  $(B)/ded/files.o \
	$(B)/ded/game_profile.o \
  $(B)/ded/alice_alias.o \
  $(B)/ded/alice_lip.o \
  $(B)/ded/alice_runtime.o \
  $(B)/ded/alice_tiki.o \
  $(B)/ded/com_alice_ui.o \
  $(B)/ded/native_seh.o \
  $(B)/ded/history.o \
  $(B)/ded/keys.o \
  $(B)/ded/md4.o \
  $(B)/ded/md5.o \
  $(B)/ded/msg.o \
  $(B)/ded/mohaa_msg.o \
  $(MOHAAOBJ) \
  $(B)/ded/net_chan.o \
  $(B)/ded/net_ip.o \
  $(B)/ded/huffman.o \
  $(B)/ded/huffman_static.o \
  \
  $(B)/ded/q_math.o \
  $(B)/ded/q_shared.o \
  \
  $(B)/ded/unzip.o \
  \
  $(B)/ded/be_aas_bspq3.o \
  $(B)/ded/be_aas_cluster.o \
  $(B)/ded/be_aas_debug.o \
  $(B)/ded/be_aas_entity.o \
  $(B)/ded/be_aas_file.o \
  $(B)/ded/be_aas_main.o \
  $(B)/ded/be_aas_move.o \
  $(B)/ded/be_aas_optimize.o \
  $(B)/ded/be_aas_reach.o \
  $(B)/ded/be_aas_route.o \
  $(B)/ded/be_aas_routealt.o \
  $(B)/ded/be_aas_sample.o \
  $(B)/ded/be_ai_char.o \
  $(B)/ded/be_ai_chat.o \
  $(B)/ded/be_ai_gen.o \
  $(B)/ded/be_ai_goal.o \
  $(B)/ded/be_ai_move.o \
  $(B)/ded/be_ai_weap.o \
  $(B)/ded/be_ai_weight.o \
  $(B)/ded/be_ea.o \
  $(B)/ded/be_interface.o \
  $(B)/ded/l_crc.o \
  $(B)/ded/l_libvar.o \
  $(B)/ded/l_log.o \
  $(B)/ded/l_memory.o \
  $(B)/ded/l_precomp.o \
  $(B)/ded/l_script.o \
  $(B)/ded/l_struct.o

ifdef MINGW
  Q3DOBJ += \
  $(B)/ded/win_main.o \
  $(B)/client/win_resource.o \
  $(B)/ded/win_shared.o \
  $(B)/ded/win_syscon.o
else
  Q3DOBJ += \
  $(B)/ded/linux_signals.o \
  $(B)/ded/unix_syscon.o \
  $(B)/ded/unix_main.o \
  $(B)/ded/unix_shared.o
endif

  Q3DOBJ += \
  $(B)/ded/qvm/vm.o \
  $(B)/ded/qvm/vm_interpreted.o

ifeq ($(HAVE_VM_COMPILED),true)
  ifeq ($(ARCH),x86)
    Q3DOBJ += $(B)/ded/qvm/vm_x86.o
  endif
  ifeq ($(ARCH),x86_64)
    Q3DOBJ += $(B)/ded/qvm/vm_x86.o
  endif
  ifeq ($(ARCH),arm)
    Q3DOBJ += $(B)/ded/qvm/vm_armv7l.o
  endif
  ifeq ($(ARCH),aarch64)
    Q3DOBJ += $(B)/ded/qvm/vm_aarch64.o
  endif
  ifeq ($(ARCH),ppc64le)
    Q3DOBJ += $(B)/ded/qvm/vm_powerpc.o
  endif
  ifeq ($(ARCH),ppc64)
    Q3DOBJ += $(B)/ded/qvm/vm_powerpc.o
  endif
endif

$(B)/$(TARGET_SERVER): $(Q3DOBJ)
	$(echo_cmd) $(Q3DOBJ)
	$(echo_cmd) "LD $@"
	$(Q)$(CXX) -o $@ $(Q3DOBJ) $(LDFLAGS)

#############################################################################
## CLIENT/SERVER RULES
#############################################################################

$(B)/client/%.o: $(ADIR)/%.s
	$(DO_AS)

$(B)/client/%.o: $(CDIR)/%.c
	$(DO_CC)

$(B)/client/%.o: $(CDIR)/%.cpp
	$(DO_CXX)

$(B)/client/%.o: $(CLIENTAUDIODIR)/%.cpp
	$(DO_CXX)

$(B)/client/%.o: $(CLIENTAUDIOCODECSDIR)/%.cpp
	$(DO_CXX)

$(B)/client/%.o: $(CLIENTAUDIOLEGACYDIR)/%.cpp
	$(DO_CXX)

$(B)/client/%.o: $(CLIENTAUDIOOPENALDIR)/%.cpp
	$(DO_CXX)

$(B)/client/%.o: $(SDIR)/%.c
	$(DO_CC)

$(B)/client/%.o: $(SDIR)/%.cpp
	$(DO_CXX)

$(B)/client/%.o: $(CMDIR)/%.c
	$(DO_CC)

$(B)/client/%.o: $(CMDIR)/%.cpp
	$(DO_CXX)

$(B)/client/%.o: $(COMPATDIR)/%.c
	$(DO_CC)

$(B)/mohaa/%.o: $(MOHAADIR)/%.cpp
	$(DO_MOHAA_CXX)

$(B)/mohaa/%.o: $(MOHAADIR)/corepp/%.cpp
	$(DO_MOHAA_CXX)

$(B)/mohaa/%.o: $(MOHAADIR)/qcommon/%.c
	$(DO_MOHAA_CC)

$(B)/mohaa/%.o: $(MOHAADIR)/skeletor/%.cpp
	$(DO_MOHAA_CXX)

$(B)/mohaa/%.o: $(MOHAADIR)/tiki/%.cpp
	$(DO_MOHAA_CXX)

$(B)/client/%.o: $(PDIR)/%.cpp
	$(DO_CXX)

$(B)/client/qvm/%.o: $(CMDIR)/%.c
	$(DO_CC_QVM)

$(B)/client/%.o: $(BLIBDIR)/%.c
	$(DO_BOT_CC)

$(B)/client/%.o: $(BLIBDIR)/%.cpp
	$(DO_BOT_CXX)

$(B)/client/%.o: $(SDLDIR)/%.c
	$(DO_CC)

$(B)/client/%.o: $(SDLDIR)/%.cpp
	$(DO_CXX)

$(B)/client/%.o: $(SDLDIR)/%.m
	$(DO_CC)

$(B)/rendx/%.o: $(R1DIR)/%.c
	$(DO_GLX_REND_CC)

$(B)/rendx/%.o: $(RCDIR)/%.c
	$(DO_GLX_REND_CC)

$(B)/rendx/%.o: $(CMDIR)/%.c
	$(DO_GLX_REND_CC)

$(B)/rendx/%.o: $(RXDIR)/%.cpp
	$(DO_GLX_REND_CXX)

$(B)/rendx/%.o: $(RCDIR)/%.cpp
	$(DO_GLX_REND_CXX)

$(B)/rendv/%.o: $(RVDIR)/%.c
	$(DO_RENDV_CC)

$(B)/rendv/%.o: $(RCDIR)/%.c
	$(DO_RENDV_CC)

$(B)/rendv/%.o: $(CMDIR)/%.c
	$(DO_RENDV_CC)

$(B)/rendv/%.o: $(RCDIR)/%.cpp
	$(DO_RENDV_CXX)

$(B)/rendrtx/%.o: $(RTXDIR)/%.c
	$(DO_RENDRTX_CC)

$(B)/rendrtx/%.o: $(RCDIR)/%.c
	$(DO_RENDRTX_CC)

$(B)/rendrtx/%.o: $(CMDIR)/%.c
	$(DO_RENDRTX_CC)

$(B)/rendrtx/%.o: $(RCDIR)/%.cpp
	$(DO_RENDRTX_CXX)

$(B)/client/%.o: $(UDIR)/%.c
	$(DO_CC)

$(B)/client/%.o: $(UDIR)/%.cpp
	$(DO_CXX)

$(B)/client/%.o: $(W32DIR)/%.c
	$(DO_CC)

$(B)/client/%.o: $(W32DIR)/%.cpp
	$(DO_CXX)

$(B)/client/win_resource.o $(B)/ded/win_resource.o: assets/icons/fntech3.ico
# The client's script includes win_resource.rc and every game's icon.
$(B)/client/win_client_resource.o: assets/icons/fntech3.ico $(W32DIR)/win_resource.rc \
    $(W32DIR)/win_game_icons.rc $(wildcard assets/icons/games/*/*.ico)

$(B)/client/%.o: $(W32DIR)/%.rc
	$(DO_WINDRES)

$(B)/ded/%.o: $(ADIR)/%.s
	$(DO_AS)

$(B)/ded/%.o: $(SDIR)/%.c
	$(DO_DED_CC)

$(B)/ded/%.o: $(SDIR)/%.cpp
	$(DO_DED_CXX)

$(B)/ded/%.o: $(CMDIR)/%.c
	$(DO_DED_CC)

$(B)/ded/%.o: $(CMDIR)/%.cpp
	$(DO_DED_CXX)

$(B)/ded/%.o: $(COMPATDIR)/%.c
	$(DO_DED_CC)

$(B)/ded/%.o: $(PDIR)/%.cpp
	$(DO_DED_CXX)

$(B)/ded/qvm/%.o: $(CMDIR)/%.c
	$(DO_DED_CC_QVM)

$(B)/ded/%.o: $(BLIBDIR)/%.c
	$(DO_BOT_CC)

$(B)/ded/%.o: $(BLIBDIR)/%.cpp
	$(DO_BOT_CXX)

$(B)/ded/%.o: $(UDIR)/%.c
	$(DO_DED_CC)

$(B)/ded/%.o: $(UDIR)/%.cpp
	$(DO_DED_CXX)

$(B)/ded/%.o: $(W32DIR)/%.c
	$(DO_DED_CC)

$(B)/ded/%.o: $(W32DIR)/%.cpp
	$(DO_DED_CXX)

$(B)/ded/%.o: $(W32DIR)/%.rc
	$(DO_WINDRES)

$(B)/$(ROOT_ARCHIVE): $(TARGETS) $(PKG_FILES) scripts/build_root_archive.py scripts/root_archive.py scripts/stock_q3_assets.py scripts/stock_ql_maps.py scripts/stock_rtcw_maps.py scripts/fntech3_meta.py
	$(echo_cmd) "ROOTARCHIVE $@"
	$(Q)$(PYTHON) scripts/build_root_archive.py --package-root "$(PKG_ROOT)" --output "$@"

$(B)/$(FNQL_WEBPAK): $(FNQL_WEBPAK_SOURCES) scripts/build_webpak.py
	$(echo_cmd) "WEBPAK $@"
	$(Q)mkdir -p "$(B)"
	$(Q)$(PYTHON) scripts/build_webpak.py --source-root "$(FNQL_WEBPAK_ROOT)" --output "$@"

#############################################################################
# MISC
#############################################################################

install: release
	@for i in $(TARGETS); do \
		if [ -f $(BR)$$i ]; then \
			$(INSTALL) -D -m 0755 "$(BR)/$$i" "$(DESTDIR)$$i"; \
			$(STRIP) "$(DESTDIR)$$i"; \
		fi \
	done
	$(INSTALL) -D -m 0644 "$(BR)/$(ROOT_ARCHIVE)" "$(DESTDIR)/$(ROOT_ARCHIVE)"
	$(INSTALL) -D -m 0644 "$(BR)/$(FNQL_WEBPAK)" "$(DESTDIR)/$(FNQL_WEBPAK)"

clean: clean-debug clean-release

clean2:
	@echo "CLEAN $(B)"
	@if [ -d $(B) ];then (find $(B) -name '*.d' -exec rm {} \;)fi
	@rm -f $(Q3OBJ) $(Q3DOBJ)
	@rm -f $(TARGETS)

clean-debug:
	@rm -rf $(BD)

clean-release:
	@echo $(BR)
	@rm -rf $(BR)

distclean: clean
	@rm -rf $(BUILD_DIR)

#############################################################################
# DEPENDENCIES
#############################################################################

D_FILES=$(shell if [ -d "$(BUILD_DIR)" ]; then find "$(BUILD_DIR)" -type f -name '*.d'; fi)

ifneq ($(strip $(D_FILES)),)
 include $(D_FILES)
endif

.PHONY: all clean clean2 clean-debug clean-release copyfiles \
	debug default dist distclean makedirs release \
	targets tools toolsclean
