/*
** Copyright 2010, The Android Open-Source Project
** Copyright (c) 2011, Code Aurora Forum. All rights reserved.
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <getopt.h>

#include "alsa_audio.h"

#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

#define FORMAT_PCM 1

static struct wav_header hdr;
static int fd;
static struct pcm *pcm;
static debug = 0;
static pcm_flag = 1;
static duration = 0;
static char *filename;
static char *data;

static struct option long_options[] =
{
    {"pcm", 0, 0, 'P'},
    {"debug", 0, 0, 'V'},
    {"Mmap", 0, 0, 'M'},
    {"HW", 1, 0, 'D'},
    {"Rate", 1, 0, 'R'},
    {"channel", 1, 0, 'C'},
    {"duration", 1, 0, 'T'},
    {0, 0, 0, 0}
};

struct wav_header {
    uint32_t riff_id;
    uint32_t riff_sz;
    uint32_t riff_fmt;
    uint32_t fmt_id;
    uint32_t fmt_sz;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;       /* sample_rate * num_channels * bps / 8 */
    uint16_t block_align;     /* num_channels * bps / 8 */
    uint16_t bits_per_sample;
    uint32_t data_id;
    uint32_t data_sz;
};

static int set_params(struct pcm *pcm)
{
     struct snd_pcm_hw_params *params;
     struct snd_pcm_sw_params *sparams;

     unsigned long periodSize, bufferSize, reqBuffSize;
     unsigned int periodTime, bufferTime;
     unsigned int requestedRate = pcm->rate;

     params = (struct snd_pcm_hw_params*) calloc(1, sizeof(struct snd_pcm_hw_params));
     if (!params) {
          fprintf(stderr, "Arec:Failed to allocate ALSA hardware parameters!");
          return -ENOMEM;
     }

     param_init(params);

     param_set_mask(params, SNDRV_PCM_HW_PARAM_ACCESS,
                    (pcm->flags & PCM_MMAP)? SNDRV_PCM_ACCESS_MMAP_INTERLEAVED : SNDRV_PCM_ACCESS_RW_INTERLEAVED);
     param_set_mask(params, SNDRV_PCM_HW_PARAM_FORMAT,
                    SNDRV_PCM_FORMAT_S16_LE);
     param_set_mask(params, SNDRV_PCM_HW_PARAM_SUBFORMAT,
                    SNDRV_PCM_SUBFORMAT_STD);
     param_set_min(params, SNDRV_PCM_HW_PARAM_PERIOD_TIME, 1000);
     param_set_int(params, SNDRV_PCM_HW_PARAM_SAMPLE_BITS, 16);
     param_set_int(params, SNDRV_PCM_HW_PARAM_FRAME_BITS,
                    pcm->channels - 1 ? 32 : 16);
     param_set_int(params, SNDRV_PCM_HW_PARAM_CHANNELS,
                    pcm->channels);
     param_set_int(params, SNDRV_PCM_HW_PARAM_RATE, pcm->rate);

     param_set_hw_refine(pcm, params);

     if (param_set_hw_params(pcm, params)) {
         fprintf(stderr, "Arec:cannot set hw params");
         return -errno;
     }
     if (debug)
          param_dump(params);

     pcm->buffer_size = pcm_buffer_size(params);
     pcm->period_size = pcm_period_size(params);
     pcm->period_cnt = pcm->buffer_size/pcm->period_size;
     if (debug) {
        fprintf (stderr,"period_size (%d)", pcm->period_size);
        fprintf (stderr," buffer_size (%d)", pcm->buffer_size);
        fprintf (stderr," period_cnt  (%d)\n", pcm->period_cnt);
     }
     sparams = (struct snd_pcm_sw_params*) calloc(1, sizeof(struct snd_pcm_sw_params));
     if (!sparams) {
         fprintf(stderr, "Arec:Failed to allocate ALSA software parameters!\n");
         return -ENOMEM;
     }
    sparams->tstamp_mode = SNDRV_PCM_TSTAMP_NONE;
    sparams->period_step = 1;
    sparams->avail_min = (pcm->flags & PCM_MONO) ? pcm->period_size/2 : pcm->period_size/4;
    sparams->start_threshold = 1;
    sparams->stop_threshold = (pcm->flags & PCM_MONO) ? pcm->buffer_size/2 : pcm->buffer_size/4;
    sparams->xfer_align = (pcm->flags & PCM_MONO) ? pcm->period_size/2 : pcm->period_size/4; /* needed for old kernels */
    sparams->silence_size = 0;
    sparams->silence_threshold = 0;

    if (param_set_sw_params(pcm, sparams)) {
         fprintf(stderr, "Arec:cannot set sw params");
         return -errno;
    }
    if (debug) {
        fprintf (stderr,"avail_min (%d)\n", sparams->avail_min);
        fprintf (stderr,"start_threshold (%d)\n", sparams->start_threshold);
        fprintf (stderr,"stop_threshold (%d)\n", sparams->stop_threshold);
        fprintf (stderr,"xfer_align (%d)\n", sparams->xfer_align);
    }
    return 0;

}

