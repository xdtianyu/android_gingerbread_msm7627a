/* amrnbtest.c - native AMRNB test application
 *
 * Based on native pcm test application platform/system/extras/sound/playwav.c
 *
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include "audiotest_def.h"
#include <sys/ioctl.h>
#include <linux/msm_audio_amrnb.h>
#include <pthread.h>
#include <errno.h>

#define AMRNB_PKT_SIZE 36

/* http://ccrma.stanford.edu/courses/422/projects/WaveFormat/ */
struct wav_header {		/* Simple wave header */
	char Chunk_ID[4];	/* Store "RIFF" */
	unsigned int Chunk_size;
	char Riff_type[4];	/* Store "WAVE" */
	char Chunk_ID1[4];	/* Store "fmt " */
	unsigned int Chunk_fmt_size;
	unsigned short Compression_code;	/*1 - 65,535,  1 - pcm */
	unsigned short Number_Channels;	/* 1 - 65,535 */
	unsigned int Sample_rate;	/*  1 - 0xFFFFFFFF */
	unsigned int Bytes_Sec;	/*1 - 0xFFFFFFFF */
	unsigned short Block_align;	/* 1 - 65,535 */
	unsigned short Significant_Bits_sample;	/* 1 - 65,535 */
	char Chunk_ID2[4];	/* Store "data" */
	unsigned int Chunk_data_size;
} __attribute__ ((packed));

static struct wav_header append_header = {
	{'R', 'I', 'F', 'F'}, 0, {'W', 'A', 'V', 'E'},
	{'f', 'm', 't', ' '}, 16, 1, 1, 8000, 16000, 2,
	16, {'d', 'a', 't', 'a'}, 0
	};

struct meta_in{
	unsigned short offset;
	long long timestamp;
	unsigned int nflags;
} __attribute__ ((packed));

struct meta_out{
	unsigned short offset;
	long long timestamp;
	unsigned int nflags;
	unsigned short errflag;
	unsigned short sample_frequency;
	unsigned short channel;
	unsigned int tick_count;
} __attribute__ ((packed));

#ifdef _ANDROID_
static const char *cmdfile = "/data/audio_test";
/* static const char *outfile = "/data/pcm.wav"; */
#else
static const char *cmdfile = "/tmp/audio_test";
/* static const char *outfile = "/tmp/pcm.wav"; */
#endif

static void create_wav_header(int Datasize)
{
	append_header.Chunk_size = Datasize + 8 + 16 + 12;
	append_header.Chunk_data_size = Datasize;
}


static void *amrnb_dec(void *arg)
{
	struct meta_out meta;
	int fd, ret_val = 0;
        struct audio_pvt_data *audio_data = (struct audio_pvt_data *) arg;
        int afd = audio_data->afd;
	int len, total_len;
	len = 0;
	total_len = 0;

	fd = open(audio_data->outfile, O_RDWR | O_CREAT,
		  S_IRWXU | S_IRWXG | S_IRWXO);
	if (fd < 0) {
		printf("Err while opening file decoder output file \n");
		pthread_exit((void *)ret_val);
	}

	printf(" amrnb_read Thread \n");

	lseek(fd, 44, SEEK_SET);	/* Set Space for Wave Header */
	do {
		if (audio_data->suspend == 1) {
			printf("enter suspend mode\n");
			ioctl(afd, AUDIO_STOP, 0);
			while (audio_data->suspend == 1)
				sleep(1);
			ioctl(afd, AUDIO_START, 0);
			printf("exit suspend mode\n");
		}
		len = read(afd, audio_data->recbuf, audio_data->recsize);
		#ifdef DEBUG_LOCAL
		printf(" Read = %d PCM samples\n", len/2);
		#endif
		if (len < 0) {
			printf("error reading the PCM samples \n");
			goto fail;
		} else if (len != 0) {
			memcpy(&meta, audio_data->recbuf, sizeof(struct meta_out));
			#ifdef DEBUG_LOCAL
			printf("\t\tMeta Out Timestamp:%lld\n", meta.timestamp);
			#endif
			if (meta.nflags == 1) {
				printf("Reached end of file\n");
				break;
			}
			len = (len - sizeof(struct meta_out));
			if (len > 0) {
				if (write(fd, (audio_data->recbuf +
				sizeof(struct meta_out)), len) != len) {
					printf(" error writing the PCM \
							samples to file \n");
					goto fail;
				}
			}
		} else if (len == 0)
			printf("Unexpected case: read count zero\n");
		total_len += len;
	} while (1);
	create_wav_header(total_len);
	lseek(fd, 0, SEEK_SET);
	write(fd, (char *)&append_header, 44);
	close(fd);
	free(audio_data->recbuf);
	pthread_exit((void *)ret_val);

fail:
	close(fd);
	free(audio_data->recbuf);
	pthread_exit((void *)ret_val);
    return NULL;
}

