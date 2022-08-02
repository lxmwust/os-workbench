#include <common.h>

#define KALLOC_CHECK 1
#define HB_MAX (1 << 16)
#define HB_MIN (1 << 4)
#define HB_HEAD_SIZE (2 * HB_MAX / HB_MIN + 1)
#define HB_CONT_SIZE HB_MAX
#define HB_WHOL_SIZE (HB_HEAD_SIZE + HB_CONT_SIZE)

// use HB to represent heap block
typedef struct heap_block {
	char *head;		// head infomation
	void *cont;		// content address
	char stat;		// 0: one HB
								// 1: start of continuous HB
								// 2: in continuous HB
								// 3: end of continuous HB
} heap_block;
static size_t HB_number;		// number of HB
static void  *HB_head_base;	// basic addr of HB head
static void  *HB_cont_base; // basic addr of HB content

static size_t lowbit(size_t x) {
	return x & -x;
}

static size_t highbit(size_t x) {
	size_t ret = lowbit(x);
	while (x -= lowbit(x)) {
		ret = lowbit(x);
	}
	return ret;
}

// head idx to size in HB
static size_t hb_idx2size(size_t idx) {
	size_t size = HB_MAX;
	while (idx >>= 1) {
		size >>= 1;
	}
	return size;
}

// head idx to address in HB
static void *hb_idx2addr(heap_block *hb, size_t idx) {
	size_t start = highbit(idx);
	size_t size = hb_idx2size(idx);
	return hb->cont + (idx - start) * size;
}

// init one HB
static void hb_init(heap_block *hb, void *start) {
	hb->head = start;
	hb->cont = start + HB_HEAD_SIZE;
	hb->stat = 0;

	memset(hb->head, 0, HB_HEAD_SIZE);
}

// size less than heap size
//			=> return 0
// else => return 1
static size_t hb_check_size(size_t size) {
	if (size >= HB_number * HB_WHOL_SIZE) {
		return 1;
	}
	return 0;
}

// addr in heap head range
//			=> return 0
// else => return 1
static size_t hb_check_cont_addr(void *addr) {
	if (addr <  HB_cont_base ||
			addr >= HB_cont_base + HB_number * HB_WHOL_SIZE) {
		return 1;
	}
	return 0;
}

// addr in heap cont range
//			=> return 0
// else => return 1
static size_t hb_check_head_addr(void *addr) {
	if (addr <  HB_head_base ||
			addr >= HB_cont_base) {
		return 1;
	}
	return 0;
}

// idx in HB head
//			=> return 0
// else => return 1
static size_t hb_check_idx(size_t idx) {
	if (idx < HB_HEAD_SIZE) {
		return 0;
	}
	return 1;
}

// roundup size to 2^k
static size_t hb_roundup(size_t size) {
	size_t new_size = 1;

	if (size <= 16) {
		new_size = 16;
	}
	else {
		while (new_size < size) {
			new_size <<= 1;
		}
	}
	panic_on(new_size < size, "size fault");

	return new_size;
}

// pushup segment tree state
static void hb_pushup(char *head, size_t idx) {
	if (hb_idx2size(idx) == HB_MIN) {
		return;
		panic_on(1, "shouldn't pushup");
	}
	size_t le = idx << 1, ri = le + 1;

	// if both free
	if (head[le] == 0 && head[ri] == 0) {
		head[idx] = 0;
	}
	// if both full
	else if (head[le] & 1 && head[ri] & 1) {
		head[idx] = 3;
	}
	// if both not full
	else {
		head[idx] = 2;
	}
}

// 0 free
// 1 occupied
// 2 splited and not full
// 3 splited and full
// sucess: return index in head
// fail:   return 0
static size_t hb_find(char *head, size_t idx, size_t block_size, size_t size) {
	// if size not enough
	if (head[idx] == 1 || head[idx] == 3) {
		return 0;
	}

	// find successfully
	if (head[idx] == 0 && block_size == size) {
		head[idx] = 1;
		return idx;
	}

	// if size == HB_MIN, find failed
	if (hb_idx2size(idx) == HB_MIN) {
		return 0;
	}

	// find recursively
	size_t le = idx << 1, ri = le + 1;
	if ((le = hb_find(head, le, block_size >> 1, size))) {
		// if left success
		hb_pushup(head, idx);
		return le;
	}
	if ((ri = hb_find(head, ri, block_size >> 1, size))) {
		// if right success
		hb_pushup(head, idx);
		return ri;
	}

	// find recursively failed
	return 0;
}

