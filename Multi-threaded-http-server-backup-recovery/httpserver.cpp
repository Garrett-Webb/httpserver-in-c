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
#include <vector>
#include <time.h>
#include <string>
#include <dirent.h>

#define BUFFER_SIZE 4196
    //static const char status_200[] = "HTTP/1.1 200 OK\r\n";
    //static const char status_201[] = "HTTP/1.1 201 Created\r\n";
    static const char status_400[] = "HTTP/1.1 400 Bad Request\r\n";
    static const char status_404[] = "HTTP/1.1 404 Not Found\r\n";
    static const char status_403[] = "HTTP/1.1 403 Forbidden\r\n";
    static const char status_500[] = "HTTP/1.1 500 Internal Server Error\r\n";
    bool backupflag = false;
    bool recoveryflag = false;
    bool listflag = false;
    std::vector<char*> list;

struct httpObject {
    char method[5];
    char filename[200];
    char timestamp[200];
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
ssize_t recv_full(struct httpObject *message, ssize_t client_sockd){ //returns total amount of bytes received
        ssize_t total = 0;
        ssize_t bytes_recv = 1; //init to 1 for loop
        uint8_t buffer[BUFFER_SIZE + 1];
        ssize_t bytes_written = 0;
        int fd = open(message->filename, O_CREAT | O_RDWR | O_TRUNC | O_APPEND, 0666);
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

/*
    check_filename()

    This function will check to see if a filename is within the size limit (10)
    as well as adhering to the alphanumeric character rule.
*/
bool check_filename(struct httpObject *message){
    int length = strlen(message->filename);
    if (length != 10){return false;}
    for (int i = 0; i < length; i++ ){
        if ((isalpha(message->filename[i]) != 0 || isdigit(message->filename[i]) != 0)){continue;}
        else{return false;}
    }
    //printf("    Filename checked true\n");
    return true;
}

bool backup_helper(struct httpObject *message, ssize_t client_sockd){
    printf("    Backup Functionality signaled.\n");
    
    //make the new directory
    unsigned long int timestamp = time(NULL); 
    printf("    Timestamp: %lu\n", timestamp);
    std::string dirname = "backup-" + std::to_string(timestamp);
    const char* dir_name = dirname.c_str(); 
    struct stat st;
    if (stat(dir_name, &st) == -1) {
        mkdir(dir_name, 0700);
    }

    //find applicable files and copy them
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            strcpy(message->filename, dir->d_name);
            if (!strcmp(dir->d_name, "httpserver")){continue;}
            if(check_filename(message)){
                printf("        copying: %s\n", dir->d_name);
                //copy contents: http://cs.boisestate.edu/~amit/teaching/297/notes/files-and-processes-handout.pdf
                std::string fname = "./" + dirname + "/" + std::string(dir->d_name);
                const char* dest_f_name = fname.c_str(); 
                int in, out;
                char buf[BUFFER_SIZE];
                printf("          source: %s\n", dir->d_name);
                printf("          dest:   %s\n", dest_f_name);
                int source = open(dir->d_name, O_RDONLY);
                int dest   = open(dest_f_name, O_CREAT | O_RDWR | O_TRUNC | O_APPEND, 0666);
                while (1) {
                    in = read(source, buf, BUFFER_SIZE);
                    if (in <= 0) break;
                    out = write(dest, buf, in);
                    if (out <= 0) break;
                }
                close(source); 
                close(dest);
            }
        }
        closedir(d);
        backupflag = false;
        dprintf(client_sockd, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
    }
    else{
        dprintf(client_sockd, "HTTP/1.1 500 Internal Server Error\r\n\r\n");
        return false;
    }
    return true;
}

bool list_helper(struct httpObject *message, char* buflist){
    struct stat statbuf;
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    list.clear();
    message->content_length = 0;
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            lstat(dir->d_name,&statbuf);
            if(S_ISDIR(statbuf.st_mode)) { //if the file is a directory
                if(strcmp(".",dir->d_name) == 0 || strcmp("..",dir->d_name) == 0){continue;}
                char* start;
                if(strstr(dir->d_name, "backup-") != NULL) {
                    start = strstr(dir->d_name, "-");
                    start = start + sizeof(char);
                    //printf("        directory: %s\n", dir->d_name);
                    //printf("          timestamp: %s\n", start);
                    list.push_back(start);
                }
            }
        }
        //printf("        size: %li\n", list.size());
        for (long unsigned int i=0; i<list.size();i++){
            strcat(buflist, list.at(i));
            strcat(buflist, "\r\n");
            message->content_length += 12;
        }
        closedir(d);
        
    }
    return true;
}

