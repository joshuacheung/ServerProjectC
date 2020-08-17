#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>
#include <getopt.h>

#define PORT 8080
#define DEFAULT_NUM_THREADS 4
#define true 1
#define false 0

// Thread variables 
pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t pwrite_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t thread_sem;
size_t thread_in_use = 0;


// client response functions
char *successString = (char *)" 200 OK\r\n";
char *notFoundString = (char *)" 404 Not found\r\n";
char *forbiddenString = (char *)" 403 Forbidden\r\n";
char *badRequestString = (char *)" 400 Bad request\r\n";
char *ending = "\r\n";
char *validChars = (char *)"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";

// log file variables
char *success_length = (char *)" length ";
char *log_bad_request = (char *)" HTTP/1.1 --- response 400\n";
char *log_forbidden = (char *)" HTTP/1.1 --- response 403\n";
char *log_not_found = (char *)" HTTP/1.1 --- response 404\n";
char *end_log = (char *)"========\n";
char *log_new_line = (char *)"\n";
char *log_space = (char *)" ";
bool logFileFlag = false;
size_t numLogEntries = 0;
size_t numLogErrors = 0;
int offset = 0;


// request Functions
void *getRequest(void *clientResponse);
void *putRequest(void *clientResponse);
void *headRequest(void *clientResponse);
void *otherRequest(void *clientResponse);


typedef struct thread_arg_t
{
    char *headerRecv;
    int new_socket;
    char *logFileName;

} ThreadArg;

int main(int argc, char *argv[])
{
  /* create sockaddr_in with server information */
  //char* port = 8080;/* server operating port */

  size_t valread;
  size_t num_threads = 4;
  char logFileName[125];
  char *header_buffer = (char *)malloc(sizeof(char) * 4096);
  const char *getReq = "GET";
  const char *putReq = "PUT";
  const char *headReq = "HEAD";

  int opt;
  char *port_num;

  // put ':' in the starting of the
  // string so that program can
  //distinguish between '?' and ':'
  while ((opt = getopt(argc, argv, "N:l:")) != -1)
  {
    switch (opt)
    {
    case 'N':
      printf("N flag raised\n");
      if (optarg == NULL)
      {
        num_threads = DEFAULT_NUM_THREADS;
      }
      else
      {
        num_threads = atoi(optarg);
      }

      break;
    case 'l':
      if (optarg == NULL)
      {
        printf("no log\n");
      }
      else
      {
        /* code */
        strcpy(logFileName, optarg);
        int l_fd = open(logFileName, O_CREAT | O_TRUNC, 0666);
        if(l_fd > 0)
        {
            printf("log already exists\n");
        }
        logFileFlag = true;
        break;
      }
    }
  }
  sem_init(&thread_sem, 0, num_threads);
 

  port_num = argv[optind];
//   printf("number of threads: %d\n", num_threads);
  printf("port num: %s\n", port_num);
  printf("log file name: %s\n", logFileName);
  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof server_addr);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(atoi(port_num));
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  socklen_t ai_addrlen = sizeof server_addr;

  /* create server socket */
  int server_sockd = socket(AF_INET, SOCK_STREAM, 0);
  /* configure server socket */
  int enable = 1;
  /* this allows you to avoid 'Bind: Address Already in Use' error */
  int ret = setsockopt(server_sockd, SOL_SOCKET, SO_REUSEADDR,
                       &enable, sizeof(enable));
  if (ret < 0)
  {
      printf("ret error\n");
  }
  /* bind server address to socket that is open */
  ret = bind(server_sockd, (struct sockaddr *)&server_addr, ai_addrlen);
  /* listen for incoming connctions */
  ret = listen(server_sockd, SOMAXCONN); /* 5 should be enough, if not use SOMAXCONN */
  /* connecting with a client */
  struct sockaddr client_addr;
  socklen_t client_addrlen = sizeof(client_addr);

  pthread_t thread_pool[num_threads];

  while (true)
  {
    size_t valid = -1;
    size_t client_sockd = accept(server_sockd, &client_addr, &client_addrlen);
    if (client_sockd == valid)
    {
      perror("client_sockd");
      exit(EXIT_FAILURE);
    }
    /* remember errors happen */
    //initial test of server response

    valread = read(client_sockd, header_buffer, 4096);
    if (valread == valid)
    {
      perror("read header");
      exit(EXIT_FAILURE);
    }

    //Header_Parser
    char *requestType = (char *)malloc(sizeof(char) * 4);

    sscanf(header_buffer, "%s", requestType);
    printf("headeR: %s\n", header_buffer);


    ThreadArg func_arg;
    func_arg.headerRecv = header_buffer;
    func_arg.new_socket = client_sockd;
    func_arg.logFileName = logFileName;


    // GET REQUEST
    if (strcmp(requestType, getReq) == 0)
    {
      // wait only if all threads are busy
      sem_wait(&thread_sem);
      // as long as 
      for (size_t j = 0; j < num_threads; j++)
      {
        pthread_mutex_lock(&thread_mutex);
        thread_in_use++;
        pthread_mutex_unlock(&thread_mutex);
        int thread_created = pthread_create(&thread_pool[j], NULL, getRequest, &func_arg);
        if (thread_created != -1)
        {
          break;
        }
      }
    }

    // PUT REQUEST
    else if (strcmp(requestType, putReq) == 0)
    {
      sem_wait(&thread_sem);
      for (size_t j = 0; j < num_threads; j++)
      {
        pthread_mutex_lock(&thread_mutex);
        thread_in_use++;
        pthread_mutex_unlock(&thread_mutex);
        int thread_created = pthread_create(&thread_pool[j], NULL, putRequest, &func_arg);
        if (thread_created != -1)
        {
          break;
        }
      }
    }

    // HEAD REQUEST
    else if (strcmp(requestType, headReq) == 0)
    {
      sem_wait(&thread_sem);
      for (size_t j = 0; j < num_threads; j++)
      {
        pthread_mutex_lock(&thread_mutex);
        thread_in_use++;
        pthread_mutex_unlock(&thread_mutex);
        int thread_created = pthread_create(&thread_pool[j], NULL, headRequest, &func_arg);
        if (thread_created != -1)
        {
          break;
        }
      }
    }
    else
    {
      sem_wait(&thread_sem);
      for (size_t j = 0; j < num_threads; j++)
      {
        pthread_mutex_lock(&thread_mutex);
        thread_in_use++;
        pthread_mutex_unlock(&thread_mutex);
        int thread_created = pthread_create(&thread_pool[j], NULL, otherRequest, &func_arg);
        if (thread_created != -1)
        {
          break;
        }
      }
    }

    free(requestType);
    
    // get rid of thread once we are done using it
    for (size_t j = 0; j < thread_in_use; j++)
    {
      pthread_join(thread_pool[j], NULL);
    }
  }
  free(header_buffer);

  
  return 0;
}


