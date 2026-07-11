/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <getopt.h>
#include <linux/types.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <limits.h>

/* Local defines */
#ifndef min
	#define min(a,b) (((a)>(b))?(b):(a))
#endif

typedef unsigned char bool;

#define TRUE 1
#define FALSE 0

/* device to pull random data from */
#define RANDOM_DEVICE       "/dev/random"

/* Buffer for reading random data */
#define MAX_BUFFER 8192
static unsigned char databuf[MAX_BUFFER];

/* Random data check buffer */
static unsigned int rnd_ctr[256];

/* Return codes */
#define EXIT_NO_ERRROR                   0
#define EXIT_BAD_PARAMETER              -1
#define EXIT_COULD_NOT_OPEN_DEVICE      -2
#define EXIT_COULD_NOT_READ_DEVICE      -3
#define EXIT_TIMED_OUT_READING_DEVICE   -4
#define EXIT_RANDOM_TEST_FAILED         -5
#define POLL_TIMEOUT 2000
#define NUM_FILE_DESCRIPTORS 1
#define NUM_ADVERSE_FAIL_COUNT 3
#define NUM_TEST_INTERATIONS 100
#define REPEAT_INTERATIONS 500
#define MAX_NUM_THREADS 10

/* Version number of this source */
#define APP_VERSION "1.00b"
#define APP_NAME    "qrngtest"

static int nominal_test(void);
static int repeat_test(void);
static int stress_test(void);
static int continuous_test(void);
static int adversarial_test(void);

const char *program_version =
APP_NAME " " APP_VERSION "\n"
	"Copyright (c) 2016, The Linux Foundation. All rights reserved.\n"
	"This is free software; see the source for copying conditions.  There is NO\n"
	"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n\n";

enum test_types {
	NOMINAL,
	ADVERSARIAL,
	REPEAT,
	STRESS,
	CONTINUOUS,
	LAST_TEST,
};

