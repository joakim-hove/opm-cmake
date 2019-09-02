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


#ifndef EMBEDDED_PYTHON_INTERP
#define EMBEDDED_PYTHON_INTERP

#include <string>
#include <stdexcept>

#ifdef EMBEDDED_PYTHON

#include <pybind11/embed.h>
namespace py = pybind11;

#endif



namespace Opm {

#ifdef EMBEDDED_PYTHON

class PythonInterp {
public:
    bool exec(const std::string& python_code);
    explicit operator bool() const { return true; }
private:
    py::scoped_interpreter guard = {};
};


#else

class PythonInterp {
public:
    bool exec(const std::string&) {
        return this->fail();
    };

    explicit operator bool() const { return false; }
private:
    bool fail() { throw std::logic_error("The current opm code has been built without Python support;"); }
};

#endif

}

#endif
