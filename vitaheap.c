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

#include <vitasdk.h>
#include <stdlib.h>
#include <malloc.h>
#include "vitaheap.h"
#define SCE_KERNEL_MAX_MAIN_CDIALOG_MEM_SIZE 0x8C6000
#define VH_ALIGN(x, a) (((x) + ((a)-1)) & ~((a)-1))

static void *mempool_mspace[VHEAP_MEM_NUM_POOLS] = {NULL, NULL, NULL, NULL, NULL};
static SceUID mempool_blk[VHEAP_MEM_NUM_POOLS] = {0, 0, 0, 0, 0};
size_t mempool_sizes[VHEAP_MEM_NUM_POOLS] = {0, 0, 0, 0, 0};
static void *mempool_addr[VHEAP_MEM_NUM_POOLS] = {NULL, NULL, NULL, NULL, NULL};

static int vitaheap_inited = 0;
static int vitaheap_has_cached_mem = 0;

void vh_config(int use_cached_mem) {
	vitaheap_has_cached_mem = use_cached_mem;
}

void vh_init(size_t user_rw_threshold, size_t cdram_threshold, size_t phycont_threshold, size_t cdlg_threshold) {
	if (vitaheap_inited) {
		sceClibPrintf("Suppressed an attempt at initing vitaheap when already initialized\n");
		return;
	}
	
	SceKernelFreeMemorySizeInfo info;
	info.size = sizeof(SceKernelFreeMemorySizeInfo);
	sceKernelGetFreeMemorySize(&info);
	
	mempool_sizes[VHEAP_MEM_USER_RW] = info.size_user > user_rw_threshold ? info.size_user - user_rw_threshold : 0;
	mempool_sizes[VHEAP_MEM_CDRAM] = info.size_cdram > user_rw_threshold ? info.size_user - user_rw_threshold : 0;
	mempool_sizes[VHEAP_MEM_PHYCONT] = info.size_phycont > user_rw_threshold ? info.size_user - user_rw_threshold : 0;
	mempool_sizes[VHEAP_MEM_CDLG] = SCE_KERNEL_MAX_MAIN_CDIALOG_MEM_SIZE > cdlg_threshold ? SCE_KERNEL_MAX_MAIN_CDIALOG_MEM_SIZE - cdlg_threshold : 0;
	
	if (!vitaheap_has_cached_mem && mempool_sizes[VHEAP_MEM_USER_RW] > 0xC800000) // Vita has a smaller address mapping range for uncached mem
		mempool_sizes[VHEAP_MEM_USER_RW] = 0xC800000;
		
	mempool_sizes[VHEAP_MEM_USER_RW] = VH_ALIGN(mempool_sizes[VHEAP_MEM_USER_RW], 4 * 1024);
	mempool_sizes[VHEAP_MEM_CDRAM] = VH_ALIGN(mempool_sizes[VHEAP_MEM_CDRAM], 256* 1024);
	mempool_sizes[VHEAP_MEM_PHYCONT] = VH_ALIGN(mempool_sizes[VHEAP_MEM_PHYCONT], 1024 * 1024);
	mempool_sizes[VHEAP_MEM_CDLG] = VH_ALIGN(mempool_sizes[VHEAP_MEM_CDLG], 4 * 1024);
	
	if (mempool_sizes[VHEAP_MEM_CDRAM])
		mempool_blk[VHEAP_MEM_CDRAM] = sceKernelAllocMemBlock("CDRam vitaheap", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW, mempool_sizes[VHEAP_MEM_CDRAM], NULL);
	if (vitaheap_has_cached_mem) {
		if (mempool_sizes[VHEAP_MEM_USER_RW])
			mempool_blk[VHEAP_MEM_USER_RW] = sceKernelAllocMemBlock("User RW vitaheap", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, mempool_sizes[VHEAP_MEM_USER_RW], NULL);
		if (mempool_sizes[VHEAP_MEM_PHYCONT])
			mempool_blk[VHEAP_MEM_PHYCONT] = sceKernelAllocMemBlock("PhyCont vitaheap", SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_RW, mempool_sizes[VHEAP_MEM_PHYCONT], NULL);
		if (mempool_sizes[VHEAP_MEM_CDLG])
			mempool_blk[VHEAP_MEM_CDLG] = sceKernelAllocMemBlock("CDLG vitaheap", SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_CDIALOG_RW, mempool_sizes[VHEAP_MEM_CDLG], NULL);
	} else {
		if (mempool_sizes[VHEAP_MEM_USER_RW])
			mempool_blk[VHEAP_MEM_USER_RW] = sceKernelAllocMemBlock("User RW vitaheap", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW_UNCACHE, mempool_sizes[VHEAP_MEM_USER_RW], NULL);
		if (mempool_sizes[VHEAP_MEM_PHYCONT])
			mempool_blk[VHEAP_MEM_PHYCONT] = sceKernelAllocMemBlock("PhyCont vitaheap", SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_PHYCONT_NC_RW, mempool_sizes[VHEAP_MEM_PHYCONT], NULL);
		if (mempool_sizes[VHEAP_MEM_CDLG])
			mempool_blk[VHEAP_MEM_CDLG] = sceKernelAllocMemBlock("CDLG vitaheap", SCE_KERNEL_MEMBLOCK_TYPE_USER_MAIN_CDIALOG_NC_RW, mempool_sizes[VHEAP_MEM_CDLG], NULL);
	}
	
	for (int i = 0; i < VHEAP_MEM_NEWLIB; i++) {
		if (mempool_sizes[i]) {
			mempool_addr[i] = NULL;
			sceKernelGetMemBlockBase(mempool_blk[i], &mempool_addr[i]);

			if (mempool_addr[i]) {
				sceGxmMapMemory(mempool_addr[i], mempool_sizes[i], SCE_GXM_MEMORY_ATTRIB_RW);
				mempool_mspace[i] = sceClibMspaceCreate(mempool_addr[i], mempool_sizes[i]);
			}
		}
			
	}
	
	void *dummy = malloc(1);
	free(dummy);

	SceKernelMemBlockInfo info2;
	info2.size = sizeof(SceKernelMemBlockInfo);
	sceKernelGetMemBlockInfoByAddr(dummy, &info2);
	sceGxmMapMemory(info2.mappedBase, info2.mappedSize, SCE_GXM_MEMORY_ATTRIB_RW);

	mempool_sizes[VHEAP_MEM_NEWLIB] = info2.mappedSize;
	mempool_addr[VHEAP_MEM_NEWLIB] = info2.mappedBase;

	vitaheap_inited = 1;
}