static void *event_notify(void *arg)
{
	long ret_drv;
        struct audio_pvt_data *audio_data = (struct audio_pvt_data *) arg;
        int afd = audio_data->afd;
	struct msm_audio_event suspend_event;
	do {
		printf("event_notify thread started\n");
		suspend_event.timeout_ms = 0;
		ret_drv = ioctl(afd, AUDIO_GET_EVENT, &suspend_event);
		if (ret_drv < 0) {
			printf("event_notify thread exiting: \
				Got Abort event or timedout\n");
			break;
		} else {
			if (suspend_event.event_type == AUDIO_EVENT_SUSPEND) {
				printf("event_notify: RECEIVED EVENT FROM \
					DRIVER OF TYPE: AUDIO_EVENT_SUSPEND: \
					%d\n", suspend_event.event_type);
				audio_data->suspend = 1;
				sleep(1);
			} else if
			(suspend_event.event_type == AUDIO_EVENT_RESUME) {
				printf("event_notify: RECEIVED EVENT FROM \
					DRIVER OF TYPE: AUDIO_EVENT_RESUME : \
					%d\n", suspend_event.event_type);
				audio_data->suspend = 0;
			}
		}
	} while (1);
	return NULL;
}

static int initiate_play(struct audtest_config *clnt_config,
						 int (*fill)(void *buf, unsigned sz, void *cookie),
						 void *cookie)
{
	struct msm_audio_config config;
	// struct msm_audio_stats stats;
	unsigned n;
	pthread_t thread, event_th;
	int sz, used =0;
	char *buf; 
	int afd;
	int cntW=0;
	int ret = 0;

#ifdef AUDIOV2
	unsigned short dec_id;
#endif

	struct audio_pvt_data *audio_data = (struct audio_pvt_data *) clnt_config->private_data;

	if (audio_data->mode) {
		printf("non-tunnel mode\n");
		afd = open("/dev/msm_amrnb", O_RDWR);
	} else {
		printf("tunnel mode\n");
		afd = open("/dev/msm_amrnb", O_WRONLY);

	}
	if (afd < 0) {
		perror("amrnb_play: cannot open AMRNB device");
		return -1;
	}

#ifdef AUDIOV2
	if (!audio_data->mode) {
		if (ioctl(afd, AUDIO_GET_SESSION_ID, &dec_id)) {
			perror("could not get decoder session id\n");
			close(afd);
			return -1;
		}
#if defined(QC_PROP)
		if (devmgr_register_session(dec_id, DIR_RX) < 0) {
			ret = -1;
			goto exit;
		}
#endif
	}
#endif

	audio_data->afd = afd; /* Store */

	pthread_create(&event_th, NULL, event_notify, (void *) audio_data);

	if (ioctl(afd, AUDIO_GET_CONFIG, &config)) {
		perror("could not get config");
		ret = -1;
		goto err_state;
	}

	if (audio_data->mode) {
		config.meta_field = 1;
		if (ioctl(afd, AUDIO_SET_CONFIG, &config)) {
			perror("could not set config");
			ret = -1;
			goto err_state;
		}
	}

	buf = (char*) malloc(sizeof(char) * config.buffer_size);
	if (buf == NULL) {
		perror("fail to allocate buffer\n");
		ret = -1;
		goto err_state;
	}

	config.buffer_size -= (config.buffer_size % AMRNB_PKT_SIZE);

	printf("initiate_play: buffer_size=%d, buffer_count=%d\n",
			config.buffer_size, config.buffer_count);

	fprintf(stderr,"prefill\n");

	if (audio_data->mode) {
		/* non - tunnel portion */
		struct msm_audio_pcm_config config_rec;
		printf(" selected non-tunnel part\n");
		if (ioctl(afd, AUDIO_GET_PCM_CONFIG, &config_rec)) {
			printf("could not get PCM config\n");
			free(buf);
			ret = -1;
			goto err_state;
		}
		printf(" config_rec.pcm_feedback = %d, \
			config_rec.buffer_count = %d , \
			config_rec.buffer_size=%d \n", \
			config_rec.pcm_feedback, \
			config_rec.buffer_count, config_rec.buffer_size);
		config_rec.pcm_feedback = 1;
		audio_data->recsize = config_rec.buffer_size;
		audio_data->recbuf = (char *)malloc(config_rec.buffer_size);
		if (!audio_data->recbuf) {
			printf("could not allocate memory for decoding\n");
			free(buf);
			ret = -1;
			goto err_state;
		}
		memset(audio_data->recbuf, 0, config_rec.buffer_size);
		if (ioctl(afd, AUDIO_SET_PCM_CONFIG, &config_rec)) {
			printf("could not set PCM config\n");
			free(audio_data->recbuf);
			free(buf);
			ret = -1;
			goto err_state;
		}
		pthread_create(&thread, NULL, amrnb_dec, (void *) audio_data);

	}

	for (n = 0; n < config.buffer_count; n++) {
		if ((sz = fill(buf, config.buffer_size, cookie)) < 0)
			break;
		if (write(afd, buf, sz) != sz)
			break;
	}
	cntW=cntW+config.buffer_count; 

	sz = 0;
	fprintf(stderr,"start playback\n");
	if (ioctl(afd, AUDIO_START, 0) >= 0) {
		for (;;) {
#if 0
			if (ioctl(afd, AUDIO_GET_STATS, &stats) == 0)
				fprintf(stderr,"%10d\n", stats.out_bytes);
#endif
			if (sz == 0) {
				if (((sz = fill(buf, config.buffer_size,
					cookie)) < 0) || (audio_data->quit == 1)) {
					if ((audio_data->repeat == 0) || (audio_data->quit == 1)) {
						printf(" file reached end or quit cmd issued, exit loop \n");
						if (audio_data->mode) {
							struct meta_in meta;
							meta.offset =
								sizeof(struct meta_in);
							meta.timestamp =
								(audio_data->frame_count * 20000);
							meta.nflags = 1;
							memset(buf, 0,
							sizeof(config.buffer_size));
							memcpy(buf, &meta,
								sizeof(struct meta_in));
							if (write(afd, buf,
							sizeof(struct meta_in)) < 0)
								printf(" writing buffer\
								for EOS failed\n");
						} else {
							printf("FSYNC: Reached end of \
								file, calling fsync\n");
							if (fsync(afd) < 0)
								printf("fsync \
								failed\n");
						}
						break;
					} else {
						printf("\nRepeat playback\n");
						audio_data->avail = audio_data->org_avail;
						audio_data->next  = audio_data->org_next;
						cntW = 0;
						if(audio_data->repeat > 0)
							audio_data->repeat--;
						sleep(1);
						continue;
					}
				}
			} else
				printf("amrnb_play: continue with unconsumed data\n");

			if (audio_data->suspend == 1) {
				printf("enter suspend mode\n");
				ioctl(afd, AUDIO_STOP, 0);
				while (audio_data->suspend == 1)
					sleep(1);
				ioctl(afd, AUDIO_START, 0);
				printf("exit suspend mode\n");
			}
			used = write(afd, buf, sz);
			printf(" amrnb_play: instance=%d repeat_cont=%d cntW=%d\n",
					(int) audio_data, audio_data->repeat, cntW);
			if (used > -1) {
				sz-=used;
				cntW++;
			} else {
				printf("amrnb_play: IO busy err#%d wait 5 ms\n", errno);
				sz = 0;
				usleep(5000);
			}
		}
		printf("end of amrnb play, stop audio\n");
		sleep(3);
		ioctl(afd, AUDIO_ABORT_GET_EVENT, 0);
		ioctl(afd, AUDIO_STOP, 0);
		sleep(10);

	} else {
		printf("amrnb_play: Unable to start driver\n");
	}
	free(buf);
err_state:
#if defined(QC_PROP) && defined(AUDIOV2)
	if (!audio_data->mode) {
		if (devmgr_unregister_session(dec_id, DIR_RX) < 0)
			ret = -1;
	}
exit:
#endif
	close(afd);
	return ret;
} 


