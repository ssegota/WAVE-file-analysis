#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sndfile.h>
#include <SPTK.h>
#include <math.h>

#define LPC_ORDER       11
#define MIN_DET 0.000001
#define FFT_SIZE        512

//functions
void compute_fft(double *buffer, int buffer_size, const char *out_filename, double fft_buf[]);
void compute_lpc(double *buffer, int buffer_size, const char *out_filename, double lpc_buf[]);
double standard_deviation(double *data, int n);
double average(double *data, int n);

struct lab_entry{
        int le_start;
        int le_end;
        char le_voice[3];
	int le_start_sample;
	int le_end_sample;
};

struct voice_std_dev{
	char vsd_voice[3];
	char vsd_lpc_dev;
	char vsd_fft_dev;
};

int main(int argc, char **argv){
	
	//Check arguments
	if(argc != 3){
		printf("Wrong number of arguments.\nProper usage: rb [WAVE file name] [LAB file name]\n");
		return 1;
	}
	//FILES
	SNDFILE *sf;
	SF_INFO info;
	FILE *out;
	FILE *std_dev_out;
        FILE *infile;
	
	//variables
	double *buf;
	double *sample_buf;
        double lpc_buf[LPC_ORDER+1];
        double fft_buf[FFT_SIZE];
	double time_per_sample;
	int sample_buf_length;
    	int num_items;
    	int f,sr,c;
    	int j;
	int i = 0;
        int start_sample = 0;
        int end_sample = 0;
        int length, samples_per_sound;
        int start_time=0;
        int end_time=0;
        int current_voice = 0;
	int last;
	int ch, number_of_lines=0;
        char start[100];
        char end[100];
        char voice[3];
        char line[100];
        char filename[255];
	char fft_filename[255];
	char lpc_filename[255];

	// open lab file to get times
        infile = fopen(argv[2], "r");

        if(infile==NULL){
		printf("LAB file %s doesn't exist\n", argv[2]);
		return 1;
	}

	//get number of lines
	do{
                ch = fgetc(infile);
                if(ch == '\n')
                        number_of_lines++;
        } 
	while (ch != EOF);

        if(ch != '\n' && number_of_lines != 0) 
                number_of_lines++;

	//initialize fields to store lab file values
        struct lab_entry entries[number_of_lines];

	//return to start of file
        rewind(infile);

	//read the file and store values in struct fields
        while(fgets(line, sizeof(line), infile)!=NULL){
                sscanf(line, "%s %s %s", start, end, voice);
                entries[i].le_start = atoi(start);
                entries[i].le_end = atoi(end);
                strcpy(entries[i].le_voice, voice);
                i++;
        }

	// Open the WAV file.
   	info.format = 0;
    	sf = sf_open(argv[1],SFM_READ,&info);
    	if (sf == NULL){
        	printf("Failed to open the file.\n");
        	return 1;
        }

	// get info, and figure out how much data to read.
    	f = info.frames;
   	sr = info.samplerate;
    	c = info.channels;
	time_per_sample = 1/(double)sr;
    	num_items = f*c;

	// Allocate space for the data to be read, then read it.
    	buf = (double *) malloc(num_items*sizeof(double));
    	sf_read_double(sf,buf,num_items);
    	sf_close(sf);

	//set samples per each voice
	for(i = 0; i<number_of_lines-1; i++){
		start_time=end_time;
		end_time=entries[i].le_end;
		length = end_time-start_time;
		samples_per_sound = ((length/10000) / time_per_sample)/1000;  //Adjust size
		start_sample=end_sample+1;
		end_sample = start_sample+samples_per_sound;
		entries[i].le_start_sample = start_sample;
		entries[i].le_end_sample = end_sample;
		printf("%d\t%d\t%s\t%d\t%d\n", entries[i].le_start, entries[i].le_end, entries[i].le_voice, entries[i].le_start_sample, entries[i].le_end_sample);
	}

	//set last voice from iterator i
	last = i;

	//Open file to write mean and standard deviation values
	std_dev_out = fopen("sounds_means.std","w");
	if(std_dev_out == NULL){
		printf("Can't create sounds_means.std.\n");
		return 1;
	}
	fprintf(std_dev_out, "V\tLPC COEFFICIENTS MEAN\tSTD DEV\n");

	//create filenames (sounds_STRING-LABEL_INT-ORDER)
	sprintf(filename, "sounds_%s%d.raw", entries[current_voice].le_voice, current_voice);
    	sprintf(fft_filename, "sounds_%s%d.fft", entries[current_voice].le_voice, current_voice);
	sprintf(lpc_filename, "sounds_%s%d.lpc", entries[current_voice].le_voice, current_voice);

	// Write the data to files.
    	out = fopen(filename,"w");
	if(out == NULL){
		printf("Can't create file %s.\n", filename);
        	return 1;
        }

	//Iterate through all samples
    	for (i = 0; i < entries[last-1].le_end_sample; i += c){
		//figure out which voice does the sample belong to
		if(i>entries[current_voice].le_end_sample){
			//Close RAW file and compute values from data collected on voice
			fclose(out);
			sample_buf = &buf[entries[current_voice].le_start_sample];
			sample_buf_length=entries[current_voice].le_end_sample - entries[current_voice].le_start_sample;
			compute_fft(sample_buf, sample_buf_length, fft_filename, fft_buf);
			compute_lpc(sample_buf, sample_buf_length, lpc_filename, lpc_buf);
			fprintf(std_dev_out, "%s\t%.20f\t%.20f\n",entries[current_voice].le_voice, average(lpc_buf, LPC_ORDER+1), standard_deviation(lpc_buf, LPC_ORDER+1));
			//Change voice and create new files and filenames
			current_voice++;
			sprintf(filename,"sounds_%s%d.raw", entries[current_voice].le_voice, current_voice); 
		        sprintf(fft_filename, "sounds_%s%d.fft", entries[current_voice].le_voice, current_voice);
		        sprintf(lpc_filename, "sounds_%s%d.lpc", entries[current_voice].le_voice, current_voice);

			out = fopen(filename, "w");
			if(out == NULL){
				printf("Can't create file %s.\n", filename);
				return 1;
			}
		}
 		//Write frame to file
  	     	printf("Wrote frame %d to file %s\n", i, entries[current_voice].le_voice);
		for (j = 0; j < c; ++j)
            		fprintf(out,"%.20f ",buf[i+j]);
        	fprintf(out,"\n");
        	}

	//close files
    	fclose(out);
	fclose(std_dev_out);
	sf_close(sf);
	return 0;
}

