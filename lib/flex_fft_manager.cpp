#include "flex_fft_manager.h"

flex_fft_manager::flex_fft_manager(std::string address, short port)
{
    m_address = address;
    m_port = port;
    m_buffer_size = 0;
    m_write_side = 0;
    m_connected = false;
    m_packets_to_sort = NULL;
    m_running = true;
    m_connected = false;
    m_flip_buffers = false;
    m_socket = 0;
    m_flex_parser.reset(new flex_fft_parser());
    setup_connection();
}

flex_fft_manager::~flex_fft_manager()
{

}

void
flex_fft_manager::add_stream(int stream_id)
{
    boost::uint32_t last_index = m_flex_streams.size();
    m_flex_streams.resize(m_flex_streams.size() + 1);
    m_flex_streams[last_index].stream_id = stream_id;
    m_flex_streams[last_index].packet_counter = -1;
    m_flex_streams[last_index].flex_packets.set_capacity(1000);
}

unsigned int
flex_fft_manager::get_num_streams()
{
    return m_flex_streams.size();
}

void
flex_fft_manager::clear_streams()
{
    m_flex_streams.clear();
}

void
flex_fft_manager::copy_data(void** target_buffers,
                            int request_amount,
                            std::vector<int> &amounts_returned,
                            std::vector<std::vector<stream_change> > 
                            &stream_changes)
{
    // Sanity checks first...
    if (amounts_returned.size() != m_flex_streams.size() ||
            stream_changes.size() != m_flex_streams.size()) {
        for (boost::uint32_t i = 0; i < amounts_returned.size(); i++) {
            amounts_returned[i] = 0;
        }
        return;
    }
    if (m_packets_to_sort == NULL) {
        // Get a new list of processed packets.
        m_packets_to_sort = m_flex_parser->get_packet_list();
    }else if(m_packets_to_sort->empty()){
        m_packets_to_sort = m_flex_parser->get_packet_list();
    }
    // Put the packets in the correct location
    bool popped_packet = false;
    while (m_packets_to_sort != NULL && m_packets_to_sort->size() > 0) {
        flex_packet::sptr temp_packet = m_packets_to_sort->at(0);
        for (boost::uint32_t j = 0; j < m_flex_streams.size(); j++) {
            if (m_flex_streams[j].stream_id == temp_packet->get_stream_id()) {
                m_flex_streams[j].flex_packets.push_back(temp_packet);
                m_packets_to_sort->pop_front();
                popped_packet = true;
                break;
            }
        }
        if (!popped_packet) {
            m_packets_to_sort->erase(m_packets_to_sort->begin());
        }
        popped_packet = false;
    }

    // Find out how many packets we can fit in the requested amount 
    // for each flex stream we own.
    for (boost::uint32_t i = 0; i < m_flex_streams.size(); i++) {
        // Start copying packets until we can't any more for this
        // stream.
        int amount_copied = 0;
        while (amount_copied < request_amount 
               && !m_flex_streams[i].flex_packets.empty()) {
            flex_packet::sptr temp_packet = m_flex_streams[i].flex_packets[0];
            if (!temp_packet->is_data_ready()) {
                std::cout << "Data was not prepared for processing"
                    << std::endl;
                break;
            }
            int number_of_samples = temp_packet->get_num_samples();

            // If we don't have enough room to copy, don't.
            if (number_of_samples + amount_copied > request_amount) {
                break;
            }

            // If we got here, we know that we need to copy this data.
            // First, lets go ahead and remove it from the list.
            m_flex_streams[i].flex_packets.pop_front();

            // Copy out our data
            float* fft_data_ptr = temp_packet->get_fft_data();
            if (fft_data_ptr == NULL) {
                std::cout << "Null data passed to flex_manager." << 
                    std::endl;
                break;
            }
            float* target_data_ptr = reinterpret_cast<float*>(
                target_buffers[i]);
            memcpy(&target_data_ptr[amount_copied], fft_data_ptr,
                   sizeof(float) * number_of_samples);
            amount_copied += number_of_samples;

            // We also need to include the metadata associated with
            // this packet.  This is shipped out in a list of 
            // "stream_change" structs for each flex_stream.
            
            // First, let's check if we even need to notify the 
            // user about a change (we only notify when something is
            // different from our last stream_change).
            stream_change last_change = m_flex_streams[i].last_stream_change;
            bool needs_stream_change = false;
            if (last_change.fft_size != temp_packet->get_fft_size()){
                needs_stream_change = true;
            }
            if (last_change.frequency != temp_packet->get_frequency()){
                needs_stream_change = true;
            }
            if (last_change.num_ave != temp_packet->get_num_averages()) {
                needs_stream_change = true;
            }
            if (last_change.reference_level != temp_packet->get_reference_level()) {
                needs_stream_change = true;
            }
            if (last_change.sample_rate != temp_packet->get_sample_rate()) {
                needs_stream_change = true;
            }

            // Let's also remember to check the packet counter...
            if (m_flex_streams[i].packet_counter < 0) {
                // This is the first packet we have recv'd for this
                // stream.  Just record the packet_counter, no loss
                // could have been reported.
                m_flex_streams[i].packet_counter = 
                    temp_packet->get_data_packet_counter();
            }else{
                m_flex_streams[i].packet_counter += 1;
                if (m_flex_streams[i].packet_counter > 15) {
                    m_flex_streams[i].packet_counter = 0;
                }
                if (m_flex_streams[i].packet_counter != 
                    temp_packet->get_data_packet_counter()) {
                    std::cout << FLEX_LOSS_MSG <<
                        m_flex_streams[i].stream_id;
                }
                m_flex_streams[i].packet_counter = 
                    temp_packet->get_data_packet_counter();
            }

            if (needs_stream_change){
                stream_change temp;
                temp.starting_sample = amount_copied - 
                    number_of_samples;
                temp.fft_size = temp_packet->get_fft_size();
                temp.frequency = temp_packet->get_frequency();
                temp.num_ave = temp_packet->get_num_averages();
                temp.reference_level = 
                    temp_packet->get_reference_level();
                temp.sample_rate = temp_packet->get_sample_rate();
                stream_changes[i].push_back(temp);
                m_flex_streams[i].last_stream_change = temp;
            }
        }
        amounts_returned[i] = amount_copied;
    }
}