//***********************************************************************/
// otherRequest - args - client response struct 
//              - returns - void
// Will return 400 bad request back to client
//***********************************************************************/
void *otherRequest(void *clientResponse)
{
    ThreadArg *clientStruct = (ThreadArg *)clientResponse;
    char *fileBuffer = clientStruct->headerRecv;
    int new_socket = clientStruct->new_socket;
    char *log_name = clientStruct->logFileName;
    int p_offset;
    char *failString = (char *)"FAIL: ";
    char *serverTypeString = (char *)"HTTP/1.1 --- response 400\n";
    char *httpServerString = (char *)"HTTP/1.1";
    char *space = (char *)" ";
    char request[10] = {0};
    char textFileName[500] = {0};

    // printf("header: %s\n", fileBuffer);

    sscanf(fileBuffer, "%s", request);
    sscanf(fileBuffer, "%*s %s", textFileName);
    // printf("request: %s\n", request);
    // printf("textFileName: %s\n", textFileName);
    if(logFileFlag)
    {
        pthread_mutex_lock(&pwrite_mutex);
        ++numLogEntries;
        ++numLogErrors;
        p_offset = offset;
        offset = offset + strlen(failString) + +strlen(request) + strlen(space) + strlen(textFileName) + strlen(space) + strlen(serverTypeString) + strlen(end_log);
        pthread_mutex_unlock(&pwrite_mutex);

        int l_fd = open(log_name, O_CREAT | O_WRONLY, 0666);
        pwrite(l_fd, failString, strlen(failString), p_offset);
        p_offset += strlen(failString);
        pwrite(l_fd, request, strlen(request), p_offset);
        p_offset += strlen(request);
        pwrite(l_fd, space, strlen(space), p_offset);
        p_offset += strlen(space);
        pwrite(l_fd, textFileName, strlen(textFileName), p_offset);
        p_offset += strlen(textFileName);
        pwrite(l_fd, space, strlen(space), p_offset);
        p_offset += strlen(space);
        pwrite(l_fd, serverTypeString, strlen(serverTypeString), p_offset);
        p_offset += strlen(serverTypeString);
        pwrite(l_fd, end_log, strlen(end_log), p_offset);
        p_offset += strlen(end_log);
        close(l_fd);
    }
    
printf("bad1\n");
    send(new_socket, httpServerString, strlen(httpServerString), 0);
    send(new_socket, badRequestString, strlen(badRequestString), 0);
    close(new_socket);
    return 0;

}

