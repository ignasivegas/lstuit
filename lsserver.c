//
//  lsserver.c
//  LsTuit - Server
//
//  Created by Ignasi Vegas on 30/5/15.
//

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>

#define MAX_USERS 10
#define DATA_MAX_LENGTH 80
#define USER_MAX_LENGTH 7

typedef struct{
    char source[USER_MAX_LENGTH];
    char destination[USER_MAX_LENGTH];
    char type;
    int ack; //number of pending frames
    char data[DATA_MAX_LENGTH];
}LsFrame;


typedef struct{
    char nick[USER_MAX_LENGTH];
    int fd;
}User;

typedef struct{
    User users[MAX_USERS];
    int current_users;
}UsersIndex;

//Global users index, to be shared by all the processes.
UsersIndex users_index;

//semaphore. Mutual exclusion for  users_index
pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;

//Socket listener. We want to close it, so it should be global var.
int sockfd;


void Print(char disp[]){
    write(STDOUT_FILENO, disp, strlen(disp));
}

void Print_n_users(){
    char display[100];
    
    sprintf(display, "There are %d users connected \n", users_index.current_users);
    Print(display);
}

void send_error(int socket, char error[], char dest[]){
    //Send error frame
    
    LsFrame response;
    
    strcpy(response.source, "server");
    strcpy(response.destination, dest);
    response.type = 'E';
    strcpy(response.data, error);
    
    write(socket, &response, sizeof(LsFrame));
    
}

int init_server(int port){
    //Server starts
    int sockfd;
    struct sockaddr_in serv_addr;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sockfd < 0)
    {
        perror("ERROR opening socket");
        exit(1);
    }
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);
    
    
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("ERROR on binding");
        exit(1);
    }
    
    listen(sockfd,5);
    
    return sockfd;
    
}

int validate_frame(LsFrame frame, char type){
    //Receive a frame and validates
    switch (type) {
            
        case 'C':
            
            if (!frame.source || strcmp(frame.destination, "server") != 0 || frame.type != 'C' || strcmp(frame.data, "CONNEXIO") != 0){
                
                return -1;
            }
            
            break;
            
        case 'L':
            
            if (!frame.source || strcmp(frame.destination, "server") != 0 || frame.type != 'L' || strcmp(frame.data, "PETICIO LLISTA USUARIS") != 0){
                
                return -1;
            }
            
            
            break;
            
        case 'S':
            if (!frame.source || !frame.destination || frame.type != 'S' || !frame.data ){
                
                return -1;
            }
            break;
        
        case 'B':
            if (!frame.source || strcmp(frame.destination, "server") != 0 || frame.type != 'B' || !frame.data ){
                
                return -1;
            }
            break;
            
        default:
            break;
    }
    
    return 0;
}



int confirm_connection(int newsockfd){
    //Waits for a valid connection
    
    LsFrame response, initial;
    User user;
    char display[50];
    int n, return_value;
    return_value = 0;
    
    read(newsockfd, &initial, sizeof(LsFrame));
    
    if (validate_frame(initial, 'C') == 0){
        
        
        pthread_mutex_lock(&users_mutex);
        if (users_index.current_users < MAX_USERS){
            
            //n position -  users_index
            n = users_index.current_users;
            users_index.current_users = users_index.current_users+1;
            
            strcpy(user.nick, initial.source);
            user.fd = newsockfd;
            
            users_index.users[n] = user;
            
            
            strcpy(response.source, "server");
            strcpy(response.destination, initial.source);
            response.type = 'O';
            strcpy(response.data, "CONNEXIO OK");
            write(newsockfd, &response, sizeof(LsFrame));
            
            sprintf(display, "%s connected\n", initial.source);
            write(STDOUT_FILENO, display, strlen(display));
            Print_n_users();
            
        }else{

            send_error(newsockfd, "ERROR - SERVER IS FULL", initial.source);
            
            return_value =  -1;
        }
        
        pthread_mutex_unlock(&users_mutex);
        
        
    }else{
        
        
        send_error(newsockfd, "ERROR - INVALID FRAME", initial.source);
        
        
        return_value = -1;
        
    }
    
    return return_value;
    
}

void disconnect_user(int socketfd){
    //Disconnect a user
    
    int i, n;
    n = -1;
    
    //Start mutex, protect the users_index
    pthread_mutex_lock(&users_mutex);
    
    //Search for the user that i want to delete
    for (i = 0; i < users_index.current_users; i++) {
        if (users_index.users[i].fd == socketfd){
            n = i;
            break;
        }
    }
    
    //delete the user and order de array again.
    for ( i = n; i < users_index.current_users - 1 ; i++ )
        users_index.users[i] = users_index.users[i+1];
    
    users_index.current_users = users_index.current_users-1;
    
    //Close user socket and stop mutex.
    close(socketfd);
    
    pthread_mutex_unlock(&users_mutex);
    

}

void exit_ok(){
    //Free resources
    
    int i;
    
    pthread_mutex_lock(&users_mutex);
    for (i = 0; i < users_index.current_users; i++) {
        close(users_index.users[i].fd);
    }
    pthread_mutex_unlock(&users_mutex);

    close(sockfd);
    
    pthread_mutex_destroy(&users_mutex);
    
    exit(EXIT_SUCCESS);
}



int get_user_socket(char username[]){
    //receive an username and return the socket.
    
    int i;
    int socket;
    socket = -1;
    
    for (i = 0; i < users_index.current_users; i++) {
        if (strcmp(users_index.users[i].nick, username) == 0){
            socket = users_index.users[i].fd;
        }
    }
    
    return socket;
}

