PRECISION  = DOUBLE
PROFILE    = FALSE

DEBUG      = FALSE

DIM        = 3

COMP	   = SICM

USE_MPI    = FALSE
USE_OMP    = TRUE
USE_CUDA   = FALSE

TINY_PROFILE = FALSE

# define the location of the CASTRO top directory
CASTRO_HOME  := ../
TOP := $(CASTRO_HOME)

AMREX_HOME ?= /path/to/amrex

NETWORK_DIR ?= aprox13
EOS_DIR ?= helmholtz

ifdef MICROPHYSICS_HOME
   EOS_PATH := $(MICROPHYSICS_HOME)/EOS/$(strip $(EOS_DIR))
   NETWORK_PATH := $(MICROPHYSICS_HOME)/networks/$(strip $(NETWORK_DIR))
else
   $(error StarLord requires Microphysics, and the MICROPHYSICS_HOME environment variable to be set)
endif

EXTERN_CORE += $(EOS_HOME)
EXTERN_CORE += $(EOS_PATH)

EXTERN_CORE += $(NETWORK_HOME)
EXTERN_CORE += $(NETWORK_PATH)

EXTERN_CORE += $(MICROPHYSICS_HOME)/EOS
EXTERN_CORE += $(MICROPHYSICS_HOME)/networks

Bpack += $(foreach dir, $(EXTERN_CORE), $(dir)/Make.package)
Blocs += $(foreach dir, $(EXTERN_CORE), $(dir))

EBASE = Castro

# This application only supports 3D.
ifneq ($(DIM),3)
  $(error StarLord only supports BL_SPACEDIM == 3)
endif

# EXTERN_CORE is simply a list of the directories we use that
# will be searched for runtime parameters
EXTERN_CORE ?= $(TOP)/Source

include $(AMREX_HOME)/Tools/GNUMake/Make.defs

# Castro uses a coarse grained OMP approach
DEFINES += -DCRSEGRNDOMP

ifeq ($(USE_ACC), TRUE)
   DEFINES += -DACC
endif

all: $(executable)
	@echo SUCCESS

# the helmholtz EOS has an include file -- also add a target to link
# the table into the problem directory.
ifeq ($(findstring helmholtz, $(EOS_DIR)), helmholtz)
   all: table
endif

table:
	@if [ ! -f helm_table.dat ]; then echo Linking helm_table.dat; ln -s $(EOS_PATH)/helm_table.dat .;  fi

# AMReX directories -- note the Make.package for these adds these
# directories into VPATH_LOCATIONS and INCLUDE_LOCATIONS for us, so we
# don't need to do it here
Pdirs 	:= Base AmrCore Amr Boundary

Bpack	+= $(foreach dir, $(Pdirs), $(AMREX_HOME)/Src/$(dir)/Make.package)

# Castro directories
Bdirs 	:= Source

Bpack	+= $(foreach dir, $(Bdirs), $(TOP)/$(dir)/Make.package)
Blocs	+= $(foreach dir, $(Bdirs), $(TOP)/$(dir))

include $(Bpack)

INCLUDE_LOCATIONS += $(Blocs)
VPATH_LOCATIONS   += $(Blocs)

ifeq ($(MEGA_FORTRAN), TRUE)
   F90EXE_sources += mega_f.F90
else
   F90EXE_sources += $(ca_F90EXE_sources)
endif

mega_f.F90: $(ca_F90EXE_sources)
	$(AMREX_HOME)/Tools/F_scripts/f90cat.py $^

# runtime parameter support for extern/ routines


EXTERN_TEMPLATE := $(TOP)/Source/extern_probin.template

EXTERN_PARAMETERS := $(shell $(AMREX_HOME)/Tools/F_scripts/findparams.py $(EXTERN_CORE))

# job_info support
CEXE_headers += $(AMREX_HOME)/Tools/C_scripts/AMReX_buildInfo.H
INCLUDE_LOCATIONS +=  $(AMREX_HOME)/Tools/C_scripts

# we make AMReX_buildInfo.cpp as we make the .o file, so we can delete it
# immediately.  this way if the build is interrupted, we are guaranteed
# to remake it

objForExecs += $(objEXETempDir)/AMReX_buildInfo.o

.FORCE:
.PHONE: .FORCE

$(objEXETempDir)/AMReX_buildInfo.o: .FORCE
	$(AMREX_HOME)/Tools/C_scripts/makebuildinfo_C.py \
          --amrex_home "$(AMREX_HOME)" \
          --COMP "$(COMP)" --COMP_VERSION "$(COMP_VERSION)" \
          --CXX_comp_name "$(CXX)" --CXX_flags "$(CXXFLAGS) $(CPPFLAGS) $(includes)" \
          --F_comp_name "$(F90)" --F_flags "$(F90FLAGS)" \
          --link_flags "$(LDFLAGS)" --libraries "$(libraries)" \
          --MODULES "$(MNAMES)" $(EXTRA_BUILD_INFO) \
          --GIT "$(TOP) $(AMREX_HOME)"
	$(SILENT) $(CCACHE) $(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $(CXXEXEFLAGS) AMReX_buildInfo.cpp -o $(objEXETempDir)/AMReX_buildInfo.o
	$(SILENT) $(RM) AMReX_buildInfo.cpp


include $(AMREX_HOME)/Tools/GNUMake/Make.rules

clean::
	$(SILENT) $(RM) AMReX_buildInfo.cpp

#-----------------------------------------------------------------------------
# for debugging.  To see the value of a Makefile variable,
# e.g. Fmlocs, simply do "make print-Fmlocs".  This will
# print out the value.
print-%::
	@echo "$* is $($*)"
	@$(RM) AMReX_buildInfo.cpp
