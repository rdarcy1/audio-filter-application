/*
    Copy a wav file using the portsf library.

    This is going to be as minimal as possible
    with error checking.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "portsf.h"

// Samples in input/output buffers
#define NUM_SAMPLES_IN_FRAME 1024

// Audio files must be mono
#define NUM_CHANNELS 1

// To prevent clipping because of Gibb's Phenomenom
#define OUTPUT_SCALE_FACTOR 0.7

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
int mod (int value, int max);

enum float_clipping { DO_NOT_CLIP_FLOATS, CLIP_FLOATS };
enum minheader { DO_NOT_MINIMISE_HDR, MINIMISE_HDR };
enum auto_rescale { DO_NOT_AUTO_RESCALE, AUTO_RESCALE };

typedef enum { LOWPASS, HIGHPASS } FILTERTYPE;
FILTERTYPE filterType;

typedef enum { HAMMING, HANNING, BARTLETT, BLACKMAN, RECTANGULAR } WINDOWTYPE;
WINDOWTYPE windowType;


int main( int argc, char *argv[]) {

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

    // Parameters to be supplied via command line
    double cutoff = 0;
    char *input_filename = NULL;
    char *output_filename = NULL;
    int filter_order = 126;
    float user_volume = 1.;

    double *coefficients = NULL;

    // Error flags
    int parameter_error = 0;
    int mem_error = 0;

    // Compulsory command line arguments
    // Check old filename, new filename and cutoff have been supplied
    if (!argv[1] || !argv[2] || !argv[3]) {
        parameter_error = 1;
    } else {
        // Copy arguments to sensibly named variables
        if ((input_filename = malloc(sizeof(argv[1]))) == NULL)
            mem_error = 1;
        else
            strcpy(input_filename,argv[1]);

    
        if ((output_filename = malloc(sizeof(argv[2]))) == NULL)
            mem_error = 1;
        else
            strcpy(output_filename,argv[2]);

        if ((cutoff = atof(argv[3])) <= 0)
            parameter_error = 1;
    
    }

    if (mem_error) {
        puts("Unable to allocate memory for input and/or output filenames.");
        goto CLEAN_UP;
    }

    if (parameter_error) {
        printf("Usage:\n\n\t %s <old filename> <new filename> <filter cutoff>\n\nwhere filter cutoff is greater than 0.\n", argv[0]);
        goto CLEAN_UP;
    }

    // Optional command line arguments
    for (int a = 4; a < argc; a++) {

        // Filter order
        if (!strcmp(argv[a], "-filterorder")) {

            // Get value of parameter with error checking
            a++;
            if(a >= argc) {
                puts("Too few arguments supplied.");
                goto CLEAN_UP;
            }

            filter_order = atoi(argv[a]);
            printf("filter order: %d\n", filter_order);

            if (filter_order < 1 || filter_order > 1000 || filter_order % 2) {
                puts("Filter order must be even, and between 1 and 1000.");
                goto CLEAN_UP;
            }

        // Filter type
        } else if (!strcmp(argv[a], "-filtertype")) {

            // Get value of parameter with error checking
            a++;
            if(a >= argc) {
                puts("Too few arguments supplied.");
                goto CLEAN_UP;
            }

            if (!strcmp(argv[a], "lowpass")) {
                // Already a low pass filter
                puts("lowpass filter chosen.");
                filterType = LOWPASS;
            } else if (!strcmp(argv[a], "highpass")) {
                puts("highpass filter chosen.");
                filterType = HIGHPASS;
            } else {
                puts("Unrecognised filter type.");
                goto CLEAN_UP;
            }

        // Window type
        } else if (!strcmp(argv[a], "-windowtype")) {

            // Get value of parameter with error checking
            a++;
            if(a >= argc) {
                puts("Too few arguments supplied.");
                goto CLEAN_UP;
            }

            if (!strcmp(argv[a], "hamming")) {
                windowType = HAMMING;
            } else if (!strcmp(argv[a], "hanning")) {
                windowType = HANNING;
            } else if (!strcmp(argv[a], "bartlett")) {
                windowType = BARTLETT;
            } else if (!strcmp(argv[a], "blackman")) {
                windowType = BLACKMAN;
            } else if (!strcmp(argv[a], "rectangular")) {
                windowType = RECTANGULAR;
            } else {
                puts("Unrecognised window type.");
                goto CLEAN_UP;
            }

        // Volume
        } else if (!strcmp(argv[a], "-volume")) {

            // Get value of parameter with error checking
            a++;
            if(a >= argc) {
                puts("Too few arguments supplied.");
                goto CLEAN_UP;
            }

            user_volume = atof(argv[a]);

            if (user_volume <= 0 || user_volume > 2) {
                puts("Volume must be greater than 0 and less than or equal to 2");
                goto CLEAN_UP;
            }

        } else {
            printf("Command %s not recognised.", argv[a]);
            goto CLEAN_UP;
        }
    }

    

    // Initialise portsf library
    if(psf_init()) {
        printf("Unable to start portsf library.\n");
        return EXIT_FAILURE;
    }
  

    // Open the input file
    if ((in_fID = psf_sndOpen(input_filename, &audio_properties,
        DO_NOT_AUTO_RESCALE))<0) {

        printf("Unable to open file %s\n",input_filename);
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
    if ((out_fID = psf_sndCreate(output_filename, &audio_properties,
        CLIP_FLOATS, DO_NOT_MINIMISE_HDR, PSF_CREATE_RDWR))<0) {
        
        printf("Unable to open file %s\n",output_filename);
        return_value = EXIT_FAILURE;
        goto CLEAN_UP;
    }
  

    // Calculate memory required for input and output buffers.
    int buffer_memory = nFrames*audio_properties.chans*sizeof(float);

    // Allocate memory for buffers
    input_buffer = (float*) malloc(buffer_memory);
    output_buffer = (float*) malloc(buffer_memory);
    ringBuf = RingBuffer_create(filter_order+1);

    // Get sample rate
    int sample_rate = audio_properties.srate;

    // Coefficient variables
    coefficients = calloc((filter_order+1),sizeof(double));
    double window_coefficient;
    double fourier_coefficient;

    // Check that memory has been allocated.
    if (input_buffer == NULL || output_buffer == NULL || ringBuf == NULL || coefficients == NULL) {
        printf("Unable to allocate memory.\n");
        return_value = EXIT_FAILURE;
        goto CLEAN_UP;
    }

    // Calculate coefficients
    for (int x = 0; x <= filter_order; x++) {


        switch (windowType) {
            case HAMMING:
                window_coefficient = 0.54 - 0.46 * cos((2*M_PI*x)/filter_order);
                break;
            case HANNING:
                window_coefficient = 0.5 - 0.5 * cos((2*M_PI*x)/filter_order);
                break;
            case BARTLETT:
                window_coefficient = (1 - (2*fabs(x - filter_order/2.)))/filter_order;
                break;
            case BLACKMAN:
                window_coefficient = 0.42 - 0.5 * cos((2*M_PI*x)/filter_order) + 0.08 * cos((4*M_PI*x)/filter_order);
                break;
            case RECTANGULAR:
                window_coefficient = 1;
                break;
            default:
                puts("Unrecognised window type when calculating coefficients.");
                goto CLEAN_UP;
        }
        

        switch (filterType) {
            case LOWPASS:
                fourier_coefficient = ((2*cutoff)/sample_rate) * sinc(((2*x - filter_order)*cutoff)/sample_rate);
                break;
            case HIGHPASS:
                if (x == filter_order/2 + 1)
                    fourier_coefficient = 1 - ((2*cutoff)/sample_rate);
                else
                    fourier_coefficient = ((-2*cutoff)/sample_rate) * sinc(((2*x - filter_order)*cutoff)/sample_rate);
                break;
            default:
                puts("Unrecognised filter type when calculating coefficients.");
                goto CLEAN_UP;
        }

        
        coefficients[x] = window_coefficient * fourier_coefficient;

    }

    int keep_looping = 1;

    while (keep_looping) { 

        // Read frames from input file into input buffer
        num_frames_read = psf_sndReadFloatFrames(in_fID, input_buffer, nFrames);
        
        // If end of file is reached, perform one more loop with a buffer of 0s so as not to truncate
        if (num_frames_read <= 0) {

            keep_looping = 0;

            // Set input buffer to 0
            for (int x = 0; x < ringBuf->length; x++) {
                input_buffer[x] = 0;
            }

            // Only loop and write the necessary number of frames
            nFrames = num_frames_read = ringBuf->length;
        }

        // Loop through frames in input buffer
        for (int z = 0; z < nFrames; z++) {

            // Assign sample from input buffer to ring buffer
            ringBuf->buffer[ringBuf->current_index] = input_buffer[z];

            // Clear current output buffer sample
            output_buffer[z] = 0;

            // Apply filter using coefficients and assign to output buffer
            for (int x = 0; x <= filter_order; x++) {
                output_buffer[z] += coefficients[x] * 
                    ringBuf->buffer[mod((ringBuf->current_index) - x,ringBuf->length)];
            }

            // Scale filtered sample
            output_buffer[z] *= OUTPUT_SCALE_FACTOR * user_volume;

            // Increment the current index, wrapping so it does not exceed buffer length
            ringBuf->current_index = (ringBuf->current_index + 1) % ringBuf->length;

        }

        // Write the output buffer to the output file
        if (psf_sndWriteFloatFrames(out_fID,output_buffer,num_frames_read)!=num_frames_read) {
            printf("Unable to write to %s\n",output_filename);
            return_value = EXIT_FAILURE;
            break;
        }

    }

    // Handle errors when reading from input file
    if (num_frames_read<0) {
        printf("Error reading file %s. The output file (%s) is incomplete.\n",
            input_filename,output_filename);
        return_value = EXIT_FAILURE;
    }

CLEAN_UP:

    // Free the memory for the buffers
    if (input_buffer) {
        puts("Freeing input buffer");
        free(input_buffer);
        puts("input buffer freed.");
    }
    if (output_buffer) {
        puts("Freeing output buffer");
        free(output_buffer);
        puts("output buffer freed.");
    }

    RingBuffer_destroy(ringBuf);

    if (coefficients)
        free(coefficients);

    if (input_filename) {
        puts("Freeing input_filename");
        free (input_filename);
        puts("input filename freed.");
    }

    if (output_filename) {
        free (output_filename);
        puts("output filename freed.");
    }
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
    return x ? sin(M_PI*x)/(M_PI*x) : 1;
}

// Create new ring buffer
RingBuffer *RingBuffer_create(int length)
{
    // Allocate memory for struct and initialise to 0
    RingBuffer *new_buffer = calloc(1, sizeof(RingBuffer));
    if (new_buffer == NULL) return NULL;
    new_buffer->length  = length;
    // Allocate memory for buffer array and initialise to 0
    new_buffer->buffer = calloc(new_buffer->length, sizeof(float));
    if (new_buffer->buffer == NULL) return NULL;
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
int mod (int value, int max)
{
    return ((value < 0) ? ((value % max) + max) : value) % max;
}
