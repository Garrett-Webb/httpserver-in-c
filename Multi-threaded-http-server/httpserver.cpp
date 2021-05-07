#include <sys/socket.h>
#include <sys/stat.h>
#include <stdio.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <fcntl.h>
#include <unistd.h> // write
#include <string.h> // memset
#include <stdlib.h> // atoi
#include <stdbool.h> // true, false
#include <errno.h>
#include <ctype.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <getopt.h>
#include <err.h>
#include <unordered_map>
#include <iostream> 
#include <string> 

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define BUFFER_SIZE 4196
static const char status_400[] = "HTTP/1.1 400 Bad Request\r\n";
static const char status_404[] = "HTTP/1.1 404 Not Found\r\n";
static const char status_403[] = "HTTP/1.1 403 Forbidden\r\n";
static const char status_500[] = "HTTP/1.1 500 Internal Server Error\r\n";
static const char copy1[] = "./copy1/";
static const char copy2[] = "./copy2/";
static const char copy3[] = "./copy3/";

struct httpObject {
    char method[5];
    char filename[200];
    char redund_filename1[200];
    char redund_filename2[200];
    char redund_filename3[200];
    char httpversion[9];    // HTTP/1.1
    ssize_t content_length;
    int status_code;
    uint8_t buffer[BUFFER_SIZE];
};
struct requestObject {
    int id;
    int clientfd;
    struct requestObject *next;
};

pthread_mutex_t request_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
pthread_mutex_t hash_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;
pthread_cond_t request_cond = PTHREAD_COND_INITIALIZER;

int threads = 1;
bool redund = false;
struct requestObject* request_ptr = NULL;     
struct requestObject* last_request = NULL; 
int queue_size = 0;
std::unordered_map<std::string, pthread_mutex_t *> map;

/*
    clean_httpObject()

    This function will simply reset the object we are working with, 
    clearing buffers and what-not.
    This is for multiple subsequent requests.
*/
void clean_httpObject (struct httpObject *message){
    memset(message->method, 0, sizeof(message->method));
    memset(message->filename, 0, sizeof(message->filename));
    memset(message->redund_filename1, 0, sizeof(message->redund_filename1));
    memset(message->redund_filename2, 0, sizeof(message->redund_filename2));
    memset(message->redund_filename3, 0, sizeof(message->redund_filename3));
    memset(message->httpversion, 0, sizeof(message->httpversion));
    message->content_length = 0;
    message->status_code = 0;
}