void
flex_fft_manager::setup_connection()
{
    // Setup our recv thread...
    m_receive_thread.reset(new boost::thread(
        boost::bind(&flex_fft_manager::receive_loop, this)));
    m_process_thread.reset(new boost::thread(
        boost::bind(&flex_fft_manager::process_loop, this)));
}

void
flex_fft_manager::process_loop()
{
    // This function just requests data from the recv loop
    // and then requests that the flex_fft_parser processes it.
    int amount_processed = 0;
    while (m_running) {
        if (m_buffer_size == amount_processed) {
            // Request data...
            m_flip_buffers = true;
            while (m_flip_buffers && m_running) {
                usleep(1000);
            }
            amount_processed = 0;
        }
        int process_side = !m_write_side;
        int i = 0;
        while (amount_processed < m_buffer_size && m_running) {
            m_flex_parser->parse_flex_packet(
                &m_buffer[process_side][amount_processed], 
                m_copy_sizes[process_side][i]);
            amount_processed += m_copy_sizes[process_side][i];
            i += 1;
        }
    }
}

void
flex_fft_manager::receive_loop()
{
    try {
        m_socket = socket(AF_INET, SOCK_DGRAM, 0);
        if (m_socket < 0) {
            std::cout << "Failed to bind socket..." << std::endl;
            return;
        }
        struct sockaddr_in myaddr;
        memset((char *)&myaddr, 0, sizeof(myaddr));
        myaddr.sin_family = AF_INET;
        myaddr.sin_addr.s_addr = inet_addr(m_address.c_str());
        myaddr.sin_port = htons(m_port);

        if (bind(m_socket, (struct sockaddr *)&myaddr, 
                 sizeof(myaddr)) < 0) {
            m_connected = false;
            std::cout << "Failed to bind socket..." << std::endl;
        }else{
            m_connected = true;
        }

        if (m_connected) {
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 500;
            if (setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) {
                std::cout << "Failed to set timeout value on socket." << std::endl;
            }
        }
    } catch (std::exception &e) {
        std::cerr << e.what() << std::endl;
        std::cout << "Failed to setup socket for UDP connection."
            << std::endl;
        m_connected = false;
    }
    if (!m_connected) {
        return;
    }

    // Setup our variables
    int recv_size = -1;
    int i = 0;
    int amount_received = 0;
    while (m_running) {
        if (amount_received + MAX_FLEX_RECV_SIZE > 
            FLEX_RECV_BUFFER_SIZE) {
            std::cout << FLEX_OVERFLOW_MSG;
            amount_received = 0;
            i = 0;
        }
        recv_size = recv(m_socket, 
                         &m_buffer[m_write_side][amount_received],
                         MAX_FLEX_RECV_SIZE, 0);

        if (recv_size > 0) {
            amount_received += recv_size;
            m_copy_sizes[m_write_side][i] = recv_size;
            i += 1;
        }
        if (amount_received > 0 && m_flip_buffers) {
            m_buffer_size = amount_received;
            m_write_side = !m_write_side;
            amount_received = 0;
            i = 0;
            m_flip_buffers = false;
        }
    }
}
