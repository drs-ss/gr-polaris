#include "flex_fft_parser.h"

flex_packet::flex_packet(boost::uint8_t* data, int num_bytes)
{
    m_raw_data = data;
    m_raw_data_size = num_bytes;
    m_stream_id = 0;
    m_num_samples = 0;
    m_sample_rate = 0.0;
    m_fft_size = 0;
    m_start_index = 0;
    m_end_index = 0;
    m_num_averages = 0.0;
    m_frequency = 0.0;
    m_ref_level = 0.0;
    m_frame_counter = 0;
    m_frame_size = 0;
    m_data_packet_size = 0;
    m_context_packet_size = 0;
    m_location = 0;
    m_data_packet_count = 0;
    m_context_packet_count = 0;
    m_sample_start = NULL;
    m_processing = false;
}

flex_packet::~flex_packet()
{

}

bool
flex_packet::try_parse(int &frame_size)
{
    frame_size = MAX_FRAME_SIZE;
    if (m_raw_data == NULL || m_raw_data_size < 88) {
        return false;
    }
    // Let's try to parse this otherwise...
    // Now let's look for the VRLP start line...
    int i = 0;
    int j = 0;
    while (j < m_raw_data_size && 
           ntohl(
               reinterpret_cast<boost::uint32_t*>(&m_raw_data[j])[0]
               ) != 0x56524C50) {
        j += 1;
    }

    // First let's make our array jump by 32-bit words...
    boost::uint32_t* packet_data = 
        reinterpret_cast<boost::uint32_t*>(&m_raw_data[j]);
    int num_words = (m_raw_data_size - j) / sizeof(boost::uint32_t);

    if (i == num_words) {
        // Didn't find VRLP...
        return false;
    }

    // Now that we have found our starting line, lets get some 
    // information about our frame.
    i += 1;
    if (i >= num_words){
        return false;
    }
    boost::uint32_t temp = ntohl(packet_data[i]);
    m_frame_counter = (temp & 0xFFF00000) >> 20;
    m_frame_size = temp & 0xFFFFF;

    // Report back how much of the data we actually need
    frame_size = (m_frame_size * sizeof(boost::uint32_t)) + j;

    // Check if we have enough data here to have the whole frame...
    if (m_frame_size > num_words) {
        return false;
    }

    // We now know that we have a whole frame here, lets parse out
    // the information about it.
    i += 1;
    temp = ntohl(packet_data[i]);
    flex_data_header* data_header = 
        reinterpret_cast<flex_data_header*>(&temp);
    if (data_header->packet_type != 3) {
        frame_size = -1;
        return false;
    }
    m_data_packet_count = data_header->packet_count;
    m_data_packet_size = data_header->packet_size;
    m_num_samples = (m_data_packet_size - 5) * 2;
    
    // Grab the stream id...
    i += 1;
    m_stream_id = static_cast<int>(ntohl(packet_data[i]));

    // Setup the sample start pointer
    i += 4;
    m_sample_start = reinterpret_cast<boost::uint8_t*>(&packet_data[i]);

    // Now let's get the context packet which should be in there...
    i += (m_data_packet_size - 5);
    temp = ntohl(packet_data[i]);
    flex_context_header* context_header = 
        reinterpret_cast<flex_context_header*>(&temp);
    if (context_header->packet_type != 5) {
        frame_size = -1;
        return false;
    }
    if (context_header->packet_size != 14) {
        frame_size = -1;
        return false;
    }

    // Check that the stream id makes sense
    i += 1;
    int context_stream_id = ntohl(packet_data[i]);
    if (context_stream_id != m_stream_id) {
        frame_size = -1;
        return false;
    }

    // Get the reference frequency
    i += 4;
    boost::uint64_t freq_first, freq_second;
    freq_first = ntohl(packet_data[i]);
    i += 1;
    freq_second = ntohl(packet_data[i]);
    m_frequency = static_cast<double>(
        (freq_first << (32) | freq_second)
        ) / 0x100000;

    // Get the reference level
    i += 1;
    m_ref_level = static_cast<boost::int16_t>(ntohl(packet_data[i]) & 0xFFFF);
    m_ref_level /= 128;

    // Get the sample rate
    i += 1;
    boost::uint64_t sr_first, sr_second;
    sr_first = ntohl(packet_data[i]);
    i += 1;
    sr_second = ntohl(packet_data[i]);
    m_sample_rate = static_cast<double>(
        (sr_first << (32) | sr_second)
        ) / 0x100000;

    // Get the FFT size
    i += 1;
    m_fft_size = ntohl(packet_data[i]);

    // Get the start index
    i += 1;
    m_start_index = ntohl(packet_data[i]);

    // Get the end index
    i += 1;
    m_end_index = ntohl(packet_data[i]);

    // Get the number of averages
    i += 1;
    m_num_averages = ntohl(packet_data[i]);

    // This next bit had better be the end...
    i += 1;
    temp = ntohl(packet_data[i]);
    if (ntohl(packet_data[i]) != 0x56454E44) {
        frame_size = -1;
        return false;
    }

    // Spawn our processing thread...
    m_processing = true;
    m_thread.reset(
        new boost::thread(
            boost::bind(&flex_packet::prepare_samples, this)));

    // Report success!
    return true;
}

