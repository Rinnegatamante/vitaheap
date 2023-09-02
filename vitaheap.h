/*
 * This file is part of vitaheap
 * Copyright 2023 Rinnegatamante
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
 
#ifndef _VITAHEAP_H_
#define _VITAHEAP_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	VHEAP_MEM_USER_RW,
	VHEAP_MEM_CDRAM,
	VHEAP_MEM_PHYCONT,
	VHEAP_MEM_CDLG,
	VHEAP_MEM_NEWLIB,
	VHEAP_MEM_NUM_POOLS
} vh_mem_type;

// Init/Config
void vh_config(int use_cached_mem);
void vh_init(size_t user_rw_threshold, size_t cdram_threshold, size_t phycont_threshold, size_t cdlg_threshold);

// Misc
vh_mem_type vh_mem_get_type_by_addr(void *addr);
size_t vh_mem_get_free_space(vh_mem_type type);
size_t vh_mem_get_total_space(vh_mem_type type);

// Allocators
size_t vh_malloc_usable_size(void *ptr);
void *vh_malloc(size_t size);
void *vh_malloc_for_gpu(size_t size);
void *vh_calloc(size_t num, size_t size);
void *vh_calloc_for_gpu(size_t num, size_t size);
void *vh_memalign(size_t alignment, size_t size);
void *vh_memalign_for_gpu(size_t alignment, size_t size);
void *vh_realloc(void *ptr, size_t size);
void vh_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif