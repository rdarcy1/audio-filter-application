#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "portsf.h"

// Samples in input/output buffers
#define NUM_SAMPLES_IN_FRAME 1024

// Maximum cutoff frequency in Hz
#define CUTOFF_LIMIT 24000

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
int str_to_double (char *str, double *output_value);
int str_to_int (char *str, int *output_value);

enum float_clipping { DO_NOT_CLIP_FLOATS, CLIP_FLOATS };
enum minheader { DO_NOT_MINIMISE_HDR, MINIMISE_HDR };
enum auto_rescale { DO_NOT_AUTO_RESCALE, AUTO_RESCALE };

typedef enum { LOWPASS, HIGHPASS, BANDPASS, BANDSTOP } FILTERTYPE;
FILTERTYPE filterType = LOWPASS;

typedef enum { HAMMING, HANNING, BARTLETT, BLACKMAN, RECTANGULAR } WINDOWTYPE;
WINDOWTYPE windowType = HAMMING;


int main( int argc, char *argv[]) {

    DWORD nFrames = NUM_SAMPLES_IN_FRAME; 
    int in_fID = INVALID_PORTSF_FID;
    int out_fID = INVALID_PORTSF_FID;
    PSF_PROPS audio_properties;    
    long num_frames_read;
    int return_value = EXIT_SUCCESS;
    
    // Declare buffers
    float *block_buffer = NULL;
    RingBuffer *ringBuf = NULL;

    // Parameters to be supplied via command line
    double cutoff = 0;
    char *input_filename = NULL;
    char *output_filename = NULL;
    int filter_order = 126;
    double user_volume = 1.;

    double *coefficients = NULL;
    double upper_cutoff;

    // Error flags
    int parameter_error = 0;
    int mem_error = 0;

    // Compulsory command line arguments
    // Check old filename, new filename and cutoff have been supplied
    if (!argv[1] || !argv[2] || !argv[3]) {
        parameter_error = 1;
    } else {
        // Copy arguments to sensibly named variables
        // Input filename
        if ((input_filename = malloc(sizeof(argv[1]))) == NULL)
            mem_error = 1;
        else
            strcpy(input_filename,argv[1]);

        // Output filename
        if ((output_filename = malloc(sizeof(argv[2]))) == NULL)
            mem_error = 1;
        else
            strcpy(output_filename,argv[2]);

        // Cutoff frequency
        if (str_to_double(argv[3], &cutoff))
            parameter_error = 1;
    }

    if (mem_error) {
        puts("Unable to allocate memory for input and/or output filenames.");
        goto CLEAN_UP;
    }

    if (parameter_error) {
        printf("Usage:\n\n\t %s <source_filename> <destination_filename> <filter_cutoff> [options]\n\n"
            "Filter cutoff is in Hertz and must be greater than 0 and less than %d. For bandpass and\n"
            "bandstop filters this is the lower cutoff frequency.\n\n"
            "Optional arguments:\n"
            "-filtertype (lowpass | highpass | bandpass <upper_cutoff> | bandstop <upper_cutoff>)\n"
            "\t\t\t\t\tFilter type to be applied. Defaults to lowpass if\n\t\t\t\t\toption not specified. 'bandpass' and 'bandstop'\n"
            "\t\t\t\t\tmust be proceeded by an upper cutoff value which\n\t\t\t\t\tis greater than the required cutoff already\n"
            "\t\t\t\t\tsupplied, and less than %d.\n\n"
            "-filterorder <order>\t\t\tOrder of filter; must be even and in the range\n\t\t\t\t\t"
            "2-1000. Defaults to 126 if option not specified.\n\n"
            "-windowtype (hamming | hanning | blackman | bartlett | rectangular)\n"
            "\t\t\t\t\tWindow type to be applied. Defaults to hamming if\n\t\t\t\t\toption not specified.\n\n"
            "-volume <volume>\t\t\tValue to scale output by. Must be greater than 0\n\t\t\t\t\tand less than 5.\n"
            "\t\t\t\t\tWARNING: a value greater than 1 may cause clipping.\n\n"
            , argv[0], CUTOFF_LIMIT, CUTOFF_LIMIT);
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

            int order_status = str_to_int(argv[a], &filter_order);

            if (order_status || filter_order < 2 || filter_order > 1000 || filter_order % 2) {
                puts("Filter order must be an even integer, and between 2 and 1000.");
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
                filterType = LOWPASS;

            } else if (!strcmp(argv[a], "highpass")) {
                filterType = HIGHPASS;

            // Same code used for getting the upper cutoff, so bandpass and bandstop are grouped
            } else if (!strcmp(argv[a], "bandpass") || !strcmp(argv[a], "bandstop")) {
                if (!strcmp(argv[a], "bandpass"))
                    filterType = BANDPASS;
                else
                    filterType = BANDSTOP;

                // Get upper cutoff for bandpass/bandstop
                a++;
                if(a >= argc) {
                    puts("Too few arguments supplied.");
                    goto CLEAN_UP;
                }

                int upper_cutoff_status = str_to_double(argv[a], &upper_cutoff);

                if (upper_cutoff_status || upper_cutoff <= cutoff || upper_cutoff >= CUTOFF_LIMIT) {
                    printf("Upper cutoff must proceed bandpass/bandstop filter type:\n\n\t-filtertype ( bandpass | bandstop ) <upper cutoff>\n\n"
                        "Upper cutoff must be greater than the lower cutoff and less than %d\n", CUTOFF_LIMIT);
                    goto CLEAN_UP;
                }

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

            if (!strcmp(argv[a], "hamming"))
                windowType = HAMMING;
            else if (!strcmp(argv[a], "hanning"))
                windowType = HANNING;
            else if (!strcmp(argv[a], "bartlett"))
                windowType = BARTLETT;
            else if (!strcmp(argv[a], "blackman"))
                windowType = BLACKMAN;
            else if (!strcmp(argv[a], "rectangular"))
                windowType = RECTANGULAR;
            else {
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

            int user_volume_status = str_to_double(argv[a], &user_volume);

            if (user_volume_status || user_volume <= 0 || user_volume >= 5) {
                puts("Volume must be greater than 0 and less than 5");
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

    // Allocate memory for buffers
    block_buffer = (float*) malloc(nFrames*audio_properties.chans*sizeof(float));
    ringBuf = RingBuffer_create(filter_order+1);

    // Get sample rate
    int sample_rate = audio_properties.srate;

    // Coefficient variables
    coefficients = calloc((filter_order+1),sizeof(double));
    double window_coefficient;
    double fourier_coefficient;

    // Check that memory has been allocated.
    if (block_buffer == NULL || ringBuf == NULL || coefficients == NULL) {
        printf("Unable to allocate memory.\n");
        return_value = EXIT_FAILURE;
        goto CLEAN_UP;
    }

    // Calculate coefficients
    for (int x = 0; x <= filter_order; x++) {

        // Calculate window part of coefficient
        switch (windowType) {
            case HAMMING:
                window_coefficient = 0.54 - 0.46 * cos((2*M_PI*x)/filter_order);
                break;
            case HANNING:
                window_coefficient = 0.5 - 0.5 * cos((2*M_PI*x)/filter_order);
                break;
            case BARTLETT:
                window_coefficient = 1 - (2*fabs(x - filter_order/2.))/filter_order;
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
        
        // Calculate filter part of coefficient

        // Define f_t for bandpass filters
        double ft1, ft2;

        switch (filterType) {
            case LOWPASS:
                fourier_coefficient = ((2*cutoff)/sample_rate) * sinc(((2*x - filter_order)*cutoff)/sample_rate);
                break;

            case HIGHPASS:
                if (x == filter_order/2)
                    fourier_coefficient = 1 - ((2*cutoff)/sample_rate);
                else
                    fourier_coefficient = ((-2*cutoff)/sample_rate) * sinc(((2*x - filter_order)*cutoff)/sample_rate);
                break;

            case BANDPASS:
                ft1 = cutoff/sample_rate;
                ft2 = upper_cutoff/sample_rate;
                if (x == filter_order/2)
                    fourier_coefficient = 2*(ft2 - ft1);
                else
                    fourier_coefficient = (sin(2.*M_PI*ft2*(x - filter_order/2.))/(M_PI*(x - filter_order/2.)))
                        - (sin(2.*M_PI*ft1*(x - filter_order/2.))/(M_PI*(x - filter_order/2.)));
                break;

            case BANDSTOP:
                ft1 = cutoff/sample_rate;
                ft2 = upper_cutoff/sample_rate;
                if (x == filter_order/2)
                    fourier_coefficient = 1 - 2*(ft2 - ft1);
                else
                    fourier_coefficient = (sin(2.*M_PI*ft1*(x - filter_order/2.))/(M_PI*(x - filter_order/2.)))
                        - (sin(2.*M_PI*ft2*(x - filter_order/2.))/(M_PI*(x - filter_order/2.)));
                break;

            default:
                puts("Unrecognised filter type when calculating coefficients.");
                goto CLEAN_UP;
        }

        
        coefficients[x] = window_coefficient * fourier_coefficient;

    }

    int keep_looping = 1;

    while (keep_looping) { 

        // Read frames from input file into buffer
        num_frames_read = psf_sndReadFloatFrames(in_fID, block_buffer, nFrames);
        
        // If end of file is reached, perform one more loop with a buffer of 0s so as not to truncate audio
        if (num_frames_read <= 0) {

            keep_looping = 0;

            // Set block buffer to 0
            for (int x = 0; x < ringBuf->length; x++) {
                block_buffer[x] = 0;
            }

            // Only loop and write the necessary number of frames
            nFrames = num_frames_read = ringBuf->length;
        }

        // Loop through frames in block buffer
        for (int z = 0; z < nFrames; z++) {

            // Assign sample from block buffer to ring buffer
            ringBuf->buffer[ringBuf->current_index] = block_buffer[z];

            // Clear current block buffer sample
            block_buffer[z] = 0;

            // Apply filter using coefficients and assign to block buffer
            for (int x = 0; x <= filter_order; x++) {
                block_buffer[z] += coefficients[x] * 
                    ringBuf->buffer[mod((ringBuf->current_index) - x,ringBuf->length)];
            }

            // Scale filtered sample
            block_buffer[z] *= OUTPUT_SCALE_FACTOR * user_volume;

            // Increment the current index, wrapping so it does not exceed buffer length
            ringBuf->current_index = (ringBuf->current_index + 1) % ringBuf->length;

        }

        // Write the block buffer to the output file
        if (psf_sndWriteFloatFrames(out_fID,block_buffer,num_frames_read)!=num_frames_read) {
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
    if (block_buffer) {
        free(block_buffer);
    }

    RingBuffer_destroy(ringBuf);

    if (coefficients)
        free(coefficients);

    if (input_filename) {
        free (input_filename);
    }

    if (output_filename) {
        free (output_filename);
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

// Returns 0 on success
int str_to_double (char *str, double *output_value) {

    char trailing_chars[sizeof(str)/sizeof(str[0])];

    // Check for the presence of a double and any trailing characters
    int n = sscanf(str, "%lf%c", output_value, trailing_chars);

    // Return status. Double value is outputted via the supplied pointer
    return n != 1;
}

// Returns 0 on success
int str_to_int (char *str, int *output_value) {

    char trailing_chars[sizeof(str)/sizeof(str[0])];

    // Check for the presence of an integer and any trailing characters
    int n = sscanf(str, "%d%c", output_value, trailing_chars);

    // Return status. Integer value is outputted via the supplied pointer
    return n != 1;
}

