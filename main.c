/*
    Copy a wav file using the portsf library.

    This is going to be as minimal as possible
    with error checking.
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "portsf.h"
#include "dbg.h"
#include "cw2.h"

#define INPUT_FILENAME "Original.wav"
#define OUTPUT_FILENAME "Copy.wav"
#define NUM_SAMPLES_IN_FRAME 1024

#define MUTE_LEFT 0
#define DELAY_IN_SAMPLES 1000
#define DELAY_MIX 0.5

// To ensure that we do not use portsf to close a file that was never opened.
#define INVALID_PORTSF_FID -1

enum float_clipping { DO_NOT_CLIP_FLOATS, CLIP_FLOATS };
enum minheader { DO_NOT_MINIMISE_HDR, MINIMISE_HDR };
enum auto_rescale { DO_NOT_AUTO_RESCALE, AUTO_RESCALE };  

int main() {

    DWORD nFrames = NUM_SAMPLES_IN_FRAME; 
    int in_fID = INVALID_PORTSF_FID;
    int out_fID = INVALID_PORTSF_FID;
    PSF_PROPS audio_properties;    
    float *buffer = NULL;
    long num_frames_read;
    int return_value = EXIT_SUCCESS;
  

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
  

    // Open the output file
    if ((out_fID = psf_sndCreate(OUTPUT_FILENAME, &audio_properties,
        CLIP_FLOATS, DO_NOT_MINIMISE_HDR, PSF_CREATE_RDWR))<0) {
        
        printf("Unable to open file %s\n",OUTPUT_FILENAME);
        return_value = EXIT_FAILURE;
        goto CLEAN_UP;
    }
  

    // Allocate memory for frame.
    if ((buffer = (float*)malloc(nFrames
        *audio_properties.chans*sizeof(float)))==NULL) {

        printf("Unable to allocate memory for frame.\n");
        return_value = EXIT_FAILURE;
        goto CLEAN_UP;
    }

    // Read frames from input file
    while ((num_frames_read=psf_sndReadFloatFrames(in_fID, buffer, nFrames)) > 0) { 

        printf("%lu\n",sizeof buffer/sizeof buffer[0]);

        // Write the frame to the output file
        if (psf_sndWriteFloatFrames(out_fID,buffer,num_frames_read)!=num_frames_read) {
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

    // Free the memory for the frame
    if (buffer)
        free(buffer);

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
