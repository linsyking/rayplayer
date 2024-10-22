#include "theorafile.h"
#include <stdio.h>
#include <stdlib.h>

int main() {
    OggTheora_File fileIn;
    int            width = 0, height = 0;
    double         fps      = 0;
    th_pixel_fmt   fmt      = 0;
    int            curframe = 0, newframe = 0;
    char          *frame = NULL;

    if (tf_fopen("test.ogv", &fileIn) < 0) {
        printf("Error opening file\n");
        return 1;
    }

    if (!tf_hasvideo(&fileIn)) {
        printf("No video stream found\n");
        return 1;
    }

    /* Get the video metadata, allocate first frame */
    tf_videoinfo(&fileIn, &width, &height, &fps, &fmt);
    printf("Video: %dx%d %g fps %d fmt\n", width, height, fps, fmt);
    frame = (char *)malloc(width * height * 2);

    while (!tf_readvideo(&fileIn, frame, 1))
        ;
    if (tf_eos(&fileIn)) {
        tf_reset(&fileIn);
    }
    while (1) {
        newframe = tf_readvideo(&fileIn, frame, 1);
        if (newframe) {
            printf("Frame %d\n", curframe++);
        }
    }
    free(frame);
}
