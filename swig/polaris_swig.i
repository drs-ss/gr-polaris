/* -*- c++ -*- */

#define POLARIS_API

%include "gnuradio.i"			// the common stuff

//load generated python docstrings
%include "polaris_swig_doc.i"

%{
#include "polaris/polaris_src.h"
%}


%include "polaris/polaris_src.h"
GR_SWIG_BLOCK_MAGIC2(polaris, polaris_src);
