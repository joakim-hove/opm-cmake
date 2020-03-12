/*
  Copyright 2020 Equinor ASA.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef RST_CONNECTION
#define RST_CONNECTION

#include <array>

#include <opm/parser/eclipse/EclipseState/Schedule/Well/Connection.hpp>

namespace Opm {
class UnitSystem;

namespace RestartIO {

class Header;

struct RstConnection {
    RstConnection(const ::Opm::UnitSystem& unit_system, const int* icon, const float* scon, const double *xcon);

    int insert_index;
    std::array<int,3> ijk;
    Connection::State state;
    int drain_sat_table;
    int imb_sat_table;
    int completion;
    Connection::Direction dir;
    int segment;
    Connection::CTFKind cf_kind;

    float cf;
    float depth;
    float diameter;
    float kh;
    float segdist_end;
    float segdist_start;

    double oil_rate;
    double water_rate;
    double gas_rate;
    double pressure;
    double resv_rate;
};


}
}




#endif