int compareFiles(struct httpObject *message, ssize_t client_sockd) 
{
    //returns 0 for redundancy error
    //returns integer value 1 || 2 || 3 for which copy file to read
    struct stat st1, st2, st3;
    int fd1 = open(message->redund_filename1, O_RDONLY);
    int fd2 = open(message->redund_filename2, O_RDONLY);
    int fd3 = open(message->redund_filename3, O_RDONLY);
    int return_num = 0;
    int num_bad = 0;
    if(stat(message->redund_filename1, &st1) != 0){
        //printf("    filename: %s --Does Not Exist\n", message->redund_filename1);
        num_bad++;
    }
    if(stat(message->redund_filename2, &st2) != 0){
        //printf("    filename: %s --Does Not Exist\n", message->redund_filename2);
        num_bad++;
    }
    if(stat(message->redund_filename3, &st3) != 0){
        //printf("    filename: %s --Does Not Exist\n", message->redund_filename3);
        num_bad++;
    }
    if (!(st1.st_mode & S_IROTH) && !(st2.st_mode & S_IROTH)){ //others permissions read.
        //printf("    403 read permissions error\n");
        send(client_sockd, &status_403, sizeof(status_403), 0);
        return 0;
    }
    else if (!(st2.st_mode & S_IROTH) && !(st3.st_mode & S_IROTH)){
        //printf("    403 read permissions error\n");
        send(client_sockd, &status_403, sizeof(status_403), 0);
        return 0;
    }
    else if (!(st1.st_mode & S_IROTH) && !(st3.st_mode & S_IROTH)){
        //printf("    403 read permissions error\n");
        send(client_sockd, &status_403, sizeof(status_403), 0);
        return 0;
    }
    else {
        //printf("    Permissions OK\n");
    }
    if(num_bad == 3){
        dprintf(client_sockd, "HTTP/1.1 404 Not Found\r\n");
        return 0;
    }
    else if(num_bad == 2){
        dprintf(client_sockd, "HTTP/1.1 500 Internal Server Error\r\n");
        return 0;
    }
    ssize_t size1 = st1.st_size;
    ssize_t size2 = st2.st_size;
    ssize_t size3 = st3.st_size;
    ssize_t size = 0;
    size = size; //to get rid of warning
    if (size1 == size2){
        size = size1;
    }
    else if (size2 == size3){
        size = size2;
    }
    else if (size1 == size3){
        size = size1;
    }
    else{
        //none of the files are the same size
        //printf("    redudant files are not the same size\n");
        dprintf(client_sockd, "HTTP/1.1 500 Internal Server Error\r\n");
        close(fd1);
        close(fd2);
        close(fd3);
        return 0;
    }

    uint8_t buffer1[BUFFER_SIZE + 1];
    uint8_t buffer2[BUFFER_SIZE + 1];
    uint8_t buffer3[BUFFER_SIZE + 1];
    read(fd1, &buffer1, BUFFER_SIZE);
    read(fd2, &buffer2, BUFFER_SIZE);
    read(fd3, &buffer3, BUFFER_SIZE);
    
    if (memcmp(buffer1, buffer2, MAX(size1, size2)) == 0){
        //printf("    1 and 2 are the same, use file in copy1\n");
        return_num = 1;
    }
    else if (memcmp(buffer1, buffer3, MAX(size1, size3)) == 0){
        //printf("    1 and 3 are the same, use file in copy1\n");
        return_num = 1;
    }
    else if (memcmp(buffer2, buffer3, MAX(size2, size3)) == 0){
        //printf("    2 and 3 are the same, use file in copy2\n");
        return_num = 2;
    }
    
    memset(&buffer1, 0, sizeof(buffer1));
    memset(&buffer2, 0, sizeof(buffer2));
    memset(&buffer3, 0, sizeof(buffer3));
    close(fd1);
    close(fd2);
    close(fd3);
    if (return_num == 0){
        //printf("    redudant files are not the same content\n");
        dprintf(client_sockd, "HTTP/1.1 500 Internal Server Error\r\n");
        return 0;
    }

    //TODO check permissions of the file we chose

    else return return_num;
} 

/*
    recv_full()

    This function will continuously call recv on a given socket until a given length requirement is met.
    This is useful for the PUT request.
    it will allow files bigger than the buffer size to be recieved.
*/
ssize_t recv_full(struct httpObject *message, ssize_t client_sockd){ //returns total amount of bytes received
    if (redund){
        ssize_t total = 0;
        ssize_t bytes_recv = 1;
        uint8_t buffer[BUFFER_SIZE + 1];
        ssize_t bytes_written1 = 0;
        ssize_t bytes_written2 = 0;
        ssize_t bytes_written3 = 0;
        int fd1 = open(message->redund_filename1, O_CREAT | O_RDWR | O_TRUNC | O_APPEND, 0666);
        int fd2 = open(message->redund_filename2, O_CREAT | O_RDWR | O_TRUNC | O_APPEND, 0666);
        int fd3 = open(message->redund_filename3, O_CREAT | O_RDWR | O_TRUNC | O_APPEND, 0666);
        while ( bytes_recv > 0 ){
            bytes_recv = read(client_sockd, buffer, BUFFER_SIZE);
            if (bytes_recv > 0){
                bytes_written1 = write(fd1, &buffer, bytes_recv);
                bytes_written2 = write(fd2, &buffer, bytes_recv);
                bytes_written3 = write(fd3, &buffer, bytes_recv);
                total += bytes_written1;
                
                if((bytes_written1 != bytes_written2) || (bytes_written1 != bytes_written3) || (bytes_written2 != bytes_written3)){
                    printf("warning: redundant write error");
                    close(fd1);
                    close(fd2);
                    close(fd3);
                    return total;
                }
                total += bytes_written1;
                if(total >= message->content_length){
                    close(fd1);
                    close(fd2);
                    close(fd3);
                    return total;
                }
            }
        }
        close(fd1);
        close(fd2);
        close(fd3);
        return total;
    }
    else if (!redund){
        ssize_t total = 0;
        ssize_t bytes_recv = 1; //init to 1 for loop
        uint8_t buffer[BUFFER_SIZE + 1];
        ssize_t bytes_written = 0;
        int fd = open(message->filename, O_CREAT | O_RDWR | O_TRUNC | O_APPEND);
        while (bytes_recv > 0){
            bytes_recv = read(client_sockd, buffer, BUFFER_SIZE);
            if (bytes_recv > 0){
                bytes_written = write(fd,buffer,bytes_recv);
                total += bytes_written;
                if(total >= message->content_length){
                    close(fd);
                    return total;
                }
            }
        }
        close(fd);
        return total;
    }
    return 0;
}