void send_message(LsFrame source, int source_socket){
    //Send a message
    
    int dest_socket;
    LsFrame dest, source_ok;
    int n_frames;
    int i;
    
    dest_socket = get_user_socket(source.destination);
    if (dest_socket == -1){
        
        send_error(source_socket, "Error, destination user doesn't exist", source.source);
        
    }else{
        
        strcpy(dest.source, source.source);
        strcpy(dest.destination, source.destination);
        dest.type = 'R';
        dest.ack = source.ack;
        strncpy(dest.data, source.data, DATA_MAX_LENGTH);
        
        n_frames = source.ack;
        
        if (n_frames > 0){
            
            write(dest_socket, &dest, sizeof(LsFrame));
            for (i = 0; i < n_frames; i++) {
                read(source_socket, &dest, sizeof(LsFrame));
                
                dest.type = 'R';
                write(dest_socket, &dest, sizeof(LsFrame));
            }
            
        }else{
            write(dest_socket, &dest, sizeof(LsFrame));
        }
        
        
        strcpy(source_ok.source, "server");
        strcpy(source_ok.destination, source.source);
        source_ok.type = 'O';
        strcpy(source_ok.data, "Message sent succesfully");
        
        write(source_socket, &source_ok, sizeof(LsFrame));
        
    }
    
    
    
}

void send_broadcast(LsFrame frame, int source_socket){
    //Send a broadcast
    
    LsFrame broad;
    int i, j, n_frames;
    
    strcpy(broad.source,frame.source);
    broad.type = 'Q';
    broad.ack = frame.ack;
    strncpy(broad.data, frame.data, DATA_MAX_LENGTH);
    
    n_frames = frame.ack;
    
    if (n_frames > 0) {
        for (i = 0; i < users_index.current_users; i++)
            write(users_index.users[i].fd, &broad, sizeof(LsFrame));
        
        for (j = 0; j < n_frames; j++) {
            read(source_socket, &broad, sizeof(LsFrame));
            strcpy(broad.destination, users_index.users[i].nick);
            broad.type = 'Q';
            for (i = 0; i < users_index.current_users; i++) {
                write(users_index.users[i].fd, &broad, sizeof(LsFrame));
            }
        }
        
    }else{
        for (i = 0; i < users_index.current_users; i++){
            write(users_index.users[i].fd, &broad, sizeof(LsFrame));
        }
        
    }
    
}

void send_show_users(LsFrame source, int socketdest){
    
    LsFrame response;
    char data[50];
    int i;
    
    sprintf(data, "%d", users_index.current_users);
    
    strcpy(response.source, "server");
    strcpy(response.destination, source.source);
    response.type = 'N';
    strcpy(response.data, data);
    
    write(socketdest, &response, sizeof(LsFrame));
    
    response.type = 'U';
    for (i = 0; i < users_index.current_users; i++) {
        strcpy(response.data, users_index.users[i].nick);
        write(socketdest, &response, sizeof(LsFrame));
    }
    
}

void * client_thread(int *arg){
    //Client process
    
    int newsockfd;
    int bytes;
    LsFrame frame;
    char display[100];
    
    newsockfd = *arg;
    
    
    if (confirm_connection(newsockfd) == 0){
        
        
        bytes = read(newsockfd, &frame, sizeof(LsFrame));
        if (bytes == -1){
            //Disconnect
            disconnect_user(newsockfd);
        }
        
        while (frame.type != 'Q') {
            
            switch (frame.type) {
                    
                case 'L':
                    if (validate_frame(frame, 'L') == 0){
                        
                        send_show_users(frame, newsockfd);
                        
                    }else{
                        
                        send_error(newsockfd, "ERROR - INVALID FRAME", frame.source);
                    }
                    
                    break;
                    
                case 'S':
                    
                    if (validate_frame(frame, 'S') == 0){
                        
                        send_message(frame, newsockfd);
                        
                    }else{
                        
                        send_error(newsockfd, "ERROR - INVALID FRAME", frame.source);
                    }
                    
                    break;
                    
                case 'B':
                    
                    if (validate_frame(frame, 'B') == 0) {
                        
                        send_broadcast(frame, newsockfd);
                        
                    }else{
                        
                        send_error(newsockfd, "ERROR - INVALID FRAME", frame.source);
                    }
                    
                    break;
                    
                default:
                    
                    send_error(newsockfd, "ERROR - INVALID FRAME", frame.source);
                    
                    break;
            }
            
            
            bytes = read(newsockfd, &frame, sizeof(LsFrame));
            if (bytes == -1){
                //Disconnect
                disconnect_user(newsockfd);
            }
        }
        
        //Disconnect, exit frame received.
        disconnect_user(newsockfd);
        
        sprintf(display, "%s disconnected \n", frame.source);
        write(STDOUT_FILENO, display, strlen(display));
        Print_n_users();
    
    }
    

    return NULL;
}

int main(int argc, const char * argv[]){
    
    int newsockfd, s, port;
    char display[100];
    struct sockaddr_in cli_addr;
    socklen_t c_len = sizeof(cli_addr);
    pthread_t id;
    
    if (argc != 2) {
        Print("Error, parameter port is needed \n");
        exit(EXIT_FAILURE);
    }
    
    port = atoi(argv[1]);
    
    signal(SIGINT, exit_ok);
    
    //Start server
    sockfd = init_server(port);
    
    
    sprintf(display, "LsTuit server is running, listening on port %d \n", port);
    Print(display);
    
    //Start receiving
    while (1)
    {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &c_len);
        if (newsockfd < 0){
            perror("ERROR on accept");
            exit(EXIT_SUCCESS);
        }
        

        s = pthread_create(&id,NULL, (void *)client_thread,(void *)&newsockfd);
        if (s != 0) {
            Print("Error en pthread_create \n");
            exit (EXIT_FAILURE);
        }
    }
    
    
    return 0;
    
}
