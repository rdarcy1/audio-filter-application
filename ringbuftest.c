#include <stdio.h>
#include <math.h>
#include <stdlib.h>


typedef struct {
	int *buffer;
	int current_index;
	int length;
} RingBuffer;

RingBuffer *RingBuffer_create(int length);

void RingBuffer_destroy(RingBuffer *buffer_to_destroy);

int buffer_index (RingBuffer *buffer, int offset);

int wrap (int value, int max);

int main () {
	RingBuffer *myBuf = RingBuffer_create(10);
	

	int array[100];
	for (int y=0; y<100; y++) {
		array[y] = y;
		myBuf->buffer[wrap(y,myBuf->length)] = 0;
	}

	for (int x=0;x<myBuf->length;x++) {
		printf("%d\n", myBuf->buffer[x]);
	}
	int offset = 8;

	int current_val, prev_val = 0;
	puts("x\tindex\tcur\tprev");
	for (int x=0;x<200;x++) {
		myBuf->buffer[(int)fmod(x,myBuf->length)] = x;

		current_val = myBuf->buffer[wrap(x,myBuf->length)];

		prev_val = myBuf->buffer[wrap(x-offset,myBuf->length)];


		printf("%d\t%d\t%d\t%d\n",x,(int)fmod(x,myBuf->length),current_val,prev_val);
		
	}

	// for (int x=0;x<myBuf->length;x++) {
	// 	printf("%d\n", myBuf->buffer[x]);
	// 	// printf("%d\n", wrap(-x,19));
	// }

	RingBuffer_destroy(myBuf);

	return 0;
}

RingBuffer *RingBuffer_create(int length)
{
    RingBuffer *new_buffer = calloc(1, sizeof(RingBuffer));
    new_buffer->length  = length;
    new_buffer->buffer = calloc(new_buffer->length, sizeof(new_buffer->length));

    return new_buffer;

    // error:
    // 	return NULL;
}

void RingBuffer_destroy(RingBuffer *buffer_to_destroy)
{
    if(buffer_to_destroy) {
        free(buffer_to_destroy->buffer);
        free(buffer_to_destroy);
    }
    return;
}

int buffer_index (RingBuffer *buffer, int offset)
{
	if (!(offset >= 0)) offset = 0;
	return (int)fmod(offset, buffer->length);
}

int wrap (int value, int max)
{
	// int ret = value - mavalue*floor(value/mavalue);
	return (((value < 0) ? ((value % max) + max) : value) % max);
}