vh_mem_type vh_mem_get_type_by_addr(void *addr) {
	for (int i = 0; i < VHEAP_MEM_NUM_POOLS; i++) {
		if (addr >= mempool_addr[i] && (addr < mempool_addr[i] + mempool_sizes[i]))
			return i;
	}
	return -1;
}

size_t vh_malloc_usable_size(void *ptr) {
	vh_mem_type type = vh_mem_get_type_by_addr(ptr);
	if (type == VHEAP_MEM_NEWLIB)
		return malloc_usable_size(ptr);
	else
		return sceClibMspaceMallocUsableSize(ptr);
}

void *vh_malloc(size_t size) {
	void *r = malloc(size);
	if (r)
		return r;
	
	r = sceClibMspaceMalloc(mempool_mspace[VHEAP_MEM_USER_RW], size);
	if (r)
		return r;
	
	r = sceClibMspaceMalloc(mempool_mspace[VHEAP_MEM_PHYCONT], size);
	if (r)
		return r;
	
	r = sceClibMspaceMalloc(mempool_mspace[VHEAP_MEM_CDLG], size);
	if (r)
		return r;
	
	r = sceClibMspaceMalloc(mempool_mspace[VHEAP_MEM_CDRAM], size);
	return r;
}

void *vh_malloc_for_gpu(size_t size) {
	void *r = sceClibMspaceMalloc(mempool_mspace[VHEAP_MEM_CDRAM], size);
	if (r)
		return r;
	
	r = sceClibMspaceMalloc(mempool_mspace[VHEAP_MEM_PHYCONT], size);
	if (r)
		return r;
	
	r = sceClibMspaceMalloc(mempool_mspace[VHEAP_MEM_USER_RW], size);
	if (r)
		return r;
	
	r = sceClibMspaceMalloc(mempool_mspace[VHEAP_MEM_CDLG], size);
	if (r)
		return r;
	
	r = malloc(size);
	return r;
}

