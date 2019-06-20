/*
  Copyright 2018 Statoil ASA.

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

#include <algorithm>

#include <opm/parser/eclipse/Deck/Deck.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/UDQ/UDQInput.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/UDQ/UDQEnums.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/UDQ/UDQParams.hpp>


namespace Opm {

    namespace {
        std::string strip_quotes(const std::string& s) {
            if (s[0] == '\'')
                return s.substr(1, s.size() - 2);
            else
                return s;
        }

    }

    UDQInput::UDQInput(const Deck& deck) :
        udq_params(deck),
        udqft(this->udq_params)
    {
    }

    const UDQParams& UDQInput::params() const {
        return this->udq_params;
    }


    void UDQInput::add_record(const DeckRecord& record) {
        auto action = UDQ::actionType(record.getItem("ACTION").get<std::string>(0));
        const auto& quantity = record.getItem("QUANTITY").get<std::string>(0);
        const auto& data = record.getItem("DATA").getData<std::string>();

        if (action == UDQAction::UNITS)
            this->assign_unit( quantity, data[0] );
        else if (action == UDQAction::ASSIGN) {
            std::vector<std::string> selector(data.begin(), data.end() - 1);
            double value = std::stod(data.back());
            this->m_assignments.emplace_back( quantity, selector, value );
        } else
            this->m_definitions.emplace_back(this->udq_params, quantity, data);
        this->keywords.insert(quantity);
    }


    const std::vector<UDQDefine>& UDQInput::definitions() const {
        return this->m_definitions;
    }

    std::vector<UDQDefine> UDQInput::definitions(UDQVarType var_type) const {
        std::vector<UDQDefine> filtered_defines;

        std::copy_if(this->m_definitions.begin(),
                     this->m_definitions.end(),
                     std::back_inserter(filtered_defines),
                     [&var_type](const UDQDefine& def) { return def.var_type() == var_type; });

        return filtered_defines;
    }


    const std::vector<UDQAssign>& UDQInput::assignments() const {
        return this->m_assignments;
    }


    std::vector<UDQAssign> UDQInput::assignments(UDQVarType var_type) const {
        std::vector<UDQAssign> filtered_assignments;

        std::copy_if(this->m_assignments.begin(),
                     this->m_assignments.end(),
                     std::back_inserter(filtered_assignments),
                     [&var_type](const UDQAssign& assignment) { return assignment.var_type() == var_type; });

        return filtered_assignments;
    }


    const std::string& UDQInput::unit(const std::string& key) const {
        const auto pair_ptr = this->units.find(key);
        if (pair_ptr == this->units.end())
            throw std::invalid_argument("No such UDQ quantity: " + key);

        return pair_ptr->second;
    }


    void UDQInput::assign_unit(const std::string& keyword, const std::string& quoted_unit) {
        const std::string unit = strip_quotes(quoted_unit);
        const auto pair_ptr = this->units.find(keyword);
        if (pair_ptr != this->units.end()) {
            if (pair_ptr->second != unit)
                throw std::invalid_argument("Illegal to change unit of UDQ keyword runtime");

            return;
        }
        this->units[keyword] = unit;
    }

    bool UDQInput::has_unit(const std::string& keyword) const {
        return (this->units.count(keyword) > 0);
    }

    bool UDQInput::has_keyword(const std::string& keyword) const {
        return (this->keywords.count(keyword) > 0);
    }

    const UDQFunctionTable& UDQInput::function_table() const {
        return this->udqft;
    }
}
