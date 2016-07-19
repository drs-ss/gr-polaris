/* -*- c++ -*- */
/* 
 * Copyright 2015 DRS.
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
/** \file polaris_src.h
 *  \brief Contains the polaris_src public object.
 *  
 *  Contains the polaris_src public class that swig sees.
 */

#ifndef INCLUDED_POLARIS_POLSRC_H
#define INCLUDED_POLARIS_POLSRC_H

#include <polaris/api.h>
#include <gnuradio/sync_block.h>

namespace gr
{
namespace polaris
{

/*!
 * \brief Recieves IQ data from Polaris.
 * \ingroup polaris
 *
 */
class POLARIS_API polaris_src : virtual public gr::sync_block
{
public:
    typedef boost::shared_ptr<polaris_src> sptr;

    /*!
     * \brief Return a shared_ptr to a new instance of polaris::POLSRC.
     *
     * To avoid accidental use of raw pointers, polaris::POLSRC's
     * constructor is in a private implementation
     * class. polaris::POLSRC::make is the public interface for
     * creating new instances.
     */
    static sptr make(std::string ip, 
                     int port, 
                     std::string streamip, 
                     std::string fibip, 
                     int num_outputs, 
                     bool i_op, 
                     int phys);

    virtual void update_preamp(bool pam, int stream) = 0;
    virtual void update_tuners(int tuner, int stream) = 0;
    virtual void update_freq(double freq, int stream) = 0;
    virtual void update_samprate(double sr) = 0;
    virtual void update_atten(double atten, int stream) = 0;
};

} // namespace polaris
} // namespace gr

#endif /* INCLUDED_POLARIS_POLSRC_H */

