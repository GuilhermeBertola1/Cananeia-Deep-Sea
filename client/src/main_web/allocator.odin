package main_web

import "core:c"
import "core:mem"

// O Odin no WASM normalmente cresce a memória sozinho, mas aqui quem manda
// na memória é o emscripten (raylib usa malloc). Então pedimos tudo pro malloc dele.
@(default_calling_convention = "c")
foreign {
	aligned_alloc :: proc(alignment, size: c.size_t) -> rawptr ---
	free :: proc(ptr: rawptr) ---
	malloc_usable_size :: proc(ptr: rawptr) -> c.size_t ---
}

emscripten_allocator :: proc() -> mem.Allocator {
	return {procedure = emscripten_allocator_proc}
}

@(private = "file")
emscripten_allocator_proc :: proc(
	_: rawptr,
	mode: mem.Allocator_Mode,
	size, alignment: int,
	old_memory: rawptr,
	old_size: int,
	_ := #caller_location,
) -> ([]byte, mem.Allocator_Error) {
	alloc :: proc(size, alignment: int, zero: bool) -> ([]byte, mem.Allocator_Error) {
		align := max(alignment, size_of(rawptr))
		rounded := (size + align - 1) / align * align // aligned_alloc pede múltiplo do alinhamento
		ptr := aligned_alloc(c.size_t(align), c.size_t(max(rounded, align)))
		if ptr == nil do return nil, .Out_Of_Memory
		if zero do mem.zero(ptr, size)
		return ([^]byte)(ptr)[:size], nil
	}

	switch mode {
	case .Alloc:
		return alloc(size, alignment, true)
	case .Alloc_Non_Zeroed:
		return alloc(size, alignment, false)
	case .Free:
		free(old_memory)
		return nil, nil
	case .Resize, .Resize_Non_Zeroed:
		if old_memory == nil do return alloc(size, alignment, mode == .Resize)
		data, err := alloc(size, alignment, false)
		if err != nil do return nil, err
		copy(data, ([^]byte)(old_memory)[:min(old_size, size)])
		if mode == .Resize && size > old_size do mem.zero(&data[old_size], size - old_size)
		free(old_memory)
		return data, nil
	case .Query_Features:
		set := (^mem.Allocator_Mode_Set)(old_memory)
		if set != nil do set^ = {.Alloc, .Alloc_Non_Zeroed, .Free, .Resize, .Resize_Non_Zeroed, .Query_Features}
		return nil, nil
	case .Free_All, .Query_Info:
		return nil, .Mode_Not_Implemented
	}
	return nil, .Mode_Not_Implemented
}
