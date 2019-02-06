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
    git      = os.path.expandvars("file:///${SICM_DIR}")

    version('develop', commit='HEAD')

    depends_on('jemalloc@5.1.0+je')
    depends_on('numactl')

    def cmake_args(self):
        args = []
        return args