//***********************************************************************/
// getRequest - args - client response struct 
//            - returns - void
// Will get file if exists in server, and send servertype, error/success codes,
// contents back to the client, and the content length
//***********************************************************************/
void *getRequest(void *clientResponse)
{
  ThreadArg *argp = (ThreadArg *)clientResponse;
  char *header_buffer = argp->headerRecv;
  int new_socket = argp->new_socket;
  char *log_name = argp->logFileName;

  int file_name_length = 0;
  int p_offset = 0;
//   size_t log_line_length = 20;
  char file_size_arr[750];
  char byte_padding[9];
  int byte = 0;

  char *get_content_length = (char *)"Content-Length: ";
  char *fail_get = (char *)"FAIL: GET ";
  char *correct_HTTP = (char *)"HTTP/1.1";
  char *success_get = (char *)"GET ";
  char *slashN = (char *)"\n";
  
  bool valid_file_name = true;

  char requestType[4] = {0};
  char textFileName[500] = {0};
  char textFileNameWithoutSlash[500] = {0};
  char serverType[20] = {0};

  printf("headerbuffer: %s\n", header_buffer);
  
  
  sprintf(byte_padding, "%08d", byte);
  sscanf(header_buffer, "%s", requestType);
  sscanf(header_buffer, "%*s %s", textFileName);
  sscanf(header_buffer, "%*s %*s %s", serverType);
  printf("textname: %s\n", textFileName);
  //remove the initial slash for the file name
  int j = 0;
  for (size_t i = 1; i < sizeof(textFileName); ++i)
  {
    if (textFileName[i] != 0)
    {
      textFileNameWithoutSlash[j] = textFileName[i];
      file_name_length++;
    }
    j++;
  }
  

  char *namingConvention = textFileNameWithoutSlash;

  // adapted from source: https://stackoverflow.com/questions/6605282/how-can-i-check-if-a-string-has-special-characters-in-c-effectively
    // checks for special characters excluding '-' and '_'
  if (namingConvention[strspn(namingConvention, validChars)] != 0)
  {
    valid_file_name = false;
  }


  // if the length of the textfile is not 27 chars
  if (file_name_length > 27 || !valid_file_name || strcmp(serverType, "HTTP/1.1") != 0)
  {
    printf("file name length: %d\n", file_name_length);
    printf("valid: %d\n", valid_file_name);

    //lock here
    if (logFileFlag)
    {
      pthread_mutex_lock(&pwrite_mutex);
      p_offset = offset;
      offset = offset + strlen(fail_get) + strlen(textFileName) + strlen(log_bad_request) + strlen(end_log);
      ++numLogEntries;
      ++numLogErrors;
      pthread_mutex_unlock(&pwrite_mutex);

      int l_fd = open(log_name, O_WRONLY, 0666);
      pwrite(l_fd, fail_get, strlen(fail_get), p_offset);
      p_offset += strlen(fail_get);
      pwrite(l_fd, textFileName, strlen(textFileName), p_offset);
      p_offset += strlen(textFileName);
      pwrite(l_fd, log_bad_request, strlen(log_bad_request), p_offset);
      p_offset += strlen(log_bad_request);
      pwrite(l_fd, end_log, strlen(end_log), p_offset);
      p_offset += strlen(end_log);
      close(l_fd);
    }
printf("bad2\n");
    send(new_socket, correct_HTTP, strlen(correct_HTTP), 0);
    send(new_socket, badRequestString, strlen(badRequestString), 0);
  }
  else
  {
    if (strcmp(textFileNameWithoutSlash, "healthcheck") == 0)
    {
      if(!logFileFlag)
      {
        send(new_socket, serverType, strlen(serverType), 0);
        send(new_socket, notFoundString, strlen(notFoundString), 0);
      }
      else
      {
        char numEntries[10];
        sprintf(numEntries, "%zu", numLogEntries);
        char numErrors[10];
        sprintf(numErrors, "%zu", numLogErrors);
        printf("numLogEntries: %s\n", numEntries);
        printf("numLogErrors: %s\n", numErrors);

        char combinedEntries[10];
        strcat(combinedEntries, numErrors);
        strcat(combinedEntries, slashN);
        strcat(combinedEntries, numEntries);

        size_t combinedEntriesLength = strlen(combinedEntries);
        char charCombinedEntriesLength[10];
        sprintf(charCombinedEntriesLength, "%zu", combinedEntriesLength);
        

        send(new_socket, serverType, strlen(serverType), 0);
        send(new_socket, successString, strlen(successString), 0);
        send(new_socket, get_content_length, strlen(get_content_length), 0);
        send(new_socket, charCombinedEntriesLength, strlen(charCombinedEntriesLength), 0);
        send(new_socket, ending, strlen(ending), 0);
        send(new_socket, ending, strlen(ending), 0);

        send(new_socket, combinedEntries, strlen(combinedEntries), 0);
      }
    

    }
    else
    {
        struct stat file_type;
        stat(textFileNameWithoutSlash, &file_type);
        size_t file_size = file_type.st_size;

        sprintf(file_size_arr, "%zu", file_size);
        // int error = lstat(textFileNameWithoutSlash, &file_type);
        int s_file = open(textFileNameWithoutSlash, O_RDONLY);
        printf("s_file fd: %d\n", s_file);
        // int permission = file_type.st_mode;
        
        if (s_file == -1)
        {
        //if it is denied access, one of the errors in open man page
        if (errno == EACCES)
        {
            // char *response = (char *)"HTTP/1.1 403 Forbidden\r\n";
            /* Write to Log file here */
            printf("GET not found\n");
            //lock here
            if (logFileFlag)
            {
                pthread_mutex_lock(&pwrite_mutex);
                p_offset = offset;
                offset = offset + strlen(fail_get) + strlen(textFileName) + strlen(log_forbidden) + strlen(end_log);
                ++numLogEntries;
                ++numLogErrors;
                pthread_mutex_unlock(&pwrite_mutex);

                printf("logging file\n");
                int l_fd = open(log_name, O_WRONLY, 0666);
                pwrite(l_fd, fail_get, strlen(fail_get), p_offset);
                p_offset += strlen(fail_get);
                pwrite(l_fd, textFileName, strlen(textFileName), p_offset);
                p_offset += strlen(textFileName);
                pwrite(l_fd, log_forbidden, strlen(log_forbidden), p_offset);
                p_offset += strlen(log_forbidden);
                pwrite(l_fd, end_log, strlen(end_log), p_offset);
                p_offset += strlen(end_log);
                close(l_fd);
            }

            //send forbidden to socket
            printf("sending responses\n");
            send(new_socket, serverType, strlen(serverType), 0);
            printf("sent client source: %s\n", serverType);
            send(new_socket, forbiddenString, strlen(forbiddenString), 0);
            printf("sent forbidden: %s\n", forbiddenString);
        }
        //other wise it doesn't exist
        else
        {

            if (logFileFlag)
            {
                pthread_mutex_lock(&pwrite_mutex);
                p_offset = offset;
                offset = offset + strlen(fail_get) + strlen(textFileName) + strlen(log_not_found) + strlen(end_log);
                ++numLogEntries;
                ++numLogErrors;
                pthread_mutex_unlock(&pwrite_mutex);

                int l_fd = open(log_name, O_WRONLY, 0666);
                pwrite(l_fd, fail_get, strlen(fail_get), p_offset);
                p_offset += strlen(fail_get);
                pwrite(l_fd, textFileName, strlen(textFileName), p_offset);
                p_offset += strlen(textFileName);
                pwrite(l_fd, log_not_found, strlen(log_not_found), p_offset);
                p_offset += strlen(log_not_found);
                pwrite(l_fd, end_log, strlen(end_log), p_offset);
                p_offset += strlen(end_log);
                close(l_fd);
            }

            send(new_socket, serverType, strlen(serverType), 0);
            send(new_socket, notFoundString, strlen(notFoundString), 0);
        }
        }
        else
        {

        char *buffer = (char *)calloc(32768, sizeof(char));

        send(new_socket, serverType, strlen(serverType), 0);
        send(new_socket, successString, strlen(successString), 0);
        send(new_socket, get_content_length, strlen(get_content_length), 0);
        send(new_socket, file_size_arr, strlen(file_size_arr), 0);
        send(new_socket, ending, strlen(ending), 0);
        send(new_socket, ending, strlen(ending), 0);
        printf("GET response sent\n");

        while (true)
        {
            int size_read = read(s_file, buffer, sizeof(buffer));
            if (size_read <= 0)
            {
                break;
            }
            else
            {
            //size += size_read;
                send(new_socket, buffer, size_read, 0);
                free(buffer);
                buffer = (char *)calloc(32768, sizeof(char));
            }
        }
        close(s_file);

        if (logFileFlag)
        {
            char *log_buffer = (char *)calloc(32768, sizeof(char));
            char *hex_buffer = (char *)calloc(40, sizeof(char));
            size_t log_written_size = 0;
            int l_fd = open(log_name, O_WRONLY, 0644);
            //lock here
            pthread_mutex_lock(&pwrite_mutex);
            size_t hex_count = 3 * file_size;
            size_t newline_count = file_size % 20;
            size_t pad_count = 8 * newline_count;
            p_offset = offset;
            offset += strlen(success_get) + strlen(textFileName) + strlen(success_length) + strlen(log_new_line) + hex_count + newline_count + pad_count + strlen(log_new_line) + strlen(end_log);
            ++numLogEntries;
            pthread_mutex_unlock(&pwrite_mutex);

            pwrite(l_fd, success_get, strlen(success_get), p_offset);
            p_offset += strlen(success_get);
            pwrite(l_fd, textFileName, strlen(textFileName), p_offset);
            p_offset += strlen(textFileName);
            pwrite(l_fd, success_length, strlen(success_length), p_offset);
            p_offset += strlen(success_length);
            pwrite(l_fd, file_size_arr, strlen(file_size_arr), p_offset);
            p_offset += strlen(file_size_arr);
            pwrite(l_fd, log_new_line, strlen(log_new_line), p_offset);
            p_offset += strlen(log_new_line);

            int fd = open(textFileNameWithoutSlash, O_RDONLY);
            while (true)
            {
            size_t size_read = read(fd, log_buffer, sizeof(log_buffer));
            if (size_read <= 0)
            {
                break;
            }
            else
            {
                for (size_t i = 0; i < size_read; ++i)
                {

                if (log_written_size % 20 == 0)
                {
                    if (log_written_size != 0)
                    {
                        pwrite(l_fd, log_new_line, strlen(log_new_line), p_offset);
                        p_offset += strlen(log_new_line);
                    }
                    pwrite(l_fd, byte_padding, strlen(byte_padding), p_offset);
                    p_offset += strlen(byte_padding);
                    pwrite(l_fd, log_space, strlen(log_space), p_offset);
                    p_offset += strlen(log_space);
                    byte += 20;
                    sprintf(byte_padding, "%08d", byte);
                }
                sprintf(hex_buffer, " %02x", log_buffer[i]);
                pwrite(l_fd, hex_buffer, strlen(hex_buffer), p_offset);
                p_offset += strlen(hex_buffer);
                free(hex_buffer);
                hex_buffer = (char *)calloc(40, sizeof(char));
                log_written_size += 1;
                }
                free(log_buffer);
                log_buffer = (char *)calloc(32768, sizeof(char));
            }
            }
            free(log_buffer);
            if (file_size > 0)
            {
                pwrite(l_fd, log_new_line, strlen(log_new_line), p_offset);
                p_offset += strlen(log_new_line);
            }
            printf("logging end log\n");
            pwrite(l_fd, end_log, strlen(end_log), p_offset);
            p_offset += strlen(end_log);
            offset = p_offset;

            close(l_fd);
        }

        free(buffer);
        }
    }
    
  }
  close(new_socket);
  sem_post(&thread_sem);
  pthread_mutex_lock(&thread_mutex);
  thread_in_use--;
  pthread_mutex_unlock(&thread_mutex);
  return NULL;
}

