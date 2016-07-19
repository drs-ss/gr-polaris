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

/** @file aligned_buffer.h
 *  @brief Contains the aligned_buffer object.
 *  
 *  Contains the aligned_buffer class.
 */

#include <boost/shared_array.hpp>
#include <boost/noncopyable.hpp>
#include <vector>

class aligned_buffer : boost::noncopyable
{
public:
    typedef boost::shared_ptr<aligned_buffer> sptr;
    typedef void *void_ptr;

    virtual ~aligned_buffer(void) = 0;

    /**
     * Create a new aligned buffer, with num_buffs 
     * buffers, each of size buff_size. 
     * 
     * 
     * @param num_buffs - Number of buffers to create
     * @param buff_size - Size of each buffer
     * @param alignment - Where to align the buffers
     * 
     * @return sptr 
     */
    static sptr make(
        const size_t num_buffs,
        const size_t buff_size,
        const size_t alignment = 16
        );

    /**
     * Get the void* pointer for a buffer at this index
     * 
     * 
     * @param index - Index of the requested buffer
     * 
     * @return void_ptr 
     */
    virtual void_ptr at(const size_t index) const = 0;

    /**
     * Get how many buffers are in this single AlignedBuffer
     * 
     * 
     * @return size_t
     */
    virtual size_t size(void) const = 0;
};
