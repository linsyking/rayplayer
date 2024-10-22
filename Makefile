all: video ray audio rayaudio

video: test/video.c
	gcc test/video.c theoraplay.c -logg -lvorbis -ltheoradec -lao -o video

video-test: test/video.c
	clang -O2 test/video.c theoraplay.c -logg -lvorbis -ltheoradec -lao -o video-test

ray: ray.c apvf.c apvf.h
	gcc ray.c apvf.c theoraplay.c -lraylib -logg -lvorbis -ltheoradec -o ray
