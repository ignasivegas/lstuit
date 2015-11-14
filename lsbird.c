//
//  lsbird.c
//
//  LsTuit - Client
//
//  Created by Ignasi Vegas on 17/5/15.


#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <netdb.h>

#define CONFIG_FILENAME "config.dat"
#define USER_MAX_LENGTH 7
#define DATA_MAX_LENGTH 80
#define MAX_COMMAND_LENGTH 1024

typedef struct{
    char url[512];
    char port[512];
    char nick[512];
} LsTuitConnection;

typedef struct{
    char source[USER_MAX_LENGTH];
    char destination[USER_MAX_LENGTH];
    char type;
    int ack;
    char data[DATA_MAX_LENGTH];
}LsFrame;


int CONNECTED = 0;

int socketfd;
LsTuitConnection connection_data;

void Print(char disp[]){
    write(STDOUT_FILENO, disp, strlen(disp));
}

void Print_Prompt(){
    char display[100];
    sprintf(display, "%s>", connection_data.nick);
    write(STDOUT_FILENO, display, strlen(display));
}

void Print_help(){
    
    if (CONNECTED == 1) {
        Print("show users - return a list of connected users\n");
        Print("send <username> message - send a specific message to a user \n");
        Print("broadcast <message> - send a message to all the connected user \n");
        
    }else{
        Print("connect - Connect to the LsTuit server\n");

    }
        Print("exit - exit LsTuit\n");
    
}


int words_count(char sentence[]){
    
    int counted = 0;
    int save = 0;
    int flag = 0;
    
    char *it = sentence;
    
    while (*it++) {
        if (*it == ' ' || *it == '\n' || *it == '\0') {
            counted++;
        }else if(*it == '"'){
            if (flag == 0){
                save = counted;
                flag = 1;
            }else{
                counted = save;
            }
            
        }
    }
    
    
    
    return counted;
}


void Read_Line(int fd_in, char value[]){

    int i;
    
    i = 0;
    do{
        read(fd_in, &value[i], 1);
        i++;
    }while (value[i-1]!='\n');
    value[i-1]='\0';
}

void Read_Configuration(){
    
    int fd_in;
    
    if ((fd_in = open(CONFIG_FILENAME, O_RDONLY)) < 0){
        perror(CONFIG_FILENAME);
        exit(EXIT_FAILURE);
    }
    
    Read_Line(fd_in, connection_data.url);
    Read_Line(fd_in, connection_data.port);
    Read_Line(fd_in, connection_data.nick);
    if (strlen(connection_data.nick) > USER_MAX_LENGTH) {
        Print("Error. User nickname must be MAX 7 characters \n");
        close(fd_in);
        exit(EXIT_FAILURE);
    }
    
    
    close(fd_in);
    
}

void exit_server(){
    
    LsFrame frame;
    
    CONNECTED = 0;
    
    strcpy(frame.source, connection_data.nick);
    strcpy(frame.destination, "server");
    strcpy(frame.data, "DESCONNEXIO");
    frame.type = 'Q';
    
    write(socketfd, &frame, sizeof(LsFrame));
    
    close(socketfd);
    
}



void send_show_users(){
    
    LsFrame showusers;
    
    strcpy(showusers.source, connection_data.nick);
    strcpy(showusers.destination, "server");
    showusers.type = 'L';
    strcpy(showusers.data, "PETICIO LLISTA USUARIS");
    
    write(socketfd, &showusers, sizeof(LsFrame));
    
}

void receive_show_users(int n_users){
    
    int i;
    char display[100];
    LsFrame list;
    
    sprintf(display, "\n%d users online:", n_users);
    Print(display);
    
    for (i = 0; i < n_users; i++) {
        read(socketfd, &list, sizeof(LsFrame));
        sprintf(display, "\n -%s", list.data);
        Print(display);
    }
    Print("\n");
    Print_Prompt();
}


void exit_ok(){
    
    if (CONNECTED == 1){
        Print("\nDiconnecting from server...\n");
        exit_server();
    }
    
    Print("Bye\n");
    exit(EXIT_SUCCESS);
}


void show_error(LsFrame frame){
    char display[100];
    sprintf(display, "%s\n", frame.data);
    Print(display);
}

