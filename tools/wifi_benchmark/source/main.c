#include <3ds.h>
#include <inttypes.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>

#define TEST_BYTES (32u * 1024u * 1024u)
#define RECEIVE_CHUNK (256u * 1024u)
#define HTTP_TIMEOUT_NS (15ULL * 1000ULL * 1000ULL * 1000ULL)

typedef struct {
	volatile u64 bytes;
	volatile bool finished;
	volatile bool cancel_requested;
	Result result;
	u32 status;
	u32 requested;
	u32 range_start;
} DownloadJob;

static LightLock counter_lock;

static void add_bytes(DownloadJob *job, u32 bytes)
{
	LightLock_Lock(&counter_lock);
	job->bytes += bytes;
	LightLock_Unlock(&counter_lock);
}

static Result download_discard(DownloadJob *job)
{
	const char *url = "http://speedtest.tele2.net/100MB.zip";

	httpcContext context;
	Result res = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1);
	if(R_FAILED(res))
		return res;

	httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED);
	httpcAddRequestHeaderField(&context, "User-Agent", "Nocturne-WiFi-Test/1.0");
	httpcAddRequestHeaderField(&context, "Accept-Encoding", "identity");
	httpcAddRequestHeaderField(&context, "Cache-Control", "no-cache");
	char range[64];
	snprintf(range, sizeof(range), "bytes=%" PRIu32 "-%" PRIu32,
		job->range_start, job->range_start + job->requested - 1);
	httpcAddRequestHeaderField(&context, "Range", range);

	res = httpcBeginRequest(&context);
	if(R_FAILED(res))
		goto done;

	res = httpcGetResponseStatusCodeTimeout(&context, &job->status, HTTP_TIMEOUT_NS);
	if(R_FAILED(res))
		goto done;
	if(job->status != 206)
	{
		res = (Result)0xD8A0A008;
		goto done;
	}

	u8 *buffer = (u8 *)memalign(0x1000, RECEIVE_CHUNK);
	if(!buffer)
	{
		res = (Result)0xD860A001;
		goto done;
	}

	do {
		u32 read = 0;
		res = httpcDownloadData(&context, buffer, RECEIVE_CHUNK, &read);
		if(read)
			add_bytes(job, read);
		if(job->cancel_requested)
			break;
	} while(res == (Result)HTTPC_RESULTCODE_DOWNLOADPENDING);

	free(buffer);

done:
	httpcCloseContext(&context);
	return res;
}

static void download_thread(void *arg)
{
	DownloadJob *job = (DownloadJob *)arg;
	job->result = download_discard(job);
	job->finished = true;
}

static void print_header(void)
{
	printf("\x1b[2J\x1b[1;1H");
	printf("\x1b[38;5;218mNOCTURNE WIFI LAB\x1b[0m\n");
	printf("\x1b[90mNetwork-only throughput test\x1b[0m\n\n");
}

static void wait_for_start(void)
{
	print_header();
	printf("Downloads are discarded from RAM.\n");
	printf("The SD card and CIA installer are not used.\n\n");
	printf("\x1b[38;5;218m[A]\x1b[0m  Single connection (32 MiB)\n");
	printf("\x1b[38;5;218m[X]\x1b[0m  Dual connection (2 x 16 MiB)\n");
	printf("\x1b[38;5;218m[START]\x1b[0m  Exit\n\n");
	printf("Use the same Wi-Fi position as Nocturne.\n");
}

