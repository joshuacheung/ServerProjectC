# Multi Threaded HTTP Server with Logging

## Goals
The goal of this program is to create a simple multi-threaded HTTP server with GET. PUT, and
HEAD command capabilities. These commands will allow us to write and read data into or from
a server. For GET, we must be able to grab the specified file and send the contents and its length
back to the client. For PUT, we must be able to read the contents of the file sent in by the client,
and write it to a new file created in our server. Our HEAD request will simply return the header
response back to the client.

## Design
First, we need to create a connection between the server and the client that will be able to read
the headers that come in from the client, and read the requests accordingly. We will be using the
curl command as a client placement to test our command.

#### Handling Command Line Arguments 
Our program must be able to handle several scenarios:
1. N flag for number of desired threads
2. l flag for log_file name
3. Port number

To handle this we will be using the getopt() function which checks for specific flag characters.
This will be used for the log and number of threads specification. Then, we will use optind to get
the argument without a flag, which is the port number. As said in the document specification, the
log and threads flags are optional whereas the port number is a mandatory argument.

#### Making Server and Connecting to the Client
There are several steps needed in order to create a connection to the server:
1. Connect to the server using a socket()
2. Use setsockopt() to be able to reuse address and port of server
3. Bind() the socket to the specified port number, for ex. Localhost
4. Listen() to wait for the client to connect to the server
5. Use the Accept() and connect() function to establish a connection between client and
server. We will be infinitely running this part using a while loop to be able to receive
multiple client requests without the server closing.

After we have created a connection between the server and client, we will parse the header
received from the client to check whether it is a GET, PUT, or HEAD by using sscanf() which
separates strings by white space, allowing us to grab specific parts of the header. After
determining the request type, we will pass the socket and header as a char array into our
respective request functions. Our response headers will consist of the server type, success/fail
code, and if succeeded we will send the content length of the file.

### Function 1: GET REQUEST
1. Read the header returned by the client into a buffer.
2. Parse the header using sscanf to get the server type and file name we are getting.
   - We will need to loop through the filename to get rid of the ‘/’ character that
precedes it
3. Check for bad request and forbidden errors when opening the file with a file descriptor.
The specification for these errors will be discussed in another section below.
4. Send the appropriate header response
5. Similar to our dog assignment, read() through the file using our buffer size at a time, and
then while our read() is greater than 0 (while there is something left to read) send the
contents of the file back to the client. In each iteration, we will be sending:
   - Arg1: socket
   - Arg2: contents of our buffer
   - Arg3: Length of what we have read
Afterwards we will clear what we have read into our buffer to prepare for the next
iteration.
6. After we have finished sending, we will free our file buffer and close our descriptor


### Function 2: PUT REQUEST
1. Read the header returned by the client into a buffer.
2.Parse the header using sscanf to get the server type and file name we are getting.
   - We will need to loop through the filename to get rid of the ‘/’ character that
precedes it.
3. Check if the file we are putting already exists. If so, delete the file and recreate it, then
check for permission errors and bad request errors.
4. Send the appropriate header response.
5. Next, we will read data from the client again to get the contents of the file, and read those
contents into a buffer using recv().
   - If the content length of the file exceeds our buffer size, we will need to repeatedly
create a new buffer until all the contents of the file are read.
6. Then we will write the buffer with the contents of the file into a new file in our server.

### Function 3: HEAD REQUEST
1. Read the header returned by the client into a buffer
2. Parse the header using sscanf to get the server type and file name we are getting.
   - We will need to loop through the filename to get rid of the ‘/’ character that
precedes it.
3. Check for bad request and forbidden errors when opening the file with a file descriptor.
The specification for these errors will be discussed in another section below.
4.  Send the appropriate header response.

### Error Handling
1. 200 OK\r\n → if there are no errors and we have sent everything correctly.
2. 01 Created\r\n → during a put request, if the file did not previously exists in the server.
3. 00 Bad Request\r\n → If the filename is greater than 27 chars, if there exists and invalid
character within the file name specified in the asgn spec, or if the request type is neither a
GET, PUT, or HEAD. If we ask for a healthcheck when there exists no log file
4. 03 Forbidden → If the permissions for opening the requesting file is denied.
5. 04 Not Found → If the file requested is not found.
6. 00 Internal Server Error → If there is a server error.
