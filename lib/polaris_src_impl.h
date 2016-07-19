/* -*- c++ -*- */
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

/** @file polaris_src_impl.h
 *  @brief Contains the polaris_src_impl object.
 *  
 *  Contains the polaris_src_impl class which holds all of the
 *  frontend logic and radio interface.
 */

#ifndef INCLUDED_POLARIS_POLSRC_IMPL_H
#define INCLUDED_POLARIS_POLSRC_IMPL_H

#include <polaris/polaris_src.h>

// Network stuff
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string>
#include <string.h>
#include <stdlib.h>

#include <boost/thread.hpp>
#include <boost/scoped_array.hpp>
#include "complex_manager.h"

#define MAX_OUTPUTS 4
#define TCP_PORT 8081
#define DEFAULT_SAMP_RATE 100000
#define DEFAULT_FREQ 100000000
#define DISABLE_OUTPUT_MNE "SYN1"
#define ENABLE_OUTPUT_MNE "SYN0"
#define MIN_FREQ_MHZ 2
#define MAX_FREQ_MHZ 6200
#define MHZ_SCALE 1000000
#define MIN_ATTEN 0
#define MAX_ATTEN 46
#define MIN_SAMP_RATE_MHZ 0.000977f
#define MAX_SAMP_RATE_MHZ 128.0f

/** 
 * @def FRQ_MNE 
 * @brief Mnemonic command to change the radio's frequency.
 *  
 * FRQ_MNE prepares a formatted string for a stringstream object
 * to create. This mnemonic will tune both the tuner and ddc to 
 * get the selected frequency. 
*/
#define FRQ_MNE(tuner,ddc,frq)  "FRQ" << tuner << "," << ddc \
                                << "," << frq << ";" 

/** 
 * @def SPR_MNE 
 * @brief Mnemonic command to change the radio's sample rate.
 *  
 * SPR_MNE prepares a formatted string for a stringstream object
 * to create. This mnemonic will change the sample rate for the 
 * associated tuner/ddc combo. 
*/
#define SPR_MNE(tuner,ddc,spr)  "SPR" << tuner << "," << ddc \
                                << "," << spr << ";"

/** 
 * @def STE_MNE 
 * @brief Mnemonic command to enable data streams.
 *  
 * STE_MNE prepares a formatted string for a stringstream object
 * to create. This mnemonic will enable the Vita49 data stream 
 * for the associated tuner/ddc. 
*/
#define STE_MNE(tuner,ddc,ste)  "STE" << tuner << "," << ddc \
                                << "," << ste << ";"

/** 
 * @def ATN_MNE 
 * @brief Mnemonic command to change the radio's attenuation.
 *  
 * ATN_MNE prepares a formatted string for a stringstream object
 * to create. This mnemonic will change the attenuation level 
 * for the specified tuner. 
*/
#define ATN_MNE(tuner,atten)    "RCH" << tuner << ";ATN" << \
                                atten << ";RCH0;"

/** 
 * @def PAM_MNE 
 * @brief Mnemonic command to enabled the radio's preamp.
 *  
 * PAM_MNE prepares a formatted string for a stringstream object
 * to create. This mnemonic will turn on or off the preamp for 
 * the specified tuner. 
*/
#define PAM_MNE(tuner,pam)      "RCH" << tuner << ";PAM" << \
                                pam << ";RCH0;"

/** 
 * @def STO_MNE 
 * @brief Mnemonic command to change the 10 gig port for 
 *        streaming.
 *  
 * STO_MNE prepares a formatted string for a stringstream object
 * to create. This mnemonic will allow you to select which 10 
 * gig port the tuner/ddc combo will stream out. 
*/
#define STO_MNE(tuner,ddc,port) "STO" << tuner << "," << ddc \
                                << "," << port << ";"

/** 
 * @def CFG_MNE 
 * @brief Mnemonic command to enable configuration mode.
 *  
 * CFG_MNE prepares a formatted string for a stringstream object
 * to create. This mnemonic can enable/disable configuration 
 * mode for the radio. 
*/
#define CFG_MNE(cfg)            "CFG" << cfg << ";"

/** 
 * @def UDP_MNE 
 * @brief Mnemonic command to change the radio's 10gig ip 
 *        address.
 *  
 * FRQ_MNE prepares a formatted string for a stringstream object
 * to create. This mnemonic will set the ip address of the 
 * specified tuner/ddc for streaming. 
*/
#define UDP_MNE(tuner,ddc,addr,port,mac)  "#UDP" << tuner << \
                                          "," << ddc << "," <<\
                                          addr << "," << port \
                                          << "," << mac << ";"

