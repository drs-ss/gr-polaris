#!/usr/bin/env python2
##################################################
# GNU Radio Python Flow Graph
# Title: Polaris Source Example
# Author: drs
# Description: A simple example graph to output two complex streams of data from the polaris source block.
# Generated: Tue Jul 19 11:06:49 2016
##################################################

if __name__ == '__main__':
    import ctypes
    import sys
    if sys.platform.startswith('linux'):
        try:
            x11 = ctypes.cdll.LoadLibrary('libX11.so')
            x11.XInitThreads()
        except:
            print "Warning: failed to XInitThreads()"

from PyQt4 import Qt
from gnuradio import eng_notation
from gnuradio import gr
from gnuradio import qtgui
from gnuradio.eng_option import eng_option
from gnuradio.filter import firdes
from gnuradio.qtgui import Range, RangeWidget
from optparse import OptionParser
import polaris
import sip
import sys


class polaris_example(gr.top_block, Qt.QWidget):

    def __init__(self):
        gr.top_block.__init__(self, "Polaris Source Example")
        Qt.QWidget.__init__(self)
        self.setWindowTitle("Polaris Source Example")
        try:
             self.setWindowIcon(Qt.QIcon.fromTheme('gnuradio-grc'))
        except:
             pass
        self.top_scroll_layout = Qt.QVBoxLayout()
        self.setLayout(self.top_scroll_layout)
        self.top_scroll = Qt.QScrollArea()
        self.top_scroll.setFrameStyle(Qt.QFrame.NoFrame)
        self.top_scroll_layout.addWidget(self.top_scroll)
        self.top_scroll.setWidgetResizable(True)
        self.top_widget = Qt.QWidget()
        self.top_scroll.setWidget(self.top_widget)
        self.top_layout = Qt.QVBoxLayout(self.top_widget)
        self.top_grid_layout = Qt.QGridLayout()
        self.top_layout.addLayout(self.top_grid_layout)

        self.settings = Qt.QSettings("GNU Radio", "polaris_example")
        self.restoreGeometry(self.settings.value("geometry").toByteArray())

        ##################################################
        # Variables
        ##################################################
        self.samp_rate = samp_rate = 64e6
        self.freq_4 = freq_4 = 111500000
        self.freq_3 = freq_3 = 164000000
        self.freq_2 = freq_2 = 100000000
        self.freq_1 = freq_1 = 100000000

        ##################################################
        # Blocks
        ##################################################
        self._samp_rate_range = Range(9000, 128e6, 1e5, 64e6, 200)
        self._samp_rate_win = RangeWidget(self._samp_rate_range, self.set_samp_rate, "samp_rate", "counter_slider", float)
        self.top_layout.addWidget(self._samp_rate_win)
        self._freq_2_range = Range(2e6, 6200e6, 100e3, 100000000, 500)
        self._freq_2_win = RangeWidget(self._freq_2_range, self.set_freq_2, "freq_2", "counter_slider", float)
        self.top_grid_layout.addWidget(self._freq_2_win, 1,1,1,1)
        self._freq_1_range = Range(2e6, 6200e6, 1e6, 100000000, 500)
        self._freq_1_win = RangeWidget(self._freq_1_range, self.set_freq_1, "freq_1", "counter_slider", float)
        self.top_grid_layout.addWidget(self._freq_1_win, 1,0,1,1)
        self.qtgui_freq_sink_x_0_0 = qtgui.freq_sink_c(
        	1024, #size
        	firdes.WIN_BLACKMAN_hARRIS, #wintype
        	freq_2, #fc
        	samp_rate, #bw
        	"Tuner 2", #name
        	1 #number of inputs
        )
        self.qtgui_freq_sink_x_0_0.set_update_time(0.1)
        self.qtgui_freq_sink_x_0_0.set_y_axis(-140, 10)
        self.qtgui_freq_sink_x_0_0.set_trigger_mode(qtgui.TRIG_MODE_FREE, 0.0, 0, "")
        self.qtgui_freq_sink_x_0_0.enable_autoscale(False)
        self.qtgui_freq_sink_x_0_0.enable_grid(False)
        self.qtgui_freq_sink_x_0_0.set_fft_average(1.0)
        self.qtgui_freq_sink_x_0_0.enable_control_panel(False)
        
        if not True:
          self.qtgui_freq_sink_x_0_0.disable_legend()
        
        if complex == type(float()):
          self.qtgui_freq_sink_x_0_0.set_plot_pos_half(not True)
        
        labels = ["", "", "", "", "",
                  "", "", "", "", ""]
        widths = [1, 1, 1, 1, 1,
                  1, 1, 1, 1, 1]
        colors = ["blue", "red", "green", "black", "cyan",
                  "magenta", "yellow", "dark red", "dark green", "dark blue"]
        alphas = [1.0, 1.0, 1.0, 1.0, 1.0,
                  1.0, 1.0, 1.0, 1.0, 1.0]
        for i in xrange(1):
            if len(labels[i]) == 0:
                self.qtgui_freq_sink_x_0_0.set_line_label(i, "Data {0}".format(i))
            else:
                self.qtgui_freq_sink_x_0_0.set_line_label(i, labels[i])
            self.qtgui_freq_sink_x_0_0.set_line_width(i, widths[i])
            self.qtgui_freq_sink_x_0_0.set_line_color(i, colors[i])
            self.qtgui_freq_sink_x_0_0.set_line_alpha(i, alphas[i])
        
        self._qtgui_freq_sink_x_0_0_win = sip.wrapinstance(self.qtgui_freq_sink_x_0_0.pyqwidget(), Qt.QWidget)
        self.top_grid_layout.addWidget(self._qtgui_freq_sink_x_0_0_win, 0,1,1,1)
        (self.qtgui_freq_sink_x_0_0).set_processor_affinity([2])
        self.qtgui_freq_sink_x_0 = qtgui.freq_sink_c(
        	1024, #size
        	firdes.WIN_BLACKMAN_hARRIS, #wintype
        	freq_1, #fc
        	samp_rate, #bw
        	"Tuner 1", #name
        	1 #number of inputs
        )
        self.qtgui_freq_sink_x_0.set_update_time(0.1)
        self.qtgui_freq_sink_x_0.set_y_axis(-140, 10)
        self.qtgui_freq_sink_x_0.set_trigger_mode(qtgui.TRIG_MODE_FREE, 0.0, 0, "")
        self.qtgui_freq_sink_x_0.enable_autoscale(False)
        self.qtgui_freq_sink_x_0.enable_grid(False)
        self.qtgui_freq_sink_x_0.set_fft_average(1.0)
        self.qtgui_freq_sink_x_0.enable_control_panel(False)
        
        if not True:
          self.qtgui_freq_sink_x_0.disable_legend()
        
        if complex == type(float()):
          self.qtgui_freq_sink_x_0.set_plot_pos_half(not True)
        
        labels = ["", "", "", "", "",
                  "", "", "", "", ""]
        widths = [1, 1, 1, 1, 1,
                  1, 1, 1, 1, 1]
        colors = ["blue", "red", "green", "black", "cyan",
                  "magenta", "yellow", "dark red", "dark green", "dark blue"]
        alphas = [1.0, 1.0, 1.0, 1.0, 1.0,
                  1.0, 1.0, 1.0, 1.0, 1.0]
        for i in xrange(1):
            if len(labels[i]) == 0:
                self.qtgui_freq_sink_x_0.set_line_label(i, "Data {0}".format(i))
            else:
                self.qtgui_freq_sink_x_0.set_line_label(i, labels[i])
            self.qtgui_freq_sink_x_0.set_line_width(i, widths[i])
            self.qtgui_freq_sink_x_0.set_line_color(i, colors[i])
            self.qtgui_freq_sink_x_0.set_line_alpha(i, alphas[i])
        
        self._qtgui_freq_sink_x_0_win = sip.wrapinstance(self.qtgui_freq_sink_x_0.pyqwidget(), Qt.QWidget)
        self.top_grid_layout.addWidget(self._qtgui_freq_sink_x_0_win, 0,0,1,1)
        (self.qtgui_freq_sink_x_0).set_processor_affinity([2])
        self.polaris_polaris_src_0 = polaris.polaris_src("192.168.10.50", 4991, "192.168.11.61", "192.168.11.71", 2, True, 0)
        self.polaris_polaris_src_0.update_tuners(4, 1)
        self.polaris_polaris_src_0.update_tuners(2, 2)
        self.polaris_polaris_src_0.update_tuners(3, 3)
        self.polaris_polaris_src_0.update_tuners(4, 4)
        self.polaris_polaris_src_0.update_preamp(False, 1)
        self.polaris_polaris_src_0.update_preamp(False, 2)
        self.polaris_polaris_src_0.update_preamp(False, 3)
        self.polaris_polaris_src_0.update_preamp(False, 4)
        self.polaris_polaris_src_0.update_freq(freq_1, 1)
        self.polaris_polaris_src_0.update_freq(freq_2, 2)
        self.polaris_polaris_src_0.update_freq(100000000, 3)
        self.polaris_polaris_src_0.update_freq(100000000, 4)
        self.polaris_polaris_src_0.update_atten(0, 1)
        self.polaris_polaris_src_0.update_atten(0, 2)
        self.polaris_polaris_src_0.update_atten(0, 3)
        self.polaris_polaris_src_0.update_atten(0, 4)
        self.polaris_polaris_src_0.update_samprate(samp_rate)
            
        self._freq_4_range = Range(2e6, 6200e6, 100000, 111500000, 500)
        self._freq_4_win = RangeWidget(self._freq_4_range, self.set_freq_4, "freq_4", "counter_slider", float)
        self.top_grid_layout.addWidget(self._freq_4_win, 3,1,1,1)
        self._freq_3_range = Range(2e6, 6200e6, 100e3, 164000000, 500)
        self._freq_3_win = RangeWidget(self._freq_3_range, self.set_freq_3, "freq_3", "counter_slider", float)
        self.top_grid_layout.addWidget(self._freq_3_win, 3,0,1,1)

        ##################################################
        # Connections
        ##################################################
        self.connect((self.polaris_polaris_src_0, 0), (self.qtgui_freq_sink_x_0, 0))    
        self.connect((self.polaris_polaris_src_0, 1), (self.qtgui_freq_sink_x_0_0, 0))    

    def closeEvent(self, event):
        self.settings = Qt.QSettings("GNU Radio", "polaris_example")
        self.settings.setValue("geometry", self.saveGeometry())
        event.accept()

    def get_samp_rate(self):
        return self.samp_rate

    def set_samp_rate(self, samp_rate):
        self.samp_rate = samp_rate
        self.qtgui_freq_sink_x_0_0.set_frequency_range(self.freq_2, self.samp_rate)
        self.qtgui_freq_sink_x_0.set_frequency_range(self.freq_1, self.samp_rate)
        self.polaris_polaris_src_0.update_samprate(self.samp_rate)

    def get_freq_4(self):
        return self.freq_4

    def set_freq_4(self, freq_4):
        self.freq_4 = freq_4

    def get_freq_3(self):
        return self.freq_3

    def set_freq_3(self, freq_3):
        self.freq_3 = freq_3

    def get_freq_2(self):
        return self.freq_2

    def set_freq_2(self, freq_2):
        self.freq_2 = freq_2
        self.qtgui_freq_sink_x_0_0.set_frequency_range(self.freq_2, self.samp_rate)
        self.polaris_polaris_src_0.update_freq(self.freq_2, 2)

    def get_freq_1(self):
        return self.freq_1

    def set_freq_1(self, freq_1):
        self.freq_1 = freq_1
        self.qtgui_freq_sink_x_0.set_frequency_range(self.freq_1, self.samp_rate)
        self.polaris_polaris_src_0.update_freq(self.freq_1, 1)


if __name__ == '__main__':
    parser = OptionParser(option_class=eng_option, usage="%prog: [options]")
    (options, args) = parser.parse_args()
    from distutils.version import StrictVersion
    if StrictVersion(Qt.qVersion()) >= StrictVersion("4.5.0"):
        Qt.QApplication.setGraphicsSystem(gr.prefs().get_string('qtgui','style','raster'))
    qapp = Qt.QApplication(sys.argv)
    tb = polaris_example()
    tb.start()
    tb.show()

    def quitting():
        tb.stop()
        tb.wait()
    qapp.connect(qapp, Qt.SIGNAL("aboutToQuit()"), quitting)
    qapp.exec_()
    tb = None  # to clean up Qt widgets
