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
void free_heap(struct min_heap* my_heap) {
	// Loop through heap and free every element
	struct line_val* min;
	while (my_heap->length > 0) {
		min = extract_min(my_heap);
		free(min->text);
		free(min);
	}
	free(my_heap);
}

// First arg is the internet address, the second is the port
int main(int argc, char * argv[]){
	// Check number of command line arguments is correct
	if(argc!=3){
		printf("incorrect number of command line args, should be file_name, internet_address, port_num\n");
		return incorrect_arg_num;
	}

	// Initialize socket related variables
	int my_socket, client_socket;
	struct sockaddr_in addr;
	struct in_addr inp;
	
	// Create socket
	my_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(my_socket==-1){
		printf("socket failed\n");
		return create_socket_failed;
	}

	// Set address variables
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[2]));
	if(inet_aton(argv[1], &inp) == 0){
		printf("inet_aton failed\n");
		return inet_aton_failed;
	}
	addr.sin_addr = inp;

	// Connect to socket
	if(connect(my_socket, (struct sockaddr *) &addr, sizeof(struct sockaddr_in))==-1){
		printf("connection failed\n");
		return connect_failed;
	}

	// Create/initialize variables
	// Create buffer to read in line
	int line_byte_count = 16384;
	char buffer[line_byte_count];
	memset(buffer, 0, sizeof(buffer));
	ssize_t length = 0;
	int line_ptr = 0;
	// Create heap with line.
	int num_lines = 1000;
	struct min_heap* my_heap = create_heap(num_lines);
	if ((int)my_heap == create_heap_failed) {
		printf("create_heap failed");
		return create_heap_failed;
	}
	int l_count = 0;
	bool start_line = true;
	int last_line = -1;
	bool done = false;
	int n_index = 0;

	// Loop until all lines have been read
	while(1) {
		last_line = -1;
		// Read in data and set length of line
		if((length = read(my_socket, &buffer[line_ptr], line_byte_count))!=-1){
			// Loop through every char in buffer
			for(int k = 0; k<length; k++){
				// If EOF character end loop
				if(buffer[k]=='\0'){
					done = true;
					break;
				}
				// If newline char is found add line to heap etc.
				if(buffer[k] == '\n'){
					// Create new element to contain line in heap
					struct line_val* new_element = calloc(1, sizeof(struct line_val));
					if (!new_element) {
						printf("new_element calloc failed");
						free_heap(my_heap);
						return allocation_failed;
					}
					start_line = true;
					// calloc buffer for number
					char * num = calloc(n_index, sizeof(char));
					if (!num) {
						printf("num calloc failed");
						free_heap(my_heap);
						free(new_element);
						return allocation_failed;
					}
					for(int n = 0; n<n_index; n++){
						num[n] = buffer[last_line + 1 + n];
					}
					// Update fields of heap element
					new_element->line_num = atoi(num);
					new_element->line_length = k - last_line;
					// Add text to heap element char by char
					new_element->text = calloc((k - last_line), sizeof(char));
					if (!new_element->text) {
						printf("num_element->text calloc failed");
						free_heap(my_heap);
						free(new_element->text);
						free(new_element);
						free(num);
						return allocation_failed;
					}
					new_element->offset = 0;
				    for(int n = 0; n<(k - last_line); n++){
						new_element->text[n] = buffer[(last_line + n + 1)];
					}

					// Insert element into heap and update variables
					insert(my_heap, new_element);
					l_count = l_count + 1;
					last_line = k;
					n_index = 0;

					// Free dynamic char * for num and continue to next loop
					free(num);
					continue;
				}
				// Get digits from start of string char by char
				else if(isdigit(buffer[k]) && start_line){
					n_index = n_index + 1;		
				}
				// Otherwise we are not at start of line and set variable to false
				else{
					start_line = false;
				}
			}
		}
		// Catch problem reading from socket and free heap
		else{
			printf("problem reading from socket\n");
			free_heap(my_heap);
			return socket_read_failed;
		}
		// If done simply exit the loop
		if(done){
			break;
		}
	}	

	// Use heap-sort to send lines back to server in right order
	ssize_t w_length = 0;
	while (my_heap->length > 0) {
		// Create dynamic buffer to write to server
		struct line_val* min = extract_min(my_heap);
		// If write fails free heap and buffer and return
		w_length =  write(my_socket, &min->text[min->offset], min->line_length - min->offset);
		if(w_length ==-1){
			printf("problem with the write\n");
			free_heap(my_heap);
			free(min->text);
			free(min);
			return socket_write_failed;
		}
		else if(w_length!=min->line_length - min->offset){
			min->offset = w_length;
			insert(my_heap, min);
		}
		// Free buffer for each loop as a new one will be created
		else{
			free(min->text);
			free(min);
		}
	}

	// Send null terminator only to signal done
	char n_terminate = '\0';
	if((w_length = write(my_socket, &n_terminate, 1 * sizeof(char)))==-1){
		printf("problem sending null terminator\n");
		free_heap(my_heap);
		return socket_write_failed;
	}

	// Wait for server to send confirmation that everything has been read	
	while (read(my_socket, &buffer[line_ptr], line_byte_count) == -1) {}

	// Free memory and return
	free_heap(my_heap);
	close(my_socket);
	return success;
}