int record_file(unsigned rate, unsigned channels, int fd, unsigned count,  unsigned flags, const char *device)
{
    unsigned avail, xfer, bufsize;
    int r;
    int nfds = 1;
    static int start = 0;
    struct snd_xferi x;
    long frames;
    unsigned offset = 0;
    int err;
    struct pollfd pfd[1];
    int rec_size = 0;

    flags |= PCM_IN;

    if (channels == 1)
        flags |= PCM_MONO;
    else
        flags |= PCM_STEREO;

    pcm = pcm_open(flags, device);
    if (!pcm_ready(pcm)) {
        pcm_close(pcm);
        goto fail;
    }
    pcm->channels = channels;
    pcm->rate = rate;
    pcm->flags = flags;
    if (set_params(pcm)) {
        fprintf(stderr, "Arec:params setting failed\n");
        pcm_close(pcm);
        return -EINVAL;
    }

    if (!pcm_flag) {
        if (pcm_prepare(pcm)) {
            fprintf(stderr, "Arec:Failed in pcm_prepare\n");
            pcm_close(pcm);
            return -errno;
        }
        while(1);
   }

    if (flags & PCM_MMAP) {
        if (mmap_buffer(pcm)) {
             fprintf(stderr, "Arec:params setting failed\n");
             pcm_close(pcm);
             return -EINVAL;
        }
        if (debug)
            fprintf(stderr, "Arec:mmap_buffer done\n");

        if (pcm_prepare(pcm)) {
            fprintf(stderr, "Arec:Failed in pcm_prepare\n");
            pcm_close(pcm);
            return -errno;
        }

        bufsize = pcm->period_size;
        if (debug)
	    fprintf(stderr, "Arec:bufsize = %d\n", bufsize);
	data = calloc(1, bufsize);
	if (!data) {
	    fprintf(stderr, "Arec:could not allocate %d bytes\n", bufsize);
	    return -ENOMEM;
	}
        if (ioctl(pcm->fd, SNDRV_PCM_IOCTL_START)) {
		if (errno == EPIPE) {
			fprintf(stderr, "Arec:Failed in SNDRV_PCM_IOCTL_START\n");
			/* we failed to make our window -- try to restart */
			pcm->running = 0;
		} else {
			fprintf(stderr, "Arec:Error no %d \n", errno);
			return -errno;
		}
        }

        pfd[0].fd = pcm->fd;
        pfd[0].events = POLLIN;

        hdr.data_sz = 0;
        frames = (pcm->flags & PCM_MONO) ? (bufsize / 2) : (bufsize / 4);
           for(;;) {
                x.frames = (pcm->flags & PCM_MONO) ? (bufsize / 2) : (bufsize / 4);
		if (!pcm->running) {
                    if (pcm_prepare(pcm))
                        return --errno;
                    start = 0;
                }
		pcm->sync_ptr->flags = SNDRV_PCM_SYNC_PTR_APPL | SNDRV_PCM_SYNC_PTR_AVAIL_MIN;//SNDRV_PCM_SYNC_PTR_HWSYNC;
                err = sync_ptr(pcm);
                if (err == EPIPE) {
                     fprintf(stderr, "Arec:Failed in sync_ptr \n");
                     /* we failed to make our window -- try to restart */
                     //pcm->overruns++;
                     pcm->running = 0;
                     continue;
                }
                avail = pcm_avail(pcm);
                if (debug)
                     fprintf(stderr, "Arec:avail 1 = %d frames = %d\n",avail, frames);
                if (avail < 0)
                        return avail;
                if (avail < pcm->sw_p->avail_min) {
                        poll(pfd, nfds, TIMEOUT_INFINITE);
                        continue;
                }
		if (x.frames > avail)
                        frames = avail;

		err = mmap_transfer_capture(pcm, data , offset, frames);
		if (err == EPIPE) {
		    fprintf(stderr, "Arec:Failed in mmap_transfer \n");
		    /* we failed to make our window -- try to restart */
		    pcm->running = 0;
		    continue;
		}
                x.frames -= frames;
                pcm->sync_ptr->c.control.appl_ptr += frames;
		pcm->sync_ptr->flags = 0;
                err = sync_ptr(pcm);
                if (err == EPIPE) {
                     fprintf(stderr, "Arec:Failed in sync_ptr \n");
                     /* we failed to make our window -- try to restart */
                     pcm->running = 0;
                     continue;
                }
                if (write(fd, data, bufsize) != bufsize) {
                    fprintf(stderr, "Arec:could not write %d bytes\n", bufsize);
                    return -errno;
                }
                rec_size += bufsize;
                hdr.data_sz += bufsize;
                if (rec_size >= count)
                      break;
           }
    } else {
	    bufsize = pcm->period_size;
            if (pcm_prepare(pcm)) {
                fprintf(stderr, "Arec:Failed in pcm_prepare\n");
                pcm_close(pcm);
                return -errno;
            }

	    data = calloc(1, bufsize);
	    if (!data) {
		fprintf(stderr, "Arec:could not allocate %d bytes\n", bufsize);
		return -ENOMEM;
	    }

	    while (!pcm_read(pcm, data, bufsize)) {
		if (write(fd, data, bufsize) != bufsize) {
		    fprintf(stderr, "Arec:could not write %d bytes\n", bufsize);
		    return -errno;
		}
                rec_size += bufsize;
                hdr.data_sz += bufsize;
                if (rec_size >= count)
                      break;
	    }
    }

    close(fd);
    free(data);
    pcm_close(pcm);
    return hdr.data_sz;

fail:
    fprintf(stderr, "Arec:pcm error: %s\n", pcm_error(pcm));
    return -errno;
}

