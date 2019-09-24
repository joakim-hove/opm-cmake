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
#include <cmath>
#include <string>

#include <opm/parser/eclipse/Parser/ParserKeywords/L.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Group/GuideRateModel.hpp>

namespace Opm {

GuideRateModel::GuideRateModel(double time_interval_arg,
                               Target target_arg,
                               double A_arg,
                               double B_arg,
                               double C_arg,
                               double D_arg,
                               double E_arg,
                               double F_arg,
                               bool allow_increase_arg,
                               double damping_factor_arg,
                               bool use_free_gas_arg) :
    time_interval(time_interval_arg),
    m_target(target_arg),
    A(A_arg),
    B(B_arg),
    C(C_arg),
    D(D_arg),
    E(E_arg),
    F(F_arg),
    allow_increase_(allow_increase_arg),
    damping_factor_(damping_factor_arg),
    use_free_gas(use_free_gas_arg),
    default_model(false),
    alpha(UDAValue(ParserKeywords::LINCOM::ALPHA::defaultValue)),
    beta(UDAValue(ParserKeywords::LINCOM::BETA::defaultValue)),
    gamma(UDAValue(ParserKeywords::LINCOM::GAMMA::defaultValue))
{
    if (this->A > 3 || this->A < -3)
        throw std::invalid_argument("Invalid value for A must be in interval [-3,3]");

    if (this->B < 0)
        throw std::invalid_argument("Invalid value for B must be > 0");

    if (this->D > 3 || this->D < -3)
        throw std::invalid_argument("Invalid value for D must be in interval [-3,3]");

    if (this->F > 3 || this->F < -3)
        throw std::invalid_argument("Invalid value for F must be in interval [-3,3]");

    if (this->m_target == Target::COMB)
        throw std::logic_error("Sorry - the 'COMB' mode is not supported");
}




double GuideRateModel::eval(double oil_pot, double gas_pot, double wat_pot) const {
    if (this->default_model)
        throw std::invalid_argument("The default GuideRateModel can not be evaluated - must enter GUIDERAT information explicitly.");

    double pot;
    double R1;
    double R2;

    switch (this->m_target) {
    case Target::OIL:
        pot = oil_pot;
        R1 = wat_pot / oil_pot;
        R2 = gas_pot / oil_pot;
        break;

    case Target::LIQ:
        pot = oil_pot + wat_pot;;
        R1 = oil_pot / (oil_pot + wat_pot);
        R2 = wat_pot / (oil_pot + wat_pot);
        break;

    case Target::GAS:
        pot = gas_pot;
        R1 = wat_pot / gas_pot;
        R2 = oil_pot / gas_pot;
        break;

    case Target::COMB:
        throw std::logic_error("Not implemented - don't have a clue?");

    case Target::RES:
        throw std::logic_error("Not implemented - don't have a clue?");

    default:
        throw std::logic_error("Hmmm - should not be here?");
    }


    double denom = this->B + this->C*std::pow(R1, this->D) + this->E*std::pow(R2, this->F);
    /*
      The values pot, R1 and R2 are runtime simulation results, so here
      basically anything could happen. Quite dangerous to have hard error
      handling here?
    */
    if (denom <= 0)
        throw std::range_error("Invalid denominator: " + std::to_string(denom));

    return std::pow(pot, this->A) / denom;
}

bool GuideRateModel::operator==(const GuideRateModel& other) const {
    return (this->time_interval == other.time_interval) &&
        (this->m_target == other.m_target) &&
        (this->A == other.A) &&
        (this->B == other.B) &&
        (this->C == other.C) &&
        (this->D == other.D) &&
        (this->E == other.E) &&
        (this->F == other.F) &&
        (this->allow_increase_ == other.allow_increase_) &&
        (this->damping_factor_ == other.damping_factor_) &&
        (this->use_free_gas == other.use_free_gas);
}

bool GuideRateModel::operator!=(const GuideRateModel& other) const {
    return !(*this == other);
}


double GuideRateModel::update_delay() const {
    return this->time_interval;
}

double GuideRateModel::damping_factor() const {
    return this->damping_factor_;
}

bool GuideRateModel::allow_increase() const {
    return this->allow_increase_;
}

GuideRateModel::Target GuideRateModel::target() const {
    return this->m_target;
}


GuideRateModel::Target GuideRateModel::TargetFromString(const std::string& s) {
    if (s == "OIL")
        return Target::OIL;

    if (s == "LIQ")
        return Target::LIQ;

    if (s == "GAS")
        return Target::GAS;

    if (s == "RES")
        return Target::RES;

    if (s == "COMB")
        return Target::COMB;

    if (s == "NONE")
        return Target::NONE;

    throw std::invalid_argument("Could not convert: " + s + " to a valid Target enum value");
}


/*
  The COMB target - which requires parameters from the LINCOM keyword, is not
  supported. There are at least two pieces of missing functionality:

    1. The parameters in the LINCOM come with unit specified by the LCUNIT
       keyword; seemingly decoupled from the unit system in the rest of deck.
       This functionality is not supported.

    2. The values in the LINCOM kewyords are UDA values, have not yet integrated
       the necessary SummaryState into this.
*/

bool GuideRateModel::updateLINCOM(const UDAValue& , const UDAValue& , const UDAValue& ) {
    if (this->m_target == Target::COMB)
        throw std::logic_error("The LINCOM keyword is not supported - at all!");

    return true;
}


GuideRateModel::Target GuideRateModel::convert_target(Well2::GuideRateTarget well_target) {
    if (well_target == Well2::GuideRateTarget::OIL)
        return Target::OIL;

    if (well_target == Well2::GuideRateTarget::GAS)
        return Target::GAS;

    if (well_target == Well2::GuideRateTarget::LIQ)
        return Target::LIQ;

    throw std::logic_error("Can not convert this .... ");
}

GuideRateModel::Target GuideRateModel::convert_target(Group2::GuideRateTarget group_target) {
    if (group_target == Group2::GuideRateTarget::OIL)
        return Target::OIL;

    if (group_target == Group2::GuideRateTarget::GAS)
        return Target::GAS;

    if (group_target == Group2::GuideRateTarget::LIQ)
        return Target::LIQ;

    throw std::logic_error("Can not convert this .... ");
}
}
