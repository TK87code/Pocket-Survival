#ifndef BIHEAP_H
#define BIHEAP_H

#include <stdint.h> // intxx_t
#include <stddef.h> // size_t

/*
 * == Binary Heap Optimization Configuration ==
 * If this macro is enabled, the biheap node structure will be compressed to 
 * 4 bytes. If disabled, the size will be 8 bytes (default). 
 */
#ifdef BIHEAP_OPTIMIZE_16BIT
	typedef uint16_t biheap_id_t;
	typedef int16_t biheap_score_t;
#else
	typedef uint32_t biheap_id_t;
	typedef int32_t biheap_score_t;
#endif // BIHEAP_OPTIMIZE_16BIT

enum biheap_mode {
	MAX_HEAP,
	MIN_HEAP,
};

struct biheap_node { // 4 bytes in 16bit mode, 8 bytes in 32bit mode.
	biheap_id_t id;
	biheap_score_t score;
};

struct biheap_manager { // 28 + 4 bytes
	struct biheap_node *buffer;	// A pointer to the biheap_node array. 
	size_t n;			// Current number of elements.
	size_t capacity; 		// Capacity (Max number of elements).
	enum biheap_mode mode;		// Indicates whether this is a max heap or min heap.
};

/**
 * @brief Initializes the biheap_manager structure.
 *
 * @param  manager A pointer to biheap_manager structure.
 * @param  buffer A pointer to the head of biheap_node buffer.
 * @param  capacity Capacity of the binary heap.
 * @param  mode Specifies whether to operate as a max heap or min heap.
 *
 * @return 0 on success. Errors: -1 invalid pointer to manager. -2 invalid pointer to buffer.
 */
int biheap_init(struct biheap_manager *manager, struct biheap_node *buffer, size_t capacity, enum biheap_mode mode);

/**
 * @brief Push new element to the binary heap. 
 *
 * @param  manager A pointer to biheap_manager struct
 * @param  id ID of the element.
 * @param  score The score used to determining the position of the element inside of the array.
 *
 * @return 0 on success. Errors: -1 invalid manager pointer. -2 exceed capacity limit.
 */
int biheap_push(struct biheap_manager *manager, unsigned int id, int score);

/**
 * @brief  Pops the highest score element from the binary heap.
 *
 * @param  manager A pointer to the biheap_manager structure.
 * @param  out_id A pointer to store the popped element's ID. Can be NULL.
 * @param  out_score A pointer to store the popped elements score. Can be NULL.
 *
 * @return 0 on success. Errors: -1 invalid manager pointer. -2 heap is empty.
 */
int biheap_pop(struct biheap_manager *manager, unsigned int *out_id, int *out_score);

/**
 * @brief  Updates the score of an existing element in the heap.
 *
 * @param  manager A pointer to the biheap_manager structure.
 * @param  id The ID of the element to update.
 * @param  new_score The new score to assign to the element.
 *
 * @return 0 on success. Errors: -1 invalid manager. -2 heap is empty. -3 element not found.
 */
int biheap_update(struct biheap_manager *manager, unsigned int id, int new_score);

#endif // BIHEAP_H
