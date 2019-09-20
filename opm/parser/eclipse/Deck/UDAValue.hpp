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

#ifndef UDA_VALUE_HPP
#define UDA_VALUE_HPP

#include <stdexcept>
#include <vector>
#include <string>
#include <iosfwd>

#include <opm/parser/eclipse/Units/Dimension.hpp>

namespace Opm {

class UDAValue {
public:
    UDAValue();
    UDAValue(double, Dimension dim);
    UDAValue(const std::string&, Dimension dim);

    template<typename T>
    T get() const;

    template<typename T>
    bool is() const;

    void reset(double value);
    void reset(const std::string& value);

    void assert_numeric() const;
    void assert_numeric(const std::string& error_msg) const;
    const Dimension& get_dim() const;

    bool operator==(const UDAValue& other) const;
    bool operator!=(const UDAValue& other) const;
private:
    bool numeric_value;
    double double_value;
    std::string string_value;

    Dimension dim;
};

std::ostream& operator<<( std::ostream& stream, const UDAValue& uda_value );
}



#endif