/*
    check_filename()

    This function will check to see if a filename is within the size limit (10)
    as well as adhering to the alphanumeric character rule.
*/
bool check_filename(struct httpObject *message){
    int length = strlen(message->filename);
    if (length != 10){
        //printf("    Filename checked false\n");
        return false;
    }

        
    for (int i = 0; i < length; i++ ){
        if ((isalpha(message->filename[i]) != 0 || isdigit(message->filename[i]) != 0)){
            continue;
        }
        else{
            //printf("    Filename checked false\n");
            return false;
        }
    }
    //printf("    Filename checked true\n");
    return true;
}

/*
    Want to read incoming HTTP request, 
    parse through the string to get important info.
    This function will fill the message struct with data such as:
    content length, filename, httpver, method

    It will also check this request to see if it is good or bad (400)
*/
void read_http_response(ssize_t client_sockd, struct httpObject* message) {
    //printf("[+] Reading HTTP Message\n");
    clean_httpObject(message);
    /*
     * Start constructing HTTP request based off data from socket
     */
    char temp[BUFFER_SIZE + 1];
    recv(client_sockd, temp, BUFFER_SIZE, MSG_PEEK);
    //size_t bytes = recv(client_sockd, buffer, BUFFER_SIZE, 0);
    char *beginning;
    char *end;

    //get the content length from the string
    const char searchstring1[] = "Content-Length: ";
    const char searchstring2[] = "\n";
    beginning = strstr(temp, searchstring1);
    if (beginning != NULL){ //if the content-length was included, we do stuff with it
        end = strstr(beginning, searchstring2);
        beginning = (char*)beginning + 16 * (sizeof(char));
        end = (char*)end - (sizeof(char));
        *end = '\0';
        message->content_length = atoi(beginning);
        //printf("    Content-Length: %zu\n", message->content_length);
    }

    //get the method
    recv(client_sockd, temp, BUFFER_SIZE, MSG_PEEK);
    const char searchstring3[] = " ";
    end = strstr(temp, searchstring3);
    *end = '\0';
    strcpy(message->method, temp);
    //printf("    method: %s\n", message->method);

    //get the filename
    recv(client_sockd, temp, BUFFER_SIZE, MSG_PEEK);
    const char searchstring4[] = " ";
    const char searchstring5[] = "HTTP/";
    beginning = strstr(temp, searchstring4);
    end = strstr(temp, searchstring5);
    beginning = (char*)beginning + (sizeof(char));
    if(beginning[0] == '/'){
        //printf("The filename begins with /");
        beginning = (char*)beginning + (sizeof(char));
    }
    end = (char*)end - (sizeof(char));
    *end = '\0';
    strcpy(message->filename, beginning);
    if (redund){
        strcat(message->redund_filename1, copy1);
        strcat(message->redund_filename1, message->filename);
        strcat(message->redund_filename2, copy2);
        strcat(message->redund_filename2, message->filename);
        strcat(message->redund_filename3, copy3);
        strcat(message->redund_filename3, message->filename);
        
        //printf("    copy_filename1: %s\n", message->redund_filename1);
        //printf("    copy_filename2: %s\n", message->redund_filename2);
        //printf("    copy_filename3: %s\n", message->redund_filename3);
    }
    //printf("    filename: %s\n", message->filename);

    //get the http version
    recv(client_sockd, temp, BUFFER_SIZE, MSG_PEEK);
    const char searchstring6[] = "HTTP";
    const char searchstring7[] = "\r\n";
    beginning = strstr(temp, searchstring6);
    end = strstr(temp, searchstring7);
    *end = '\0';
    strcpy(message->httpversion, beginning);
    //printf("    HTTP Ver: %s\n", message->httpversion);
    int bytesread = recv(client_sockd, temp, BUFFER_SIZE, 0);
    bytesread = bytesread;
    if (!((strcmp(message->method, "GET") != 0) || (strcmp(message->method, "PUT") != 0))){
        message->status_code = 400;
        //printf("    Error 400 method\n");
        return;
    }

    if ((strcmp(message->httpversion, "HTTP/1.1") != 0)){
        message->status_code = 400;
        //printf("    Error 400 httpver\n");
        return;
    }
    
    if(check_filename(message) == false){
        message->status_code = 400;
        //printf("    Error 400 filename\n");
        return;
    }

    return;
}

