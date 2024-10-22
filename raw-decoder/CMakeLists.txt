cmake_minimum_required(VERSION 3.20)
project(decoder)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror -O3 -DOC_X86_ASM -DOC_X86_64_ASM")


add_library(theorafile STATIC
    theorafile.c
    lib/ogg/bitwise.c
    lib/ogg/framing.c
    lib/vorbis/analysis.c
    lib/vorbis/bitrate.c
    lib/vorbis/block.c
    lib/vorbis/codebook.c
    lib/vorbis/envelope.c
    lib/vorbis/floor0.c
    lib/vorbis/floor1.c
    lib/vorbis/vinfo.c
    lib/vorbis/lookup.c
    lib/vorbis/lpc.c
    lib/vorbis/lsp.c
    lib/vorbis/mapping0.c
    lib/vorbis/mdct.c
    lib/vorbis/psy.c
    lib/vorbis/registry.c
    lib/vorbis/res0.c
    lib/vorbis/sharedbook.c
    lib/vorbis/smallft.c
    lib/vorbis/synthesis.c
    lib/vorbis/window.c
    lib/theora/apiwrapper.c
    lib/theora/bitpack.c
    lib/theora/decapiwrapper.c
    lib/theora/decinfo.c
    lib/theora/decode.c
    lib/theora/dequant.c
    lib/theora/fragment.c
    lib/theora/huffdec.c
    lib/theora/idct.c
    lib/theora/tinfo.c
    lib/theora/internal.c
    lib/theora/quant.c
    lib/theora/state.c
	lib/theora/x86/mmxfrag.c
	lib/theora/x86/mmxidct.c
	lib/theora/x86/mmxstate.c
	lib/theora/x86/sse2idct.c
	lib/theora/x86/x86cpu.c
	lib/theora/x86/x86state.c
)

add_executable(decoder main.c)
target_link_libraries(decoder theorafile m)

include_directories(lib)
include_directories(lib/vorbis)
include_directories(lib/theora)
