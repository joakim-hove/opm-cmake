/*
  Copyright 2018 Statoil ASA.

  This file is part of the Open Porous Media Project (OPM).

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

#ifndef OPM_DOUBHEAD_HEADER_INCLUDED
#define OPM_DOUBHEAD_HEADER_INCLUDED

#include <chrono>
#include <cstddef>
#include <vector>

namespace Opm {
    class Tuning;
    class Schedule;
    class UDQParams;
}

namespace Opm { namespace RestartIO {

    class DoubHEAD
    {
    public:
        struct TimeStamp {
            std::chrono::time_point<std::chrono::system_clock>          start;
            std::chrono::duration<double, std::chrono::seconds::period> elapsed;
        };

        DoubHEAD();

        ~DoubHEAD() = default;
        DoubHEAD(const DoubHEAD& rhs) = default;
        DoubHEAD(DoubHEAD&& rhs) = default;

        DoubHEAD& operator=(const DoubHEAD& rhs) = default;
        DoubHEAD& operator=(DoubHEAD&& rhs) = default;

        DoubHEAD& tuningParameters(const Tuning&     tuning,
                                   const std::size_t lookup_step,
                                   const double      cnvT);

        DoubHEAD& timeStamp(const TimeStamp& ts);
        DoubHEAD& nextStep(const double nextTimeStep);

        DoubHEAD& drsdt(const Schedule&   sched,
                        const std::size_t lookup_step,
			const double      cnvT);
	
	DoubHEAD& udqParam(const UDQParams& udqPar);

        const std::vector<double>& data() const
        {
            return this->data_;
        }

    private:
        std::vector<double> data_;
    };

}} // Opm::RestartIO

#endif  // OPM_DOUBHEAD_HEADER_INCLUDED