/*
    construct_http_response()

    This function will begin constructing a response to the request.
    it will also check to see if the file exists (404), and if the file is forbidden (403)
*/
void construct_http_response(ssize_t client_sockd, struct httpObject *message) {
    if(strcmp(message->method, "PUT") == 0){
        if (message->status_code != 400 && message->status_code != 404 && message->status_code != 403 && message->status_code != 500 ){
            if(!redund){
                int fd;
                uint8_t buffer[BUFFER_SIZE + 1];
                if(recv(client_sockd,&buffer, sizeof(buffer), MSG_PEEK) == 0 && message->content_length != 0){
                    printf("Client has disconnected. Halting Execution and Closing Connection.\n");
                    return;
                }
                memset(buffer, 0, sizeof(buffer));
                fd = open(message->filename, O_CREAT | O_RDWR | O_TRUNC, 0666); // http://permissions-calculator.org
                struct stat fst;
                stat(message->filename, &fst);
                if (!(fst.st_mode & S_IWUSR)){
                    send(client_sockd, &status_403, sizeof(status_403), 0);
                    close(fd);
                    return;
                }
                close(fd);
                ssize_t result = recv_full(message, client_sockd);
                if (result < message->content_length){
                    message->status_code = 500;
                    dprintf(client_sockd, "HTTP/1.1 500 Internal Server Error\r\n");
                    close(client_sockd);
                }
                dprintf(client_sockd, "HTTP/1.1 201 CREATED\r\n");
                message->status_code = 201;
            }
            else if (redund){
                uint8_t buffer[BUFFER_SIZE + 1];
                if(recv(client_sockd,&buffer, sizeof(buffer), MSG_PEEK) == 0 && message->content_length != 0){
                    printf("Client has disconnected. Halting Execution and Closing Connection.\n");
                    return;
                }
                memset(buffer, 0, sizeof(buffer));
                ssize_t result = recv_full(message, client_sockd);
                if (result < message->content_length){
                    message->status_code = 500;
                    dprintf(client_sockd, "HTTP/1.1 500 Internal Server Error\r\n");
                    close(client_sockd);
                }
                int x = compareFiles(message, client_sockd);
                if (x == 0){
                    dprintf(client_sockd, "HTTP/1.1 500 Internal Server Error\r\n");
                    return;
                }
                dprintf(client_sockd, "HTTP/1.1 201 CREATED\r\n");
                message->status_code = 201;
                return;
            }
        }
        else if(message->status_code == 400){
            send(client_sockd, &status_400, sizeof(status_400), 0);
        }
        else if(message->status_code == 404){
            send(client_sockd, &status_404, sizeof(status_404), 0);
        }
        else if(message->status_code == 500){
            send(client_sockd, &status_500, sizeof(status_500), 0);
        }
        else if(message->status_code == 403){
            send(client_sockd, &status_403, sizeof(status_403), 0);
        }
        else{
            send(client_sockd, &status_500, sizeof(status_500), 0);
        }
        //printf("    PUT  Status Code: %u\n", message->status_code);
        //printf("    PUT  Content Length: %zu\n", message->content_length);
    }
    else if(strcmp(message->method, "GET") == 0){
        struct stat fst;
        stat(message->filename, &fst);
        ssize_t fsize = fst.st_size;

        if (message->status_code != 400 && message->status_code != 404 && message->status_code != 403 && message->status_code != 500 ){
            if (redund){
                int x = compareFiles(message, client_sockd);
                if (x == 0){
                    return;
                }
                uint8_t buffer[BUFFER_SIZE + 1];
                memset(buffer, 0, sizeof(buffer));
                struct stat st;
                int fd = 0;
                if (x == 1){
                    fd = open(message->redund_filename1, O_RDONLY);
                    stat(message->redund_filename1, &st);
                }
                else if (x == 2){
                    fd = open(message->redund_filename2, O_RDONLY);
                    stat(message->redund_filename2, &st);
                }
                else {
                    dprintf(client_sockd, "HTTP/1.1 500 Internal Server Error\r\n");
                    return;
                }
                dprintf(client_sockd, "HTTP/1.1 200 OK\r\nContent-Length: %li\r\n\r\n", st.st_size);
                ssize_t total = 0;
                ssize_t bytes_send=0;
                ssize_t bytes_read = 0;
                while ((bytes_send = read(fd, buffer, BUFFER_SIZE)) > 0){
                    bytes_read = write(client_sockd,buffer,bytes_send);
                    total += bytes_read;
                    if(total >= message->content_length){
                        close(fd);
                        break;
                    }
                }
                message->content_length = total;
                if (total != fsize){
                    close(client_sockd);
                }
                message->status_code = 200;
                close(fd);
            }
            else{
                uint8_t buffer[BUFFER_SIZE + 1];
                memset(buffer, 0, sizeof(buffer));
                struct stat st;
                int fd = open(message->filename, O_RDONLY);
            
                if(stat(message->filename, &st) != 0){
                    message->status_code = 404;
                    send(client_sockd, &status_404, sizeof(status_404), 0);
                    return;
                }
                if (!(st.st_mode & S_IROTH)){
                    send(client_sockd, &status_403, sizeof(status_403), 0);
                    return;
                }
                dprintf(client_sockd, "HTTP/1.1 200 OK\r\nContent-Length: %li\r\n\r\n", st.st_size);
                ssize_t total = 0;
                ssize_t bytes_send=0;
                ssize_t bytes_read = 0;
                while ((bytes_send = read(fd, buffer, BUFFER_SIZE)) > 0){
                    bytes_read = write(client_sockd,buffer,bytes_send);
                    total += bytes_read;
                    if(total >= message->content_length){
                        close(fd);
                        break;
                    }
                }
                message->content_length = total;
                if (total != fsize){
                    close(client_sockd);
                }
                message->status_code = 200;
                close(fd);
            }
        }
        else if(message->status_code == 400){
            send(client_sockd, &status_400, sizeof(status_400), 0);
        }
        else if(message->status_code == 404){
            send(client_sockd, &status_404, sizeof(status_404), 0);
        }
        else if(message->status_code == 500){
            send(client_sockd, &status_500, sizeof(status_500), 0);
        }
        else if(message->status_code == 403){
            send(client_sockd, &status_403, sizeof(status_403), 0);
        }
        else{
            send(client_sockd, &status_500, sizeof(status_500), 0);
        }
        //printf("    GET  Status Code: %u\n", message->status_code);
        //printf("    GET  Content Length: %zu\n", message->content_length);
    }
    else{
        //printf("    method: %s\n", message->method);
        //printf("    Status Code: %u\n", message->status_code);
        send(client_sockd, &status_400, sizeof(status_400), 0);
    }
    
    return;
}