void show_message(LsFrame frame){
    char *display;
    char *long_message;
    char display_short[100];
    int n_frames, i, length;
    
    if (frame.ack > 0){
        n_frames = frame.ack;
        long_message = malloc(sizeof(char)*((n_frames+1)*DATA_MAX_LENGTH));
        if(long_message == NULL){
            Print("Memory error on show_message \n");
            exit_ok();
        }
        
        strcat(long_message, frame.data);
        
        for (i = 0; i < n_frames; i++) {
            read(socketfd, &frame, sizeof(LsFrame));
            
            strcat(long_message, frame.data);
            
        }
        
        length = (int)strlen(long_message);
        
        display = malloc(sizeof(char)*(length)+10);
        
        if (frame.type == 'Q')
            sprintf(display, "\nBROADCAST - %s: %s \n", frame.source, long_message);
        else
            sprintf(display, "\n%s: %s \n", frame.source, long_message);
        Print(display);
        Print_Prompt();
        free(long_message);
        
        
    }else{
        if (frame.type == 'Q')
            sprintf(display_short, "\nBROADCAST - %s: %s \n", frame.source, frame.data);
        else
            sprintf(display_short, "\n%s: %s \n", frame.source, frame.data);
        Print(display_short);
        Print_Prompt();
    }
   
}

void * listener(){
    ssize_t bytes;
    LsFrame frame;
    int n_users;
    char display[100];
    
    bytes = read(socketfd, &frame, sizeof(LsFrame));
    while (bytes > 0) {
        switch (frame.type) {
                
            case 'E':
                show_error(frame);
                Print_Prompt();
                break;
                
            case 'N':
                n_users = atoi(frame.data);
                receive_show_users(n_users);
                break;
                
            case 'O':
                sprintf(display, "%s \n", frame.data);
                Print(display);
                Print_Prompt();
                break;
            
            case 'Q':
            case 'R':
                show_message(frame);
                break;
                
            default:
                break;
        }
        
        
        bytes = read(socketfd, &frame, sizeof(LsFrame));
        
    }
    //if there's no connection and we haven't closed the connection, maybe the server is down...
    if (CONNECTED == 1){
        Print("Connection with server failed, maybe server is closed \n");
        exit(EXIT_FAILURE);
    }
    
    return NULL;
}


