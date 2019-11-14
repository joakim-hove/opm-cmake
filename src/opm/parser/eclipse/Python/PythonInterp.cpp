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


#ifdef EMBEDDED_PYTHON
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include <opm/parser/eclipse/Deck/Deck.hpp>
#include <opm/parser/eclipse/Parser/Parser.hpp>

#include "python/cxx/export.hpp"
#include "PythonInterp.hpp"
#include "EmbedModule.hpp"

namespace py = pybind11;
namespace Opm {

OPM_EMBEDDED_MODULE(context, module) {
    python::common::export_all(module);
}


bool PythonInterp::exec(const std::string& python_code, const Parser& parser, Deck& deck) {
    auto context = py::module::import("context");
    context.attr("deck") = &deck;
    context.attr("parser") = &parser;
    py::exec(python_code, py::globals(), py::dict(py::arg("context") = context));
    return true;
}


bool PythonInterp::exec(const std::string& python_code) {
    py::exec(python_code, py::globals());
    return true;
}

}

#endif
