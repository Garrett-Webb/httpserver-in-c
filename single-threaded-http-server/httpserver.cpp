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

#define BUFFER_SIZE 4196
    //static const char status_200[] = "HTTP/1.1 200 OK\r\n";
    //static const char status_201[] = "HTTP/1.1 201 Created\r\n";
    static const char status_400[] = "HTTP/1.1 400 Bad Request\r\n";
    static const char status_404[] = "HTTP/1.1 404 Not Found\r\n";
    static const char status_403[] = "HTTP/1.1 403 Forbidden\r\n";
    static const char status_500[] = "HTTP/1.1 500 Internal Server Error\r\n";

struct httpObject {
    char method[5];
    char filename[200];
    char httpversion[9];    // HTTP/1.1
    ssize_t content_length;
    int status_code;
    uint8_t buffer[BUFFER_SIZE];
};
/*
    clean_httpObject()

    This function will simply reset the object we are working with, 
    clearing buffers and what-not.
    This is for multiple subsequent requests.
*/
void clean_httpObject (struct httpObject *message){
    memset(message->method, 0, sizeof(message->method));
    memset(message->filename, 0, sizeof(message->filename));
    memset(message->httpversion, 0, sizeof(message->httpversion));
    message->content_length = 0;
    message->status_code = 0;
}

/*
    recv_full()

    This function will continuously call recv on a given socket until a given length requirement is met.
    This is useful for the PUT request.
    it will allow files bigger than the buffer size to be recieved.
*/
ssize_t recv_full(struct httpObject *message, ssize_t size, ssize_t client_sockd){ //returns total amount of bytes received
    ssize_t total = 0;
    ssize_t bytes_recv;
    uint8_t buffer[BUFFER_SIZE + 1];
    ssize_t offset = BUFFER_SIZE;
    int fd = open(message->filename, O_CREAT | O_RDWR | O_TRUNC | O_APPEND);
    printf("    recieving all\n");
    printf("    Total Size: %zu\n", size);
    while (total < size){
        memset(&buffer, 0, sizeof(buffer));
        if ((size - total) < BUFFER_SIZE){
            offset = size - total;
            printf("        offset: %zu\n", offset);
        }
        
        bytes_recv = recv(client_sockd, &buffer[total], offset, 0);
        printf("    Bytes Recieved: %zu\n", bytes_recv);
        if (bytes_recv < 0 || bytes_recv > size){
            if (errno == EAGAIN){ //https://man7.org/linux/man-pages/man2/accept.2.html
                continue;
            }
            return bytes_recv;
        }

        else if (bytes_recv == 0){
            return total;
        }
        else{
            write(fd, &buffer, bytes_recv);
            total += bytes_recv;
            printf("    writing buffer: %zu\n", total);
        }
    }
    close(fd);
    printf("    finished recieving all\n");
    return total;
}


