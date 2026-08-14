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
 * @param  score The score used to determing the position of the element inside of the array.
 *
 * @return 0 on success. Errors: -1 invalid manager pointer. -2 exceed capacity limit.
 */
int biheap_push(struct biheap_manager *manager, unsigned int id, int score);

#endif // BIHEAP_H