//***********************************************************************/
// putRequest  - args - headerbuffer: buffer returned from client
//                    - new_socket: socket connection initialized in main
//             - returns - void
// Will create file in server from client
//***********************************************************************/
void *putRequest(void *clientResponse)
{
    

    printf("IN HERERERERE\n");
  ThreadArg *argp = (ThreadArg *)clientResponse;
  char *header_buffer = argp->headerRecv;
  int new_socket = argp->new_socket;
  char *log_name = argp->logFileName;


  char *created = (char *)" 201 Created\r\n";
  char *put_content_length = (char *)"Content-Length: 0\r\n";
  bool valid_name = true;
  const size_t buffer_size = 32768;

  char textFileNameWithoutSlash[500] = {0};
  char textFileName[500] = {0};
  char serverType[20] = {0};
  int readFileLength;

  
  char byte_padding[9];
  int byte = 0;
  char content_length[20] = {0};
  char content_length_phrase[20] = {0};

  bool writeFailed = false;

  char *fileBuffer = (char *)calloc(buffer_size, sizeof(char));
  char *hex_buffer = (char *)calloc(40, sizeof(char));
  bool denied = false;
  bool file_created = false;
  int name_length = 0;

  int int_content_length;
  bool missing_content_length = false;

  //log file stuff
  char *fail_get = (char *)"FAIL: PUT ";
  char *success_get = (char *)"PUT ";
  
  int p_offset = 0;

  sprintf(byte_padding, "%08d", byte);

  sscanf(header_buffer, "%*s %s", textFileName);
  sscanf(header_buffer, "%*s %*s %s", serverType);
  sscanf(header_buffer, "%*s %*s %*s %*s %*s %*s %*s %*s %*s %s", content_length_phrase);
  sscanf(header_buffer, "%*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %s", content_length);
  int j = 0;

  for (size_t i = 1; i < sizeof(textFileName); ++i)
  {
    if (textFileName[i] != 0)
    {
      textFileNameWithoutSlash[j] = textFileName[i];
      name_length++;
    }
    j++;
  }
  printf("file name: %s\n", textFileNameWithoutSlash);
  int_content_length = atoi(content_length);

  char *namingConvention = textFileNameWithoutSlash;
  if (namingConvention[strspn(namingConvention, validChars)] != 0)
  {
    valid_name = false;
  }

  if (strcmp(textFileNameWithoutSlash, "healthcheck") == 0)
  {
      send(new_socket, serverType, strlen(serverType), 0);
      send(new_socket, forbiddenString, strlen(forbiddenString), 0);
  }
  // adapted from source: https://stackoverflow.com/questions/6605282/how-can-i-check-if-a-string-has-special-characters-in-c-effectively
    // checks for special characters excluding '-' and '_'
  else if (missing_content_length == true || name_length > 27                    || 
      valid_name == false            || strcmp(serverType, "HTTP/1.1") != 0 )
  {
    char *correct_HTTP = (char *)"HTTP/1.1";

    if (logFileFlag)
    {
      int l_fd = open(log_name, O_WRONLY, 0666);
      pthread_mutex_lock(&pwrite_mutex);
      p_offset = offset;
      offset += strlen(fail_get) + strlen(textFileName) + strlen(log_bad_request) + strlen(end_log);
      ++numLogEntries;
      ++numLogErrors;
      pthread_mutex_unlock(&pwrite_mutex);

      pwrite(l_fd, fail_get, strlen(fail_get), p_offset);
      p_offset += strlen(fail_get);
      pwrite(l_fd, textFileName, strlen(textFileName), p_offset);
      p_offset += strlen(textFileName);
      pwrite(l_fd, log_bad_request, strlen(log_bad_request), p_offset);
      p_offset += strlen(log_bad_request);
      pwrite(l_fd, end_log, strlen(end_log), p_offset);
      p_offset += strlen(end_log);
      close(l_fd);
    }
printf("bad3\n");
    send(new_socket, correct_HTTP, strlen(correct_HTTP), 0);
    send(new_socket, badRequestString, strlen(badRequestString), 0);
  }
  
  else
  {
    int fileDescriptor = open(textFileNameWithoutSlash, O_WRONLY);

    if (fileDescriptor < 0)
    {

      // send error code 400 forbidden
      if (errno == EACCES)
      {
        /* Write to Log file here */
        if (logFileFlag)
        {
          int l_fd = open(log_name, O_WRONLY, 0666);

          pthread_mutex_lock(&pwrite_mutex);
          p_offset = offset;
          offset += strlen(fail_get) + strlen(textFileName) + strlen(log_forbidden) + strlen(end_log);
          ++numLogEntries;
          ++numLogErrors;
          pthread_mutex_lock(&pwrite_mutex);

          pwrite(l_fd, fail_get, strlen(fail_get), p_offset);
          p_offset += strlen(fail_get);
          pwrite(l_fd, textFileName, strlen(textFileName), p_offset);
          p_offset += strlen(textFileName);
          pwrite(l_fd, log_forbidden, strlen(log_forbidden), p_offset);
          p_offset += strlen(log_forbidden);
          pwrite(l_fd, end_log, strlen(end_log), p_offset);
          p_offset += strlen(end_log);
          close(l_fd);
        }

        send(new_socket, serverType, strlen(serverType), 0);
        send(new_socket, forbiddenString, strlen(forbiddenString), 0);
        denied = true;
      }
      else
      {
        fileDescriptor = open(textFileNameWithoutSlash, O_CREAT | O_WRONLY, 0666);
        file_created = true;
      }
    }
    else
    {
      remove(textFileNameWithoutSlash);
      fileDescriptor = open(textFileNameWithoutSlash, O_CREAT | O_WRONLY, 0666);
    }

    if (denied == false)
    {
        printf("going in here\n");
      int read_size = 0;

      while (true)
      {
        fcntl(new_socket, F_SETFL, O_NONBLOCK);
        // printf("new socket: %d\n", new_socket);
        readFileLength = read(new_socket, fileBuffer, 32768);
        if (readFileLength != -1)
        {
          int writeToFile = write(fileDescriptor, fileBuffer, readFileLength);
        
          if (writeToFile < 0)
          {
            if (errno == EACCES)
            {
              writeFailed = true;
            }
          }
          read_size += readFileLength;
          // memset(fileBuffer, 0, sizeof(fileBuffer));
          memset(fileBuffer, 0, 32768);
          
        
        }
        if (readFileLength < 0)
        {
            warn("readFile less than 0: %s\n", textFileNameWithoutSlash);
        }
        
        
        if (readFileLength == 0 || read_size == int_content_length)
        {
          printf("reaching break statement\n");
          break;
        }
      }
      free(fileBuffer);
      printf("exited while loop\n");
      close(fileDescriptor);
      //log here
      if (logFileFlag)
      {
        size_t log_written_size = 0;
        int l_fd = open(log_name, O_WRONLY, 0666);

        pthread_mutex_lock(&pwrite_mutex);
        struct stat file_type;
        stat(textFileNameWithoutSlash, &file_type);
        size_t file_size = file_type.st_size;

        size_t hex_count = 3 * file_size;
        size_t newline_count = file_size % 20;
        size_t pad_count = 8 * newline_count;
        p_offset = offset;
        offset += strlen(success_get) + strlen(textFileName) + strlen(success_length) + strlen(content_length) + strlen(log_new_line) + hex_count + newline_count + pad_count + strlen(log_new_line) + strlen(end_log);
        ++numLogEntries;
        pthread_mutex_unlock(&pwrite_mutex);

        pwrite(l_fd, success_get, strlen(success_get), p_offset);
        p_offset += strlen(success_get);
        pwrite(l_fd, textFileName, strlen(textFileName), p_offset);
        p_offset += strlen(textFileName);
        pwrite(l_fd, success_length, strlen(success_length), p_offset);
        p_offset += strlen(success_length);
        pwrite(l_fd, content_length, strlen(content_length), p_offset);
        p_offset += strlen(content_length);
        pwrite(l_fd, log_new_line, strlen(log_new_line), p_offset);
        p_offset += strlen(log_new_line);
        int s_file = open(textFileNameWithoutSlash, O_RDONLY);
        char *buffer = (char *)calloc(32768, sizeof(char));
        while (true)
        {
          size_t size_read = read(s_file, buffer, sizeof(buffer));
          if (size_read <= 0)
          {
            break;
          }
          else
          {
            //size += size_read;
            for (size_t i = 0; i < size_read; ++i)
            {
              if (log_written_size % 20 == 0)
              {
                if (log_written_size != 0)
                {
                  pwrite(l_fd, log_new_line, strlen(log_new_line), p_offset);
                  p_offset += strlen(log_new_line);
                }
                pwrite(l_fd, byte_padding, strlen(byte_padding), p_offset);
                p_offset += strlen(byte_padding);
                pwrite(l_fd, log_space, strlen(log_space), p_offset);
                p_offset += strlen(log_space);
                byte += 20;
                sprintf(byte_padding, "%08d", byte);
              }
              sprintf(hex_buffer, " %02x", buffer[i]);
              pwrite(l_fd, hex_buffer, strlen(hex_buffer), p_offset);
              p_offset += strlen(hex_buffer);
              free(hex_buffer);
              hex_buffer = (char *)calloc(40, sizeof(char));
              log_written_size += 1;
            }
            free(buffer);
            buffer = (char *)calloc(32768, sizeof(char));
          }
        }
        free(buffer);
        if (file_size > 0)
        {
          pwrite(l_fd, log_new_line, strlen(log_new_line), p_offset);
          p_offset += strlen(log_new_line);
        }
        pwrite(l_fd, end_log, strlen(end_log), p_offset);
        p_offset += strlen(end_log);
        offset = p_offset;
        close(l_fd);
        printf("unlocked mutex\n");
      }

      // send error code 403 forbidden
      if (writeFailed == true)
      {
        if (logFileFlag)
        {
          int l_fd = open(log_name, O_WRONLY, 0666);

          pthread_mutex_lock(&pwrite_mutex);
          p_offset = offset;
          offset += strlen(fail_get) + strlen(textFileName) + strlen(log_forbidden) + strlen(end_log);
          ++numLogEntries;
          ++numLogErrors;
          pthread_mutex_unlock(&pwrite_mutex);

          pwrite(l_fd, fail_get, strlen(fail_get), p_offset);
          p_offset += strlen(fail_get);
          pwrite(l_fd, textFileName, strlen(textFileName), p_offset);
          p_offset += strlen(textFileName);
          pwrite(l_fd, log_forbidden, strlen(log_forbidden), p_offset);
          p_offset += strlen(log_forbidden);
          pwrite(l_fd, end_log, strlen(end_log), p_offset);
          p_offset += strlen(end_log);
          close(l_fd);
        }

        send(new_socket, serverType, strlen(serverType), 0);
        send(new_socket, forbiddenString, strlen(forbiddenString), 0);
      }
      // send error code 201 created
      else if (file_created == true)
      {

        send(new_socket, serverType, strlen(serverType), 0);
        send(new_socket, created, strlen(created), 0);
        send(new_socket, put_content_length, strlen(put_content_length), 0);
        send(new_socket, ending, strlen(ending), 0);
      }
      else
      {
        send(new_socket, serverType, strlen(serverType), 0);
        send(new_socket, successString, strlen(successString), 0);
        send(new_socket, put_content_length, strlen(put_content_length), 0);
        send(new_socket, ending, strlen(ending), 0);
      }
    }
  }

//   free(hex_buffer);
//   free(fileBuffer);
  close(new_socket);
  sem_post(&thread_sem);
  pthread_mutex_lock(&thread_mutex);
  thread_in_use--;
  pthread_mutex_unlock(&thread_mutex);
  return NULL;
}


