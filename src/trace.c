#include "trace.h"
#include "queue.h"
#include "semaphore.h"
#include "timer.h"
#include "timer_object.h"
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#define WIN32_LEAN_AND_MEAN
#include<windows.h>

typedef struct trace_t 
{
	//Queue to order events
	queue_t* queue;
	//The heap that the trace is in
	heap_t* heap;
	//Semaphore for threads
	semaphore_t* sem;
	//Timer object
	timer_object_t* timer;
	//Max number of events the trace can hold
	int eventCapacity;
	//0 for false, 1 for start
	int startCapture;
	//File name when capture starts
	char* path;
	//Buffer for the events
	char* buffer; 
} trace_t;

typedef struct event_t
{
	//Thread ID of Event
	int tid;
	//Event Name
	char* name;
} event_t;

trace_t* trace_create(heap_t* heap, int event_capacity)
{
	//Make a Trace object
	trace_t* trace = heap_alloc(heap, sizeof(trace_t), 8);
	trace->heap = heap;
	trace->queue = queue_create(trace->heap, event_capacity);
	trace->sem = semaphore_create(1, 1);
	trace->timer = timer_object_create(heap, NULL);
	trace->eventCapacity = event_capacity;
	trace->startCapture = 0;
	trace->path = NULL;
	trace->buffer = calloc(10000, sizeof(char));
	return trace;
}

void trace_destroy(trace_t* trace)
{
	//Clean up memory
	queue_destroy(trace->queue);
	semaphore_destroy(trace->sem);
	timer_object_destroy(trace->timer);
	if (trace->path != NULL) { free(trace->path); }
	free(trace->buffer);
	heap_free(trace->heap, trace);
}

void trace_duration_push(trace_t* trace, const char* name)
{
	if (trace->startCapture == 1) //Checks to see if capture is on
	{
		//Add a new event to the queue
		event_t* ev = heap_alloc(trace->heap, sizeof(event_t),8);
		ev->name = heap_alloc(trace->heap, (strlen(name)+1),8);
		strcpy_s(ev->name, (strlen(name)+1), name);
		timer_object_update(trace->timer);
		UINT64 timeStart = timer_object_get_us(trace->timer);
		char* tmp = calloc(100, sizeof(char));
		sprintf_s(tmp, 100, "\n\t\t{\"name\":\"%s\",\"ph\":\"B\",\"pid\":0,\"tid\":\"%d\",\"ts\":\"%I64u\"},",ev->name, GetCurrentThreadId(), timeStart);
		strcat_s(trace->buffer, 10000, tmp);
		free(tmp);
		queue_push(trace->queue, ev);
	}
}

void trace_duration_pop(trace_t* trace)
{
	if (trace->startCapture == 1) //Checks to see if capture is on
	{
		semaphore_acquire(trace->sem);
		//After sem is locked, get data of thread and add to buffer
		event_t* top = queue_pop(trace->queue);
		timer_object_update(trace->timer);
		UINT64 timeEnd = timer_object_get_us(trace->timer);
		char* tmp = calloc(100, sizeof(char));
		sprintf_s(tmp, 100, "\n\t\t{\"name\":\"%s\",\"ph\":\"E\",\"pid\":0,\"tid\":\"%d\",\"ts\":\"%I64u\"},", top->name, GetCurrentThreadId(), timeEnd);
		strcat_s(trace->buffer, 10000, tmp);
		free(tmp);
		//Clear the recently popped item
		heap_free(trace->heap,top->name);
		heap_free(trace->heap, top);
		semaphore_release(trace->sem);
	}
}

void trace_capture_start(trace_t* trace, const char* path)
{
	//Set up the JSON file buffer and path, start timer
	trace->startCapture = 1;
	trace->path = calloc(strlen(path)+1, sizeof(char));
	strncpy_s(trace->path, (strlen(path) + 1), path, strlen(path));
	strncpy_s(trace->buffer, 51, "{ \n \t \"displayTimeUnit\": \"ns\", \"traceEvents\": [" , 51);
	timer_object_update(trace->timer);
}

void trace_capture_stop(trace_t* trace)
{
	//Stop the capture and write the JSON
	trace->startCapture = 0;
	strcat_s(trace->buffer, 10000, "\n\t]\n}");
	printf(trace->buffer);
	FILE* fp;
	fopen_s(&fp,trace->path, "w");
	fprintf(fp, trace->buffer);
	fclose(fp);
}