static int fill_buffer(void *buf, unsigned sz, void *cookie)
{
	struct meta_in meta;
	struct audio_pvt_data *audio_data = (struct audio_pvt_data *) cookie;
	unsigned cpy_size = (sz < audio_data->avail?sz:audio_data->avail);

	if (audio_data->avail == 0) {
		return -1;
	}

	if (audio_data->mode) {
		meta.timestamp = (audio_data->frame_count * 20000);
		meta.offset = sizeof(struct meta_in);
		meta.nflags = 0;
		audio_data->frame_count += (cpy_size / AMRNB_PKT_SIZE);
		#ifdef DEBUG_LOCAL
		printf("Meta In timestamp: %lld\n", meta.timestamp);
		#endif
		memcpy(buf, &meta, sizeof(struct meta_in));
		memcpy(((char *)buf + sizeof(struct meta_in)), audio_data->next, cpy_size);
	} else
		memcpy(buf, audio_data->next, cpy_size);

	audio_data->next += cpy_size; 
	audio_data->avail -= cpy_size;

	if (audio_data->mode)
		return cpy_size + sizeof(struct meta_in);
	else
		return cpy_size;
}

static int play_file(struct audtest_config *config, 
					 int fd, size_t count)
{
        struct audio_pvt_data *audio_data = (struct audio_pvt_data *) config->private_data;
	int ret_val = 0;
	char *content_buf;

	audio_data->next = (char*)malloc(count);

	printf(" play_file: count=%d,next=%p\n", count, audio_data->next);

	if (!audio_data->next) {
		fprintf(stderr,"could not allocate %d bytes\n", count);
		return -1;
	}

	audio_data->org_next = audio_data->next;
	content_buf = audio_data->org_next;

	if (read(fd, audio_data->next, count) != (ssize_t)count) {
		fprintf(stderr,"could not read %d bytes\n", count);
		free(content_buf);
		return -1;
	}

	audio_data->avail = count;
	audio_data->org_avail = audio_data->avail;

	ret_val = initiate_play(config, fill_buffer, audio_data);
	free(content_buf);
	return ret_val;
}

