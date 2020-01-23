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


#include <opm/parser/eclipse/EclipseState/Schedule/SummaryState.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Group/Group.hpp>

#include "../eval_uda.hpp"

namespace Opm {

Group::Group()
    : Group("", 0, 0, 0.0, UnitSystem())
{
}

Group::Group(const std::string& name, std::size_t insert_index_arg, std::size_t init_step_arg, double udq_undefined_arg, const UnitSystem& unit_system_arg) :
    m_name(name),
    m_insert_index(insert_index_arg),
    init_step(init_step_arg),
    udq_undefined(udq_undefined_arg),
    unit_system(unit_system_arg),
    group_type(GroupType::NONE),
    gefac(1),
    transfer_gefac(true),
    vfp_table(0)
{
    // All groups are initially created as children of the "FIELD" group.
    if (name != "FIELD")
        this->parent_group = "FIELD";
}

Group::Group(const std::string& gname,
             std::size_t insert_idx,
             std::size_t initstep,
             double udqUndef,
             const UnitSystem& units,
             GroupType gtype,
             double groupEF,
             bool transferGroupEF,
             int vfp,
             const std::string& parentName,
             const IOrderSet<std::string>& well,
             const IOrderSet<std::string>& group,
             const std::map<Phase, GroupInjectionProperties>& injProps,
             const GroupProductionProperties& prodProps) :
    m_name(gname),
    m_insert_index(insert_idx),
    init_step(initstep),
    udq_undefined(udqUndef),
    unit_system(units),
    group_type(gtype),
    gefac(groupEF),
    transfer_gefac(transferGroupEF),
    vfp_table(vfp),
    parent_group(parentName),
    m_wells(well),
    m_groups(group),
    injection_properties(injProps),
    production_properties(prodProps)
{
}


std::size_t Group::insert_index() const {
    return this->m_insert_index;
}

std::size_t Group::initStep() const {
    return this->init_step;
}

double Group::udqUndefined() const {
    return this->udq_undefined;
}

const UnitSystem& Group::units() const {
    return this->unit_system;
}

Group::GroupType Group::type() const {
    return this->group_type;
}

bool Group::defined(size_t timeStep) const {
    return (timeStep >= this->init_step);
}

const std::string& Group::name() const {
    return this->m_name;
}

const Group::GroupProductionProperties& Group::productionProperties() const {
    return this->production_properties;
}

const std::map<Phase, Group::GroupInjectionProperties>& Group::injectionProperties() const {
    return this->injection_properties;
}

int Group::getGroupNetVFPTable() const {
    return this->vfp_table;
}

const IOrderSet<std::string>& Group::iwells() const {
    return m_wells;
}

const IOrderSet<std::string>& Group::igroups() const {
    return m_groups;
}

bool Group::updateNetVFPTable(int vfp_arg) {
    if (this->vfp_table != vfp_arg) {
        this->vfp_table = vfp_arg;
        return true;
    } else
        return false;
}

namespace {
namespace detail {

bool has_control(int controls, Group::InjectionCMode cmode) {
    return ((controls & static_cast<int>(cmode)) != 0);
}

bool has_control(int controls, Group::ProductionCMode cmode) {
    return ((controls & static_cast<int>(cmode)) != 0);
}
}
}

bool Group::updateInjection(const GroupInjectionProperties& injection) {
    bool update = false;

    if (!this->hasType(GroupType::INJECTION)) {
        this->addType(GroupType::INJECTION);
        update = true;
    }

    auto iter = this->injection_properties.find(injection.phase);
    if (iter == this->injection_properties.end()) {
        this->injection_properties.insert(std::make_pair(injection.phase, injection));
        update = true;
    } else {
        if (iter->second != injection) {
            iter->second = injection;
            update = true;
        }
    }

    if (detail::has_control(injection.injection_controls, Group::InjectionCMode::RESV) ||
        detail::has_control(injection.injection_controls, Group::InjectionCMode::REIN) ||
        detail::has_control(injection.injection_controls, Group::InjectionCMode::VREP)) {
        auto topup_phase = std::make_pair(injection.phase, true);
        if (topup_phase != this->m_topup_phase) {
            this->m_topup_phase = topup_phase;
            update = true;
        }
    } else {
        if (this->m_topup_phase == std::make_pair(injection.phase, true))
            this->m_topup_phase = std::make_pair(injection.phase, false);
    }
    return update;
}


bool Group::updateProduction(const GroupProductionProperties& production) {
    bool update = false;

    if (this->production_properties != production) {
        this->production_properties = production;
        update = true;
    }

    if (!this->hasType(GroupType::PRODUCTION)) {
        this->addType(GroupType::PRODUCTION);
        update = true;
    }

    return update;
}


bool Group::GroupInjectionProperties::operator==(const GroupInjectionProperties& other) const {
    return
        this->phase                 == other.phase &&
        this->cmode                 == other.cmode &&
        this->surface_max_rate      == other.surface_max_rate &&
        this->resv_max_rate         == other.resv_max_rate &&
        this->target_reinj_fraction == other.target_reinj_fraction &&
        this->injection_controls    == other.injection_controls &&
        this->target_void_fraction  == other.target_void_fraction &&
        this->reinj_group           == other.reinj_group &&
        this->voidage_group         == other.voidage_group;
}


bool Group::GroupInjectionProperties::operator!=(const GroupInjectionProperties& other) const {
    return !(*this == other);
}


bool Group::GroupProductionProperties::operator==(const GroupProductionProperties& other) const {
    return
        this->cmode               == other.cmode &&
        this->exceed_action       == other.exceed_action &&
        this->oil_target          == other.oil_target &&
        this->water_target        == other.water_target &&
        this->gas_target          == other.gas_target &&
        this->liquid_target       == other.liquid_target &&
        this->guide_rate          == other.guide_rate &&
        this->guide_rate_def      == other.guide_rate_def &&
        this->production_controls == other.production_controls &&
        this->resv_target         == other.resv_target;
}


bool Group::GroupProductionProperties::operator!=(const GroupProductionProperties& other) const {
    return !(*this == other);
}

bool Group::hasType(GroupType gtype) const {
    return ((this->group_type & gtype) == gtype);
}

void Group::addType(GroupType new_gtype) {
    this->group_type = this->group_type | new_gtype;
}

bool Group::isProductionGroup() const {
    return this->hasType(GroupType::PRODUCTION);
}

bool Group::isInjectionGroup() const {
    return this->hasType(GroupType::INJECTION);
}

void Group::setProductionGroup() {
    this->addType(GroupType::PRODUCTION);
}

void Group::setInjectionGroup() {
    this->addType(GroupType::INJECTION);
}


std::size_t Group::numWells() const {
    return this->m_wells.size();
}

const std::vector<std::string>& Group::wells() const {
    return this->m_wells.data();
}

const std::vector<std::string>& Group::groups() const {
    return this->m_groups.data();
}

bool Group::wellgroup() const {
    if (this->m_groups.size() > 0)
        return false;
    return true;
}

bool Group::addWell(const std::string& well_name) {
    if (!this->m_groups.empty())
        throw std::logic_error("Groups can not mix group and well children. Trying to add well: " + well_name + " to group: " + this->name());

    if (this->m_wells.count(well_name) == 0) {
        this->m_wells.insert(well_name);
        return true;
    }
    return false;
}

bool Group::hasWell(const std::string& well_name) const  {
    return (this->m_wells.count(well_name) == 1);
}

void Group::delWell(const std::string& well_name) {
    auto rm_count = this->m_wells.erase(well_name);
    if (rm_count == 0)
        throw std::invalid_argument("Group: " + this->name() + " does not have well: " + well_name);
}

bool Group::addGroup(const std::string& group_name) {
    if (!this->m_wells.empty())
        throw std::logic_error("Groups can not mix group and well children. Trying to add group: " + group_name + " to group: " + this->name());

    if (this->m_groups.count(group_name) == 0) {
        this->m_groups.insert(group_name);
        return true;
    }
    return false;
}

bool Group::hasGroup(const std::string& group_name) const  {
    return (this->m_groups.count(group_name) == 1);
}

void Group::delGroup(const std::string& group_name) {
    auto rm_count = this->m_groups.erase(group_name);
    if (rm_count == 0)
        throw std::invalid_argument("Group does not have group: " + group_name);
}

bool Group::update_gefac(double gf, bool transfer_gf) {
    bool update = false;
    if (this->gefac != gf) {
        this->gefac = gf;
        update = true;
    }

    if (this->transfer_gefac != transfer_gf) {
        this->transfer_gefac = transfer_gf;
        update = true;
    }

    return update;
}

double Group::getGroupEfficiencyFactor() const {
    return this->gefac;
}

bool Group::getTransferGroupEfficiencyFactor() const {
    return this->transfer_gefac;
}

const std::string& Group::parent() const {
    return this->parent_group;
}

const Phase& Group::topup_phase() const {
    if (this->m_topup_phase.second)
        return this->m_topup_phase.first;
    else
        throw std::logic_error("Asked for topup phase in well without topup phase defined");
}


bool Group::has_topup_phase() const {
    return this->m_topup_phase.second;
}


bool Group::updateParent(const std::string& parent) {
    if (this->parent_group != parent) {
        this->parent_group = parent;
        return true;
    }

    return false;
}

Group::ProductionControls Group::productionControls(const SummaryState& st) const {
    Group::ProductionControls pc;

    pc.cmode = this->production_properties.cmode;
    pc.exceed_action = this->production_properties.exceed_action;
    pc.oil_target = UDA::eval_group_uda(this->production_properties.oil_target, this->m_name, st, this->udq_undefined);
    pc.water_target = UDA::eval_group_uda(this->production_properties.water_target, this->m_name, st, this->udq_undefined);
    pc.gas_target = UDA::eval_group_uda(this->production_properties.gas_target, this->m_name, st, this->udq_undefined);
    pc.liquid_target = UDA::eval_group_uda(this->production_properties.liquid_target, this->m_name, st, this->udq_undefined);
    pc.guide_rate = this->production_properties.guide_rate;
    pc.guide_rate_def = this->production_properties.guide_rate_def;
    pc.resv_target = this->production_properties.resv_target;

    return pc;
}

Group::InjectionControls Group::injectionControls(Phase phase, const SummaryState& st) const {
    Group::InjectionControls ic;
    const auto& inj = this->injection_properties.at(phase);

    ic.phase = inj.phase;
    ic.cmode = inj.cmode;
    ic.injection_controls = inj.injection_controls;
    ic.surface_max_rate = UDA::eval_group_uda_rate(inj.surface_max_rate, this->m_name, st, this->udq_undefined, ic.phase, this->unit_system);
    ic.resv_max_rate = UDA::eval_group_uda(inj.resv_max_rate, this->m_name, st, this->udq_undefined);
    ic.target_reinj_fraction = UDA::eval_group_uda(inj.target_reinj_fraction, this->m_name, st, this->udq_undefined);
    ic.target_void_fraction = UDA::eval_group_uda(inj.target_void_fraction, this->m_name, st, this->udq_undefined);
    ic.reinj_group = inj.reinj_group;
    ic.voidage_group = inj.voidage_group;

    return ic;
}

bool Group::hasInjectionControl(Phase phase) const {
    return (this->injection_properties.count(phase) > 0);
}




Group::ProductionCMode Group::production_cmode() const {
    return this->production_properties.cmode;
}

const Group::GroupType& Group::getGroupType() const {
    return this-> group_type;
}

bool Group::ProductionControls::has_control(Group::ProductionCMode control) const {
    return detail::has_control(this->production_controls, control);
}

bool Group::InjectionControls::has_control(InjectionCMode cmode_arg) const {
    return detail::has_control(this->injection_controls, cmode_arg);
}

bool Group::has_control(Group::ProductionCMode control) const {
    return detail::has_control(production_properties.production_controls, control);
}


const std::string Group::ExceedAction2String( ExceedAction enumValue ) {
    switch(enumValue) {
    case ExceedAction::NONE:
        return "NONE";
    case ExceedAction::CON:
        return "CON";
    case ExceedAction::CON_PLUS:
        return "+CON";
    case ExceedAction::WELL:
        return "WELL";
    case ExceedAction::PLUG:
        return "PLUG";
    case ExceedAction::RATE:
        return "RATE";
    default:
        throw std::invalid_argument("unhandled enum value");
    }
}


Group::ExceedAction Group::ExceedActionFromString( const std::string& stringValue ) {

    if (stringValue == "NONE")
        return ExceedAction::NONE;
    else if (stringValue == "CON")
        return ExceedAction::CON;
    else if (stringValue == "+CON")
        return ExceedAction::CON_PLUS;
    else if (stringValue == "WELL")
        return ExceedAction::WELL;
    else if (stringValue == "PLUG")
        return ExceedAction::PLUG;
    else if (stringValue == "RATE")
        return ExceedAction::RATE;
    else
        throw std::invalid_argument("Unknown enum state string: " + stringValue );
}


const std::string Group::InjectionCMode2String( InjectionCMode enumValue ) {
    switch( enumValue ) {
    case InjectionCMode::NONE:
        return "NONE";
    case InjectionCMode::RATE:
        return "RATE";
    case InjectionCMode::RESV:
        return "RESV";
    case InjectionCMode::REIN:
        return "REIN";
    case InjectionCMode::VREP:
        return "VREP";
    case InjectionCMode::FLD:
        return "FLD";
    default:
        throw std::invalid_argument("Unhandled enum value");
    }
}


Group::InjectionCMode Group::InjectionCModeFromString( const std::string& stringValue ) {
    if (stringValue == "NONE")
        return InjectionCMode::NONE;
    else if (stringValue == "RATE")
        return InjectionCMode::RATE;
    else if (stringValue == "RESV")
        return InjectionCMode::RESV;
    else if (stringValue == "REIN")
        return InjectionCMode::REIN;
    else if (stringValue == "VREP")
        return InjectionCMode::VREP;
    else if (stringValue == "FLD")
        return InjectionCMode::FLD;
    else
        throw std::invalid_argument("Unknown enum state string: " + stringValue );
}

Group::GroupType operator|(Group::GroupType lhs, Group::GroupType rhs) {
    return static_cast<Group::GroupType>(static_cast<std::underlying_type<Group::GroupType>::type>(lhs) | static_cast<std::underlying_type<Group::GroupType>::type>(rhs));
}


Group::GroupType operator&(Group::GroupType lhs, Group::GroupType rhs) {
    return static_cast<Group::GroupType>(static_cast<std::underlying_type<Group::GroupType>::type>(lhs) & static_cast<std::underlying_type<Group::GroupType>::type>(rhs));
}


const std::string Group::ProductionCMode2String( ProductionCMode enumValue ) {
    switch( enumValue ) {
    case ProductionCMode::NONE:
        return "NONE";
    case ProductionCMode::ORAT:
        return "ORAT";
    case ProductionCMode::WRAT:
        return "WRAT";
    case ProductionCMode::GRAT:
        return "GRAT";
    case ProductionCMode::LRAT:
        return "LRAT";
    case ProductionCMode::CRAT:
        return "CRAT";
    case ProductionCMode::RESV:
        return "RESV";
    case ProductionCMode::PRBL:
        return "PRBL";
    case ProductionCMode::FLD:
        return "FLD";
    default:
        throw std::invalid_argument("Unhandled enum value");
    }
}


Group::ProductionCMode Group::ProductionCModeFromString( const std::string& stringValue ) {
    if (stringValue == "NONE")
        return ProductionCMode::NONE;
    else if (stringValue == "ORAT")
        return ProductionCMode::ORAT;
    else if (stringValue == "WRAT")
        return ProductionCMode::WRAT;
    else if (stringValue == "GRAT")
        return ProductionCMode::GRAT;
    else if (stringValue == "LRAT")
        return ProductionCMode::LRAT;
    else if (stringValue == "CRAT")
        return ProductionCMode::CRAT;
    else if (stringValue == "RESV")
        return ProductionCMode::RESV;
    else if (stringValue == "PRBL")
        return ProductionCMode::PRBL;
    else if (stringValue == "FLD")
        return ProductionCMode::FLD;
    else
        throw std::invalid_argument("Unknown enum state string: " + stringValue );
}

Group::GuideRateTarget Group::GuideRateTargetFromString( const std::string& stringValue ) {
    if (stringValue == "OIL")
        return GuideRateTarget::OIL;
    else if (stringValue == "WAT")
        return GuideRateTarget::WAT;
    else if (stringValue == "GAS")
        return GuideRateTarget::GAS;
    else if (stringValue == "LIQ")
        return GuideRateTarget::LIQ;
    else if (stringValue == "COMB")
        return GuideRateTarget::COMB;
    else if (stringValue == "WGA")
        return GuideRateTarget::WGA;
    else if (stringValue == "CVAL")
        return GuideRateTarget::CVAL;
    else if (stringValue == "INJV")
        return GuideRateTarget::INJV;
    else if (stringValue == "POTN")
        return GuideRateTarget::POTN;
    else if (stringValue == "FORM")
        return GuideRateTarget::FORM;
    else if (stringValue == " ")
        return GuideRateTarget::NO_GUIDE_RATE;
    else
        return GuideRateTarget::NO_GUIDE_RATE;
}

bool Group::operator==(const Group& data) const
{
    return this->name() == data.name() &&
           this->insert_index() == data.insert_index() &&
           this->initStep() == data.initStep() &&
           this->udqUndefined() == data.udqUndefined() &&
           this->units() == data.units() &&
           this->type() == data.type() &&
           this->getGroupEfficiencyFactor() == data.getGroupEfficiencyFactor() &&
           this->getTransferGroupEfficiencyFactor() == data.getTransferGroupEfficiencyFactor() &&
           this->getGroupNetVFPTable() == data.getGroupNetVFPTable() &&
           this->parent() == data.parent() &&
           this->iwells() == data.iwells() &&
           this->igroups() == data.igroups() &&
           this->m_topup_phase == data.m_topup_phase &&
           this->injection_properties == data.injection_properties &&
           this->productionProperties() == data.productionProperties();
}

}
