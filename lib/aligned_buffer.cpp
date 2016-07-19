/* 
 * Copyright 2016 DRS.
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */
 // Original source taken from https://github.com/EttusResearch/uhd

#include "aligned_buffer.h"

class aligned_buffer_impl : public aligned_buffer
{
public:
    aligned_buffer_impl(
        const std::vector<void_ptr> &ptrs,
        boost::shared_array<char> mem
        ) : pointers(ptrs), memory(mem)
    {
    }

    void_ptr at(const size_t index) const;

    size_t size(void) const;

private:
    std::vector<void_ptr> pointers;
    boost::shared_array<char> memory;
};

aligned_buffer::~aligned_buffer()
{
}

static size_t 
pad_buffer(const size_t bytes, const size_t alignment)
{
    return bytes + (alignment - bytes) % alignment;
}

aligned_buffer::sptr 
aligned_buffer::make(
    const size_t num_buffs,
    const size_t buff_size,
    const size_t alignment
    )
{
    // Pad the buffer start location
    const size_t padded_buff_size = 
        pad_buffer(buff_size, alignment);

    boost::shared_array<char> mem(
        new char[padded_buff_size * num_buffs + alignment - 1]);

    const size_t mem_start = 
        pad_buffer(size_t(mem.get()), alignment);
    std::vector<void_ptr> ptrs(num_buffs);
    // Create our actual buffers
    for (size_t i = 0; i < num_buffs; i++) {
        ptrs[i] = void_ptr(mem_start + padded_buff_size * i);
    }

    return sptr(new aligned_buffer_impl(ptrs, mem));
}

aligned_buffer::void_ptr 
aligned_buffer_impl::at(const size_t index) const
{
    return pointers.at(index);
}

size_t 
aligned_buffer_impl::size(void) const
{
    return pointers.size();
}