/*
    check_filename()

    This function will check to see if a filename is within the size limit (10)
    as well as adhering to the alphanumeric character rule.
*/
bool check_filename(struct httpObject *message){
    int length = strlen(message->filename);
    if (length != 10){
        printf("    Filename checked false\n");
        return false;
    }

        
    for (int i = 0; i < length; i++ ){
        if ((isalpha(message->filename[i]) != 0 || isdigit(message->filename[i]) != 0)){
            continue;
        }
        else{
            printf("    Filename checked false\n");
            return false;
        }
    }
    printf("    Filename checked true\n");
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
    printf("[+] Reading HTTP Message\n");
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
        printf("    Content-Length: %zu\n", message->content_length);
    }

    //get the method
    recv(client_sockd, temp, BUFFER_SIZE, MSG_PEEK);
    const char searchstring3[] = " ";
    end = strstr(temp, searchstring3);
    *end = '\0';
    strcpy(message->method, temp);
    printf("    method: %s\n", message->method);

    //get the filename
    recv(client_sockd, temp, BUFFER_SIZE, MSG_PEEK);
    const char searchstring4[] = "/";
    const char searchstring5[] = "HTTP/";
    beginning = strstr(temp, searchstring4);
    end = strstr(temp, searchstring5);
    beginning = (char*)beginning + (sizeof(char));
    end = (char*)end - (sizeof(char));
    *end = '\0';
    strcpy(message->filename, beginning);
    printf("    filename: %s\n", message->filename);

    //get the http version
    recv(client_sockd, temp, BUFFER_SIZE, MSG_PEEK);
    const char searchstring6[] = "HTTP";
    const char searchstring7[] = "\r\n";
    beginning = strstr(temp, searchstring6);
    end = strstr(temp, searchstring7);
    *end = '\0';
    strcpy(message->httpversion, beginning);
    printf("    HTTP Ver: %s\n", message->httpversion);
    int bytesread = recv(client_sockd, temp, BUFFER_SIZE, 0);
    bytesread = bytesread;
    if (!((strcmp(message->method, "GET") != 0) || (strcmp(message->method, "PUT") != 0))){
        message->status_code = 400;
        printf("    Error 400 method\n");
        return;
    }

    if ((strcmp(message->httpversion, "HTTP/1.1") != 0)){
        message->status_code = 400;
        printf("    Error 400 httpver\n");
        return;
    }
    
    if(check_filename(message) == false){
        message->status_code = 400;
        printf("    Error 400 filename\n");
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
    printf("[+] Constructing Response\n");

    if(strcmp(message->method, "PUT") == 0){
        
        if (message->status_code != 400 && message->status_code != 404 && message->status_code != 403 && message->status_code != 500 ){
            
            int fd;
            //int count = 0;
            uint8_t buffer[BUFFER_SIZE + 1];
            //ssize_t bytes;
            if(recv(client_sockd,&buffer, sizeof(buffer), MSG_PEEK) == 0 && message->content_length != 0){
                printf("Client has disconnected. Halting Execution and Closing Connection.\n");
                return;
            }
            memset(buffer, 0, sizeof(buffer));
            
            fd = open(message->filename, O_CREAT | O_RDWR | O_TRUNC, 0666); // http://permissions-calculator.org/

            //TODO: if FD is a forbidden file, error
            struct stat fst;
            stat(message->filename, &fst);
            if (!(fst.st_mode & S_IWUSR)){ //others permissions write.
                printf("    403 write permissions error\n");
                send(client_sockd, &status_403, sizeof(status_403), 0);
                close(fd);
                return;
            }
            else {
                printf("    Permissions OK\n");
            }
            close(fd);
            dprintf(client_sockd, "HTTP/1.1 201 CREATED\r\n");
            ssize_t result = recv_full(message, message->content_length, client_sockd);
            if (result < message->content_length){
                message->status_code = 500;
                close(client_sockd);
            }
            message->status_code = 201;
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
        printf("    PUT  Status Code: %u\n", message->status_code);
        printf("    PUT  Content Length: %zu\n", message->content_length);
    }
    else if(strcmp(message->method, "GET") == 0){
        
        
        struct stat fst;
        stat(message->filename, &fst);
        ssize_t fsize = fst.st_size;

        if (message->status_code != 400 && message->status_code != 404 && message->status_code != 403 && message->status_code != 500 ){
            
            
            uint8_t buffer[BUFFER_SIZE + 1];
            memset(buffer, 0, sizeof(buffer));
            struct stat st;
            int fd = open(message->filename, O_RDONLY);
        
            if(stat(message->filename, &st) != 0){
                printf("    filename: %s --Does Not Exist\n", message->filename);
                message->status_code = 404;
                send(client_sockd, &status_404, sizeof(status_404), 0);
                return;
            }
            else{
                printf("    filename: %s --Exists\n", message->filename);
            }

            //TODO: if file is forbidden, error 403
            if (!(st.st_mode & S_IROTH)){ //others permissions read.
                printf("    403 read permissions error\n");
                send(client_sockd, &status_403, sizeof(status_403), 0);
                return;
            }
            else {
                printf("    Permissions OK\n");
            }

            ssize_t size = st.st_size;
            ssize_t offset = BUFFER_SIZE;
            dprintf(client_sockd, "HTTP/1.1 200 OK\r\nContent-Length: %li\r\n\r\n", fsize);
            ssize_t total = 0;
            while (size > 0){
                ssize_t bytesRead = read(fd, &buffer, BUFFER_SIZE);
                
                if ((size - total) < BUFFER_SIZE){
                    offset = size - total;
                    printf("        offset: %zu\n", offset);
                }

                ssize_t bytesSend = send(client_sockd, &buffer[total], offset, 0);
                if (bytesSend <= 0){
                    printf("    Bytes read: %zu\n", bytesRead);
                    printf("    Bytes sent: %zu\n", bytesSend);
                    return;
                }

                total += bytesSend;
                size -= bytesSend;
            }
            message->content_length = total;
            if (total != fsize){
                close(client_sockd);
            }
            message->status_code = 200;
            close(fd);

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
        printf("    GET  Status Code: %u\n", message->status_code);
        printf("    GET  Content Length: %zu\n", message->content_length);
    }
    else{
        printf("    method: %s\n", message->method);
        printf("    Status Code: %u\n", message->status_code);
        send(client_sockd, &status_400, sizeof(status_400), 0);
    }
    
    return;
}


int main(int argc, char** argv) {
    /*
        Create sockaddr_in with server information
    */
    char port[6];
    if(argc > 2){
        strncpy(port, argv[2], sizeof(port));
    }
    else{
        strncpy(port, "80    ", sizeof(port));
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

    struct httpObject message;

    while (true) {
        printf("[+] Server is waiting...\n");

        //1. Accept Connection
        int client_sockd = accept(server_sockd, &client_addr, &client_addrlen);
        
        if (client_sockd == -1){
            //server error in accepting socket
            send(client_sockd, &status_500, sizeof(status_500), 0);
        }

        read_http_response(client_sockd, &message);
        
        construct_http_response(client_sockd, &message);

        printf("[+] Response Sent\n\n\n");
        close(client_sockd);
    }

    return EXIT_SUCCESS;
}