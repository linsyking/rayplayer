all: video ray

video: test/video.c
	clang -g -fsanitize=address,leak,undefined,integer,bounds,float-divide-by-zero,float-cast-overflow -fno-omit-frame-pointer -fno-sanitize-recover=all -o check-asan-lsan-ubsan test/video.c -lavformat -lavcodec -lavutil -lswscale -o video

ray: ray.c
	gcc ray.c -lraylib -lavformat -lavcodec -lavutil -lswscale -o ray

ray-test: ray.c
	clang -g -fsanitize=address,leak,undefined,integer,bounds,float-divide-by-zero,float-cast-overflow -fno-omit-frame-pointer -fno-sanitize-recover=all -o check-asan-lsan-ubsan -lraylib -lavformat -lavcodec -lavutil -lswscale ray.c -o ray
