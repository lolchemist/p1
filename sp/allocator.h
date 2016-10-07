#pragma once

#include <string>
#include <stdexcept>
#include <vector>
#include <map>

enum class AllocErrorType {
	InvalidFree,
	NoMemory,
};

class AllocError : std::runtime_error {
private:
	AllocErrorType type;

public:
	AllocError(AllocErrorType _type, std::string message) :
		runtime_error(message),
		type(_type)
	{}

	AllocErrorType getType() const { return type; }
};

class Allocator;

class Pointer {
public:
	void *get() const;
	Pointer();
	Pointer(void** block_field_ptr);
private:
	void** p_ptr;
};

class Allocator {
public:
	Allocator(void *base, size_t N);
	Pointer alloc(size_t N);
	void realloc(Pointer &p, size_t N);
	void free(Pointer &p);
	void defrag();
	std::string dump() { return ""; }
	~Allocator();

private:
	struct Blocks {
		Blocks *p_next;
		void* ptr;
		size_t size;
	};
	char *p_base;
	size_t base_size;
	Blocks *p_busy;
	Blocks *garbage;
	std::map<void*, size_t> p_free;

	Blocks* lookup_allocated_block(void* ptr);
	std::pair<void*, size_t> find_fittest_block(size_t N);
	void add_allocated_block(Blocks* new_block_ptr);
	void send_to_garbage(Blocks* block_ptr);
	void exclude_block(Blocks* block_ptr);
	void free_impl(Blocks* block_ptr);
	void release_list(Blocks* block_ptr);
};
