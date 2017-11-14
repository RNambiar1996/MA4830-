/* Maintainer: Lee Ee Wei */

#include "Global.h"
#include "System.h"

//#define _GNU_SOURCE
//#define _XOPEN_SOURCE 700
//#define _POSIX_C_SOURCE 200809L
//#include <stdio.h>

#include <string.h>
#include <stdlib.h>
// #include <stdint.h>
#include <stdbool.h>
//#include <atomic.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

// Declaration of global variables for all source codes
uintptr_t iobase[6];     // for hardware
double global_frequency;
double global_amplitude;
double global_offset;
bool kill_switch;
bool system_pause;
bool reuse_param;          // bool to check whether param file is used, if yes, do not catch ctrl + s signal, and do not save backup, will only write once, no need atomic
sigset_t all_sig_mask_set; // set of signals to block all signals for all except 1 thread, the 1 thread will do signal handling
char *outputPath = "./output.txt";

// Initializing Mutexes
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t global_var_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t global_stop_mutex = PTHREAD_MUTEX_INITIALIZER;

// To keep track of thread ids, and for joining when terminating threads
pthread_t oscilloscope_thread_handle; // output to oscilloscope thread
pthread_t hardware_thread_handle;     // handles analog/digital hardware

// global variable for only this source code
bool first_info; // boolean for printing info for the first time

int system_init(const char *D2A_port_selection, const char *file_param )
{
    // Declaration of variables
    bool D2A_Port;

    // Variables to read file_param
    FILE *fp;                  // file pointer
    char str_buffer[64];
    char *temp_str;            // temp string variable to help parse file
    int line_length = 0;        // size of line
    int count;                 // for loop counter
    // Just to make it a little bit more robust, instead of assuming they are in order
    const char *freq_str   = "Frequency: ";
    const char *amp_str    = "Amplitude: ";
    const char *offset_str = "Offset: ";

    // pthread attribute
    pthread_attr_t joinable_attr;

    // initializations
    kill_switch = false;
    first_info = true; // check whether info is printed for the first time, if yes, do not display save instructions

    // Check validity of file_param
    if ( strcmp(file_param, "0") ) // if file_param is not "0"
    {
        if ( ( fp = fopen(file_param, "r") ) == NULL ) // check whether able to open file according to path given, if not, return -1
            return -1;
        else
            reuse_param = true;

        // malloc to temp string pointer
        temp_str = (char *) malloc(64);

        for ( count = 0; count < 4; ++count ) // 1 info line + 3 variables
        {
            // get entire line first
            while( (str_buffer[line_length] = getc(fp)) != '\n' )
                ++line_length;
            
            if ( !strncmp(str_buffer, freq_str, strlen(freq_str) ) )
            {
                memset( temp_str, '\0', sizeof(temp_str)); // clear string
                strncpy( temp_str, &str_buffer[strlen(freq_str)], (line_length - strlen(freq_str)) ); // get value
                global_frequency = strtod( temp_str, NULL); // set value
            }
            else if ( !strncmp(str_buffer, amp_str, strlen(amp_str) ) )
            {
                memset( temp_str, '\0', sizeof(temp_str)); // clear string
                strncpy( temp_str, &str_buffer[strlen(amp_str)], (line_length - strlen(amp_str)) ); // get value
                global_amplitude = strtod( temp_str, NULL); // set value
            }
            else if ( !strncmp(str_buffer, offset_str, strlen(offset_str) ) )
            {
                memset( temp_str, '\0', sizeof(temp_str)); // clear string
                strncpy( temp_str, &str_buffer[strlen(offset_str)], (line_length - strlen(offset_str)) ); // get value
                global_offset = strtod( temp_str, NULL); // set value
            }

            // clear string buffer, and line length variables
            memset(str_buffer, 0, sizeof(str_buffer));
            line_length = 0;
        }

        free(temp_str);     // free temp_str memory that we malloc earlier
        fclose(fp);         // close the stream, because we are good people
    }
    else // if file_param is "0"
    {
        reuse_param = false;

        // set default global values
        global_frequency = DEFAULT_FREQUENCY;
        global_amplitude = DEFAULT_AMPLITUDE;
        global_offset    = DEFAULT_OFFSET;
    }

    D2A_Port = D2A_port_selection[0] - '0'; // since we have confirmed Arg 1 is either '0' or '1', can do this directly

    // setup signal handling mask set
    signal_handling_setup();

    // init hardware
    pci_setup();

    // init and set pthread attributes to be joinable
    if( pthread_attr_init(&joinable_attr) ) // returns 0 on success
    {
        perror("pthread_attr_init");
        exit(EXIT_FAILURE);
    }
    if( pthread_attr_setdetachstate(&joinable_attr, PTHREAD_CREATE_JOINABLE) ) // returns 0 on success
    {
        perror("pthread_attr_setdetachstate");
        exit(EXIT_FAILURE);
    }

    // Spawn all wanted threads
    if( pthread_create( &oscilloscope_thread_handle, &joinable_attr, &output_osc_func, &D2A_Port ) ) // returns 0 on success
    {
        perror("pthread_create for output_osc_func");
        exit(EXIT_FAILURE);
    }
    if( pthread_create( &hardware_thread_handle, &joinable_attr, &hardware_handle_func, NULL ) ) // returns 0 on success
    {
        perror("pthread_create for hardware_handle_func");
        exit(EXIT_FAILURE);
    }

    // Mask all signals
    //pthread_sigmask (SIG_SIG_SETMASK, &signal_mask, NULL);

    // Destroys pthread attribute object before leaving this function
    if( pthread_attr_destroy(&joinable_attr) ) // returns 0 on success
    {
        perror("pthread_attr_destroy");
        exit(EXIT_FAILURE);
    }

    return 0; // successfully init all threads
}