bool
flex_packet::is_data_ready()
{
    return !m_processing;
}

float*
flex_packet::get_fft_data()
{
    if (m_processing) {
        return NULL;
    }else{
        return m_fft_data.get();
    }
}

int
flex_packet::get_stream_id()
{
    return m_stream_id;
}

int
flex_packet::get_num_samples()
{
    return m_num_samples;
}

double
flex_packet::get_sample_rate()
{
    return m_sample_rate;
}

int
flex_packet::get_fft_size()
{
    return m_fft_size;
}

int 
flex_packet::get_start_index()
{
    return m_start_index;
}

int
flex_packet::get_end_index()
{
    return m_end_index;
}

double
flex_packet::get_num_averages()
{
    return m_num_averages;
}

double
flex_packet::get_reference_level()
{
    return m_ref_level;
}

double
flex_packet::get_frequency()
{
    return m_frequency;
}

int
flex_packet::get_frame_counter()
{
    return m_frame_counter;
}

int
flex_packet::get_data_packet_counter()
{
    return m_data_packet_count;
}

int
flex_packet::get_context_packet_counter()
{
    return m_context_packet_count;
}

void
flex_packet::prepare_samples()
{
    if (!m_processing) {
        // We weren't setup correctly.
        return;
    }

    // This thread will need to do a couple of things...
    //      * Allocate memory for the final prepared samples
    //      * Convert all data points from dBFS to dBm

    // First let's allocate our memory...
    m_fft_data.reset(new float[m_num_samples]);

    // Now let's prepare to copy our data...
    volatile boost::int16_t* sample_data = 
        reinterpret_cast<volatile boost::int16_t*>(m_sample_start);
    for (int i = 0; i < m_num_samples; i++) {
        m_fft_data[i] = 
            static_cast<float>(
                static_cast<boost::int16_t>(ntohs(sample_data[i])));
        m_fft_data[i] /= 128;
        m_fft_data[i] += m_ref_level;
        if (m_fft_data[i] < -115.0) {
            m_fft_data[i] = -115.0;
        }
    }

    // Let everybody know that we are done!
    m_processing = false;
}

void
flex_packet::set_location(int location)
{
    m_location = location;
}

int 
flex_packet::get_location()
{
    return m_location;
}

flex_fft_parser::flex_fft_parser()
{
    m_cur_packet = -1;
    m_raw_write_location = 0;
    m_current_index = -1;
    memset(m_raw_data, 0, BUFFER_SIZE * MAX_FRAME_SIZE);
    memset(m_required_data, 0, BUFFER_SIZE * sizeof(int));
    m_available_locations.reserve(BUFFER_SIZE);
    m_active_index = 0;
    m_flip_lists = false;
    m_processing_request = false;
    m_waiting_for_settle = false;
    m_packet_list[0].set_capacity(BUFFER_SIZE);
    m_packet_list[1].set_capacity(BUFFER_SIZE);
    m_active_list = &m_packet_list[m_active_index];
    for (int i = BUFFER_SIZE - 1; i > 0; i--) {
        m_available_locations.push(i);
    }
}

flex_fft_parser::~flex_fft_parser()
{
}