struct option testopts[] = {
	{"nominal", no_argument, NULL, 'n'},
	{"adversarial", no_argument, NULL, 'a'},
	{"repeatability", no_argument, NULL, 'r'},
	{"stress", no_argument, NULL, 's'},
	{"continuous", no_argument, NULL, 'c'},
	{"device", required_argument, NULL, 'd'},
	{"verbose", no_argument, NULL, 'v'},
	{"help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0},
};

static int (*test_func[]) (void) = {
	[NOMINAL] 	= nominal_test,
	[ADVERSARIAL] 	= adversarial_test,
	[REPEAT] 	= repeat_test,
	[STRESS] 	= stress_test,
	[CONTINUOUS] 	= continuous_test,
};

static char input_device_name[128];
static int verbose = 0;
static int continuous = 0;

const char *program_usage =
	"Usage: " APP_NAME " [OPTION...]\n"
	"  -n                 run nominal tests\n"
	"  -a                 run adversarial tests\n"
	"  -r                 runs the nominal and adversarial tests\n"
	"  -s                 nominal test runs by multiple threads\n"
	"  -d <device name>   random input device (default: /dev/random)\n"
	"  -c                 Nominal tests runs infinitely\n"
	"  -v                 extra debug messages\n"
	"  -h                 help (this page)\n";

static void title(void)
{
	printf("%s", program_version);
}

static void usage(void)
{
	printf("%s", program_usage);
}


/* Read (non-blocking) data from the random device */
static int read_src(int fd, void *buf, size_t size)
{
	char *chr = (char *) buf;
	struct pollfd fds[1];			/* used for polling file descriptor  */
	size_t offset = 0;
	ssize_t ret_size;
	int ret;

	/* setup poll() data */
	memset(fds, 0 , sizeof(fds));
	fds[0].fd = fd;
	fds[0].events = POLLIN;

	/* must be a valid size */
	if (!size || (size > SSIZE_MAX)) {
		if (verbose)
			printf("Invalid size passed\n");

		return EXIT_COULD_NOT_READ_DEVICE;
	}

	/* read buffer of data */
	do {
		ret = poll(fds, NUM_FILE_DESCRIPTORS, POLL_TIMEOUT);
		if (ret == 0)
			return EXIT_TIMED_OUT_READING_DEVICE;	/* no data to read */

		ret_size = read(fd, chr + offset, size);
		/* any read failure is bad */
		if (ret_size == EXIT_BAD_PARAMETER)
			break;
		size -= ret_size;
		offset += ret_size;
	} while (size > 0);

	/* should have read in all of requested data */
	if (size > 0) {
		if (verbose)
			printf("Unable to read the requested data\n");

		return EXIT_COULD_NOT_READ_DEVICE;
	}
	return 0;
}

static int nominal(void)
{
	int iterations = NUM_TEST_INTERATIONS;
	int exitval = EXIT_NO_ERRROR;
	bool pass_test = TRUE;
	bool pass_itr = TRUE;
	int random_fd = 0;
	int done = FALSE;
	int i = 0;
	int ret;

	if (verbose)
		printf("device name %s\n", input_device_name);

	/* open random device */
	random_fd = open(input_device_name, O_RDONLY);
	if (random_fd < 0) {
		printf("Can't open random device file %s\n", input_device_name);
		exitval = EXIT_COULD_NOT_OPEN_DEVICE;
		goto exit;
	}

	if (verbose)
		printf("Testing random numbers, one period displayed for every good %dKB read:\n",
			MAX_BUFFER / 1024);

	/* run the test */
	do {
		pass_itr = TRUE;
		/* clear random number checking buffer */
		memset(rnd_ctr, 0, 256);

		/* read a buffer of random numbers */
		exitval = read_src(random_fd, databuf, MAX_BUFFER);
		if (exitval == EXIT_COULD_NOT_READ_DEVICE) {
			printf("\nError reading data!\n");
			goto exit;
		}
		if (exitval == EXIT_TIMED_OUT_READING_DEVICE) {
			printf("\nTimed out reading data!\n");
			goto exit;
		}

		/* count random numbers */
		for (i = 0; i < MAX_BUFFER; ++i)
			rnd_ctr[databuf[i]]++;

		/* check random numbers to make sure they are not bogus */
		for (i = 0; i < 256; ++i) {
			if (rnd_ctr[i] == 0) {
				pass_itr = FALSE;
				pass_test = FALSE;
			}
		}

		/* display results */
		if (verbose) {
			if (pass_itr)
				fprintf(stderr, ".");
			else
				fprintf(stderr, "*");
		}

		if (!continuous) {
			if (--iterations == 0)
				done = TRUE;
		}
	} while (!done);

	if (verbose)
		printf("\n");

	if (!pass_test)
		exitval = EXIT_RANDOM_TEST_FAILED;

exit:
	close(random_fd);
	return exitval;
}

static int adverse(void)
{
	int random_fd = 0;
	int err_cnt = 0;
	int exitval = 0;
	int rc = 0;

	/* open random device */
        random_fd = open(input_device_name, O_RDONLY);
        if (random_fd < 0) {
                printf("Can't open random device file %s\n", input_device_name);
                return EXIT_COULD_NOT_OPEN_DEVICE;
        }

	/* read a buffer of random numbers */
	exitval = read_src(random_fd, NULL, MAX_BUFFER);
	if (exitval) {
		if (verbose)
			printf("Read data with NULL buffer pointer failed %d\n",
					exitval);
		err_cnt++;
	}

	exitval = read_src(random_fd, databuf, 0);
	if (exitval) {
		if (verbose)
			printf("Read data with 0 size failed %d\n",
					exitval);
		err_cnt++;
	}

	exitval = read_src(random_fd, databuf, -1);
	if (exitval) {
		if (verbose)
			printf("Read data with negative size failed %d\n",
					exitval);
		err_cnt++;
	}

	if (NUM_ADVERSE_FAIL_COUNT != err_cnt)
		rc = err_cnt;

	close(random_fd);
	return rc;
}

static int adversarial_test(void)
{
	int rc = 0;

	printf("\nRunning Adversarial Test\n");
	rc = adverse();
	if (rc || (EXIT_COULD_NOT_OPEN_DEVICE == rc)) {
		printf("\nQRNG adversarial test failed rc %d\n", rc);
		rc = -1;
	} else {
		printf("\nQRNG adversarial test passed\n");
	}

	return rc;
}
static int nominal_test(void)
{
	int rc = 0;

	printf("\nRunning nominal test...\n");

	rc = nominal();

	if (!rc)
		printf("\nQRNG Nominal test passed...\n");
	else
		printf("\nQRNG Nominal test failed...\n");

	return rc;
}

static int repeat_test(void)
{
	int errors = 0;
	int i;

	printf("\nRunning Repeatabilty Test......\n");

	if (verbose)
		printf("Iterations: %d\n\n", REPEAT_INTERATIONS);

	for (i = 0; i < REPEAT_INTERATIONS; i++) {
		errors += nominal();
		errors += adverse();
	}

	if (errors) {
		printf("\nQRNG Repeat test Failed\n");
		if (verbose)
			printf("Number of failures in %s test: 0x%x\n",
					testopts[REPEAT].name, errors);
	} else {
		printf("\nQRNG Repeat test Passed\n");
	}
	return errors;
}

static void *rep_threadfn(void *thargs)
{
	int ret = 0;
	ret = nominal();

	if (ret)
		printf("\nError : %d\n", ret);

	return NULL;
}

static int stress_test(void)
{
	pthread_t thread[MAX_NUM_THREADS];
	int iret[MAX_NUM_THREADS];
	int ret = 0;
	int i, j;

	printf("\nRunning Stress test...\n");

	for (i = 0; i < MAX_NUM_THREADS; i++) {
		iret[i] = pthread_create(&thread[i], NULL, rep_threadfn, NULL);
		if (iret[i]) {
			printf("\nFailed to create %d thread for stress "
					"test..\n", i);
			for (j = 0; j < i; j++)
				pthread_join(thread[j], NULL);
			printf("\nStress test failed in thread creation\n");
			ret = -1;
			return ret;
		}
		if (verbose)
			printf("\nRunning test thread %d\n", i);
	}

	for (i = 0; i < MAX_NUM_THREADS; i++)
		pthread_join(thread[i], NULL);

	printf("\nQRNG Stress test passed...\n");
	return ret;
}

static int continuous_test(void)
{
	int rc = 0;

	printf("\nRunning Continous test...\n");
	continuous = 1;

	rc = nominal();

	return rc;
}

static unsigned int parse_command(int argc, char *const argv[])
{
	unsigned int ret = 0;
	int help_cmd = 0;
	int command = 0;

	while ((command = getopt_long(argc, argv, "vnasrd:hc", testopts,
					NULL)) != -1) {
		switch (command) {
			case 'v':
				verbose = 1;
				break;
			case 'n':
				ret = 1 << NOMINAL;
				break;
			case 'a':
				ret |= 1 << ADVERSARIAL;
				break;
			case 'r':
				ret |= 1 << REPEAT;
				break;
			case 's':
				ret |= 1 << STRESS;
				break;
			case 'c':
				ret |= 1 << CONTINUOUS;
				break;
			case 'd':
				strncpy (input_device_name, optarg, sizeof(input_device_name));
				ret = 1 << NOMINAL;
				break;
			case 'h':
			default:
				help_cmd = 1;
				usage();
				break;
		}
	}
	/* When no option is given, nominal should run*/
	if ((ret == 0) && (!help_cmd))
		ret = 1 << NOMINAL;

	return ret;
}

int main(int argc, char **argv)
{
	int exitval = EXIT_NO_ERRROR;
	unsigned int test_mask;
	bool pass_test = TRUE;
	int pass_itr;
	int rc = 0;
	int i;

	strncpy(input_device_name, RANDOM_DEVICE, sizeof(RANDOM_DEVICE));

	title();

	test_mask = parse_command(argc, argv);

	for (i = 0; i < LAST_TEST; i++) {
		/* Look for the test that was selected */
		if (!(test_mask & (1U << i)))
			continue;

		/* This test was selected, so run it */
		rc = test_func[i]();
		if (rc)
			exitval = EXIT_RANDOM_TEST_FAILED;
	}
	return exitval;
}
