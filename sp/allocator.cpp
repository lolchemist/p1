#include <string.h>
#include "allocator.h"

Pointer::Pointer() : p_ptr { nullptr }
{}

Pointer::Pointer(void** block_field_ptr) : p_ptr{ block_field_ptr }
{}

void* Pointer::get() const
{
	if (p_ptr != nullptr)
	{
		return *p_ptr;
	}
	else
	{
		return nullptr;
	}
}

Allocator::Allocator(void *base, size_t N) : p_base{ (char*)base }, base_size{ N }, p_busy{ nullptr }, garbage{ nullptr }
{
	p_free.emplace(base, N);
}

std::pair<void*, size_t> Allocator::find_fittest_block(size_t N)
{
	size_t min_size = SIZE_MAX;
	void* ptr = nullptr;
	for (auto block : p_free)
	{
		if (block.second >= N && block.second < min_size)
		{
			min_size = block.second;
			ptr = block.first;
		}
	}

	return std::pair<void*, size_t>(ptr, min_size);
}

Pointer Allocator::alloc(size_t alloc_size) {
	auto fittest_block = find_fittest_block(alloc_size);
	void* ptr = fittest_block.first;
	size_t size = fittest_block.second;

	if (ptr != nullptr)
	{
		size_t rest_size = size - alloc_size;
		p_free.erase(ptr);
		if (rest_size > 0)
		{
			p_free.emplace(static_cast<char*>(ptr) + alloc_size, rest_size);
		}

		Blocks* block_ptr = new Blocks();
		block_ptr->ptr = ptr;
		block_ptr->size = alloc_size;
		block_ptr->p_next = nullptr;
		add_allocated_block(block_ptr);
		return Pointer(&(block_ptr->ptr));
	}
	else
	{
		throw AllocError(AllocErrorType::NoMemory, "Out of memory");
	}
}

void Allocator::add_allocated_block(Blocks* new_block_ptr)
{
	if (p_busy != nullptr)
	{
		Blocks* current_block_ptr = p_busy;
		while (current_block_ptr->p_next != nullptr &&
			current_block_ptr->p_next->ptr < new_block_ptr->ptr)
		{
			current_block_ptr = current_block_ptr->p_next;
		}
		new_block_ptr->p_next = current_block_ptr->p_next;
		current_block_ptr->p_next = new_block_ptr;
	}
	else
	{
		p_busy = new_block_ptr;
	}
}

void Allocator::realloc(Pointer &p_mem, size_t realloc_size) {
	free(p_mem);
	p_mem = alloc(realloc_size);
}

void Allocator::send_to_garbage(Blocks* block_ptr)
{
	Blocks* garbage_ptr = garbage;
	if (garbage_ptr == nullptr)
	{
		garbage_ptr = block_ptr;
	}
	else
	{
		while (garbage_ptr->p_next != nullptr)
		{
			garbage_ptr = garbage_ptr->p_next;
		}
		garbage_ptr->p_next = block_ptr;
	}
}

void Allocator::free(Pointer &p)
{
	Blocks* existing_block = lookup_allocated_block(p.get());
	if (existing_block == nullptr)
	{
		return;
	}

	size_t existing_block_size = existing_block->size;

	for (auto free_block : p_free)
	{
		void* next_byte_ptr = static_cast<char*>(existing_block->ptr) + existing_block_size;
		if (free_block.first == next_byte_ptr)
		{
			void* new_free_block_ptr = existing_block->ptr;
			size_t new_free_block_size = free_block.second + existing_block_size;
			p_free.erase(free_block.first);
			p_free.emplace(new_free_block_ptr, new_free_block_size);

			free_impl(existing_block);
			return;
		}
	}

	p_free.emplace(existing_block->ptr, existing_block_size);
	free_impl(existing_block);

}

void Allocator::free_impl(Blocks* block_ptr)
{
	block_ptr->ptr = nullptr;
	block_ptr->size = 0;
	exclude_block(block_ptr);
}

void Allocator::defrag() {
	size_t heap_size = 0;
	char* current_ptr = static_cast<char*>(p_base);
	for (Blocks* block_ptr = p_busy; block_ptr != nullptr; block_ptr = block_ptr->p_next)
	{
		void* data_ptr = block_ptr->ptr;
		size_t block_size = block_ptr->size;

		// Move data
		if (current_ptr != data_ptr)
		{
			memcpy(current_ptr, data_ptr, block_size);
		}		

		block_ptr->ptr = current_ptr;
		current_ptr += block_size;
		heap_size += block_size;
	}

	p_free.clear();
	if (heap_size < base_size)
	{
		p_free.emplace(
			static_cast<char*>(p_base) + heap_size,
			base_size - heap_size
		);
	}
}

Allocator::Blocks* Allocator::lookup_allocated_block(void* ptr)
{
	Blocks* curr_block = p_busy;
	while (curr_block != nullptr)
	{
		if (curr_block->ptr == ptr)
		{
			return curr_block;
		}
		curr_block = curr_block->p_next;
	}
	return nullptr;
}

void Allocator::exclude_block(Blocks* block_ptr)
{
	Blocks* prev_block = nullptr;
	Blocks* curr_block = p_busy;
	while (curr_block != nullptr)
	{
		if (curr_block == block_ptr)
		{
			if (prev_block != nullptr)
			{
				prev_block->p_next = curr_block->p_next;
			}
			else
			{
				p_busy = curr_block->p_next;
			}
			send_to_garbage(curr_block);
			return;
		}
		prev_block = curr_block;
		curr_block = curr_block->p_next;
	}
}

void Allocator::release_list(Blocks* head)
{
	Blocks* ptr = head;
	while (ptr != nullptr)
	{
		Blocks* current = ptr;
		ptr = ptr->p_next;
		if (current != nullptr)
		{
			delete current;
		}
	}
}

Allocator::~Allocator()
{
	release_list(p_busy);
	release_list(garbage);
}