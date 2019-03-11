/*
 * stdout output driver. This file is part of Shairport Sync.
 * Copyright (c) Mike Brady 2015
 *
 * Based on pipe output driver
 * Copyright (c) James Laird 2013
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "audio.h"
#include "common.h"
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <nats/nats.h>
#include <jansson.h>

natsConnection      *nc  = NULL;

static void start( int sample_rate, int sample_format) {
  json_t *stream_format = json_object();
  json_object_set_new(stream_format, "rate", json_integer(sample_rate));
  json_object_set_new(stream_format, "format", json_integer(sample_format));
  char * jsonarray_serialized = json_dumps(stream_format, JSON_ENCODE_ANY);
  natsConnection_Publish(nc, "system.audio.shairport-sync.start", (const void*) jsonarray_serialized, strlen(jsonarray_serialized));
}

static void play(void *buf, int samples) {
  natsConnection_Publish(nc, "system.audio.shairport-sync.play", (const void*) buf, samples);
}

static void stop(void) {
  natsConnection_Publish(nc, "system.audio.shairport-sync.stop", (const void*) NULL, 0);
}

static int init(__attribute__((unused)) int argc, __attribute__((unused)) char **argv) {
  // set up default values first
  config.audio_backend_buffer_desired_length = 1.0;
  config.audio_backend_latency_offset = 0;

  // get settings from settings file
  // do the "general" audio  options. Note, these options are in the "general" stanza!
  parse_general_audio_options();

  // start nats connection
  // Start the framework
  const char* vbus_url = getenv("VBUS_URL");
	if (natsConnection_ConnectTo(&nc, vbus_url) == NATS_OK) {
		printf("main: Start nats on %s\n", vbus_url);
	}
	else {
		printf("ERROR main:starting framework\n");
	}

  // create vbus element structure
  json_t *array = json_object();
  json_object_set(array, "path", json_string("system.audio.shairport-sync"));
  json_object_set(array, "name", json_string("shairport-sync"));
  json_object_set(array, "uuid", json_string("shairport-sync"));

  json_t *array_tags = json_array();
	json_array_append_new(array_tags, json_string("audio"));
	json_array_append_new(array_tags, json_string("source"));
	json_array_append_new(array_tags, json_string("airplay"));
	json_object_set(array, "tags", array_tags);

  json_t *array_pub = json_array();
	json_t *array_start = json_object();
	json_object_set_new(array_start, "methode", json_string("start"));
	json_object_set_new(array_start, "format", json_string("json"));
	json_object_set_new(array_start, "formula", json_string("{'format':3, 'rate':44100}"));
	json_array_append_new(array_pub, array_start);

	json_t *array_play = json_object();
	json_object_set_new(array_play, "methode", json_string("play"));
	json_object_set_new(array_play, "format", json_string("binary"));
	json_array_append_new(array_pub, array_play);

	json_t *array_stop = json_object();
	json_object_set_new(array_stop, "methode", json_string("stop"));
	json_object_set_new(array_stop, "format", json_string("none"));
	json_array_append_new(array_pub, array_stop);

	json_object_set(array, "publish", array_pub);

	// serialize json array for nats
	char * jsonarray_serialized = json_dumps(array, JSON_ENCODE_ANY);
	natsConnection_Publish(nc, "system.db.newElement", (const void*) jsonarray_serialized, strlen(jsonarray_serialized));

  return 0;
}

static void deinit(void) {
  // don't close stdout
}

static void help(void) { printf("    stdout takes no arguments\n"); }

audio_output audio_vbus = {.name = "vbus-source",
                             .help = &help,
                             .init = &init,
                             .deinit = &deinit,
                             .start = &start,
                             .stop = &stop,
                             .flush = NULL,
                             .delay = NULL,
                             .play = &play,
                             .volume = NULL,
                             .parameters = NULL,
                             .mute = NULL};