void connect_lsserver(){
    
    LsFrame initial;
    LsFrame response;
    int s;
    pthread_t id_listener;
    int r;
    struct addrinfo hints, *servinfo, *p;
    
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd < 0){
        Print("Error socket\n");
        exit(EXIT_FAILURE);
    }
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    if ((r = getaddrinfo(connection_data.url, connection_data.port, &hints, &servinfo)) != 0) {
        Print("Error getaddrinfo \n");
        exit(EXIT_FAILURE);
    }
   
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((socketfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        
        if (connect(socketfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(socketfd);
            perror("client: connect");
            continue;
        }
        
        break;
    }
    
    freeaddrinfo(servinfo);
    
    if (p == NULL) {
        Print("Connect failed, invalid server direction \n");
        exit(EXIT_FAILURE);
    }

    
    strcpy(initial.source, connection_data.nick);
    strcpy(initial.destination, "server");
    initial.type = 'C';
    strcpy(initial.data, "CONNEXIO");
    
    write(socketfd, &initial, sizeof(LsFrame));
    
    read(socketfd, &response, sizeof(LsFrame));
    
    if (response.type == 'O'){
        
        Print("You are connected to LsTuit, type help for more intructions \n");
        
        CONNECTED = 1;
        
        s = pthread_create(&id_listener, NULL, listener, NULL);
        if (s != 0) {
            Print("Error en pthread_create \n");
            exit (EXIT_FAILURE);
        }
        
    }else{
        
        show_error(response);
        close(socketfd);
    }

}

void clean_message(char message[]){
    
    int i;
    long length = strlen(message);
    char aux[length-2];
    
    if (message[0] == '"' && message[length-1] == '"'){
        for (i = 1; i < length-1; i++) {
            aux[i-1] = message[i];
        }
        
        memset(message,'\0',length);
        
        strcpy(message, aux);
        
        message[length-2] = '\0';
    }
    
    
}

void send_splitted_message(LsFrame frame, char message[], int message_length){
    
    int n_frames;
    char frame_message[DATA_MAX_LENGTH];
    
    n_frames = 1 + ((message_length - 1) / DATA_MAX_LENGTH);
    
    
    while (message_length > 0)
    {
        int len = (message_length>DATA_MAX_LENGTH) ? DATA_MAX_LENGTH : message_length;
        memset(frame_message,'\0', DATA_MAX_LENGTH);
        strncpy(frame_message, message, DATA_MAX_LENGTH);
        
        n_frames = n_frames-1;
        frame.ack = n_frames;
        strncpy(frame.data, frame_message, DATA_MAX_LENGTH);
        write(socketfd, &frame, sizeof(LsFrame));
        
        
        message_length -= len;
        message  += len;
        
    }
    
}

void send_message(char command[]){
    
    char *saveptr;
    char *dest, *message;
    LsFrame frame;
    int message_length;
    
   char *tmp = strchr(command, ' ');
    if(tmp != NULL)
        command = tmp + 1;
        dest = strtok_r(command, " ", &saveptr);
    
    message = strtok_r(NULL, "\0", &saveptr);
    
    clean_message(message);
    
    
    message_length = (int)strlen(message);
    
    if (strlen(dest) > USER_MAX_LENGTH){
        Print("Error, username not valid \n");
        
    }else if(message_length > DATA_MAX_LENGTH){
        
        
        strcpy(frame.source, connection_data.nick);
        strcpy(frame.destination, dest);
        frame.type = 'S';
        
        send_splitted_message(frame, message, message_length);
        
        
    }else{
        
        strcpy(frame.source, connection_data.nick);
        strcpy(frame.destination, dest);
        frame.type = 'S';
        frame.ack = 0;
        strcpy(frame.data, message);
        
        write(socketfd, &frame, sizeof(LsFrame));
    }
    
    
}

void send_broadcast(char command[]){
    
    LsFrame frame;
    int message_length;

    char *tmp = strchr(command, ' ');
    if(tmp != NULL)
        command = tmp + 1;
    
    clean_message(command);
    
    message_length = (int)strlen(command);
    
    if(message_length > DATA_MAX_LENGTH){
        
        strcpy(frame.source, connection_data.nick);
        strcpy(frame.destination, "server");
        frame.type = 'B';
        
        
        send_splitted_message(frame, command, message_length);

        
        
    }else{
    
        strcpy(frame.source, connection_data.nick);
        strcpy(frame.destination, "server");
        frame.type = 'B';
        frame.ack = 0;
        strcpy(frame.data, command);
        
        write(socketfd, &frame, sizeof(LsFrame));
    }
    
    
}

int first_word(char sentence[], char word[]){
    char *pch;
    int return_value = 0;
    int length;
    char *buffer;
    
    length = (int)strlen(sentence);
   
    buffer = malloc(sizeof(char)*length+2);
    if (buffer == NULL) {
        Print("Memory error on first_word \n");
        exit_ok();
    }
    
    strcpy(buffer, sentence);
    
    pch = strtok (buffer, " ");
    if (strcmp(pch, word) == 0){
        return_value = 1;
    }
    
    free(buffer);
    
    return return_value;
}

int check_send_command(char command[]){
    int return_value;
    
    return_value = 0;

    if (words_count(command) == 3 && (first_word(command, "send") == 1 || first_word(command, "SEND") == 1)){
        return_value = 1;
    }
    
    return return_value;
}

int check_broadcast_command(char command[]){
    
    int return_value;
    return_value = 0;
    
    if (words_count(command) == 2 && (first_word(command, "broadcast") == 1 || first_word(command, "BROADCAST") == 1)){
        return_value = 1;
    }
    
    return return_value;
    
}

void check_option(char buffer[]){
    
    
    if (strcmp(buffer, "show users") == 0 || strcmp(buffer, "SHOW USERS") == 0){
        if (CONNECTED == 1)
            send_show_users();
        else
            Print("You must be connected \n");
        
    }else if(strcmp(buffer, "connect") == 0 || strcmp(buffer, "CONNECT") == 0){
        if (CONNECTED == 0)
            connect_lsserver();
        else
            Print("Already connected\n");
    }else if(strcmp(buffer, "exit") == 0 || strcmp(buffer, "EXIT") == 0){
        if (CONNECTED == 1)
            exit_server();
    }
    else if(strcmp(buffer, "help") == 0 || strcmp(buffer, "HELP") == 0){
        Print_help();
    }
    else if(buffer[0] == '\n'){
        //Nothing
        
    }else if (check_send_command(buffer) == 1){
        if (CONNECTED == 0)
            Print("You must be connected \n");
        else
            send_message(buffer);
        
    }else if(check_broadcast_command(buffer) == 1){
        if (CONNECTED == 0)
            Print("You must be connected \n");
        else
            send_broadcast(buffer);
    }else{
        Print("Wrong command \n");
    }
    
    
    
}



int main() {

    char buffer[MAX_COMMAND_LENGTH];
    signal(SIGINT, exit_ok);
    
    Read_Configuration();
    
    Print("Welcome to LsTuit, type 'help' for instructions\n");
    
    //Terminal
    while (strcmp(buffer, "exit") != 0 && strcmp(buffer, "EXIT") != 0) {
        
        memset(buffer,'\0',MAX_COMMAND_LENGTH);
        Print_Prompt();
        read(STDIN_FILENO, buffer, MAX_COMMAND_LENGTH);
        
        if (buffer[0] != '\n')
            buffer[strlen(buffer)-1] = '\0';
        
        check_option(buffer);
    }
    

    
    Print("Bye! \n");
    
    
    return 0;
    
}
