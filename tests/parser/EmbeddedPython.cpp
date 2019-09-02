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

#include <stdexcept>
#include <iostream>
#include <memory>

#define BOOST_TEST_MODULE EMBEDDED_PYTHON

#include <boost/test/unit_test.hpp>

#include <opm/parser/eclipse/Python/Python.hpp>

#ifndef EMBEDDED_PYTHON

BOOST_AUTO_TEST_CASE(INSTANTIATE) {
    Opm::Python python;
    BOOST_CHECK(!python);
    BOOST_CHECK_THROW(python.exec("print('Hello world')"), std::logic_error);
}

#else

BOOST_AUTO_TEST_CASE(INSTANTIATE) {
    Opm::Python python;
    BOOST_CHECK(python);
    BOOST_CHECK_NO_THROW(python.exec("print('Hello world')"));
}

#endif

