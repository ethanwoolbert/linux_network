enum errors {
	success,
	incorrect_arg_num,
	create_heap_failed,
	allocation_failed,
	create_socket_failed,
	inet_aton_failed,
	connect_failed,
	socket_read_failed,
	socket_write_failed,
        could_not_open_file,
        fscanf_failed,
        too_few_files,
        setsockopt_failed,
        bind_failed,
        gethostname_failed,
        gethostbyname_failed,
        listen_failed,
        create_epoll_failed,
        epoll_ctl_failed,
        epoll_wait_failed,
        accept_failed,
        fputs_failed
};

struct line_val {
        int line_num;
        int line_length;
        int offset;
        char* text;
};

struct min_heap {
        int size;
        int length;
        struct line_val* heap[];
};

struct min_heap* create_heap(int size);

int parentIndex(int index);
int leftChildIndex(int index);
int rightChildIndex(int index);

void swap(struct min_heap* my_heap, int idx1, int idx2);

void bubble_up(struct min_heap* my_heap, int startIndex);
void bubble_down(struct min_heap* my_heap, int startIndex);

void insert(struct min_heap* my_heap, struct line_val* new_element);
struct line_val* extract_min(struct min_heap* my_heap);