//***********************************************************************/
// headRequest - args - headerbuffer: buffer returned from client
//                    - new_socket: socket connection initialized in main
//             - returns - void
// Will get file if exists in server, and send servertype, error/success codes,
// Only sends back the response header
//***********************************************************************/
void *headRequest(void *clientResponse)
{
    printf("going in here0\n");
  ThreadArg *argp = (ThreadArg *)clientResponse;
  char *header_buffer = argp->headerRecv;
  int new_socket = argp->new_socket;
  char *log_name = argp->logFileName;

  char *get_content_length = (char *)"Content-Length: ";

  bool valid_file_name = true;

  char *fail_get = (char *)"FAIL: HEAD ";
  char *success_get = (char *)"HEAD ";

//   char requestType[4] = {0};
  char textFileName[500] = {0};
  char textFileNameWithoutSlash[500] = {0};
  char serverType[20] = {0};
  int file_name_length = 0;
  int p_offset = 0;

  printf("buffer: %s\n", header_buffer);

  sscanf(header_buffer, "%*s %s", textFileName);
  sscanf(header_buffer, "%*s %*s %s", serverType);
  //send(new_socket, client_source, strlen(client_source), 0);

  //remove the initial slash for the file name
  int j = 0;
  for (size_t i = 1; i < sizeof(textFileName); ++i)
  {
    if (textFileName[i] != 0)
    {
      textFileNameWithoutSlash[j] = textFileName[i];
      file_name_length++;
    }
    j++;
  }
  char *namingConvention = textFileNameWithoutSlash;
// adapted from source: https://stackoverflow.com/questions/6605282/how-can-i-check-if-a-string-has-special-characters-in-c-effectively
    // checks for special characters excluding '-' and '_'  
  if (namingConvention[strspn(namingConvention, validChars)] != 0)
  {
    valid_file_name = false;
  }

  if (strcmp(textFileNameWithoutSlash, "healthcheck") == 0)
  {
      printf("going in here1\n");
      send(new_socket, serverType, strlen(serverType), 0);
      send(new_socket, forbiddenString, strlen(forbiddenString), 0);
  }
  // if the length of the textfile is not 27 chars
  else if (file_name_length > 27 || !valid_file_name || strcmp(serverType, "HTTP/1.1") != 0)
  {
      printf("going in here2\n");
    char *correct_HTTP = (char *)"HTTP/1.1";
    if (logFileFlag)
    {

      int l_fd = open(log_name, O_WRONLY, 0666);

      pthread_mutex_lock(&pwrite_mutex);
      p_offset = offset;
      offset += strlen(fail_get) + strlen(textFileName) + strlen(log_forbidden) + strlen(end_log);
      ++numLogEntries;
      ++numLogErrors;
      pthread_mutex_unlock(&pwrite_mutex);

      pwrite(l_fd, fail_get, strlen(fail_get), p_offset);
      p_offset += strlen(fail_get);
      pwrite(l_fd, textFileName, strlen(textFileName), p_offset);
      p_offset += strlen(textFileName);
      pwrite(l_fd, log_bad_request, strlen(log_bad_request), p_offset);
      p_offset += strlen(log_bad_request);
      pwrite(l_fd, end_log, strlen(end_log), p_offset);
      p_offset += strlen(end_log);
      close(l_fd);
    }
      printf("bad4\n");

    send(new_socket, correct_HTTP, strlen(correct_HTTP), 0);
    send(new_socket, badRequestString, strlen(badRequestString), 0);
  }
  else
  {
      printf("going in here3\n");
    struct stat file_type;
    stat(textFileNameWithoutSlash, &file_type);
    size_t file_size = file_type.st_size;
    char file_size_arr[750];
    sprintf(file_size_arr, "%zu", file_size);
    int permission = file_type.st_mode;
    if (permission & S_IRUSR || permission & S_IWUSR)
      printf("Has Permission\n");
    int s_file = open(textFileNameWithoutSlash, O_RDONLY);

    if (s_file == -1)
    {
      if (errno == EACCES)
      {
        if (logFileFlag)
        {
          int l_fd = open(log_name, O_WRONLY, 0666);

          pthread_mutex_lock(&pwrite_mutex);
          p_offset = offset;
          offset += strlen(fail_get) + strlen(textFileName) + strlen(log_forbidden) + strlen(end_log);
          ++numLogEntries;
          ++numLogErrors;
          pthread_mutex_unlock(&pwrite_mutex);

          pwrite(l_fd, fail_get, strlen(fail_get), p_offset);
          p_offset += strlen(fail_get);
          pwrite(l_fd, textFileName, strlen(textFileName), p_offset);
          p_offset += strlen(textFileName);
          pwrite(l_fd, log_forbidden, strlen(log_forbidden), p_offset);
          p_offset += strlen(log_forbidden);
          pwrite(l_fd, end_log, strlen(end_log), p_offset);
          p_offset += strlen(end_log);
          close(l_fd);
        }

        send(new_socket, serverType, strlen(serverType), 0);
        send(new_socket, forbiddenString, strlen(forbiddenString), 0);
      }
      else
      {
        if (logFileFlag)
        {
          int l_fd = open(log_name, O_WRONLY, 0666);

          pthread_mutex_lock(&pwrite_mutex);
          p_offset = offset;
          offset += strlen(fail_get) + strlen(textFileName) + strlen(log_not_found) + strlen(end_log);
          ++numLogEntries;
          ++numLogErrors;
          pwrite(l_fd, fail_get, strlen(fail_get), p_offset);
          pthread_mutex_unlock(&pwrite_mutex);

          p_offset += strlen(fail_get);
          pwrite(l_fd, textFileName, strlen(textFileName), p_offset);
          p_offset += strlen(textFileName);
          pwrite(l_fd, log_not_found, strlen(log_not_found), p_offset);
          p_offset += strlen(log_not_found);
          pwrite(l_fd, end_log, strlen(end_log), p_offset);
          p_offset += strlen(end_log);
          close(l_fd);
        }

        send(new_socket, serverType, strlen(serverType), 0);
        send(new_socket, notFoundString, strlen(notFoundString), 0);
      }
    }
    else
    {

      if (logFileFlag)
      {
        int l_fd = open(log_name, O_WRONLY, 0666);
        pthread_mutex_lock(&pwrite_mutex);
        p_offset = offset;
        offset += strlen(success_get) + strlen(textFileName) + strlen(success_length) + strlen(file_size_arr) + strlen(end_log);
        ++numLogEntries;
        pthread_mutex_unlock(&pwrite_mutex);

        pwrite(l_fd, success_get, strlen(success_get), p_offset);
        p_offset += strlen(success_get);
        pwrite(l_fd, textFileName, strlen(textFileName), p_offset);
        p_offset += strlen(textFileName);
        pwrite(l_fd, success_length, strlen(success_length), p_offset);
        p_offset += strlen(success_length);
        pwrite(l_fd, file_size_arr, strlen(file_size_arr), p_offset);
        p_offset += strlen(file_size_arr);
        pwrite(l_fd, log_new_line, strlen(log_new_line), p_offset);
        p_offset += strlen(log_new_line);
        pwrite(l_fd, end_log, strlen(end_log), p_offset);
        p_offset += strlen(end_log);
        close(l_fd);
      }

      
      send(new_socket, serverType, strlen(serverType), 0);          
      send(new_socket, successString, strlen(successString), 0);                    
      send(new_socket, get_content_length, strlen(get_content_length), 0); 
      send(new_socket, file_size_arr, strlen(file_size_arr), 0);         
      send(new_socket, ending, strlen(ending), 0);                        
      send(new_socket, ending, strlen(ending), 0);                        
    }
    close(s_file);
  }
  close(new_socket);
  return NULL;
}

