//Garrett Webb

#include <stdio.h>   
#include <unistd.h> 
#include <err.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_MEM 32

int main( int argc, char *argv[] ){

    int n;
    int writecheck;
    char buf[ MAX_MEM ];

    if(argc == 1){
        //if there are no arguments passed in,
        //we echo output
        n=1;
        while(n > 0){
            //if read() returns nonnegative
            //then it is a success
            n = read( 0, buf, MAX_MEM );
            writecheck = write( 1, buf, n );
            if( writecheck != n ) {
                //if write returns anything but n, error out
                perror( "dog" );
            }
        }
    }
    else{
        int fileMax = argc - 1;
        int fileCounter = 1;
        int filesDescriptor;
        int filenum; 
        char *str = "dog: ";
        struct stat st;
        while( fileCounter <= fileMax ) {

            if( strcmp( "-", argv[ fileCounter ] ) == 0 ) {
                //first check if the argument is a -
                //that means we echo input
                //this is inside the loop so that a "-" in 
                //any position will work correctly
                while( n > 0 ) {
                    //if read() returns nonnegative
                    //then it is a success
                    n = read( 0, buf, MAX_MEM );
                    writecheck = write( 1, buf, n );
                    if( writecheck != n ) {
                        //if write returns anything but n, error out
                        perror( "dog" );
                    }
                }
            }
            else{
                //it is an actual file argument, open it
                filesDescriptor = open( argv[ fileCounter ], O_RDONLY );
				if( filesDescriptor == -1 ) {
					//if the file does not exist, couldnt be opened
					//give warning and continue to next argument
					warnx( "%s: No such file or directory", argv[ fileCounter ] );
					fileCounter++;
					continue;
				}
                struct stat st;
                stat(argv[ fileCounter ], &st);
				if( S_ISDIR( st.st_mode ) ) {
					//if the argument is a directory, not a file
					//give warning and continue to next argument
                    warnx( "%s: Is a directory", argv[ fileCounter ] );
					fileCounter++;
					continue;
                }

                if ( stat( argv[ fileCounter ], &st ) != 0 ) {
                    //now that we know the file is real,
                    //make sure the program can access the file
                    //if not warn user and skip it
                    warnx( "%s: Permission denied", argv[ fileCounter ] );
                    fileCounter++;
                    continue;
                }
                else{
                    //file exists, is not directory, and is accessible
                    //now we read it to the buffer
                    filenum = 1;
                    while ( filenum > 0 ){
                        filenum = read( filesDescriptor, buf, MAX_MEM );
                        writecheck = write( 1, &buf, filenum );
                        if( writecheck != filenum ) {
                            perror( "dog" );
                        }
					}
                }
                //gg go next
                close( filesDescriptor );
            }
			//after every incarnation of the loop, go to the next file
            fileCounter++;
        }
    }
}