int amrnb_play(struct audtest_config *config)
{
	struct stat stat_buf;
	int fd;

	if (config == NULL) {
		return -1;
	}

	fd = open(config->file_name, O_RDONLY);

	if (fd < 0) {
		fprintf(stderr, "playamrnb: cannot open '%s'\n", config->file_name);
		return -1;
	}

	(void) fstat(fd, &stat_buf);

	return play_file(config, fd, stat_buf.st_size);;
}

int amrnb_rec(struct audtest_config *config)
{

	unsigned char buf[8192];
	struct msm_audio_config cfg;
	struct msm_audio_amrnb_enc_config amrnb_cfg;
	unsigned sz;
	int fd, afd;
	unsigned total = 0;
	unsigned char tmp;

        fd = open(config->file_name, O_CREAT | O_RDWR, 0666);
	if (fd < 0) {
		perror("cannot open output file");
		return -1;
	}

	afd = open("/dev/msm_amrnb_in", O_RDONLY);
	if (afd < 0) {
		perror("cannot open msm_amrnb_in");
		close(fd);
		return -1;
	}

	config->private_data = (void*) afd;

	if (ioctl(afd, AUDIO_GET_CONFIG, &cfg)) {
		perror("cannot read audio config");
		goto fail;
	}

	sz = cfg.buffer_size;
	fprintf(stderr,"buffer size %d\n", sz);
	if (sz > sizeof(buf)) {
		fprintf(stderr,"buffer size %d too large\n", sz);
		goto fail;
	}

	/* AMRNB specific settings */
	if (ioctl(afd, AUDIO_GET_AMRNB_ENC_CONFIG, &amrnb_cfg)) {
		perror("cannot read audio amrnb config");
		goto fail;
	}
#if 0
	/* Use Default Value */
	amrnb_cfg.voicememoencweight1 = 0x0000;
	amrnb_cfg.voicememoencweight2 = 0x0000;
	amrnb_cfg.voicememoencweight3 = 0x4000;
	amrnb_cfg.voicememoencweight4 = 0x0000;
#endif

	amrnb_cfg.dtx_mode_enable = config->channel_mode; /* 0 - DTX off, 0xFFFF - DTX on */
	amrnb_cfg.enc_mode = config->sample_rate;

	if (ioctl(afd, AUDIO_SET_AMRNB_ENC_CONFIG, &amrnb_cfg)) {
		perror("cannot write audio amrnb config");
		goto fail;
	}

	fprintf(stderr,"voicememoencweight1=0x%4x\n", amrnb_cfg.voicememoencweight1);
	fprintf(stderr,"voicememoencweight2=0x%4x\n", amrnb_cfg.voicememoencweight2);
	fprintf(stderr,"voicememoencweight3=0x%4x\n", amrnb_cfg.voicememoencweight3);
	fprintf(stderr,"voicememoencweight4=0x%4x\n", amrnb_cfg.voicememoencweight4);
	fprintf(stderr,"dtx_mode_enable=0x%4x\n", amrnb_cfg.dtx_mode_enable);
	fprintf(stderr,"test_mode_enable=0x%4x\n", amrnb_cfg.test_mode_enable);
	fprintf(stderr,"enc_mode=0x%4x\n", amrnb_cfg.enc_mode);/* 0-MR475,1-MR515,2-MR59,3-MR67,4-MR74
                                5-MR795, 6- MR102, 7- MR122(default) */

	if (ioctl(afd, AUDIO_START, 0)) {
		perror("cannot start audio");
		goto fail;
	}

	fcntl(0, F_SETFL, O_NONBLOCK);
	fprintf(stderr,"\n*** AMRNB PACKET RECORDING * HIT ENTER TO STOP ***\n");

	for (;;) {
		while (read(0, &tmp, 1) == 1) {
			if ((tmp == 13) || (tmp == 10)) goto done;
		}
		if (read(afd, buf, sz) != (ssize_t)sz) {
			perror("cannot read buffer");
			goto fail;
		}
		if (write(fd, buf, sz) != (ssize_t)sz) {
			perror("cannot write buffer");
			goto fail;
		}
		total += sz;
	}
	done:
	if (ioctl(afd, AUDIO_STOP, 0)) {
		perror("cannot stop audio");
		goto fail;
	}
	close(afd);
	close(fd);
	return 0;

	fail:
	close(afd);
	close(fd);
	unlink(config->file_name);
	return -1;
}

