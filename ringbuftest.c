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
			current_val += (x+1)*myBuf->buffer[wrap(myBuf->current_index-x,myBuf->length)];
			printf("%d x %d\t",x+1, myBuf->buffer[wrap(y-x,myBuf->length)]);
		}

		// Increment the current index, wrapping so it does not exceed buffer length
		// No need to call wrap() as current_index cannot be negative
		myBuf->current_index = (myBuf->current_index + 1) % myBuf->length;

		printf("%d\n", current_val);
	}

	RingBuffer_destroy(myBuf);

	return 0;
}

RingBuffer *RingBuffer_create(int length)
{
	// Create new buffer, allocate memory and initialise to 0
    RingBuffer *new_buffer = calloc(1, sizeof(RingBuffer));
    new_buffer->length  = length;
    new_buffer->buffer = calloc(new_buffer->length, sizeof(new_buffer->length));
    new_buffer->current_index = 0;

    return new_buffer;
}

void RingBuffer_destroy(RingBuffer *buffer_to_destroy)
{
	// Free allocated memory
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


// Wraps value between 0 and max; works with negative value
int wrap (int value, int max)
{
	return ((value < 0) ? ((value % max) + max) : value) % max;
}