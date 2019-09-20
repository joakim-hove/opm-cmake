/*
  Copyright 2019 Statoil ASA.

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
#include <iostream>

#include <opm/parser/eclipse/Deck/UDAValue.hpp>


namespace Opm {

UDAValue::UDAValue(double value, Dimension dim_arg):
    numeric_value(true),
    double_value(value),
    dim(dim_arg)
{
}


UDAValue::UDAValue() :
    UDAValue(0, {})
{}

UDAValue::UDAValue(const std::string& value, Dimension dim_arg):
    numeric_value(false),
    string_value(value),
    dim(std::move(dim_arg))
{
}

void UDAValue::assert_numeric() const {
    std::string msg = "Internal error: The support for use of UDQ/UDA is not complete in opm/flow. The string: '" + this->string_value + "' must be numeric";
    this->assert_numeric(msg);
}


void UDAValue::assert_numeric(const std::string& error_msg) const {
    if (this->numeric_value)
        return;

    throw std::invalid_argument(error_msg);
}

template<>
bool UDAValue::is<double>() const {
    return this->numeric_value;
}


template<>
bool UDAValue::is<std::string>() const {
  return !this->numeric_value;
}


template<>
double UDAValue::get() const {
    this->assert_numeric();
    return this->dim.convertRawToSi(this->double_value);
}

void UDAValue::reset(double value) {
    this->double_value = value;
    this->numeric_value = true;
}

void UDAValue::reset(const std::string& value) {
    this->string_value = value;
    this->numeric_value = false;
}

template<>
std::string UDAValue::get() const {
    if (!this->numeric_value)
        return this->string_value;

    throw std::invalid_argument("UDAValue does not hold a string value");
}



const Dimension& UDAValue::get_dim() const {
    return this->dim;
}


bool UDAValue::operator==(const UDAValue& other) const {
    if (this->numeric_value != other.numeric_value)
        return false;

    if (this->dim != other.dim)
        return false;

    if (this->numeric_value)
        return (this->double_value == other.double_value);

    return this->string_value == other.string_value;
}


bool UDAValue::operator!=(const UDAValue& other) const {
    return !(*this == other);
}

std::ostream& operator<<( std::ostream& stream, const UDAValue& uda_value ) {
    if (uda_value.is<double>())
        stream << uda_value.get<double>();
    else
        stream << "'" << uda_value.get<std::string>() << "'";
    return stream;
}



}