void *vh_calloc(size_t num, size_t size) {
	void *r = calloc(num, size);
	if (r)
		return r;
	
	r = sceClibMspaceCalloc(mempool_mspace[VHEAP_MEM_USER_RW], num, size);
	if (r)
		return r;
	
	r = sceClibMspaceCalloc(mempool_mspace[VHEAP_MEM_PHYCONT], num, size);
	if (r)
		return r;
	
	r = sceClibMspaceCalloc(mempool_mspace[VHEAP_MEM_CDLG], num, size);
	if (r)
		return r;
	
	r = sceClibMspaceCalloc(mempool_mspace[VHEAP_MEM_CDRAM], num, size);
	return r;
}

void *vh_calloc_for_gpu(size_t num, size_t size) {
	void *r = sceClibMspaceCalloc(mempool_mspace[VHEAP_MEM_CDRAM], num, size);
	if (r)
		return r;
	
	r = sceClibMspaceCalloc(mempool_mspace[VHEAP_MEM_PHYCONT], num, size);
	if (r)
		return r;
	
	r = sceClibMspaceCalloc(mempool_mspace[VHEAP_MEM_USER_RW], num, size);
	if (r)
		return r;
	
	r = sceClibMspaceCalloc(mempool_mspace[VHEAP_MEM_CDLG], num, size);
	if (r)
		return r;
	
	r = calloc(num, size);
	return r;
}

void *vh_memalign(size_t alignment, size_t size) {
	void *r = memalign(alignment, size);
	if (r)
		return r;
	
	r = sceClibMspaceMemalign(mempool_mspace[VHEAP_MEM_USER_RW], alignment, size);
	if (r)
		return r;
	
	r = sceClibMspaceMemalign(mempool_mspace[VHEAP_MEM_PHYCONT], alignment, size);
	if (r)
		return r;
	
	r = sceClibMspaceMemalign(mempool_mspace[VHEAP_MEM_CDLG], alignment, size);
	if (r)
		return r;
	
	r = sceClibMspaceMemalign(mempool_mspace[VHEAP_MEM_CDRAM], alignment, size);
	return r;
}

void *vh_memalign_for_gpu(size_t alignment, size_t size) {
	void *r = sceClibMspaceMemalign(mempool_mspace[VHEAP_MEM_CDRAM], alignment, size);
	if (r)
		return r;
	
	r = sceClibMspaceMemalign(mempool_mspace[VHEAP_MEM_PHYCONT], alignment, size);
	if (r)
		return r;
	
	r = sceClibMspaceMemalign(mempool_mspace[VHEAP_MEM_USER_RW], alignment, size);
	if (r)
		return r;
	
	r = sceClibMspaceMemalign(mempool_mspace[VHEAP_MEM_CDLG], alignment, size);
	if (r)
		return r;
	
	r = memalign(alignment, size);
	return r;
}

void *vh_realloc(void *ptr, size_t size) {
	void *r;
	vh_mem_type type = vh_mem_get_type_by_addr(ptr);
	if (type == VHEAP_MEM_NEWLIB) {
		r = realloc(ptr, size);
	} else {
		r = sceClibMspaceRealloc(mempool_mspace[type], ptr, size);
	}
	
	if (!r) {
		r = vh_malloc(size);
		if (r) {
			size_t old_size = vh_malloc_usable_size(ptr);
			sceClibMemcpy(r, ptr, old_size);
			vh_free(ptr);
		}
	}
	
	return r;
}

void vh_free(void *ptr) {
	vh_mem_type type = vh_mem_get_type_by_addr(ptr);
	if (type == VHEAP_MEM_NEWLIB)
		free(ptr);
	else
		sceClibMspaceFree(mempool_mspace[type], ptr);
}

size_t vh_mem_get_free_space(vh_mem_type type) {
	if (type == VHEAP_MEM_NEWLIB) { // TODO
		return 0;
	} else if (type < VHEAP_MEM_NEWLIB) {
		SceClibMspaceStats stats;
		sceClibMspaceMallocStats(mempool_mspace[type], &stats);
		return stats.capacity - stats.current_in_use;
	} else {
		size_t r;
		for (int i = 0; i < VHEAP_MEM_NUM_POOLS; i++) {
			r += vh_mem_get_free_space(i);
		}
	}	
}

size_t vh_mem_get_total_space(vh_mem_type type) {
	if (type == VHEAP_MEM_NUM_POOLS) {
		size_t size = 0;
		for (int i = 0; i < VHEAP_MEM_NUM_POOLS; i++) {
			size += vh_mem_get_total_space(i);
		}
		return size;
	} else {
		return mempool_sizes[type];
	}
}
