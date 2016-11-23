# audio-filter-application

An application written in C that applies a range of filters and windows to a given audio file.

Filters available:
- High-pass
- Low-pass
- Band-pass
- Band-stop

Filter windows available:
- Hanning
- Hamming
- Blackman
- Bartlett
- Rectangular

##Usage:
    audio_filter_application <source_filename> <destination_filename> <filter_cutoff> [options]

Filter cutoff is in Hertz and must be greater than 0 and less than 24000. For bandpass and    
bandstop filters this is the lower cutoff frequency.    

###Optional arguments:
    -filtertype (lowpass | highpass | bandpass <upper_cutoff> | bandstop <upper_cutoff>)    
Filter type to be applied. Defaults to lowpass if option not specified. 'bandpass' and 'bandstop'    
must be proceeded by an upper cutoff value which is greater than the required cutoff already    
supplied, and less than 24000.    

    -filterorder <order>
Order of filter; must be even and in the range 2-1000. Defaults to 126 if option not specified.    

    -windowtype (hamming | hanning | blackman | bartlett | rectangular)    
Window type to be applied. Defaults to hamming if option not specified.    

    -volume <volume>
Value to scale output by. Must be greater than 0 and less than 5.    
WARNING: a value greater than 1 may cause clipping.