int rec_wav(const char *fg, const char *device, int rate, int ch, const char *fn)
{
    unsigned flag = 0;
    uint32_t rec_max_sz = 2147483648LL;
    uint32_t count;
    int i = 0;

    if (pcm_flag) {
            if (!fn) {
              fd = fileno(stdout);
            } else {
	       fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0664);
	       if (fd < 0) {
	            fprintf(stderr, "Arec:arec: cannot open '%s'\n", fn);
		    return -EBADFD;
	       }
            }
	    memset(&hdr, 0, sizeof(struct wav_header));
	    hdr.riff_id = ID_RIFF;
	    hdr.riff_fmt = ID_WAVE;
	    hdr.fmt_id = ID_FMT;
	    hdr.fmt_sz = 16;
	    hdr.audio_format = FORMAT_PCM;
	    hdr.num_channels = ch;
	    hdr.sample_rate = rate;
            hdr.bits_per_sample = 16;
            hdr.byte_rate = (rate * ch * hdr.bits_per_sample) / 8;
            hdr.block_align = ( hdr.bits_per_sample * ch ) / 8;
	    hdr.data_id = ID_DATA;
	    hdr.data_sz = rec_max_sz;
            hdr.riff_sz = hdr.data_sz + 44 - 8;

            if (duration == 0) {
                count = rec_max_sz;
            } else {
                count = rate * ch * 2;
                count *= (off64_t)duration;
            }
            hdr.data_sz = count < rec_max_sz ? count : rec_max_sz;
	    if (write(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		if (debug)
		    fprintf(stderr, "arec: cannot write header\n");
		return -errno;
	    }
	    if (debug)
		fprintf(stderr, "arec: %d ch, %d hz, %d bit, %s\n",
		    hdr.num_channels, hdr.sample_rate, hdr.bits_per_sample,
		    hdr.audio_format == FORMAT_PCM ? "PCM" : "unknown");
    } else {
            hdr.sample_rate = rate;
            hdr.num_channels = ch;
    }

    if (!strcmp(fg, "M")) {
        flag = PCM_MMAP;
    } else if (!strcmp(fg, "N")) {
        flag = PCM_NMMAP;
    }
    return record_file(hdr.sample_rate, hdr.num_channels, fd, hdr.data_sz, flag, device);
}