/*
dequeue_request()

this function will be called by the worker threads to grab the first request in the queue.
The request is of type Struct requestObject* and will be returned.
it is expected that the mutex is locked before this function is called, to prevent multiple
threads working on the same request.
*/
struct requestObject* dequeue_request(){
    struct requestObject *request;
    if(queue_size <= 0){
        request = NULL;
    }else{
        request = request_ptr;
        request_ptr = request_ptr->next;
        queue_size-=1;
    }
    return request;
}
/*
enqueue_request()

This function will be called by the main dispatcher thread running on the main() function.
once called, it will create a new request object and add it to the queue. it will lock the
mutex when accessing the critical section that is the queue. It will also signal for all of
the dormant threads to grab the new request.
*/
void enqueue_request(int client_fd, int id){
    struct requestObject *request = (requestObject*)malloc(sizeof(struct requestObject));
    request->next = NULL;
    request->clientfd = client_fd;
    request->id = id;

    pthread_mutex_lock(&request_mutex);
    queue_size += 1;
    if(request_ptr != NULL){ //empty list
        last_request->next = request;
        last_request = request;
    }else{
        request_ptr = request;
        last_request = request;
    }
    pthread_cond_signal(&request_cond);
    pthread_mutex_unlock(&request_mutex);
}

void* thread_loop(void* data){
    int id = *((int*)data);
    struct requestObject *request;
    struct httpObject *message;
    while(true){
        pthread_mutex_lock(&request_mutex);
        if(queue_size > 0){
            request = dequeue_request();    //This locks, if the request is already 0 (another thread intercepts it will return NULL)
            pthread_mutex_unlock(&request_mutex);
            if(request != NULL){
                printf("    [-]  Thread ID: %i RECIEVED  request ID: %i\n", id, request->id); 
                
                message = (struct httpObject*)malloc(sizeof(struct httpObject));
                
                read_http_response(request->clientfd, message);
                std::string name(message->filename);

                pthread_mutex_lock(&hash_mutex); //accessing global map, lock
                map.emplace(name, new pthread_mutex_t(PTHREAD_MUTEX_INITIALIZER));

                pthread_mutex_lock(map[name]);  //lock the mutex for the specific file this thread is working on
                pthread_mutex_unlock(&hash_mutex); //done with map, unlock
                construct_http_response(request->clientfd, message);
                pthread_mutex_unlock(map[name]);//done working with files, unlock the mutex

                close(request->clientfd);

                printf("    [+]  Thread ID: %i COMPLETED request ID: %i %s %s\n", id, request->id, message->method, message->filename);

                free(message);
                free(request);
            }else{
                pthread_mutex_unlock(&request_mutex);
            }
        }else{
            pthread_mutex_unlock(&request_mutex);
            pthread_cond_wait(&request_cond, &request_mutex);
        }
    }
}