void* playamrnb_thread(void* arg) {
	struct audiotest_thread_context *context = 
	(struct audiotest_thread_context*) arg;
	int ret_val;

	ret_val = amrnb_play(&context->config);
        printf(" Free audio instance 0x%8x \n", (unsigned int) context->config.private_data);
        free(context->config.private_data);
	free_context(context);
	pthread_exit((void*) ret_val);
    return NULL;
}

void* recamrnb_thread(void* arg) {
	struct audiotest_thread_context *context =
	(struct audiotest_thread_context*) arg;
	int ret_val;

	ret_val = amrnb_rec(&context->config);
	free_context(context);
	pthread_exit((void*) ret_val);
    return NULL;
}

int amrnbplay_read_params(void) {
	struct audiotest_thread_context *context; 
	char *token;
	int ret_val = 0;

	if ((context = get_free_context()) == NULL) {
		ret_val = -1;
	} else {
                struct audio_pvt_data *audio_data;
                audio_data = (struct audio_pvt_data *) malloc(sizeof(struct audio_pvt_data));
                if(!audio_data) {
                        printf("error allocating audio instance structure \n");
                        free_context(context);
                        ret_val = -1;
                } else {
                        printf(" Created audio instance 0x%8x \n",(unsigned int) audio_data);
                        memset(audio_data, 0, sizeof(struct audio_pvt_data));
                        #ifdef _ANDROID_
                        audio_data->outfile = "/data/pcm.wav";
                        #else
                        audio_data->outfile = "/tmp/pcm.wav";
                        #endif
			audio_data->repeat = 0;
			audio_data->quit = 0;
			context->config.file_name = "/data/data.amr"; 
			context->type = AUDIOTEST_TEST_MOD_AMRNB_DEC;
			audio_data->mode = 0;
			
			token = strtok(NULL, " "); 
			while (token != NULL) {
				if (!memcmp(token,"-id=", (sizeof("-id=") - 1))) {
					context->cxt_id = atoi(&token[sizeof("-id=") - 1]);
				} else if
				(!memcmp(token, "-mode=", (sizeof("-mode=") - 1))) {
					audio_data->mode = atoi(&token[sizeof("-mode=") - 1]);
				} else if (!memcmp(token, "-out=",
                                        (sizeof("-out=") - 1))) {
                                        audio_data->outfile = token + (sizeof("-out=")-1);
				} else if (!memcmp(token, "-repeat=",
					(sizeof("-repeat=") - 1))) {
					audio_data->repeat = atoi(&token[sizeof("-repeat=") - 1]);
					if (audio_data->repeat == 0)
						audio_data->repeat = -1;
					else
						audio_data->repeat--;
                                } else {
					context->config.file_name = token;
				}   
				token = strtok(NULL, " ");
			}

			context->config.private_data = (struct audio_pvt_data *) audio_data;
			pthread_create( &context->thread, NULL, playamrnb_thread, (void*) context);
		}
	}
	return ret_val;
}

