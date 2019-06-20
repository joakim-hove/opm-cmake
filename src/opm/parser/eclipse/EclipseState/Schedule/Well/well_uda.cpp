/*
  Copyright 2019 Equinor ASA.

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


#include <opm/parser/eclipse/Units/UnitSystem.hpp>
#include <opm/parser/eclipse/Deck/UDAValue.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/SummaryState.hpp>

#include "injection.hpp"
#include "well_uda.hpp"

namespace Opm {
namespace UDA {

  double eval_well_uda(const UDAValue& value, const std::string& well, const SummaryState& st, double udq_default) {
    if (value.is<double>())
        return value.get<double>();

    const std::string& string_var = value.get<std::string>();
    double output_value = udq_default;

    if (st.has_well_var(well, value.get<std::string>()))
        output_value = st.get_well_var(well, string_var);
    else if (st.has(string_var))
        output_value = st.get(string_var);

    // We do not handle negative rates.
    output_value = std::max(0.0, output_value);
    return value.get_dim().convertRawToSi(output_value);
}


double eval_well_uda_rate(const UDAValue& value, const std::string& well, const SummaryState& st, double udq_default, WellInjector::TypeEnum wellType, const UnitSystem& unitSystem) {
    double raw_rate = eval_well_uda(value, well, st, udq_default);
    return injection::rateToSI(raw_rate, wellType, unitSystem);
}


}
}