int main(int argc, char** argv) {
    /*
        Create sockaddr_in with server information
    */
    char port[6];
    if(argc > 2){
        if (strcmp(argv[2], "-N") && strcmp(argv[2], "-r")){
            strncpy(port, argv[2], sizeof(port));
        }
        else{
            strncpy(port, "80", sizeof(port));
        }
        for(int i=0; i < argc; i++){
            if (!strcmp(argv[i], "-N")){
                threads = atoi(argv[i+1]);
            }
            if (!strcmp(argv[i], "-r")){
                redund = true;
            }
        }
        printf("Currently running on port: %s\n", port);
        printf("Currently running with: %i Threads\n", threads);
        printf(redund ? "Redundancy Enabled\n" : "Redundancy Disabled\n");
    }
    else{
        strncpy(port, "80", sizeof(port));
        for(int i=0; i < argc; i++){
            if (!strcmp(argv[i], "-N")){
                threads = atoi(argv[i+1]);
            }
            if (!strcmp(argv[i], "-r")){
                redund = true;
            }
        }
        printf("Currently running on port: %s\n", port);
        printf("Currently running with: %i Threads\n", threads);
        printf(redund ? "Redundancy Enabled\n" : "Redundancy Disabled\n");
    }
    if (threads < 1){
        threads = 4;
    }
    int* thread_id = (int*)malloc(threads * sizeof(int)); 
    pthread_t* p_threads = (pthread_t*)malloc(threads * sizeof(pthread_t));
        for(int i = 0; i < threads; i++){
	        thread_id[i] = i+1;
            pthread_create(&p_threads[i], NULL, thread_loop, (void*)&thread_id[i]);
        }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port));
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    socklen_t addrlen = sizeof(server_addr);
    //Create server socket
    int server_sockd = socket(AF_INET, SOCK_STREAM, 0);
    // Need to check if server_sockd < 0, meaning an error
    if (server_sockd < 0) {
        perror("socket");
    }
    //Configure server socket
    int enable = 1;
    //This allows you to avoid: 'Bind: Address Already in Use' error
    int ret = setsockopt(server_sockd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    //Bind server address to socket that is open
    ret = bind(server_sockd, (struct sockaddr *) &server_addr, addrlen);
    //Listen for incoming connections
    ret = listen(server_sockd, 5); // 5 should be enough, if not use SOMAXCONN
    if (ret < 0) {
        return EXIT_FAILURE;
    }
    //Connecting with a client
    struct sockaddr client_addr;
    socklen_t client_addrlen;
    int counter = 0;
    printf("[+]  The server is now ready to accept connections\n");
    while (true) {
        //1. Accept Connection
        int client_sockd = accept(server_sockd, &client_addr, &client_addrlen);
        if (client_sockd < 0){
            //server error in accepting socket
            warn("accept error");  
            continue; 
        }
        //if no error, enqueue the request to be picked up by a worker thread.
        enqueue_request(client_sockd, counter);
        //increment the thread ID counter
        counter++;
    }
    free(thread_id);
    free(p_threads);
    return EXIT_SUCCESS;
}
