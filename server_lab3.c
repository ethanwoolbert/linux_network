#define _GNU_SOURCE

#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <stdbool.h>
#include <ctype.h>
#include "min_heap.h"

#define UNIX_PATH_MAX 108

// free_heap implementation is different so must include here
void free_heap(struct min_heap* my_heap, struct min_heap** c_heaps, int count) {
	// Loop through array of heaps and free everything
	struct line_val* min;
	for (int i = 0; i < count; i++) {
		while (c_heaps[i]->length > 0) {
			min = extract_min(c_heaps[i]);
			free(min->text);
			free(min);
		}
		free(c_heaps[i]);
	}
	free(c_heaps);

	// Free stand alone heap for all lines
	while (my_heap->length > 0) {
		min = extract_min(my_heap);
		free(min->text);
		free(min);
	}
	free(my_heap);
}

// Must loop through every buffer and free
void free_buffer(char ** buffer, int count, int * file_line_count){
	for(int i = 0; i<count; i++){
		free(buffer[i]);
	}
	free(buffer);
}

// Should get two command line args, file name and port number
int main (int argc, char * argv[]) {
	// Create time/socket variables
	struct timeval time;
	time.tv_sec = 1;
	int my_socket, client_socket;
	struct sockaddr_in addr;
	// Check correct number of command line arguments were passed
	if (argc !=3){
		printf("too few command line args: should be file_name file_to_open port_num\n");
		return incorrect_arg_num;
	}

	// Open spec file and return if it fails
	FILE * fp = fopen(argv[1], "r");
	if(fp==NULL){
		printf("could not open file\n");
		return could_not_open_file;
	}	

	// Create variables for files
	char f_name[255];
	int f_info= 0;
	// Read first line (poem destination) into junk buffer
	char * burn = calloc(255, sizeof(char));
	if (!burn) {
		printf("burn calloc failed");
		return allocation_failed;
	}
	f_info = fscanf(fp, "%s", burn);
	if(f_info == EOF){
		printf("problem with scan\n");
		free(burn);
		return fscanf_failed;
	}
	free(burn);

	// Get number of files by reading each line
	int count = 0;
	while (1){
		f_info = fscanf(fp, "%s", f_name);
		if(f_info==EOF){
			break;
		}
		count = count +1;
	}
	
	// Must have at least two files
	if(count < 2){
		printf("not enough files to read from\n");
		return too_few_files;
	}
	// Dynamically allocate memory for line count and byte count of each file
	int * file_line_count = calloc(count , sizeof(int));
	if (!file_line_count) {
		printf("file_line_count calloc failed");
		return allocation_failed;
	}
	int * file_byte_count = calloc(count , sizeof(int));
	if (!file_byte_count) {
		printf("file_byte_count calloc failed");
		free(file_line_count);
		return allocation_failed;
	}
	// Rewind file to first line
	rewind(fp);

	// Read destination file name into w_name
	char w_name[255];
	f_info = fscanf(fp, "%s", w_name);
	// Make sure it succeeded and if not free and return
    if(f_info==EOF){
		printf("problem writing the second time\n");
		free(file_line_count);
		free(file_byte_count);
		return fscanf_failed;
	}

	// Allocate memory for file pointers
	FILE **fps = calloc(sizeof(FILE*), (count+1));
	if (!fps) {
		printf("fps calloc failed");
		free(file_line_count);
		free(file_byte_count);
		return allocation_failed;
	}
	int index = 1; // Because we want to ignore the file to write to
	ssize_t f_len = 0;
	// Loop though each file
	while(1) {
		// Read file name into f_name
		f_info = fscanf(fp, "%s", f_name);
		if(f_info==EOF){
			break;
		}

		// Open file and add to fps array
		fps[index] = fopen(f_name, "r");
		// Check that it succeeded and if not free and return
		if (fps[index]==NULL){
			printf("problem opening read file at index: %d", index);
			free(file_line_count);
			free(file_byte_count);
			free(fps);
			return could_not_open_file;
		}
		// Reset f_info
		f_info = 0;
		// Rewind the file pointer
		rewind(fps[index]);
		char c;
		// Loop though file to set file byte count and file line count
		while(!feof(fps[index])){	
			file_byte_count[index-1]++;
			c=fgetc(fps[index]);
			if(c=='\n'){
				file_line_count[index-1]++;
			}
		}
		index = index +1;		
	}

	// Get total sum of all lines in all files
	int sum = 0;
	for(int j = 0; j<count; j++){
		rewind(fps[j+1]);
		sum = sum + file_line_count[j];
	}

	// Allocate memory for heaps for each file
	struct min_heap ** c_heaps = calloc(1, count * sizeof(struct min_heap *));
	if(!c_heaps){
		printf("fps calloc failed");
		free(file_line_count);
		free(file_byte_count);
		free(fps);
		return allocation_failed;

	}
	// Loop through and create each individual heap
	for(int i = 0; i<count; i++){
		c_heaps[i] = create_heap(file_line_count[i]);
		if (!c_heaps[i]) {
			printf("create_heap failed");
			free(file_line_count);
			free(file_byte_count);
			free(fps);
			for (int j = 0; j < i; j++) {
				free(c_heaps[j]);
			}
			free(c_heaps);
			return create_heap_failed;
		}
	}

	// Creating variables
	int file_byte_count_max = 16384;
	char line[file_byte_count_max];
	memset(line, 0, sizeof(line));
	char * p_line = line;
	size_t len = file_byte_count_max;
	ssize_t read_len = 0;
	// Loop trough every file and add lines to each heap
	for(int j = 0; j<count; j++){
		read_len = 0;
		memset(line, 0, sizeof(line));
		// Loop through lines in file
		while((read_len = getline(&p_line, &len, fps[j+1]))!=-1){
			// Check that calloc succeeded and if not free and return
			struct line_val * line_v = calloc(1, sizeof(struct line_val));
			if (!line_v) {
				free(file_line_count);
				free(file_byte_count);
				free(fps);
				struct line_val* min;
				for (int i = 0; i < count; i++) {
					while (c_heaps[i]->length > 0) {
						min = extract_min(c_heaps[i]);
						free(min->text);
						free(min);
					}
					free(c_heaps[i]);
				}
				free(c_heaps);
				return allocation_failed;
			}
			// Check that calloc succeeded and if not free and return
			line_v->text = calloc(1, read_len * sizeof(char));
			if (!line_v->text) {
				free(file_line_count);
				free(file_byte_count);
				free(fps);
				struct line_val* min;
				for (int i = 0; i < count; i++) {
					while (c_heaps[i]->length > 0) {
						min = extract_min(c_heaps[i]);
						free(min->text);
						free(min);
					}
					free(c_heaps[i]);
				}
				free(c_heaps);
				free(line_v);
				return allocation_failed;
			}
			// Add text from buffer to text field char by char
			for(int i= 0; i<read_len; i++){
				line_v->text[i] = line[i];
			}
			line_v->line_num = 0;
			line_v->line_length = read_len;

			// Insert new element into the heap
			insert(c_heaps[j], line_v);
		}
	}

	// Create heap with dynamic size
	struct min_heap* my_heap = create_heap(sum);
	if (!my_heap) {
		printf("create_heap failed");
		free(file_line_count);
		free(file_byte_count);
		free(fps);
		struct line_val* min;
		for (int i = 0; i < count; i++) {
			while (c_heaps[i]->length > 0) {
				min = extract_min(c_heaps[i]);
				free(min->text);
				free(min);
			}
			free(c_heaps[i]);
		}
		free(c_heaps);
		return create_heap_failed;
	}
	// Check if calloc succeeded and if not free and return
	char ** line_buffers = calloc(count , sizeof (char*));
	if (!line_buffers) {
		printf("line_buffers calloc failed");
		free(file_line_count);
		free(file_byte_count);
		free(fps);
		free_heap(my_heap, c_heaps, count);
		return allocation_failed;
	}
	
	// Loop though each file and allocate enough memory to hold all the chars
	for(int i = 0; i<count; i++){
		file_byte_count[i] = file_byte_count[i] + 4 * file_line_count[i];
		line_buffers[i] = calloc(1, file_byte_count[i]);
		if (!line_buffers[i]) {
			printf("line_buffers[i] calloc failed");
			free(file_line_count);
			free(file_byte_count);
			free(fps);
			free_heap(my_heap, c_heaps, count);
			free_buffer(line_buffers, count, file_line_count);
			return allocation_failed;
		}
	}

	// Create socket, check that it succeeded and if not free and return
	int server_socket;
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
		printf("socket failed");
		free(file_line_count);
		free(file_byte_count);
		free(fps);
		free_heap(my_heap, c_heaps, count);
		free_buffer(line_buffers, count, file_line_count);
		return create_socket_failed;
	}

	// Set address info
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[2]));
	addr.sin_addr.s_addr = INADDR_ANY;

	// Set socket options, check that it succeeded and if not free and return
	const int opt = 1;
	if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) < 0){
		printf("set sock reuse failed\n");
		free(file_line_count);
		free(file_byte_count);
		free(fps);
		free_heap(my_heap, c_heaps, count);
		free_buffer(line_buffers, count, file_line_count);
		return setsockopt_failed;
	}

	// Bind socket, check that it succeeded and if not free and return
    if (bind(server_socket, (struct sockaddr *) &addr, sizeof(struct sockaddr_in)) == -1) {
        printf("server socket bind failed");
		free(file_line_count);
		free(file_byte_count);
		free(fps);
		free_heap(my_heap, c_heaps, count);
		free_buffer(line_buffers, count, file_line_count);
        return bind_failed;
    }
	printf("port to connect to is: %d\n",atoi(argv[2]));

	// Get the host name, check that it succeeded and if not free and return
	char name[255];
	if(gethostname(name, 255)==-1){
		printf("problem getting host name\n");
		free(file_line_count);
		free(file_byte_count);
		free(fps);
		free_heap(my_heap, c_heaps, count);
		free_buffer(line_buffers, count, file_line_count);
		return gethostname_failed;
	}

	// Check that hostname is not null, if it is free and return
	struct hostent * hs = gethostbyname(name);
	if(hs==NULL){
		printf("problem with gethostbyname\n");
		free(file_line_count);
		free(file_byte_count);
		free(fps);
		free_heap(my_heap, c_heaps, count);
		free_buffer(line_buffers, count, file_line_count);
		return gethostbyname_failed;
	}
	// Print ip address to use
	printf("ip address to use: %s\n",inet_ntoa(*(struct in_addr *)hs->h_addr));

	// Check that listen succeeded and if not free and return
    if (listen(server_socket, count) == -1) {
	    printf("listen failed");
		free(file_line_count);
		free(file_byte_count);
		free(fps);
		free_heap(my_heap, c_heaps, count);
		free_buffer(line_buffers, count, file_line_count);
	    return listen_failed;
	}

	// Create epoll, check that is succeeded and if not free and return
	int epoll_fd = epoll_create1(0);
	if(epoll_fd==-1){
		printf("problem with epoll create\n");
		free(file_line_count);
		free(file_byte_count);
		free(fps);
		free_heap(my_heap, c_heaps, count);
		free_buffer(line_buffers, count, file_line_count);
		return create_epoll_failed;
	}
	
	// Create epoll event
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = server_socket;
	struct epoll_event *evs;
	int * sockets;
	// Check that evs exists and if not free and return
	evs = calloc(sizeof(struct epoll_event), count);
	if(!evs){
		printf("ev calloc failed\n");
		free(file_line_count);
		free(file_byte_count);
		free(fps);
		free_heap(my_heap, c_heaps, count);
		free_buffer(line_buffers, count, file_line_count);
		return allocation_failed;
	}
	
	// Allocate memory for sockets, check it exists and if not free and return
	sockets = calloc(sizeof(int), count);
	if(!sockets){
		printf("sockets calloc failed\n");
		free(file_line_count);
		free(file_byte_count);
		free(fps);
		free_heap(my_heap, c_heaps, count);
		free_buffer(line_buffers, count, file_line_count);
		free(evs);
		return allocation_failed;
	}

	// Check epoll_ctl succeeded and if not free and return
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &ev) == -1) {
		printf("epoll_ctl failed for server socket");
		free(file_line_count);
		free(file_byte_count);
		free(fps);
		free_heap(my_heap, c_heaps, count);
		free_buffer(line_buffers, count, file_line_count);
		free(evs);
		free(sockets);
		return epoll_ctl_failed;
	}
	
	// Keeps track of the index of the reads from the clients to the server for 
	int * s_read_index = calloc(count, sizeof(int));
	if (!s_read_index) {
		free(file_line_count);
		free(file_byte_count);
		free(fps);
		free_heap(my_heap, c_heaps, count);
		free_buffer(line_buffers, count, file_line_count);
		free(evs);
		free(sockets);
		return allocation_failed;
	}
	for(int i=  0; i<count; i++){
		s_read_index[i]= 0;
	}
	// Each client in the case of short reads
	int cur_file_count = 0; // Current number of files that have been allocated to a process
	int done_file_count = 0; // Number of files we are done reading
	bool done_reading = false;
	
	// Continue looping until all files have been written and read
	while(!(done_reading)){
		// Check epoll_wait succeeded and if not free and return
		int nfds = epoll_wait(epoll_fd, evs, count, -1);
		if(nfds==-1){
			printf("epoll wait failed\n");
			free(file_line_count);
			free(file_byte_count);
			free(fps);
			free_heap(my_heap, c_heaps, count);
			free_buffer(line_buffers, count, file_line_count);
			free(evs);
			free(sockets);
			free(s_read_index);
			return epoll_wait_failed;
		}

		// Loop through every nfds
		int file_socket = 0;
		for (int i= 0; i<nfds; i++) {

			if(evs[i].data.fd == server_socket){
				// Check accept worked and if not free and return
				file_socket = accept(server_socket, NULL, NULL);
				if(file_socket==-1){
					printf("accept failed\n");
					free(file_line_count);
					free(file_byte_count);
					free(fps);
					free_heap(my_heap, c_heaps, count);
					free_buffer(line_buffers, count, file_line_count);
					free(evs);
					free(sockets);
					free(s_read_index);
					return accept_failed;
				}
				sockets[cur_file_count] = file_socket;
				// Checking for info and termination (writes to the client one line at a time)
				ev.events = EPOLLOUT | EPOLLRDHUP;
				ev.data.fd = file_socket;
				// Make sure epoll_ctl doesn't fail
				if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockets[cur_file_count], &ev)==-1){
					printf("epoll_ctl failed for file socket\n");
					free(file_line_count);
					free(file_byte_count);
					free(fps);
					free_heap(my_heap, c_heaps, count);
					free_buffer(line_buffers, count, file_line_count);
					free(evs);
					free(sockets);
					free(s_read_index);
					return epoll_ctl_failed;	
				}
				// Check if we are done writeing/reading all the information
				cur_file_count = cur_file_count + 1;
				if(cur_file_count==count){
					if(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, server_socket, NULL)==-1){
						printf("problem removing server socket from epoll list\n");
						free(file_line_count);
						free(file_byte_count);
						free(fps);
						free_heap(my_heap, c_heaps, count);
						free_buffer(line_buffers, count, file_line_count);
						free(evs);
						free(sockets);
						free(s_read_index);
						return epoll_ctl_failed;
					}
					close(server_socket);
				}
			}
			// Check through all of the open sockets to see if they have info
			else{ 
				// Loop through every file
				for(int j = 0; j<cur_file_count; j++){
					if(evs[i].data.fd == sockets[j]){
						// Check if connection has been closed
						if(evs[i].events & EPOLLRDHUP){
							// Make sure epoll_ctl doesn't fail
							if(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sockets[j], NULL)==-1){
								printf("problem removing from epoll_fd on socket %d\n", j);
								free(file_line_count);
								free(file_byte_count);
								free(fps);
								free_heap(my_heap, c_heaps, count);
								free_buffer(line_buffers, count, file_line_count);
								free(evs);
								free(sockets);
								free(s_read_index);
								return epoll_ctl_failed;
							}
							close(sockets[j]);
							sockets[j] = -1;
						}
						// Check for data that can be read
						else if(evs[i].events & EPOLLIN){
							size_t length = 0;
							// Read in data
							if((length = read(sockets[j], &line_buffers[j][s_read_index[j]],file_byte_count[j]-s_read_index[j]))){
								// Loop through data added by buffer and parse
								for(int i = s_read_index[j]; i<s_read_index[j] + length; i++){
									// Check if we are at the end of a file
									if(line_buffers[j][i]=='\0'){
										int l_count = 0;
										bool start_line = true;
										int last_line = -1;//index of the last completed line
										bool done = false;
										int n_index = 0;
										for (int k = 0; k < i; k++) {
											// Check if we are at the end of a line
											if(line_buffers[j][k] == '\n'){
												// Allocate memory for line and make sure it succeeds
												struct line_val * line = calloc(1, sizeof(struct line_val));
												if (!line) {
													printf("line calloc failed");
													free(file_line_count);
													free(file_byte_count);
													free(fps);
													free_heap(my_heap, c_heaps, count);
													free_buffer(line_buffers, count, file_line_count);
													free(evs);
													free(sockets);
													free(s_read_index);
													return allocation_failed;
												}

												// Allocate memory for num and make sure it succeeds
												start_line = true;
												char * num = calloc(n_index , sizeof(char));
												if (!num) {
													printf("num calloc failed");
													free(file_line_count);
													free(file_byte_count);
													free(fps);
													free_heap(my_heap, c_heaps, count);
													free_buffer(line_buffers, count, file_line_count);
													free(evs);
													free(sockets);
													free(s_read_index);
													free(line);
													return allocation_failed;
												}
												
												// Loop through and add number chars to num
												for(int n = 0; n<n_index; n++){
													num[n] = line_buffers[j][last_line + 1 + n];
												}
												// Put int num into line
												line->line_num = atoi(num);
												free(num);

												// Allocate memory for text in line and make sure it succeeds
												line->text = calloc((k - last_line) , sizeof(char));
												if (!line->text) {
													printf("line->text calloc failed");
													free(file_line_count);
													free(file_byte_count);
													free(fps);
													free_heap(my_heap, c_heaps, count);
													free_buffer(line_buffers, count, file_line_count);
													free(evs);
													free(sockets);
													free(s_read_index);
													free(line);
													return allocation_failed;
												}

												// Loop through text and add it to buffer
												for(int n = 0; n<(k - last_line- n_index); n++){
													line->text[n] = line_buffers[j][(last_line + n + n_index + 1)];
												}
												l_count = l_count + 1;
												last_line = k;
												n_index = 0;

												// Insert the line into the heap
												insert(my_heap, line);
												continue;
											}
											// Check for digits to add to line number buffer
											else if(isdigit(line_buffers[j][k]) && start_line){
												n_index = n_index + 1;		
											}
											else{
												start_line = false;
											}
										}
										done_file_count = done_file_count + 1;
										// Switch the epoll to just wait for epollrdhup
										ev.events = EPOLLRDHUP;
										ev.data.fd = sockets[j];
										if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockets[j], &ev)==-1){
											free(file_line_count);
											free(file_byte_count);
											free(fps);
											free_heap(my_heap, c_heaps, count);
											free_buffer(line_buffers, count, file_line_count);
											free(evs);
											free(sockets);
											free(s_read_index);
											return epoll_ctl_failed;
										}
										// Check if we have read/written data from every file
										if(done_file_count == (count)){
											done_reading = true;
										}
										// Check for terminating char to see if we are done
										char n_terminate = '\0';
										if((write(sockets[j], &n_terminate, 1 * sizeof(char)))==-1){
											printf("problem sending null terminator\n");
											return socket_write_failed;
										}
										break;
									}
								}
								s_read_index[j] += length;					
							}
							// Otherwise socket read failed	
							else{
								printf("problem reading from client\n");
								free(file_line_count);
								free(file_byte_count);
								free(fps);
								free_heap(my_heap, c_heaps, count);
								free_buffer(line_buffers, count, file_line_count);
								free(evs);
								free(sockets);
								free(s_read_index);
								return socket_read_failed;
							}
						}
						// Write one line of file to client, if EOF then switch to read
						else if(evs[i].events & EPOLLOUT){
							// Check that heap still has data to write
							if(c_heaps[j]->length>0){
								// Get the first element in the heap
								struct line_val * w_line = extract_min(c_heaps[j]);
								ssize_t write_length = 0;
								write_length = write(sockets[j], &w_line->text[-1 * w_line->line_num], w_line->line_length + w_line->line_num);
								if(write_length ==-1){
									printf("erorr writing to file");
									free(file_line_count);
									free(file_byte_count);
									free(fps);
									free_heap(my_heap, c_heaps, count);
									free_buffer(line_buffers, count, file_line_count);
									free(evs);
									free(sockets);
									free(s_read_index);
									free(w_line->text);
									free(w_line);
									return socket_write_failed;
								}
								// If short write is encountered decrement the line_num (so that it is at begining of heap) and insert
								else if(write_length!=w_line->line_length + w_line->line_num){
									w_line->line_num = -1 * write_length;
									insert(c_heaps[j], w_line);
								}
								// Otherwise error and must free line
								else {
									free(w_line->text);
									free(w_line);
								}
							}
							// Otherwise write to the client to signal that we are done
							else{
								// Write terminating char to the client so that it know to close socket
								char done = '\0';
								if (write(sockets[j], &done, 1 * sizeof(char)) == -1) {
									printf("problem writing termination to client %d\n", j);
									free(file_line_count);
									free(file_byte_count);
									free(fps);
									free_heap(my_heap, c_heaps, count);
									free_buffer(line_buffers, count, file_line_count);
									free(evs);
									free(sockets);
									free(s_read_index);
									return socket_write_failed;
								}
								// Check that everything is still working
								ev.events = EPOLLIN | EPOLLRDHUP;
								ev.data.fd = sockets[j];
								if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockets[j], &ev)==-1){
									printf("problem switching to write on socket %d\n", j);
									free(file_line_count);
									free(file_byte_count);
									free(fps);
									free_heap(my_heap, c_heaps, count);
									free_buffer(line_buffers, count, file_line_count);
									free(evs);
									free(sockets);
									free(s_read_index);
									return epoll_ctl_failed;
								}
							}
						}
					}
				}	
			}
		}
	}

	// Open file to dump lines too and write every line
	FILE * w_fp = fopen(w_name, "w");
	if(w_fp==NULL){
		printf("could not open file\n");
		return could_not_open_file;
	}
	while (my_heap->length > 0) {
		struct line_val* min = extract_min(my_heap);
		// Make sure write to file succeeded
		if(fputs(min->text, w_fp)==EOF){
			printf("problem writing to the file\n");
			free(file_line_count);
			free(file_byte_count);
			free(fps);
			free_heap(my_heap, c_heaps, count);
			free_buffer(line_buffers, count, file_line_count);
			free(evs);
			free(sockets);
			free(s_read_index);
			free(min->text);
			free(min);
			fclose(w_fp);
			return fputs_failed;
		}
		// Free the extracted line
		free(min->text);
		free(min);
	}
	
	// Free all dynamically allocated memory
	free(file_line_count);
	free(file_byte_count);
	free(fps);
	free_heap(my_heap, c_heaps, count);
	free_buffer(line_buffers, count, file_line_count);
	free(evs);
	free(sockets);
	free(s_read_index);
	fclose(w_fp);
	return success;
}
