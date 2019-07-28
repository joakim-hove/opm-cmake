/*
  Copyright (c) 2018 Statoil ASA

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OPM_WRITE_RESTART_HELPERS_HPP
#define OPM_WRITE_RESTART_HELPERS_HPP

#include <vector>

// Missing definitions (really belong in ert/ecl_well/well_const.h, but not
// defined there)
#define SCON_KH_INDEX 3


// Forward declarations

namespace Opm {

    class EclipseGrid;
    class EclipseState;
    class Schedule;
    class Well;
    class UnitSystem;
    class UDQActive;

} // Opm

namespace Opm { namespace RestartIO { namespace Helpers {

    const double UNIMPLEMENTED_VALUE = 1e-100; // placeholder for values not yet available 

    std::vector<double>
    createDoubHead(const EclipseState& es,
                   const Schedule&     sched,
                   const std::size_t   lookup_step,
                   const double        simTime,
                   const double        nextTimeStep);



    std::vector<int>
    createInteHead(const EclipseState& es,
                   const EclipseGrid&  grid,
                   const Schedule&     sched,
                   const double        simTime,
                   const int           num_solver_steps,
                   const int           lookup_step);   // The integer index used to look up dynamic properties, e.g. the number of well.

    std::vector<bool>
    createLogiHead(const EclipseState& es);
    
    std::vector<int>
    createUdqDims(const Schedule&     		sched,
              const std::size_t        		lookup_step,
	      const std::vector<int>&           inteHead); 


}}} // Opm::RestartIO::Helpers

#endif  // OPM_WRITE_RESTART_HELPERS_HPP