/** 
 * @def SIP_MNE 
 * @brief Mnemonic command to change the radio's stream 
 *        destination.
 *  
 * SIP_MNE prepares a formatted string for a stringstream object
 * to create.  This mnemonic will set the destination for the 
 * Vita49 data stream for the specified tuner/ddc. 
*/
#define SIP_MNE(tuner,ddc,addr,port,mac)  "SIP" << tuner << \
                                          "," << ddc << "," <<\
                                          addr << "," << port \
                                          << "," << mac << ";"

/** 
 * @def ENABLE_IOP 
 * @brief Mnemonic command to change the radio to independent 
 *        operation mode.
 *  
 * ENABLE_IOP a prepared string of mnemonics which set the radio 
 * to independent operation mode. 
*/
#define ENABLE_IOP              "RCH1;DFM2;RCH2;DFM2;RCH3;DFM2"\
                                ";RCH4;DFM2;RCH0"

namespace gr
{
namespace polaris
{

class polaris_src_impl : public polaris_src
{
private:
    struct sockaddr_in m_server;
    int m_sock;

    // Flag for whether or not we have a connection to the unit.
    bool m_connected;
    // The address of the unit
    std::string m_address;
    // The address of the unit's 10Gig connection
    std::string m_stream_address;
    // The address of the target machine's 10Gig
    std::string m_fiber_address;
    // Port to recv data on
    int m_rec_port;
    // Port to connect to mne_app, defaults to 8081 in the constructor.
    int m_port;

    int m_phys_port;

    // Pointer to the complex manager that processes our data for us.
    boost::scoped_ptr<complex_manager> m_complex_manager;

    // Independent operation
    bool m_i_op;

    // Number of outputs on this block
    int m_num_output_streams;

    // The variables associated with each of the streams.  
    // Each stream has it's own samprate, freq, 
    // atten, tuner, and preamp value
    double m_samp_rate;
    boost::scoped_array<double> m_freqs;
    boost::scoped_array<double> m_attens;
    boost::scoped_array<int> m_tuners;
    boost::scoped_array<bool> m_preamps;

    bool connect_polaris(std::string ip, int port);
    std::string send_message(std::string s, int timeout);
    void set_freq(double freq, int stream);
    void set_atten(double atten, int stream);
    void set_tuner(int tuner, int stream);
    void try_connect();
public:
    polaris_src_impl(std::string ip, std::string streamip, 
                     std::string fibip, int port, int num_outputs, 
                     bool indpendent_operation, int phys);
    ~polaris_src_impl();

    // GNURadio callbacks
    /**
     * Update's the preamp for a specific stream (1-4).  Accessable 
     * from python. 
     * 
     * 
     * @param pam - Whether to turn the preamp on or off.
     * @param stream - Which output stream to process for. 
     *  
     * @note Enabling the preamp disables attenuation. 
     */
    void update_preamp(bool pam, int stream);

    /**
     * Update's a streams output to display the specified tuner's 
     * data. Accessable from python.
     * 
     * 
     * @param tuner - Which tuner you would like data from.
     * @param stream - Which stream you would like the data to go 
     *               out.
     */
    void update_tuners(int tuner, int stream);

    /**
     * Update's the frequency for this stream's tuner.  Accessable 
     * from python. 
     * 
     * 
     * @param freq - The frequency to tune to.
     * @param stream - Which output stream to change.
     */
    void update_freq(double freq, int stream);

    /**
     * Set the samplerate for all streams.  Accessable from python.
     * 
     * 
     * @param sr - Sample rate to set.
     */
    void update_samprate(double sr);

    /**
     * Set the attenuation for this stream's tuner.  Accessable from 
     * python. 
     * 
     * 
     * @param atten - The attenuation to apply.
     * @param stream - Which stream you apply attenuation to. 
     *  
     * @note Setting attenuation disables preamp. 
     */
    void update_atten(double atten, int stream);

    // gr_block extended functions
    void forecast(int noutput_items, gr_vector_int 
                  &ninput_items_required);
    bool start();
    bool stop();
    int work(int noutput_items, gr_vector_const_void_star 
             &input_items, gr_vector_void_star &output_items);
};

} // namespace polaris
} // namespace gr

#endif /* INCLUDED_POLARIS_POLSRC_IMPL_H */