static void run_test(unsigned connections)
{
	DownloadJob jobs[2];
	Thread threads[2] = { NULL, NULL };
	memset(jobs, 0, sizeof(jobs));

	u32 bytes_per_job = TEST_BYTES / connections;
	for(unsigned i = 0; i < connections; ++i)
	{
		jobs[i].requested = bytes_per_job;
		jobs[i].range_start = i * bytes_per_job;
		threads[i] = threadCreate(download_thread, &jobs[i], 48 * 1024, 0x30, -2, false);
		if(!threads[i])
		{
			jobs[i].result = (Result)0xD860A002;
			jobs[i].finished = true;
		}
	}

	u64 started = osGetTime();
	u64 previous_time = started;
	u64 previous_bytes = 0;
	double current_mib = 0.0;
	double peak_mib = 0.0;
	bool cancelled = false;

	while(aptMainLoop())
	{
		bool finished = true;
		u64 total = 0;
		for(unsigned i = 0; i < connections; ++i)
		{
			finished &= jobs[i].finished;
			total += jobs[i].bytes;
		}

		u64 now = osGetTime();
		if(now - previous_time >= 350)
		{
			double seconds = (now - previous_time) / 1000.0;
			current_mib = ((double)(total - previous_bytes) / 1048576.0) / seconds;
			if(current_mib > peak_mib)
				peak_mib = current_mib;
			previous_time = now;
			previous_bytes = total;
		}

		double elapsed = (now - started) / 1000.0;
		double average = elapsed > 0.0 ? ((double)total / 1048576.0) / elapsed : 0.0;

		print_header();
		printf("%s test running\n\n", connections == 1 ? "Single-connection" : "Dual-connection");
		printf("Received: %5.1f / 32.0 MiB\n", (double)total / 1048576.0);
		printf("Current:  \x1b[38;5;218m%5.2f MiB/s\x1b[0m\n", current_mib);
		printf("Average:  \x1b[38;5;218m%5.2f MiB/s\x1b[0m\n", average);
		printf("Peak:     %5.2f MiB/s\n\n", peak_mib);
		printf("\x1b[90m[B] Cancel\x1b[0m\n");

		if(finished)
			break;

		hidScanInput();
		if(hidKeysDown() & KEY_B)
		{
			cancelled = true;
			for(unsigned i = 0; i < connections; ++i)
				jobs[i].cancel_requested = true;
			break;
		}

		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}

	for(unsigned i = 0; i < connections; ++i)
	{
		if(threads[i])
		{
			threadJoin(threads[i], U64_MAX);
			threadFree(threads[i]);
		}
	}

	u64 total = 0;
	Result first_error = 0;
	for(unsigned i = 0; i < connections; ++i)
	{
		total += jobs[i].bytes;
		if(R_SUCCEEDED(first_error) && R_FAILED(jobs[i].result))
			first_error = jobs[i].result;
	}
	double elapsed = (osGetTime() - started) / 1000.0;
	double average = elapsed > 0.0 ? ((double)total / 1048576.0) / elapsed : 0.0;

	print_header();
	if(cancelled)
		printf("Test cancelled.\n\n");
	else if(R_FAILED(first_error))
		printf("Test failed: \x1b[31m0x%08" PRIX32 "\x1b[0m\nHTTP status: %" PRIu32 "\n\n",
			(u32)first_error, jobs[0].status);
	else
		printf("%s result\n\n", connections == 1 ? "Single-connection" : "Dual-connection");

	printf("Average: \x1b[38;5;218m%.2f MiB/s\x1b[0m\n", average);
	printf("Peak:    %.2f MiB/s\n", peak_mib);
	printf("Data:    %.1f MiB\n\n", (double)total / 1048576.0);
	if(!cancelled && R_SUCCEEDED(first_error))
	{
		printf("Compare Average with Nocturne's\n");
		printf("download speed. A large gap means\n");
		printf("installation/SD is limiting it.\n\n");
	}
	printf("[A] Return to menu\n");

	while(aptMainLoop())
	{
		hidScanInput();
		if(hidKeysDown() & (KEY_A | KEY_B | KEY_START))
			break;
		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();
	}
}

int main(void)
{
	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);
	LightLock_Init(&counter_lock);

	Result res = acInit();
	if(R_SUCCEEDED(res))
		res = httpcInit(4 * 1024 * 1024);

	if(R_FAILED(res))
	{
		print_header();
		printf("Network initialization failed:\n0x%08" PRIX32 "\n", (u32)res);
		while(aptMainLoop())
		{
			hidScanInput();
			if(hidKeysDown() & KEY_START)
				break;
			gspWaitForVBlank();
		}
	}
	else
	{
		bool running = true;
		while(running && aptMainLoop())
		{
			wait_for_start();
			while(aptMainLoop())
			{
				hidScanInput();
				u32 down = hidKeysDown();
				if(down & KEY_A)
				{
					run_test(1);
					break;
				}
				if(down & KEY_X)
				{
					run_test(2);
					break;
				}
				if(down & KEY_START)
				{
					running = false;
					break;
				}
				gfxFlushBuffers();
				gfxSwapBuffers();
				gspWaitForVBlank();
			}
		}
		httpcExit();
		acExit();
	}

	gfxExit();
	return 0;
}
