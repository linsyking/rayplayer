all: video ray audio rayaudio

video: test/video.c
	clang -g -fsanitize=address,leak,undefined,integer,bounds,float-divide-by-zero,float-cast-overflow -fno-omit-frame-pointer -fno-sanitize-recover=all test/video.c -lavformat -lavcodec -lavutil -lswscale -o video

audio: test/audio.c
	clang test/audio.c audio.c -lavformat -lavcodec -lavutil -lswscale -lao -lswresample -o audio

ray: ray.c audio.c
	gcc ray.c audio.c -lraylib -lavformat -lavcodec -lavutil -lswscale -lm -lswresample -o ray

rayaudio: rayaudio.c
	gcc rayaudio.c -lraylib -lavformat -lavcodec -lavutil -lswscale -lm -o rayaudio

ray-test: ray.c
	clang -g -fsanitize=address,leak,undefined,integer,bounds,float-divide-by-zero,float-cast-overflow -fno-omit-frame-pointer -fno-sanitize-recover=all -lraylib -lavformat -lavcodec -lavutil -lswscale ray.c -o ray
