all: video ray audio rayaudio

video: test/video.c
	gcc test/video.c theoraplay.c -logg -lvorbis -ltheoradec -lao -o video

video-test: test/video.c
	clang -g -fsanitize=address,leak,undefined,integer,bounds,float-divide-by-zero,float-cast-overflow -fno-omit-frame-pointer -fno-sanitize-recover=all test/video.c theoraplay.c -logg -lvorbis -ltheoradec -o video

ray: ray.c apvf.c apvf.h
	gcc ray.c apvf.c theoraplay.c -lraylib -logg -lvorbis -ltheoradec -o ray