// free HB
static size_t hb_free(heap_block *hb, size_t idx, size_t size, void *addr) {
	// check if idx valid
	if (hb_check_idx(idx)) {
		// not valid means not find
		return 1;
	}

	// free successfully
	if (hb->head[idx] == 1 && hb_idx2addr(hb, idx) == addr) {
#ifdef KALLOC_CHECK
		printf("FREE:  idx: %d size: %d address: %p\n",
			idx,
			hb_idx2size(idx),
			hb_idx2addr(hb, idx)
		);
#endif
		hb->head[idx] = 0;
		hb_pushup(hb->head, idx);
		return 0;
	}
	// free recursively
	else {
		if (!hb_free(hb, idx << 1, size >> 1, addr)) {
			hb_pushup(hb->head, idx);
			return 0;
		}
		if (!hb_free(hb, (idx << 1) + 1, size >> 1, addr)) {
			hb_pushup(hb->head, idx);
			return 0;
		}
	}

	// free failed
	return 1;
}

// initialize all of HB
static void kinit() {
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);

	// initialize global variable
	HB_number    = pmsize / (HB_WHOL_SIZE + sizeof(heap_block));
	HB_head_base = heap.start;
	HB_cont_base = heap.start + HB_number * sizeof(heap_block);
	printf("kinit:\nHB_number: %ld\nHB_head_base: %p\nHB_cont_base: %p\n",
			HB_number, HB_head_base, HB_cont_base);
	
	// initialize each HB
	for (size_t i = 0; i < HB_number; i++) {
		hb_init(
			(heap_block *)(HB_head_base + i * sizeof(heap_block)),
			HB_cont_base + i * HB_WHOL_SIZE
		);
	}
}

// allocate memory
static void *kalloc(size_t size) {
	size = hb_roundup(size);

	if (!hb_check_size(size)) {
		// allocate one HB
		if (size <= HB_MAX) {
			heap_block *hb;
			size_t hb_idx;

			for (size_t i = 0; i < HB_number; i++) {
				hb = (heap_block *)(HB_head_base + i * sizeof(heap_block));
				hb_idx = hb_find(hb->head, 1, HB_MAX, size);
				if (hb_idx) {
#ifdef KALLOC_CHECK
					printf("ALLOC: idx: %d size: %d address: %p\n",
						hb_idx,
						hb_idx2size(hb_idx),
						hb_idx2addr(hb, hb_idx)
					);
#endif

					panic_on(
						hb_idx2size(hb_idx) != size,
						"size not equal"
					);

					return hb_idx2addr(hb, hb_idx);
				}
			}
		}
		// allocate one more HB
		else {
			heap_block *hb_start, *hb_next;

			for (size_t i = 0, j = 0; i < HB_number; i++) {
				j = i;
				hb_start = (heap_block *)(HB_head_base + i * sizeof(heap_block));
				hb_next = hb_start;

				// allocate continuous HB
				while (hb_next->head[1] == 0 && (j - i + 1) * HB_MAX < size) {
					j++;
					hb_next = (heap_block *)(HB_head_base + j * sizeof(heap_block));	
					panic_on(hb_check_head_addr(hb_next), "invalid address");
				}

				// if allocate success
				if ((j - i + 1) * HB_MAX >= size) {
					hb_next = hb_start;

					// mark
					for (size_t k = i; k <= j; k++) {
						hb_next = (heap_block *)(HB_head_base + k * sizeof(heap_block));	

						// occupied
						hb_next->head[1] = 1;
						// has next
						if (k == i) {
							hb_next->stat = 1;
						}
						else if (k == j) {
							hb_next->stat = 3;
						}
						else {
							hb_next->stat = 2;
						}
#ifdef KALLOC_CHECK
						printf("ALLOC: stat: %d address: %p\n",
							hb_next->stat,
							hb_idx2addr(hb_next, 1)
						);
#endif
					}
					return hb_start->cont;
				}
				// else continue;
			}
		}
	}

  return NULL;
}

// free memory
static void kfree(void *ptr) {
	panic_on(hb_check_cont_addr(ptr), "free invalid addr");

	uintptr_t addr = (uintptr_t)ptr - (uintptr_t)HB_cont_base;
	// addr should be times of HB_MIN
	panic_on((addr % HB_WHOL_SIZE - HB_HEAD_SIZE) % HB_MIN != 0, "addr should be times of HB_MIN");

	heap_block *hb = HB_head_base + addr / HB_WHOL_SIZE * sizeof(heap_block);
	if (hb->stat == 1) {
		for (int k = 1; k <= 3; k = hb->stat) {
#ifdef KALLOC_CHECK
			printf("FREE:  stat: %d address: %p\n",
				hb->stat,
				hb_idx2addr(hb, 1)
			);
#endif
			hb->head[1] = 0;
			hb->stat = 0;
			// pointer move 1
			// !!! not "hb += sizeof(heap_block);"
			hb += 1;

			if (k == 3) break;
		}
	}
	else {
		panic_on(hb_free(hb, 1, HB_MAX, ptr), "free addr error");
	}
}

static void pmm_init() {
  uintptr_t pmsize = ((uintptr_t)heap.end - (uintptr_t)heap.start);
  printf("Got %d MiB heap: [%p, %p)\n", pmsize >> 20, heap.start, heap.end);

	kinit();
}

MODULE_DEF(pmm) = {
  .init  = pmm_init,
  .alloc = kalloc,
  .free  = kfree,
};
