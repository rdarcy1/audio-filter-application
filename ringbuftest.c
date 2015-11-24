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
	RingBuffer *myBuf = RingBuffer_create(5);
	
	int test_array[] = {4,9,2,3,1,1};
	int current_val;
	for (int y=0; y<sizeof test_array/sizeof test_array[0]; y++) {

		current_val = 0;

		// Write value to ring buffer
		myBuf->buffer[wrap(y,myBuf->length)] = test_array[y];

		// Calculate sample to be output
		for (int x=0; x<5; x++) {
			current_val += (x+1)*myBuf->buffer[wrap(y-x,myBuf->length)];
			printf("%d x %d\t",x+1, myBuf->buffer[wrap(y-x,myBuf->length)]);
		}

		printf("%d\n", current_val);
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