int amrnbrec_read_params(void) {
	struct audiotest_thread_context *context;
	char *token;
	int ret_val = 0;

	if ((context = get_free_context()) == NULL) {
		ret_val = -1;
	} else {
		context->config.file_name = "/data/record.raw";
		context->config.channel_mode = 0; /* DTX off */
		context->config.sample_rate =  7; /* 12.2 Kbps */

		token = strtok(NULL, " ");

		while (token != NULL) {
			printf("%s \n", token);
			if (!memcmp(token,"-id=", (sizeof("-id=") - 1))) {
				context->cxt_id = atoi(&token[sizeof("-id=") - 1]);
			} else if (!memcmp(token,"-dtx=", (sizeof("-dtx=") - 1))) {
				context->config.channel_mode =  atoi(&token[sizeof("-dtx=") - 1]);
			} else if (!memcmp(token,"-rate=", (sizeof("-rate=") - 1))) {
				context->config.sample_rate =  atoi(&token[sizeof("-rate=") - 1]);
			} else {
				context->config.file_name = token;
			}
			token = strtok(NULL, " ");
		}
		context->type = AUDIOTEST_TEST_MOD_AMRNB_ENC;
		pthread_create( &context->thread, NULL, recamrnb_thread, (void*) context);
	}

	return ret_val;
}

