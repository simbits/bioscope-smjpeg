
/* This file plays SMJPEG format Motion JPEG movies */

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>


static int exit_player = 0;

/* Define this if you actually want to play the file */
#define PLAY_SMJPEG

#include "smjpeg_decode.h"

void Usage(const char *argv0)
{
    printf("SMJPEG " VERSION " decoder, Loki Entertainment Software and Fat N Soft\n");
    printf("Usage: %s [-2] [-l] [-f] [-v] file.mjpg [file.mjpg ...]\n", argv0);
    printf("-2 is double size video.\n");
    printf("-l is loop video playback.\n");
    printf("-f is fullscreen playback.\n");
    printf("-v displays version.\n");
}

void sigterm_callback_handler(int signum)
{
	printf("Caught SIGTERM, exiting\n");
	exit_player = 1;
}

int main(int argc, char *argv[])
{
    SMJPEG movie;
    int i;
    int doubleflag;
    int loopflag;
    int fullflag;
    int bpp;
    int rot_enc_fd = -1 ;
    struct input_event ev0[64];

    signal(SIGTERM, sigterm_callback_handler);

    if ( SDL_Init(SDL_INIT_VIDEO) < 0 ) {
        fprintf(stderr, "Couldn't init SDL: %s\n", SDL_GetError());
        exit(1);
    }
    atexit(SDL_Quit);

    doubleflag = 0;
    loopflag = 0;
    fullflag = 0;
    bpp = 16;

    for ( i=1; argv[i]; ++i ) {
        if ( (strcmp(argv[i], "-h") == 0) ||
             (strcmp(argv[i], "--help") == 0) ) {
            Usage(argv[0]);
            exit(0);
        }
        if ( strcmp(argv[i], "-2") == 0 ) {
            doubleflag = !doubleflag;
            continue;
        }
        if ( strcmp(argv[i], "-l") == 0 ) {
            loopflag = !loopflag;
            continue;
        }
        if ( strcmp(argv[i], "-f") == 0 ) {
            fullflag = SDL_FULLSCREEN;
            continue;
        }
        if ( strcmp(argv[i], "-bpp") == 0 ) {
            i ++;
            bpp = atoi(argv[i]);
            continue;
        }
        if ( strcmp(argv[i], "-v") == 0 ) {
            printf("SMJPEG " VERSION " decoder, Loki Entertainment Software and Fat N Soft\n");
            continue;
        }

        /* Load and play the animation */
        if ( SMJPEG_load(&movie, argv[i]) < 0 ) {
            fprintf(stderr, "%s\n", movie.status.message);
            continue;
        }
        /* Print out information about the file */
        if ( movie.audio.enabled ) {
            printf("Audio stream: %d bit %s audio at %d Hz\n",
                movie.audio.bits,
                (movie.audio.channels == 1) ? "mono" : "stereo",
                movie.audio.rate);
        }
        if ( movie.video.enabled ) {
            printf("Video stream: %d frames of %dx%d animation\n",
                movie.video.frames, movie.video.width, movie.video.height);
        }

        if ( movie.video.enabled ) {
            SDL_VideoInfo *vInfo;
            SDL_Surface *screen;
            int width, height;

            width = movie.video.width;
            height = movie.video.height;
            if ( doubleflag ) {
                width *= 2;
                height *= 2;
            }
            
            vInfo = SDL_GetVideoInfo();
            printf("Resolution: %d x %d\n", vInfo->current_w, vInfo->current_h);
            //screen = SDL_SetVideoMode(width, height, bpp, SDL_HWSURFACE|fullflag);
            screen = SDL_SetVideoMode(vInfo->current_w, vInfo->current_h, 
				      bpp, SDL_HWSURFACE|fullflag);

            if ( screen == NULL ) {
                fprintf(stderr, "Couldn't set %dx%d 16-bit video mode: %s\n", 
						width, height,
						SDL_GetError());
                continue;
            }
            
            if (bpp == 24)
            {
                screen->format->Rmask = 0xFF;
                screen->format->Gmask = 0xFF00;
                screen->format->Bmask = 0xFF0000;
            }
            SMJPEG_double(&movie, doubleflag);
            SMJPEG_target(&movie, NULL, 0, 0, screen, SDL_UpdateRect);
            SDL_ShowCursor(SDL_DISABLE);
        }

        rot_enc_fd = open("/dev/input/by-path/platform-rotary-encoder.0-event", O_RDONLY);

        if (rot_enc_fd < 0)
            exit(-1);

        SMJPEG_start(&movie, 0);
        SMJPEG_inc_frame(&movie);

        while (!exit_player) {
            int rd, i;
            SDL_Event event;
            fd_set rfds;
            struct timeval tv;
            int retval;

            FD_ZERO(&rfds);
            FD_SET(rot_enc_fd, &rfds);

            tv.tv_sec = 0;
            tv.tv_usec = 500000;

            retval = select(rot_enc_fd+1, &rfds, NULL, NULL, &tv);
            
            if (retval == -1) {
                perror("select()");
            } else if (retval) {
                if (FD_ISSET(rot_enc_fd, &rfds)) {
                    rd = read(rot_enc_fd, ev0, sizeof(struct input_event) * 64);
                    for (i=0; i<rd/sizeof(struct input_event); i++) {
                        printf("event[%d].type: %d, event[%d].value: %d\n", i, ev0[i].type, i, ev0[i].value);
                        if (ev0[i].type == 2) {
                            if (ev0[i].value == 1) {
                                SMJPEG_inc_frame(&movie);
                            } else if (ev0[i].value == -1 ) {
                                SMJPEG_dec_frame(&movie);
                            }
                        }
                    }
                }
            } else {
               printf("timeout\n");
            } 
        }

        SMJPEG_stop(&movie);
        SMJPEG_free(&movie);
        SDL_ShowCursor(SDL_ENABLE);
        close(rot_enc_fd);
    }
}