void compute_fft(double * buffer, int buffer_size, const char * out_filename, double fft_buf[]){

        //we are using only 512 values for fft because of needed potency of number 2 [stored in FFT_SIZE]
        double real_parts[FFT_SIZE];
        double imaginary_parts[FFT_SIZE];

        int i;
        int magnitude_size = FFT_SIZE / 2 + 1;
        double magnitude[magnitude_size];

        fillz(real_parts, FFT_SIZE, sizeof(double));

        size_t n = (buffer_size < FFT_SIZE) ? buffer_size : FFT_SIZE;

        memcpy(real_parts, buffer, n);

        //calling Fast Fourier SPTK function
        fftr(real_parts, imaginary_parts, FFT_SIZE);

        FILE * fp = fopen(out_filename, "w");
	if(fp==NULL){
		printf("Can't create FFT file.\n");
		return;
	}
        //writing magnitude using log scale
        for (i = 0; i < magnitude_size; i++){
                magnitude[i] = 10.0 * log10(sqrt(real_parts[i] * real_parts[i] + imaginary_parts[i] * imaginary_parts[i]));
                fprintf(fp, "%.20f\n", magnitude[i]);
		fft_buf[i]=magnitude[i];
        }

        fclose(fp);
}

void compute_lpc(double * buffer, int buffer_size, const char * out_filename, double lpc_buf[]){
        double coefficients[LPC_ORDER + 1];
        int i;
        //because we are not calling interface function of lpc we must define all default values
        lpc(buffer, buffer_size, coefficients, LPC_ORDER, MIN_DET);

        //open and save .fft file
        FILE * fp = fopen(out_filename, "w");
	if(fp==NULL){
		printf("Can't create LPC file.\n");
		return;
	}
        for(i=0; i<=LPC_ORDER; i++){
                fprintf(fp, "%.20f\n", coefficients[i]);
		lpc_buf[i]=coefficients[i];
	}
        fclose(fp);
}

double standard_deviation(double *data, int n){
    //calculating standard deviation
    double avg=0, sum_deviation=0;
    int i;
    for(i=0; i<n;++i)
        avg += data[i];
    avg=avg/n;
    for(i=0; i<n;++i)
    	sum_deviation+=(data[i]-avg)*(data[i]-avg);
    return sqrt(sum_deviation/n);           
}

double average(double *data, int n){

	double avg = 0;
	int i;
	for(i=0; i<n; i++)
		avg += data[i];
	
	return avg/n;
}