void signal_handling_setup()
{
    // empty the signal set first
    sigemptyset(&all_sig_mask_set);

    // Mask all signals, since 1 thread is dedicated to handle signals
    sigfillset (&all_sig_mask_set); //sigdelset() use this in the sig handle thread
}

void* hardware_handle_func(void* arg)
{

}
void* output_osc_func(void* arg)
{
	
}

void save_state(const bool *save_param)
{

}

// basically just wait all threads to join, and check whether to save the param
void system_shutdown(const bool save_param)
{
    void *status;

    if (!reuse_param)
        save_state(save_param);

    // wait for threads to join
    // if( pthread_join(oscilloscope_thread_handle, NULL) ) // returns 0 on success
    // {
    //     perror("pthread_join for oscilloscope_thread_handle");
    //     exit(EXIT_FAILURE);
    // }
    // if( pthread_join(hardware_thread_handle, NULL) ) // returns 0 on success
    // {
    //     perror("pthread_join for hardware_thread_handle");
    //     exit(EXIT_FAILURE);
    // }
    
    // initiating all threads shutdown
    pthread_mutex_lock( &global_stop_mutex );
    kill_switch = true;
    pthread_mutex_unlock( &global_stop_mutex );

    // values may still be updating by the hardware thread at this point, mutex to be safe
    pthread_mutex_lock( &print_mutex );
    printf("\"ctrl+c\" detected, ending program");
    pthread_mutex_lock( &print_mutex );

    // waiting for all threads to join
    // Wait for oscilloscope output thread to join
    if( pthread_join(oscilloscope_thread_handle, &status) ) // returns 0 on success
    {
        printf("ERROR; return code from pthread_join() is %d\n", status);
        exit(EXIT_FAILURE);
    }

    // Wait for hardware thread to join
    if( pthread_join(hardware_thread_handle, &status) ) // returns 0 on success
    {
        printf("ERROR; return code from pthread_join() is %d\n", status);
        exit(EXIT_FAILURE);
    }

    // no need mutex, all threads have terminated
    printf("All threads terminated. Good bye.");

    exit(EXIT_SUCCESS);
}

