# Copyright 2013-2018 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack import *
import os


class SicmLow(CMakePackage):
    """SICM: Simplified Interface to Complex Memory.
    Includes only the low-level interface and arena allocator."""

    homepage = "https://github.com/lanl/SICM/"
    git      = "https://github.com/lanl/SICM.git"

    version('master')

    depends_on('jemalloc +je')
    depends_on('numactl')

    def cmake_args(self):
        args = []
        return args