bool recovery_helper(struct httpObject *message, ssize_t client_sockd){
    printf("    Recovery functionality signaled.\n");
    if(!strcmp(message->filename, "r")){ //if no specified, get most recent.
        printf("        No specified timestamp. getting the most recent\n");
        char buflist[BUFFER_SIZE];
        memset(buflist, 0, sizeof(buflist));
        list_helper(message, buflist);
        if(list.size() > 0){
            unsigned long int current = 0;
            unsigned long int max = 0;
            for(unsigned long int i = 0; i < list.size(); i++){
                char *ptr;
                current = strtoul(list.at(i), &ptr, 10);
                if (current > max){
                    max = current;
                }
            }
            std::string s = "r/" + std::to_string(max);
            const char* backupname = s.c_str(); 
            strcpy(message->filename, backupname);
            printf("        most recent backup: %s\n",backupname);
        }
        else{
            printf("        No backups exist. Error 404\n");
            send(client_sockd, &status_404, sizeof(status_404), 0);
            close(client_sockd);
            return false;
        }
    }
        char* start = strstr(message->filename, "r/");
        if (start != NULL){
            start = start + 2*(sizeof(char));
            char buflist[BUFFER_SIZE];
            memset(buflist, 0, sizeof(buflist));
            list_helper(message, buflist);
            bool exists = false;
            for(unsigned long int i = 0; i < list.size(); i++){
                if (!strcmp(list.at(i), start)){
                    exists = true;
                }
                if (exists) break;
            }
            if(!exists){
                dprintf(client_sockd, "HTTP/1.1 404 Not Found\r\n");
                close(client_sockd);
                return false;
            }
            std::string s = "./backup-" + std::string(start);
            const char* backupname = s.c_str(); 
            printf("        specified timestamp: %s\n", backupname);

            //TODO copy that shit from backup folder to actual folder
            //find applicable files and copy them
            DIR *d;
            struct dirent *dir;
            d = opendir(backupname);
            
            if (d) {
                //printf("opened\n");
                while ((dir = readdir(d)) != NULL) {
                    if (!strcmp(dir->d_name, "httpserver")){continue;}
                    if (!strcmp(dir->d_name, ".")){continue;} 
                    if (!strcmp(dir->d_name, "..")){continue;}
                    printf("        copying: %s\n", dir->d_name);
                    //copy contents: http://cs.boisestate.edu/~amit/teaching/297/notes/files-and-processes-handout.pdf
                    std::string destname = std::string(dir->d_name);
                    std::string sourcename = s + "/" + std::string(dir->d_name);
                    const char* dest_f_name = destname.c_str(); 
                    const char* source_f_name = sourcename.c_str(); 
                    int in, out;
                    char buf[BUFFER_SIZE];
                    printf("          source: %s\n", source_f_name);
                    printf("          dest:   %s\n", dest_f_name);
                    int source = open(source_f_name, O_RDONLY);
                    int dest   = open(dest_f_name, O_CREAT | O_RDWR | O_TRUNC | O_APPEND, 0666);
                    while (1) {
                        in = read(source, buf, BUFFER_SIZE);
                        if (in <= 0) break;
                        out = write(dest, buf, in);
                        if (out <= 0) break;
                    }
                    close(source); 
                    close(dest);
                }
                closedir(d);
                backupflag = false;
                dprintf(client_sockd, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
            }
        }
        else{
            dprintf(client_sockd, "HTTP/1.1 500 Internal Server Error\r\n");
            close(client_sockd);
            return false;
        }

    recoveryflag = false;
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
    if (!strcmp(message->filename, "r")){
        //recovery.
        //restore the most recent backup
        recoveryflag = true;
        return;
    }
    if (message->filename[0] == 'r' && message->filename[1] == '/'){
        //recovery.
        //restore the specified backup
        //message->timestamp = timestamp after /

        //TODO: implement
        recoveryflag = true;
        return;
    }
    if (!strcmp(message->filename, "b")){
        //backup
        //create a new backup with every file on the server except binaries and executables
        //each new backup is in a folder called "./backup-[timestamp]"
        //only stores files not folders
        backupflag = true;
        return;
    }
    if (!strcmp(message->filename, "l")){
        //list
        //return a list of timestamps of all available backups. 
        //do this by searching for folders named "./backup-[timestamp]"
        listflag = true;
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
    if(strcmp(message->method, "PUT") == 0){
        if (message->status_code != 400 && message->status_code != 404 && message->status_code != 403 && message->status_code != 500 ){
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
            if(recoveryflag){
                recovery_helper(message, client_sockd);
                recoveryflag = false;
                return;
            }
            else if(backupflag){
                backup_helper(message, client_sockd);
                backupflag = false;
                return;
            }
            else if(listflag){
                printf("    list functionality signaled.\n");
                char buflist[BUFFER_SIZE];
                memset(buflist, 0, sizeof(buflist));
                list_helper(message, buflist);
                listflag = false;
                dprintf(client_sockd, "HTTP/1.1 200 OK\r\nContent-Length: %zu\r\n\r\n", message->content_length);
                write(client_sockd,buflist,message->content_length);
                return;
            }
            
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
        printf("    Error 400: method: %s\n", message->method);
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
        strncpy(port, "80", sizeof(port));
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