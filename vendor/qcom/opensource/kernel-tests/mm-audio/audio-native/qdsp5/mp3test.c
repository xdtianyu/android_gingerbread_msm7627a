/* mp3test.c - native MP3 test application
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
#include <errno.h>
#include "audiotest_def.h"
#include <pthread.h>
#include <sys/ioctl.h>
#include <linux/msm_audio.h>

#define EOS 1

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
	{'f', 'm', 't', ' '}, 16, 1, 2, 48000, 96000, 4,
	16, {'d', 'a', 't', 'a'}, 0
	};

typedef struct TIMESTAMP{
	unsigned long LowPart;
	unsigned long HighPart;
} __attribute__ ((packed)) TIMESTAMP;

struct meta_in{
	unsigned short offset;
	TIMESTAMP ntimestamp;
	unsigned int nflags;
} __attribute__ ((packed));

struct meta_out{
	unsigned short offset;
	TIMESTAMP ntimestamp;
	unsigned int nflags;
	unsigned short errflag;
	unsigned short sample_frequency;
	unsigned short channel;
	unsigned int tick_count;
} __attribute__ ((packed));

#ifdef _ANDROID_
static const char *cmdfile = "/data/audio_test";
#else
static const char *cmdfile = "/tmp/audio_test";
#endif

static void create_wav_header(int Datasize)
{
	append_header.Chunk_size = Datasize + 8 + 16 + 12;
	append_header.Chunk_data_size = Datasize;
}

static void *mp3_dec(void *arg)
{
	struct meta_out meta;
	int fd, ret_val = 0;
	struct audio_pvt_data *audio_data = (struct audio_pvt_data *) arg;
	int afd = audio_data->afd;
	unsigned long long *time;
	int len, total_len;
	len = 0;
	total_len = 0;

	fd = open(audio_data->outfile, O_RDWR | O_CREAT,
			S_IRWXU | S_IRWXG | S_IRWXO);
	if (fd < 0) {
		printf("Err while opening file decoder \
			output file: %s\n", audio_data->outfile);
		pthread_exit((void *)ret_val);
	}

	printf(" mp3_read Thread, recsize=%d \n",audio_data->recsize);

	lseek(fd, 44, SEEK_SET);	/* Set Space for Wave Header */
	do {
		if (audio_data->bitstream_error == 1) {
			printf("Bitstream error notified, exit read thread\n");
			break;
		}
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
			if ((audio_data->flush_enable == 1 ||
				audio_data->outport_flush_enable == 1)
				&& errno == EBUSY) {
				printf("Flush in progress\n");
				usleep(5000);
				continue;
			} else {
				printf(" error reading the PCM samples \n");
				goto fail;
			}
		} else if (len != 0) {
			memcpy(&meta, audio_data->recbuf, sizeof(struct meta_out));
			time = (unsigned long long *)(audio_data->recbuf + 2);
			meta.ntimestamp.LowPart = (*time & 0xFFFFFFFF);
			meta.ntimestamp.HighPart = ((*time >> 32) & 0xFFFFFFFF);
			#ifdef DEBUG_LOCAL
			printf("Meta_out High part is %lu\n",
					meta.ntimestamp.HighPart);
			printf("Meta_out Low part is %lu\n",
					meta.ntimestamp.LowPart);
			printf("Meta Out Timestamp: %llu\n",
					(((unsigned long long)meta.ntimestamp.HighPart << 32)
					 + meta.ntimestamp.LowPart));
			#endif
			if (meta.nflags == EOS) {
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
        printf(" end of recording PCM samples\n");
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
			} else if
			(suspend_event.event_type == AUDIO_EVENT_STREAM_INFO) {
				printf("event_notify: STREAM_INFO EVENT FROM \
					DRIVER:%d\n", suspend_event.event_type);
				printf("codec_type : %d\nchan_info : %d\n\
					sample_rate : %d\nstream_info: %d\n",
				 suspend_event.event_payload.stream_info.codec_type,
				 suspend_event.event_payload.stream_info.chan_info,
				 suspend_event.event_payload.stream_info.sample_rate,
				 suspend_event.event_payload.stream_info.bit_stream_info);
				#ifdef AUDIOV2
				if (audio_data->mode) {
					audio_data->outport_flush_enable = 1;
					ioctl(afd, AUDIO_OUTPORT_FLUSH, 0);
					audio_data->outport_flush_enable = 0;
				}
				#endif
			} else if
			(suspend_event.event_type ==
				AUDIO_EVENT_BITSTREAM_ERROR_INFO) {
				printf("event_notify: BITSTREAM ERROR EVENT \
					FROM DRIVER:%d\n",
					suspend_event.event_type);
				printf("BITSTREAM ERROR:\n codec_type : %d\n \
					error_count : %d\n error_type : %d\n",
					 suspend_event.event_payload.error_info.dec_id,
					 (0x0000FFFF &
					suspend_event.event_payload.error_info.err_msg_indicator),
					 suspend_event.event_payload.error_info.err_type);
				#ifdef AUDIOV2
				audio_data->bitstream_error = 1;
				#endif
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
	int sz;
	char *buf; 
	int afd;
	int cntW=0;
	int ret = 0;
	struct audio_pvt_data *audio_data = (struct audio_pvt_data *) clnt_config->private_data;
#ifdef AUDIOV2
	unsigned short dec_id = 0;
#endif

	if (audio_data->mode)   {
		printf("non tunnel mode open\n");
		afd = open("/dev/msm_mp3", O_RDWR);
	}else {
		printf("tunnel mode open\n");
		afd = open("/dev/msm_mp3", O_WRONLY);
	}

	if (afd < 0) {
		perror("mp3_play: cannot open MP3 device");
		return -1;
	}

	audio_data->afd = afd; /* Store */

#ifdef AUDIOV2
	if (!audio_data->mode)   {
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
	pthread_create(&event_th, NULL, event_notify, (void *) audio_data);

	if (ioctl(afd, AUDIO_GET_CONFIG, &config)) {
		perror("could not get config");
		ret = -1;
		goto err_state;
	}

	if (audio_data->mode) {
		config.meta_field = 1;
		#ifdef AUDIOV2
		if (ioctl(afd, AUDIO_SET_ERR_THRESHOLD_VALUE, &audio_data->err_threshold_value)) {
			perror("could not set error threshold value");
			ret = -1;
			goto err_state;
		}
		#endif
	}

	config.sample_rate = clnt_config->sample_rate;
	printf(" in initate_play, sample_rate=%d\n", config.sample_rate);
	if (ioctl(afd, AUDIO_SET_CONFIG, &config)) {
		perror("could not set config");
		ret = -1;
		goto err_state;
	}

	buf = (char*) malloc(sizeof(char) * config.buffer_size);
	if (buf == NULL) {
		perror("fail to allocate buffer\n");
		ret = -1;
		goto err_state;
	}

	if (audio_data->mode)
		config.buffer_size =
			(config.buffer_size - sizeof(struct meta_in));

	printf("initiate_play: buffer_size=%d, buffer_count=%d, sample_rate=%d\n", config.buffer_size,
		   config.buffer_count, config.sample_rate);

	fprintf(stderr,"prefill\n");

	       if (audio_data->mode) {
                /* non - tunnel portion */
                struct msm_audio_pcm_config config_rec;
                printf(" selected non-tunnel part\n");
		append_header.Sample_rate = clnt_config->sample_rate;
		append_header.Number_Channels = clnt_config->channel_mode;
		append_header.Bytes_Sec = append_header.Sample_rate *
			append_header.Number_Channels * 2;
		append_header.Block_align = append_header.Number_Channels * 2;
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
			ret  = -1;
			goto err_state;
		}
                pthread_create(&thread, NULL, mp3_dec, (void *) audio_data);

        }

	for (n = 0; n < config.buffer_count; n++) {
		if ((sz = fill(buf, config.buffer_size, cookie)) < 0)
			break;
		if (write(afd, buf, sz) != sz)
			break;
	}
	cntW=cntW+config.buffer_count; 

	fprintf(stderr,"start playback\n");
	if (ioctl(afd, AUDIO_START, 0) >= 0) {
		for (;audio_data->bitstream_error != 1;) {
#if 0
			if (ioctl(afd, AUDIO_GET_STATS, &stats) == 0)
				fprintf(stderr,"%10d\n", stats.out_bytes);
#endif
			if (((sz = fill(buf, config.buffer_size, cookie)) < 0) ||
				(audio_data->quit == 1) || audio_data->bitstream_error) {
				if(audio_data->bitstream_error == 1)
					break;
				if ((audio_data->repeat == 0) || (audio_data->quit == 1)) {
					printf(" File reached end or quit cmd issued, exit loop \n");
					if (audio_data->mode) {
						struct meta_in meta;
						meta.offset =
							sizeof(struct meta_in);
						meta.ntimestamp.LowPart =
						((audio_data->frame_count * 20000) & 0xFFFFFFFF);
						meta.ntimestamp.HighPart =
						(((unsigned long long)(audio_data->frame_count
						* 20000) >> 32) & 0xFFFFFFFF);
						meta.nflags = EOS;
						#ifdef DEBUG_LOCAL
						printf("Meta In High part is %lu\n",
							meta.ntimestamp.HighPart);
						printf("Meta In Low part is %lu\n",
							meta.ntimestamp.LowPart);
						printf("Meta In ntimestamp: %llu\n",
						(((unsigned long long)meta.ntimestamp.HighPart << 32)
						 + meta.ntimestamp.LowPart));
						#endif
						memset(buf, 0,
							(sizeof(config.buffer_size) +
							 sizeof(struct meta_in)));
						memcpy(buf, &meta,
							sizeof(struct meta_in));
						if (write(afd, buf,
						sizeof(struct meta_in)) < 0)
							printf(" writing buffer\
							for EOS failed\n");
					} else {
						printf("FSYNC: Reached end of \
							file, calling fsync\n");
						while (fsync(afd) < 0) {
							printf(" fsync failed\n");
							sleep(1);
						}
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
			if (audio_data->suspend == 1) {
				printf("enter suspend mode\n");
				ioctl(afd, AUDIO_STOP, 0);
				while (audio_data->suspend == 1)
					sleep(1);
				ioctl(afd, AUDIO_START, 0);
				printf("exit suspend mode\n");
			}
			if (write(afd, buf, sz) != sz) {
				if (audio_data->flush_enable == 1 && errno == EBUSY) {
					printf("Flush in progress\n");
					while (write(afd, buf, sz) < 0)
						usleep(10000);
					audio_data->avail = audio_data->org_avail;
					audio_data->next  = audio_data->org_next;
					audio_data->flush_enable = 0;
					printf("Flush done");
					continue;
				}
				printf(" write return not equal to sz, exit loop\n");
				break;
			} else {
				cntW++;
				printf(" mp3_play: instance=%d repeat_cont=%d cntW=%d\n",
						(int) audio_data, audio_data->repeat, cntW);
			}
		}
		ioctl(afd, AUDIO_ABORT_GET_EVENT, 0);
		usleep(900 * 1000);
		printf("end of mp3 play\n");
	} else {
		printf("mp3_play: Unable to start driver\n");
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

/* http://ccrma.stanford.edu/courses/422/projects/WaveFormat/ */

static int fill_buffer(void *buf, unsigned sz, void *cookie)
{
	struct meta_in meta;
	struct audio_pvt_data *audio_data = (struct audio_pvt_data *) cookie;
	unsigned cpy_size = (sz < audio_data->avail?sz:audio_data->avail);
	
	if (audio_data->avail == 0) {
		return -1;
	}

	if (audio_data->mode) {
		meta.ntimestamp.LowPart = ((audio_data->frame_count * 20000) & 0xFFFFFFFF);
		meta.ntimestamp.HighPart = (((unsigned long long)(audio_data->frame_count
						* 20000) >> 32) & 0xFFFFFFFF);
		meta.offset = sizeof(struct meta_in);
		meta.nflags = 0;
		audio_data->frame_count++;
		#ifdef DEBUG_LOCAL
		printf("Meta In High part is %lu\n",
				meta.ntimestamp.HighPart);
		printf("Meta In Low part is %lu\n",
				meta.ntimestamp.LowPart);
		printf("Meta In ntimestamp: %llu\n", (((unsigned long long)meta.ntimestamp.HighPart << 32) +
					meta.ntimestamp.LowPart));
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


	printf(" play_file: count=%d,next=%p\n", count, audio_data->next);
	if (read(fd, audio_data->next, count) != (ssize_t) count) {
		fprintf(stderr,"could not read %d bytes\n", count);
		free(content_buf);
		return -1;
	}

	printf(" play_file: count=%d,next=%d\n", count, (int) audio_data->next);
	audio_data->avail = count;
	audio_data->org_avail = audio_data->avail;


	ret_val =  initiate_play(config, fill_buffer, audio_data);
	free(content_buf);
	return ret_val;
}

int mp3_play(struct audtest_config *config)
{
	struct stat stat_buf;
	int fd;

	if (config == NULL) {
		return -1;
	}

	fd = open(config->file_name, O_RDONLY);

	if (fd < 0) {
		fprintf(stderr, "playmp3: cannot open '%s'\n", config->file_name);
		return -1;
	}

	(void) fstat(fd, &stat_buf);

	return play_file(config, fd, stat_buf.st_size);;
}

void* playmp3_thread(void* arg) {
	struct audiotest_thread_context *context = 
	(struct audiotest_thread_context*) arg;
	int ret_val;

	ret_val = mp3_play(&context->config);
	printf(" Free audio instance 0x%8x \n", (unsigned int) context->config.private_data);
	free(context->config.private_data);
	free_context(context);
	pthread_exit((void*) ret_val);

    return NULL;
}

int mp3play_read_params(void) {
	struct audiotest_thread_context *context; 
	char *token;
	int ret_val = 0;

	if ((context = get_free_context()) == NULL) {
		ret_val = -1;
	} else {
		context->config.sample_rate = 44100;
		context->config.channel_mode = 2;
		context->config.file_name = "/data/data.mp3";
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
			audio_data->err_threshold_value = 1;
			audio_data->bitstream_error = 0;
			audio_data->repeat = 0;
			audio_data->quit = 0;
			context->config.sample_rate = 44100;
			context->config.channel_mode = 2;
			context->config.file_name = "/data/data.mp3";
			context->type = AUDIOTEST_TEST_MOD_MP3_DEC;
                        
			token = strtok(NULL, " ");
			while (token != NULL) {

				if (!memcmp(token,"-rate=", (sizeof("-rate=" - 1)))) {
					context->config.sample_rate = atoi(&token[sizeof("-rate=") - 1]);
				} else if (!memcmp(token,"-id=", (sizeof("-id=" - 1)))) {
					context->cxt_id= atoi(&token[sizeof("-id=") - 1]);
				} else if
				(!memcmp(token, "-mode=", (sizeof("-mode=") - 1))) {
					audio_data->mode = atoi(&token[sizeof("-mode=") - 1]);
				} else if
				(!memcmp(token, "-cmode=", (sizeof("-cmode=") - 1))) {
					context->config.channel_mode =
					atoi(&token[sizeof("-cmode=") - 1]);
				} else if (!memcmp(token, "-out=",
					(sizeof("-out=") - 1))) {
					audio_data->outfile = token + (sizeof("-out=")-1);
				} else if (!memcmp(token, "-err_thr=",
					(sizeof("-err_thr=") - 1))) {
					audio_data->err_threshold_value =
						atoi(&token[sizeof("-err_thr=") - 1]);
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
			printf("%s : sample_rate=%d id=%d\n", __FUNCTION__,
			context->config.sample_rate, context->cxt_id);
			pthread_create( &context->thread, NULL, playmp3_thread,
				(void*) context);
		}
	}
	return 0;
}

int mp3_play_control_handler(void* private_data) {
	int drvfd , ret_val = 0;
	char *token;
	int volume;
	struct audio_pvt_data *audio_data = (struct audio_pvt_data *) private_data;

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
				audio_data->flush_enable = 1;
				ioctl(drvfd, AUDIO_FLUSH, 0);
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

const char *mp3play_help_txt = 
	"Play mp3 file: type \n \
echo \"playmp3 path_of_file -rate=sample_rate -id=xxx \
-mode=x -cmode=x -err_thr=x -repeat=x -out=path_of_outfile\" > %s \n\
Sample rate of MP3 file <= 48000 \n\
Channel mode 1 or 2 \n \
mode= 0(tunnel mode) or 1 (non-tunnel mode) \n\
Repeat 'x' no. of times, repeat infinitely if repeat = 0\n\
Error threshold value 0 to 0x7fff \n \
Supported control command : pause, resume, flush, volume, quit\n ";

void mp3play_help_menu(void) {
	printf(mp3play_help_txt, cmdfile);
}