void INThandler(int sig) // handles SIGINT
{
    
    //char c[32];

    // get global var and lock mutex
    //pthread_mutex_lock( &global_var_mutex );

    // pthread_mutex_lock( &global_stop_mutex );
    // system_pause = true;
    // pthread_mutex_unlock( &global_stop_mutex );



    // printf("\n---------- HI, did you hit \"ctrl + c\"? ----------\n");

    // print_info();

    // scanf("%s" , c);

    // if ( !strcmp(c, "q") || !strcmp(c, "Q") )
    // {
    //     printf("Exiting program!\n");
    //     exit(0);
    // }
    // else if( !strcmp(c, "s") || !strcmp(c, "S") )
    // {
    //     printf("Saving param!\n");
    //     if ( !outputFile(outputPath) )
    //     {
    //         printf("OUTPUT PARAM FAILED!!\n");
    //     }
    // }
    // else
    // {
    //     printf("No valid input, Continue process\n");
    //     signal(SIGINT, INThandler);
    // }

    // pthread_mutex_unlock( &global_var_mutex );
    // printf("----------  Resuming  ----------\n");
}


//output user's current param to file 
int outputFile(char *path){

	FILE *fptr;
    fptr = fopen(path, "w");
    //output time in file
	time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    
    printf("Output Path is %s\n", path);

	if(fptr == NULL)
	{
	   printf("Error with writing! Invalid Path\n");   
	   return 0;             
	}
    fprintf(fptr,"##Output Param at: %d-%d-%d %d:%d\n", tm.tm_year-100, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min);
    fprintf(fptr,"Frequency: %lf\nAmplitude: %lf\nOffset: \n%lf",global_frequency, global_amplitude, global_offset);
	fclose(fptr);
    printf("File saved!\n");
	return 1;
}

void flush_input()
{
  char flush_ch;
  while ( (flush_ch = getchar()) != '\n' && flush_ch != EOF );
}

void print_info()
{
    flush_input();   // so that "press any key to continue..." below works
    system("clear"); // clears the screen

    if ( first_info )
        printf("---------- Welcome! This program outputs waveform to the oscilloscope. ----------\n\n");

    if ( !first_info ) // if first_info == false, means all threads are initialized
    { 
        pthread_mutex_lock( &global_stop_mutex );
        system_pause = true;
        pthread_mutex_unlock( &global_stop_mutex );
    }

    printf("  -Instructions:\n");
    printf("    -Toggle switches(from left to right):\n");
    printf("       a. info switch      (toggling it will display instructions)\n");
    printf("       b. waveform         (change between sine wave and square wave)\n");
    printf("       c. frequency/offset (to choose the parameter for analog input a)\n");
    printf("    -Analog input(from left to right):\n");
    printf("       a. D/A 0            (changes frequency/offset depending on toggle switch c)\n");
    printf("       b. D/A 1            (changes amplitued of wave)\n");
    printf("    -Number of LED lit up shows the amplitude level.\n");
    printf("    -The program will update the terminal screen with the latest values of frequency, amplitude, and offset.\n");
    printf("    -Instructions to save the parameters will be displayed after \"ctrl+c\" is entered.\n\n");
    printf("If you would like to see the instructions again and/or save the parameter, please enter \"ctrl+c\"\n\n");

    if ( !first_info )
    { // display extra instructions/info
        // current values
        printf("  Current frequency: %lf\n", global_frequency);
        printf("  Current amplitude: %lf\n", global_amplitude);
        printf("  Current offset: %lf\n\n", global_offset);

        // save instructions/info
        printf("Enter 's' to save, 'q' to quit!, other inputs to continue\n");

        pthread_mutex_lock( &global_stop_mutex );
        system_pause = false;
        pthread_mutex_unlock( &global_stop_mutex );
    }

    if ( first_info )
    {
        printf("Press any key to continue...");
        first_info = false;
        getc(stdin);
    }
}
