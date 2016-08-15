# Introduction

This is the GNU Radio OOT module for command and control of the DRS Polaris radio.  This block will allow the user to easily adjust the sample rate, frequency, attenuation and streaming setup to allow for IQ data to be streamed to a host machine.

# Installation

If you already have GNURadio installed, you can install this block through the standard install process.

	git clone https://github.com/drs-ss/gr-polaris
	cd gr-polaris
	mkdir build
	cd build
	cmake ..
	make
	sudo make install
	sudo ldconfig

## Adjusting System Settings

There are a couple of things you can do to improve the performance.

* Adjust the net.core.rmem_max value in the sysctl.conf file.  The expected value in the source is 50000000.  If you change this value, you should update the define RECV_BUFF_SIZE in the udp_listener.h (located in lib/)

* Adjust your 10 Gig card for optimal settings.
	- If you are using a Myricom card...
	- Setup the module parameters by removing the module and then probing it again with the new options:

		sudo rmmod myri10ge
		sudo modprobe myri10ge myri10ge_max_slices=3

	- A full list of options is available at http://www.myricom.com/software/myri10ge/991-which-myri10ge-load-time-options-are-available-with-the-linux-in-kernel-myri10ge-driver.html
	- There is a helpful utility script which is packaged with their driver download as well.  If you download the Myri10GE_Linux_1.5.3.p5 source tgz from http://www.myricom.com/support/downloads/myri10ge.html.  You will find in the linux folder a msixbind.sh script.  This script allows you to bind memory slices to CPU cores which can be helpful when manually sorting CPU load.

* Adjust defines in complex_manager.h and udp_listener.h.  At the top of these files are a number of defines which can impact the performance of the block.  Typically NUM_THREADS/NUM_COMPLEX in complex_manager and NUM_BUFFS/RECV_BUFF_SIZE in udp_listener are all that need to be tweaked to improve performance.

* Setting the "Min Output Buffer" (which can be found in the "Advanced" tab in the block's properties) to a large value (i.e. 500,000) can improve performance when outputting two streams at different sample rates.

## Block Reporting

When data loss is detected in the block, it will respond with one of three print outs to the console.  Each has a different meaning and can help you optimize your settings for performance.
* Packet Loss ("L") - When an L is reported it means that the block detected an interrupt in the packet counter.  This means that the system failed to capture a packet that came across the wire.  Adding more slices to a Myricom card, or assigning unique cores to slices can help improve this.  In addition, increasing the net.core.rmem_max value can also help (be sure to adjust the RECV_BUFF_SIZE define as well).
* Overflow ("O") - When an O is reported the block failed to keep up with incoming data on the back end.  Each "O" represents a full buffer flush in udp_listener.  This backup can occur in a number of locations.  If you are running a graph with a lot of processing this can slow down the rate data is requested from the block, which will cause buffers to back up and overflow.  Improving your graphs performance (or capturing data ahead of time) can help with this.  You can also force the core affinity of all other blocks in the graph to be the same, this can also sometimes help performance.  If the graph is simple (just frequency or null sinks) then it could be that the block isn't using enough threads to process (or that it doesn't have enough room in the buffers).  Try increasing NUM_BUFFS in udp_listener or increasing NUM_THREADS in complex_manager.h.