static void signal_handler(int sig)
{
    long file_size;
    FILE *fp;

    fprintf(stderr, "Arec:Aborted by signal %s...\n", strsignal(sig));
    fprintf(stderr, "Arec:lseeked to %d", (int) lseek(fd, 0, SEEK_SET));
    hdr.riff_sz = hdr.data_sz + 44 - 8;
    if (write(fd, &hdr, sizeof(hdr)) != sizeof(hdr)) {
	if (debug)
            fprintf(stderr, "Arec:arec: cannot write header\n");
    } else
       fd = -1;

    if (fd > 1) {
        close(fd);
        fd = -1;
    }
    free(filename);
    free(data);
    pcm = NULL;
    raise(sig);
}

int main(int argc, char **argv)
{
    int rate = 48000;
    int ch = 1;
    int i = 0;
    int option_index = 0;
    int c;
    char *mmap = "N";
    char *device = "hw:0,0";

    if (argc < 2) {
          printf("Usage: arec [options] <file>\n"
                "options:\n"
                "-D <hw:C,D>	-- Alsa PCM by name\n"
                "-M		-- Mmap stream\n"
                "-P		-- Hostless steam[No PCM]\n"
                "-V		-- verbose\n"
                "-C		-- Channels\n"
                "-R		-- Rate\n"
                "-T		-- Time in seconds for recording\n"
                "<file> \n");
          return 0;
    }
    while ((c = getopt_long(argc, argv, "PVMD:R:C:T:", long_options, &option_index)) != -1) {
       switch (c) {
       case 'P':
          pcm_flag = 0;
          break;
       case 'V':
          debug = 1;
          break;
       case 'M':
          mmap = "M";
          break;
       case 'D':
          device = optarg;
          break;
       case 'R':
          rate = (int)strtol(optarg, NULL, 0);
          break;
       case 'C':
          ch  = (int)strtol(optarg, NULL, 0);
          break;
       case 'T':
          duration = (int)strtol(optarg, NULL, 0);
          break;
       default:
          printf("Usage: arec [options] <file>\n"
                "options:\n"
                "-D <hw:C,D>	-- Alsa PCM by name\n"
                "-M		-- Mmap stream\n"
                "-P		-- Hostless steam[No PCM]\n"
                "-V		-- verbose\n"
                "-C		-- Channels\n"
                "-R		-- Rate\n"
                "-T		-- Time in seconds for recording\n"
                "<file> \n");
          return -EINVAL;
       }
    }
    filename = (char*) calloc(1, 30);
     if (!filename) {
          fprintf(stderr, "Arec:Failed to allocate filename!");
          return -ENOMEM;
    }
    if (optind > argc - 1) {
        filename = NULL;
    } else {
        strcpy(filename, argv[optind++]);
    }

    if (pcm_flag) {
        rec_wav(mmap, device, rate, ch, filename);
    } else {
        rec_wav(mmap, device, rate, ch, "dummy");
    }
    free(filename);
    return 0;
}

