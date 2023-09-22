#include <stdlib.h>
#include <stdio.h>
#include "min_heap.h"

// Create dynamically allocated heap with input size (free_heap is defined in server_lab3.c and client_lab3.c)
struct min_heap* create_heap(int size) {
	struct min_heap* my_heap = malloc(sizeof(struct min_heap) + size * sizeof(struct line_val*));
    my_heap->size = size;
	my_heap->length = 0;
    return my_heap;
}

// Helper function
int parentIndex(int index) {
	return (index - 1) / 2;
}

// Helper function
int leftChildIndex(int index) {
	return (index * 2) + 1;
}

// Helper function
int rightChildIndex(int index) {
	return (index * 2) + 2;
}

// Helper function
void swap(struct min_heap* my_heap, int idx1, int idx2) {
	struct line_val* temp = my_heap->heap[idx1];
	my_heap->heap[idx1] = my_heap->heap[idx2];
	my_heap->heap[idx2] = temp;
}

// Logic taken from Ethan's 247 lab 3: https://github.com/wustl-cse247-sp22/cse-247-502n-m3-code-edwoolbert
void bubble_up(struct min_heap* my_heap, int startIndex) {
	while (my_heap->heap[parentIndex(startIndex)]->line_num > my_heap->heap[startIndex]->line_num && parentIndex(startIndex) >= 0) {
		swap(my_heap, startIndex, parentIndex(startIndex));
		startIndex = parentIndex(startIndex);
	}
}

// Logic taken from Ethan's 247 lab 3: https://github.com/wustl-cse247-sp22/cse-247-502n-m3-code-edwoolbert
void bubble_down(struct min_heap* my_heap, int startIndex) {
	while (leftChildIndex(startIndex) < my_heap->length) {
	        int index = leftChildIndex(startIndex);

	        if (rightChildIndex(startIndex) < my_heap->length) {
	        	if (my_heap->heap[rightChildIndex(startIndex)]->line_num < my_heap->heap[leftChildIndex(startIndex)]->line_num) {
		            index = rightChildIndex(startIndex);
		        }
	        }
	        if (my_heap->heap[index]->line_num < my_heap->heap[startIndex]->line_num) {
	            	swap(my_heap, startIndex, index);
	            	startIndex = index;
	        }
	        else {
	        	startIndex = my_heap->length;
	        }
	}
}

// Insert new element into heap at bottom and bubble up
void insert(struct min_heap* my_heap, struct line_val* new_element) {
	my_heap->heap[my_heap->length] = new_element;
	my_heap->length++;
	bubble_up(my_heap, my_heap->length - 1);
}

// Remove minimum element from heap, replace with last element, and bubble down
struct line_val* extract_min(struct min_heap* my_heap) {
	struct line_val* min = my_heap->heap[0];
	my_heap->heap[0] = my_heap->heap[my_heap->length - 1];
	my_heap->heap[my_heap->length - 1] = NULL;
	my_heap->length--;
	bubble_down(my_heap, 0);		
	return min;
}