int amrnb_play_control_handler(void* private_data) {
	int drvfd , ret_val = 0;
	int volume;
        struct audio_pvt_data *audio_data = (struct audio_pvt_data *) private_data;
	char *token;

	token = strtok(NULL, " ");
	if ((private_data != NULL) && 
		(token != NULL)) {
		drvfd = audio_data->afd;
		if(!memcmp(token,"-cmd=", (sizeof("-cmd=") -1))) {
			token = &token[sizeof("-cmd=") - 1];
			printf("%s: cmd %s\n", __FUNCTION__, token);
			if (!strcmp(token, "pause")) {
				ioctl(drvfd, AUDIO_PAUSE, 1);
			} else if (!strcmp(token, "resume")) {
				ioctl(drvfd, AUDIO_PAUSE, 0);
#if defined(QC_PROP) && defined(AUDIOV2)
			} else if (!strcmp(token, "volume")) {
				int rc;
				unsigned short dec_id;
				token = strtok(NULL, " ");
				if (!memcmp(token, "-value=",
					(sizeof("-value=") - 1))) {
					volume = atoi(&token[sizeof("-value=") - 1]);
					if (ioctl(drvfd, AUDIO_GET_SESSION_ID, &dec_id)) {
						perror("could not get decoder session id\n");
					} else {
						printf("session %d - volume %d \n", dec_id, volume);
						rc = msm_set_volume(dec_id, volume);
						printf("session volume result %d\n", rc);
					}
				}
#else
			} else if (!strcmp(token, "volume")) {
				token = strtok(NULL, " ");
				if (!memcmp(token, "-value=",
					(sizeof("-value=") - 1))) {
					volume =
					atoi(&token[sizeof("-value=") - 1]);
					ioctl(drvfd, AUDIO_SET_VOLUME, volume);
					printf("volume:%d\n", volume);
				}
#endif
			} else if (!strcmp(token, "flush")) {
				audio_data->avail = audio_data->org_avail;
				audio_data->next  = audio_data->org_next;
				ioctl(drvfd, AUDIO_FLUSH, 0);
				printf("flush\n");
			} else if (!strcmp(token, "quit")) {
                                audio_data->quit = 1;
                                printf("quit session\n");
			}
		}
	} else {
		ret_val = -1;
	}

	return ret_val;
}

int amrnb_rec_control_handler(void* private_data) {
	int /* drvfd ,*/ ret_val = 0;
	char *token;

	token = strtok(NULL, " ");
	if ((private_data != NULL) &&
		(token != NULL)) {
		/* drvfd = (int) private_data */
		if(!memcmp(token,"-cmd=", (sizeof("-cmd=") -1))) {
			token = &token[sizeof("-id=") - 1];
			printf("%s: cmd %s\n", __FUNCTION__, token);
		}
	} else {
		ret_val = -1;
	}

	return ret_val;
}

const char *amrnbplay_help_txt = 
	"Play amrnb file: type \n\
echo \"playamrnb path_of_file -id=xxx -mode=x -out=path_of_outfile\" > %s \n\
mode= 0(tunnel mode) or 1 (non-tunnel mode) \n\
Supported control command: pause, resume, volume, flush, quit \n ";

void amrnbplay_help_menu(void) {
	printf(amrnbplay_help_txt, cmdfile);
}

const char *amrnbrec_help_txt =
"Record amrnb: type \n\
echo \"recamrnb path_of_file -rate=yyyy -dtx=zz -id=xxx\" > %s \n\
dtx= 0(off) or 65535(on) -repeat=x \n\
rate= 0(MR475),1(MR515),2(MR59),3(MR67),4(MR74),5(MR795),6(MR102),7(MR122) \n\
Repeat 'x' no. of times, repeat infinitely if repeat = 0\n\
Supported control command: N/A \n ";

void amrnbrec_help_menu(void) {
	printf(amrnbrec_help_txt, cmdfile);
}
