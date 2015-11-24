/*
    Copy a wav file using the portsf library.

    This is going to be as minimal as possible
    with error checking.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "portsf.h"

#define INPUT_FILENAME "Original.wav"
#define OUTPUT_FILENAME "Copy.wav"
#define NUM_SAMPLES_IN_FRAME 1024
#define NUM_CHANNELS 1
#define RING_BUF_LENGTH 50

#define MUTE_LEFT 0
#define DELAY_IN_SAMPLES 1000
#define DELAY_MIX 0.5

#define SAMPLE_FREQUENCY 44100

// To ensure that we do not use portsf to close a file that was never opened.
#define INVALID_PORTSF_FID -1

// Define ring buffer struct
typedef struct {
    float *buffer;
    int current_index;
    int length;
} RingBuffer;

// Prototype functions
double sinc (double x);
RingBuffer *RingBuffer_create (int length);
void RingBuffer_destroy (RingBuffer *buffer_to_destroy);
int wrap (int value, int max);

enum float_clipping { DO_NOT_CLIP_FLOATS, CLIP_FLOATS };
enum minheader { DO_NOT_MINIMISE_HDR, MINIMISE_HDR };
enum auto_rescale { DO_NOT_AUTO_RESCALE, AUTO_RESCALE };  

int main() {

    DWORD nFrames = NUM_SAMPLES_IN_FRAME; 
    int in_fID = INVALID_PORTSF_FID;
    int out_fID = INVALID_PORTSF_FID;
    PSF_PROPS audio_properties;    
    long num_frames_read;
    int return_value = EXIT_SUCCESS;
    
    // Declare buffers
    float *input_buffer = NULL;
    float *output_buffer = NULL;
    RingBuffer *ringBuf = NULL;

    // Coefficient variables
    int N = 126;
    double cutoff = 220;
    double coefficients[N+1];

    // Initialise portsf library
    if(psf_init()) {
        printf("Unable to start portsf library.\n");
        return EXIT_FAILURE;
    }
  

    // Open the input file
    if ((in_fID = psf_sndOpen(INPUT_FILENAME, &audio_properties,
        DO_NOT_AUTO_RESCALE))<0) {

        printf("Unable to open file %s\n",INPUT_FILENAME);
        return_value = EXIT_FAILURE;
        goto CLEAN_UP;
    }

    // Check input file has correct number of channels
    if (audio_properties.chans != NUM_CHANNELS) {
            printf("Input audio file must be mono.\n");
            return_value = EXIT_FAILURE;
            goto CLEAN_UP;
    }
  

    // Open the output file
    if ((out_fID = psf_sndCreate(OUTPUT_FILENAME, &audio_properties,
        CLIP_FLOATS, DO_NOT_MINIMISE_HDR, PSF_CREATE_RDWR))<0) {
        
        printf("Unable to open file %s\n",OUTPUT_FILENAME);
        return_value = EXIT_FAILURE;
        goto CLEAN_UP;
    }
  

    // Calculate memory required for input and output buffers.
    int buffer_memory = nFrames*audio_properties.chans*sizeof(float);

    // Allocate memory for buffers
    input_buffer = (float*) malloc(buffer_memory);
    output_buffer = (float*) malloc(buffer_memory);
    ringBuf = RingBuffer_create(RING_BUF_LENGTH);

    // Check that memory has been allocated.
    if (input_buffer == NULL || output_buffer == NULL || ringBuf == NULL) {
        printf("Unable to allocate memory for all buffers.\n");
        return_value = EXIT_FAILURE;
        goto CLEAN_UP;
    }

    // Calculate coefficients
    for (int x = 0; x <= N; x++) {
        coefficients[x] = (0.54 - 0.46 * cos((2*M_PI*x)/N)) * 
            (((2*cutoff)/SAMPLE_FREQUENCY) * sinc(((2*x - N)*cutoff)/SAMPLE_FREQUENCY));
    }

    // Read frames from input file into input buffer
    while ((num_frames_read=psf_sndReadFloatFrames(in_fID, input_buffer, nFrames)) > 0) { 

        for (int z = 0; z < nFrames; z++) {

            // Assign sample from input buffer to ring buffer
            ringBuf->buffer[ringBuf->current_index] = input_buffer[z];

            // Clear current output buffer sample
            output_buffer[z] = 0;

            // Apply filter using coefficients
            for (int x = 0; x <= N; x++) {
                output_buffer[z] += coefficients[x] * 
                    ringBuf->buffer[wrap((ringBuf->current_index) - x,ringBuf->length)];
            }

            // Increment the current index, wrapping so it does not exceed buffer length
            // No need to call wrap() as current_index cannot be negative
            ringBuf->current_index = (ringBuf->current_index + 1) % ringBuf->length;

        }

        // Write the output buffer to the output file
        if (psf_sndWriteFloatFrames(out_fID,output_buffer,num_frames_read)!=num_frames_read) {
            printf("Unable to write to %s\n",OUTPUT_FILENAME);
            return_value = EXIT_FAILURE;
            break;
        }
    }


    // Handle errors when reading from input file
    if (num_frames_read<0) {
        printf("Error reading file %s. The output file (%s) is incomplete.\n",
            INPUT_FILENAME,OUTPUT_FILENAME);
        return_value = EXIT_FAILURE;
    }

CLEAN_UP:

    // Free the memory for the buffers
    if (input_buffer) {
        free(input_buffer);
        puts("Input buffer freed.");}
    if (output_buffer) {
        free(output_buffer);
        puts("Output buffer freed.");}
    RingBuffer_destroy(ringBuf);

    // Close the output file
    if (out_fID>=0)
        psf_sndClose(out_fID);
    
    // Close the input file
    if (in_fID>=0)
        psf_sndClose(in_fID);
    
    // Close portsf library
    psf_finish();

    return return_value;
}

// Sinc function
double sinc (double x) {
    // if (!x) return 1;
    // return sin(M_PI*x)/(M_PI*x);

    return x ? sin(M_PI*x)/(M_PI*x) : 1;
}

// Create new ring buffer
RingBuffer *RingBuffer_create(int length)
{
    // Allocate memory for struct and initialise to 0
    RingBuffer *new_buffer = calloc(1, sizeof(RingBuffer));
    new_buffer->length  = length;
    // Allocate memory for struct and initialise to 0
    new_buffer->buffer = calloc(new_buffer->length, sizeof(float));
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
}

// Wraps value between 0 and max; works with negative value
int wrap (int value, int max)
{
    return ((value < 0) ? ((value % max) + max) : value) % max;
}