bool
flex_fft_parser::parse_flex_packet(const unsigned char* data, 
                                   int size)
{
    if (m_flip_lists) {
        // So if they want a flip, we need to swap the active index
        // and release all of the buffers being held onto by that
        // list.  To do this, we'll swap our active index, and hold
        // the other thread until we are done with our data.  Once
        // done, we'll release our list to the world.
        if (!m_waiting_for_settle) {
            // If we aren't waiting for data to settle, move
            // our active index and wait.
            m_active_index = !m_active_index;
            m_active_list = &m_packet_list[m_active_index];
            m_waiting_for_settle = true;
        }else{
            // We are waiting for data to settle, check the
            // lists processing to make sure it's ok.
            bool done = true;
            for (int i = 0; i < m_packet_list[!m_active_index].size();
                  i++) {
                flex_packet::sptr temp = 
                    m_packet_list[!m_active_index][i];
                if(!temp->is_data_ready()){
                    done = false;
                    break;
                }else{
                    if (temp->get_location() > -1) {
                        m_available_locations.push(temp->get_location());
                        temp->set_location(-1);
                    }
                }
            }
            if (done) {
                // If we are done, then give the other thread control
                // of this resource.
                m_flip_lists = false;
                m_waiting_for_settle = false;
            }
        }
    }

    // First let's do some sanity checks...
    if (data == NULL || size <= 0){
        return false;
    }

    // Check which buffer we are saving this stuff in...
    if (m_current_index < 0) {
        if (!m_available_locations.pop(m_current_index)) {
            m_current_index = -1;
            return false;
        }
    }

    // Check the bounds...
    if (size > MAX_FRAME_SIZE) {
        return false;
    }
    if (m_raw_write_location > 0) {
        if (size > (MAX_FRAME_SIZE - m_raw_write_location)) {
            // Too much data for a single buffer, we
            // need to move on.
            m_raw_write_location = 0;
        }
    }

    if (m_raw_write_location + size >= MAX_FRAME_SIZE) {
        return false;
    }

    // Now copy the data into this location...
    memcpy(&m_raw_data[m_current_index][m_raw_write_location], data,
           size);
    m_raw_write_location += size;

    // Now try to parse it...
    flex_packet::sptr temp;
    temp.reset(new flex_packet(m_raw_data[m_current_index],
                               m_raw_write_location));

    if (temp->try_parse(m_required_data[m_current_index])) {
        // The parser is going to grab the data from this packet...
        // let's add this packet to the list to be watched.
        temp->set_location(m_current_index);
        m_active_list->push_back(temp);
        
        // Check if we have it too much data, if we did move some into
        // the next available buffer.
        if (m_required_data[m_current_index] < m_raw_write_location) {
            // We gave it too much.
            int temp_write_location = m_raw_write_location;
            m_raw_write_location = 0;
            int temp_required_data = m_required_data[m_current_index];
            m_required_data[m_current_index] = 0;
            // reset current index so we don't overwrite successful 
            // data
            m_current_index = -1;
            parse_flex_packet(
                &data[temp_required_data 
                - (temp_write_location - size)], 
                temp_write_location 
                - temp_required_data);
        }else{
            m_raw_write_location = 0;
            m_required_data[m_current_index] = 0;
            m_current_index = -1;
        }
    } else {
        if (m_raw_write_location >= MAX_FRAME_SIZE ||
                m_required_data[m_current_index] < 0) {
            // We don't need to reset current index here, since we 
            // didn't succeed.
            m_raw_write_location = 0;
            m_required_data[m_current_index] = 0;
        }
    }
    return true;
}

// Not thread safe!!!  Must run in same thread as parse_flex_packet
flex_packet::sptr
flex_fft_parser::get_next_packet()
{
    if (m_active_list->size() == 0) {
        return flex_packet::sptr();
    }
    if (m_active_list->at(0)->is_data_ready()) {
        flex_packet::sptr temp = m_active_list->at(0);
        m_available_locations.push(temp->get_location());
        m_active_list->erase(m_active_list->begin());
        return temp;
    }
    return flex_packet::sptr();
}

boost::circular_buffer<flex_packet::sptr>*
flex_fft_parser::get_packet_list()
{
    if (m_flip_lists) {
        return NULL;
    }
    if (m_processing_request) {
        // If we were processing a request for the user and
        // m_flip_lists is false, return this data to them.
        m_processing_request = false;

        return &m_packet_list[!m_active_index];
    }else{
        // We weren't processing a request previously, start
        // processing.
        m_processing_request = true;
        m_flip_lists = true;
        return NULL;
    }
}
