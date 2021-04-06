/*
  Copyright 2018 Statoil ASA

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

#include <opm/output/eclipse/AggregateGroupData.hpp>
#include <opm/output/eclipse/WriteRestartHelpers.hpp>
#include <opm/output/eclipse/VectorItems/group.hpp>
#include <opm/output/eclipse/VectorItems/well.hpp>
#include <opm/output/eclipse/VectorItems/intehead.hpp>

#include <opm/parser/eclipse/EclipseState/EclipseState.hpp>
#include <opm/parser/eclipse/EclipseState/Runspec.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/SummaryState.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Schedule.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Group/GTNode.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Group/Group.hpp>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <exception>
#include <string>
#include <stdexcept>

#define ENABLE_GCNTL_DEBUG_OUTPUT 0

#if ENABLE_GCNTL_DEBUG_OUTPUT
#include <iostream>
#endif // ENABLE_GCNTL_DEBUG_OUTPUT

// #####################################################################
// Class Opm::RestartIO::Helpers::AggregateGroupData
// ---------------------------------------------------------------------
/*
  15 : (2,1)      -- Insane logic
  16 : (8,0)      -- GuideRateDef
  43 : (8,1)      -- Total number of wells
  ---------------
  15 : (0,-1)
  43 : (17,7)
  ---------------
  15 : (2,1)
  16 : (8,0)
  43 : (7,4)
  ---------------
 */


/*

  1. While running the base run the groups production control = FLD (128)
  2. While writing the restart file we write higher_lev_ctrl_mode = ORAT (1)
  3. When restarting we read the control = 1 and go with ORAT?
*/



namespace {

// maximum number of groups
std::size_t ngmaxz(const std::vector<int>& inteHead)
{
    return inteHead[Opm::RestartIO::Helpers::VectorItems::NGMAXZ];
}

// maximum number of wells in any group
int nwgmax(const std::vector<int>& inteHead)
{
    return inteHead[Opm::RestartIO::Helpers::VectorItems::NWGMAX];
}


template <typename GroupOp>
void groupLoop(const std::vector<const Opm::Group*>& groups,
               GroupOp&&                             groupOp)
{
    auto groupID = std::size_t{0};
    for (const auto* group : groups) {
        groupID += 1;

        if (group == nullptr) { continue; }

        groupOp(*group, groupID - 1);
    }
}

template < typename T>
std::pair<bool, int > findInVector(const std::vector<T>  & vecOfElements, const T  & element)
{
    std::pair<bool, int > result;

    // Find given element in vector
    auto it = std::find(vecOfElements.begin(), vecOfElements.end(), element);

    if (it != vecOfElements.end())
    {
        result.second = std::distance(vecOfElements.begin(), it);
        result.first = true;
    }
    else
    {
        result.first = false;
        result.second = -1;
    }
    return result;
}

int currentGroupLevel(const Opm::Schedule& sched, const Opm::Group& group, const size_t simStep)
{
    auto current = group;
    int level = 0;
    while (current.name() != "FIELD") {
        level += 1;
        current = sched.getGroup(current.parent(), simStep);
    }

    return level;
}

void groupProductionControllable(const Opm::Schedule& sched, const Opm::SummaryState& sumState, const Opm::Group& group, const size_t simStep, bool& controllable)
{
    using wellCtrlMode   = ::Opm::RestartIO::Helpers::VectorItems::IWell::Value::WellCtrlMode;
    if (controllable)
        return;

    for (const auto& group_name : group.groups())
        groupProductionControllable(sched, sumState, sched.getGroup(group_name, simStep), simStep, controllable);

    for (const auto& well_name : group.wells()) {
        const auto& well = sched.getWell(well_name, simStep);
        if (well.isProducer()) {
            int cur_prod_ctrl = 0;
            // Find control mode for well
            const std::string sum_key = "WMCTL";
            if (sumState.has_well_var(well_name, sum_key)) {
                cur_prod_ctrl = static_cast<int>(sumState.get_well_var(well_name, sum_key));
            }
            if (cur_prod_ctrl == wellCtrlMode::Group) {
                controllable = true;
                return;
            }
        }
    }
}


bool groupProductionControllable(const Opm::Schedule& sched, const Opm::SummaryState& sumState, const Opm::Group& group, const size_t simStep) {
    bool controllable = false;
    groupProductionControllable(sched, sumState, group, simStep, controllable);
    return controllable;
}


void groupInjectionControllable(const Opm::Schedule& sched, const Opm::SummaryState& sumState, const Opm::Group& group, const Opm::Phase& iPhase, const size_t simStep, bool& controllable)
{
    using wellCtrlMode   = ::Opm::RestartIO::Helpers::VectorItems::IWell::Value::WellCtrlMode;
    if (controllable)
        return;

    for (const auto& group_name : group.groups())
        groupInjectionControllable(sched, sumState, sched.getGroup(group_name, simStep), iPhase, simStep, controllable);

    for (const auto& well_name : group.wells()) {
        const auto& well = sched.getWell(well_name, simStep);
        if (well.isInjector() && iPhase == well.wellType().injection_phase()) {
            int cur_inj_ctrl = 0;
            // Find control mode for well
            const std::string sum_key = "WMCTL";
            if (sumState.has_well_var(well_name, sum_key)) {
                cur_inj_ctrl = static_cast<int>(sumState.get_well_var(well_name, sum_key));
            }

            if (cur_inj_ctrl == wellCtrlMode::Group) {
                controllable = true;
                return;
            }
        }
    }
}

bool groupInjectionControllable(const Opm::Schedule& sched, const Opm::SummaryState& sumState, const Opm::Group& group, const Opm::Phase& iPhase, const size_t simStep) {
    bool controllable = false;
    groupInjectionControllable(sched, sumState, group, iPhase, simStep, controllable);
    return controllable;
}

/*
  Searches upwards in the group tree for the first parent group with active
  control different from NONE and FLD. The function will return an empty
  optional if no such group can be found.
*/

std::optional<Opm::Group> controlGroup(const Opm::Schedule& sched,
                                       const Opm::SummaryState& sumState,
                                       const Opm::Group& group,
                                       const std::size_t simStep) {
    auto current = group;
    while (current.name() != "FIELD") {
        current = sched.getGroup(current.parent(), simStep);
        auto cur_prod_ctrl = sumState.get_group_var(current.name(), "GMCTP", 0);
        if (cur_prod_ctrl > 0)
            return current;
    }
    return {};
}





int higherLevelInjControlGroupSeqIndex(const Opm::Schedule& sched,
                       const Opm::SummaryState& sumState,
                       const Opm::Group& group,
                       const std::string curInjCtrlKey,
                       const size_t simStep)
//
// returns the sequence number of higher (highest) level group with active control different from (NONE or FLD)
//
{
    int ctrl_grup_seq_no = -1;
    auto current = group;
    double cur_inj_ctrl = -1.;
    while (current.name() != "FIELD" && ctrl_grup_seq_no < 0) {
        current = sched.getGroup(current.parent(), simStep);
        cur_inj_ctrl = -1.;
        if (sumState.has_group_var(current.name(), curInjCtrlKey)) {
            cur_inj_ctrl = sumState.get_group_var(current.name(), curInjCtrlKey);
        } else {
#if ENABLE_GCNTL_DEBUG_OUTPUT
            std::cout << "Current injection group control: " << curInjCtrlKey
                      << " is not defined for group: " << current.name() << " at timestep: " << simStep << std::endl;
#endif // ENABLE_GCNTL_DEBUG_OUTPUT
            cur_inj_ctrl = 0.;
        }
        if (cur_inj_ctrl > 0. && ctrl_grup_seq_no < 0) {
            ctrl_grup_seq_no = current.insert_index();
        }
    }
    return ctrl_grup_seq_no;
} // namespace

std::vector<std::size_t> groupParentSeqIndex(const Opm::Schedule& sched,
                       const Opm::Group& group,
                       const size_t simStep)
//
// returns a vector with the group sequence index of all parent groups from current parent group to Field level
//
{
    std::vector<std::size_t> seq_numbers;
    auto current = group;
    while (current.name() != "FIELD") {
        current = sched.getGroup(current.parent(), simStep);
        seq_numbers.push_back(current.insert_index());
    }
    return seq_numbers;
}



bool higherLevelProdCMode_NotNoneFld(const Opm::Schedule& sched,
                                     const Opm::Group& group,
                                     const size_t simStep)
{
    auto current = group;
    while (current.name() != "FIELD") {
        current = sched.getGroup(current.parent(), simStep);
        const auto& prod_cmode = group.prod_cmode();

        if (prod_cmode != Opm::Group::ProductionCMode::FLD)
            return true;

        if (prod_cmode != Opm::Group::ProductionCMode::NONE)
            return true;

    }
    return false;
}

int higherLevelInjCMode_NotNoneFld_SeqIndex(const Opm::Schedule& sched,
                       const Opm::SummaryState& sumState,
                       const Opm::Group& group,
                       const Opm::Phase& phase,
                       const size_t simStep)
{
    int ctrl_mode_not_none_fld = -1;
    auto current = group;
    while (current.name() != "FIELD" && ctrl_mode_not_none_fld < 0) {
        current = sched.getGroup(current.parent(), simStep);
        const auto& inj_cmode = (current.hasInjectionControl(phase)) ?
            current.injectionControls(phase, sumState).cmode : Opm::Group::InjectionCMode::NONE;
        if ((inj_cmode != Opm::Group::InjectionCMode::FLD) && (inj_cmode != Opm::Group::InjectionCMode::NONE)) {
            if (ctrl_mode_not_none_fld == -1) {
                ctrl_mode_not_none_fld = current.insert_index();
            }
        }
    }
    return ctrl_mode_not_none_fld;
}



namespace IGrp {
std::size_t entriesPerGroup(const std::vector<int>& inteHead)
{
    return inteHead[Opm::RestartIO::Helpers::VectorItems::NIGRPZ];
}

Opm::RestartIO::Helpers::WindowedArray<int>
allocate(const std::vector<int>& inteHead)
{
    using WV = Opm::RestartIO::Helpers::WindowedArray<int>;

    return WV {
        WV::NumWindows{ ngmaxz(inteHead) },
            WV::WindowSize{ entriesPerGroup(inteHead) }
    };
}



template <class IGrpArray>
void gconprodCMode(const Opm::Group& group,
                   const int nwgmax,
                   IGrpArray& iGrp) {
    using IGroup = ::Opm::RestartIO::Helpers::VectorItems::IGroup::index;

    const auto& prod_cmode = group.prod_cmode();
    switch (prod_cmode) {
    case Opm::Group::ProductionCMode::NONE:
        iGrp[nwgmax + IGroup::GConProdCMode] = 0;
        break;
    case Opm::Group::ProductionCMode::ORAT:
        iGrp[nwgmax + IGroup::GConProdCMode] = 1;
        break;
    case Opm::Group::ProductionCMode::WRAT:
        iGrp[nwgmax + IGroup::GConProdCMode] = 2;
        break;
    case Opm::Group::ProductionCMode::GRAT:
        iGrp[nwgmax + IGroup::GConProdCMode] = 3;
        break;
    case Opm::Group::ProductionCMode::LRAT:
        iGrp[nwgmax + IGroup::GConProdCMode] = 4;
        break;
    case Opm::Group::ProductionCMode::RESV:
        iGrp[nwgmax + IGroup::GConProdCMode] = 5;
        break;
    case Opm::Group::ProductionCMode::FLD:
        iGrp[nwgmax + IGroup::GConProdCMode] = 0; // need to be checked!!
        break;
    default:
        iGrp[nwgmax + IGroup::GConProdCMode] = 0; // need to be checked!!
    }
}


template <class IGrpArray>
void productionGroup(const Opm::Schedule&     sched,
                     const Opm::Group&        group,
                     const int                nwgmax,
                     const std::size_t        simStep,
                     const Opm::SummaryState& sumState,
                     const std::map<int, Opm::Group::ProductionCMode>& pCtrlToPCmode,
                     IGrpArray&               iGrp)
{
    using IGroup = ::Opm::RestartIO::Helpers::VectorItems::IGroup::index;
    namespace Value = ::Opm::RestartIO::Helpers::VectorItems::IGroup::Value;
    gconprodCMode(group, nwgmax, iGrp);

    if (group.name() == "FIELD") {
        iGrp[nwgmax + IGroup::GuideRateDef] = Value::GuideRateMode::None;
        iGrp[nwgmax + 7] = 0;
        return;
    }

    const auto& production_controls = group.productionControls(sumState);
    const auto& prod_guide_rate_def = production_controls.guide_rate_def;
    Opm::Group::ProductionCMode active_cmode = Opm::Group::ProductionCMode::NONE;
    {
        auto cur_prod_ctrl = sumState.get_group_var(group.name(), "GMCTP", -1);
        if (cur_prod_ctrl >= 0)
            active_cmode = pCtrlToPCmode.at(static_cast<int>(cur_prod_ctrl));
    }

#if ENABLE_GCNTL_DEBUG_OUTPUT
    else {
        // std::stringstream str;
        // str << "Current group production control is not defined for group: " << group.name() << " at timestep: " <<
        // simStep;
        std::cout << "Current group production control is not defined for group: " << group.name()
                  << " at timestep: " << simStep << std::endl;
        // throw std::invalid_argument(str.str());
    }
#endif // ENABLE_GCNTL_DEBUG_OUTPUT

    /*IGRP[NWGMAX + 5]
     - the value is determined by a relatively complex logic, a pseudo code scetch follows:
     if (group is free to respond to higher level production rate target_reinj_fraction)
         iGrp[nwgmax + 5] = 0
     else if (group::control mode > 0) (controlled by own limit)
         iGrp[nwgmax + 5] = -1
     else if (a higher level group control is active constraint)
         if (group control mode is NOT == ("FLD" OR "NONE"))
             iGrp[nwgmax + 5] = group_sequence_no_of controlling group
         else
             iGrp[nwgmax + 5] = 1
     else if (a higher level group has a control mode NOT == ("FLD" OR "NONE"))
         if (group control mode is NOT == ("FLD" OR "NONE"))
            iGrp[nwgmax + 5] = -1
         else
             iGrp[nwgmax + 5] = 1
     else if (group control mode is == ("FLD" OR "NONE"))
            iGrp[nwgmax + 5] = -1
         else
             iGrp[nwgmax + 5] = -1

    */
    // default value

    const auto& cgroup = controlGroup(sched, sumState, group, simStep);
    const auto& deck_cmode = group.prod_cmode();
    // Start branching for determining iGrp[nwgmax + 5]
    // use default value if group is not available for group control

    if (cgroup && cgroup->name() == "FIELD")
        throw std::logic_error("Got cgroup == FIELD - uncertain logic");


    iGrp[nwgmax + 5] = -1;
    if (groupProductionControllable(sched, sumState, group, simStep)) {
        // this section applies if group is controllable - i.e. has wells that may be controlled
        if (!group.productionGroupControlAvailable() && (!cgroup)) {
            // group can respond to higher level control
            iGrp[nwgmax + 5] = 0;
            goto CGROUP_DONE;
        }


        if (cgroup) {
            iGrp[nwgmax + 5] = 1;
            if (prod_guide_rate_def != Opm::Group::GuideRateProdTarget::NO_GUIDE_RATE) {
                if (deck_cmode == Opm::Group::ProductionCMode::FLD)
                    iGrp[nwgmax + 5] = cgroup->insert_index();

                if (deck_cmode == Opm::Group::ProductionCMode::NONE)
                    iGrp[nwgmax + 5] = cgroup->insert_index();
            }
            goto CGROUP_DONE;
        }


        if (higherLevelProdCMode_NotNoneFld(sched, group, simStep)) {

            if (deck_cmode == Opm::Group::ProductionCMode::FLD)
                iGrp[nwgmax] = 1;
            if (deck_cmode == Opm::Group::ProductionCMode::NONE)
                iGrp[nwgmax] = 1;

            goto CGROUP_DONE;
        }

    } else if (deck_cmode == Opm::Group::ProductionCMode::NONE) {
        iGrp[nwgmax + 5] = 1;
    }
 CGROUP_DONE:


    // Set iGrp for [nwgmax + 7]
    /*
    For the reduction option RATE the value is generally = 4

    For the reduction option NONE the values are as shown below, however, this is not a very likely case.

    = 0 for group with  "FLD" or "NONE"
    = 4 for "GRAT" FIELD
    = -40000 for production group with "ORAT"
    = -4000  for production group with "WRAT"
    = -400    for production group with "GRAT"
    = -40     for production group with "LRAT"

    Other reduction options are currently not covered in the code
    */

    if (cgroup && (group.getGroupType() != Opm::Group::GroupType::NONE)) {
        auto cgroup_control = static_cast<int>(sumState.get_group_var(cgroup->name(), "GMCTP", 0));
        iGrp[nwgmax + IGroup::ProdActiveCMode]
            = (prod_guide_rate_def != Opm::Group::GuideRateProdTarget::NO_GUIDE_RATE) ? cgroup_control : 0;
    } else {
        switch (active_cmode) {
        case Opm::Group::ProductionCMode::NONE:
            iGrp[nwgmax + IGroup::ProdActiveCMode] = 0;
            break;
        case Opm::Group::ProductionCMode::ORAT:
            iGrp[nwgmax + IGroup::ProdActiveCMode] = 1;
            break;
        case Opm::Group::ProductionCMode::WRAT:
            iGrp[nwgmax + IGroup::ProdActiveCMode] = 2;
            break;
        case Opm::Group::ProductionCMode::GRAT:
            iGrp[nwgmax + IGroup::ProdActiveCMode] = 3;
            break;
        case Opm::Group::ProductionCMode::LRAT:
            iGrp[nwgmax + IGroup::ProdActiveCMode] = 4;
            break;
        case Opm::Group::ProductionCMode::RESV:
            iGrp[nwgmax + IGroup::ProdActiveCMode] = 5;
            break;
        case Opm::Group::ProductionCMode::FLD:
            iGrp[nwgmax + IGroup::ProdActiveCMode] = 0; // need to be checked!!
            break;
        default:
            iGrp[nwgmax + IGroup::ProdActiveCMode] = 0;
        }
    }
    iGrp[nwgmax + 9] = iGrp[nwgmax + IGroup::ProdActiveCMode];

    iGrp[nwgmax + IGroup::GuideRateDef] = Value::GuideRateMode::None;

    const auto& p_exceed_act = production_controls.exceed_action;
    switch (deck_cmode) {
    case Opm::Group::ProductionCMode::NONE:
        iGrp[nwgmax + IGroup::ExceedAction] = (p_exceed_act == Opm::Group::ExceedAction::NONE) ? 0 : 4;
        break;
    case Opm::Group::ProductionCMode::ORAT:
        iGrp[nwgmax + IGroup::ExceedAction] = (p_exceed_act == Opm::Group::ExceedAction::NONE) ? -40000 : 4;
        break;
    case Opm::Group::ProductionCMode::WRAT:
        iGrp[nwgmax + IGroup::ExceedAction] = (p_exceed_act == Opm::Group::ExceedAction::NONE) ? -4000 : 4;
        break;
    case Opm::Group::ProductionCMode::GRAT:
        iGrp[nwgmax + IGroup::ExceedAction] = (p_exceed_act == Opm::Group::ExceedAction::NONE) ? -400 : 4;
        break;
    case Opm::Group::ProductionCMode::LRAT:
        iGrp[nwgmax + IGroup::ExceedAction] = (p_exceed_act == Opm::Group::ExceedAction::NONE) ? -40 : 4;
        break;
    case Opm::Group::ProductionCMode::RESV:
        iGrp[nwgmax + IGroup::ExceedAction] = (p_exceed_act == Opm::Group::ExceedAction::NONE) ? -4 : 4; // need to be checked
        break;
    case Opm::Group::ProductionCMode::FLD:
        if (cgroup && (prod_guide_rate_def != Opm::Group::GuideRateProdTarget::NO_GUIDE_RATE)) {
            iGrp[nwgmax + IGroup::GuideRateDef] = Value::GuideRateMode::Form;
        }
        iGrp[nwgmax + IGroup::ExceedAction] = (p_exceed_act == Opm::Group::ExceedAction::NONE) ? 4 : 4;
        break;
    default:
        iGrp[nwgmax + IGroup::ExceedAction] = 0;
    }
}


template <class IGrpArray>
void injectionGroup(const Opm::Schedule&     sched,
                    const Opm::Group&        group,
                    const int                nwgmax,
                    const std::size_t        simStep,
                    const Opm::SummaryState& sumState,
                    const std::map<Opm::Group::InjectionCMode, int>& cmodeToNum,
                    IGrpArray&               iGrp)
{
    using IGroup = ::Opm::RestartIO::Helpers::VectorItems::IGroup::index;
    const bool is_field = group.name() == "FIELD";
    auto group_parent_list = groupParentSeqIndex(sched, group, simStep);
    using IGroup = ::Opm::RestartIO::Helpers::VectorItems::IGroup::index;


    // set "default value" in case a group is only injection group
    if (group.isInjectionGroup() && !group.isProductionGroup()) {
        iGrp[nwgmax + 5] = 1;
    }
    // use default value if group is not available for group control
    if (groupInjectionControllable(sched, sumState, group, Opm::Phase::WATER, simStep)) {
        if ((group.hasInjectionControl(Opm::Phase::WATER)) || (group.getGroupType() == Opm::Group::GroupType::NONE)) {

            if (is_field) {
                iGrp[nwgmax + 17] = 0;
                iGrp[nwgmax + 22] = 0;
            } else {
                const double cur_winj_ctrl = sumState.get_group_var(group.name(), "GMCTW", -1);
                const auto& winj_cmode = (group.hasInjectionControl(Opm::Phase::WATER))
                    ? group.injectionControls(Opm::Phase::WATER, sumState).cmode
                    : Opm::Group::InjectionCMode::NONE;
                int higher_lev_winj_ctrl = higherLevelInjControlGroupSeqIndex(sched, sumState, group, "GMCTW", simStep);
                int higher_lev_winj_cmode
                    = higherLevelInjCMode_NotNoneFld_SeqIndex(sched, sumState, group, Opm::Phase::WATER, simStep);
                std::size_t winj_control_ind = 0;
                std::size_t inj_cmode_ind = 0;

                // WATER INJECTION GROUP CONTROL

                // Start branching for determining iGrp[nwgmax + 17]
                if (cur_winj_ctrl > 0.) {
                    iGrp[nwgmax + 17] = 0;
                }
                if (!group.injectionGroupControlAvailable(Opm::Phase::WATER) && (higher_lev_winj_ctrl <= 0)) {
                    // group can respond to higher level control
                    iGrp[nwgmax + 17] = 0;
                } else if (higher_lev_winj_ctrl > 0 || higher_lev_winj_cmode > 0) {
                    if (!((winj_cmode == Opm::Group::InjectionCMode::FLD)
                          || (winj_cmode == Opm::Group::InjectionCMode::NONE))) {
                        if (!(higher_lev_winj_ctrl == higher_lev_winj_cmode)) {

                            auto result = findInVector<std::size_t>(group_parent_list, higher_lev_winj_ctrl);
                            if (result.first) {
                                winj_control_ind = result.second;
                            } else {
                                winj_control_ind = 99999;
                            }

                            result = findInVector<std::size_t>(group_parent_list, higher_lev_winj_cmode);
                            if (result.first) {
                                inj_cmode_ind = result.second;
                            } else {
                                inj_cmode_ind = 99999;
                            }

                            if (winj_control_ind < inj_cmode_ind) {
                                iGrp[nwgmax + 17] = higher_lev_winj_ctrl;
                            } else {
                                iGrp[nwgmax + 17] = higher_lev_winj_cmode;
                            }
                            iGrp[nwgmax + 17] = higher_lev_winj_ctrl;
                        } else {
                            iGrp[nwgmax + 17] = higher_lev_winj_ctrl;
                        }
                    } else {
                        iGrp[nwgmax + 17] = 1;
                    }
                } else {
                    iGrp[nwgmax + 17] = -1;
                }
            }
            // item[nwgmax + 16] - mode for operation for water injection
            // 1 - RATE
            // 2 - RESV
            // 3 - REIN
            // 4 - VREP
            // 0 - ellers
            const auto& inj_mode = (group.hasInjectionControl(Opm::Phase::WATER))
                ? group.injectionControls(Opm::Phase::WATER, sumState).cmode
                : Opm::Group::InjectionCMode::NONE;
            const auto it = cmodeToNum.find(inj_mode);
            if (it != cmodeToNum.end()) {
                iGrp[nwgmax + IGroup::WInjCMode] = it->second;
                iGrp[nwgmax + 18] = iGrp[nwgmax + IGroup::WInjCMode];
                iGrp[nwgmax + 19] = iGrp[nwgmax + IGroup::WInjCMode];
            }
        }
    }

    // use default value if group is not available for group control
    if (groupInjectionControllable(sched, sumState, group, Opm::Phase::GAS, simStep)) {
        if ((group.hasInjectionControl(Opm::Phase::GAS)) || (group.getGroupType() == Opm::Group::GroupType::NONE)) {
            if (is_field) {
                iGrp[nwgmax + 17] = 0;
                iGrp[nwgmax + 22] = 0;
                // parameters connected to oil injection - not implemented in flow yet
                iGrp[nwgmax + 11] = 0;
                iGrp[nwgmax + 12] = 0;
            }
            else {
                const double cur_ginj_ctrl = sumState.get_group_var(group.name(), "GMCTG", -1);
                const auto& ginj_cmode = (group.hasInjectionControl(Opm::Phase::GAS))
                    ? group.injectionControls(Opm::Phase::GAS, sumState).cmode
                    : Opm::Group::InjectionCMode::NONE;

                int higher_lev_ginj_ctrl = higherLevelInjControlGroupSeqIndex(sched, sumState, group, "GMCTG", simStep);
                int higher_lev_ginj_cmode
                    = higherLevelInjCMode_NotNoneFld_SeqIndex(sched, sumState, group, Opm::Phase::GAS, simStep);
                std::size_t ginj_control_ind = 0;
                std::size_t inj_cmode_ind = 0;
                // GAS INJECTION GROUP CONTROL
                // Start branching for determining iGrp[nwgmax + 22]
                if (cur_ginj_ctrl > 0.) {
                    iGrp[nwgmax + 22] = 0;
                }
                if (!group.injectionGroupControlAvailable(Opm::Phase::GAS) && (higher_lev_ginj_ctrl <= 0)) {
                    // group can respond to higher level control
                    iGrp[nwgmax + 22] = 0;
                } else if (higher_lev_ginj_ctrl > 0 || higher_lev_ginj_cmode > 0) {
                    if (!((ginj_cmode == Opm::Group::InjectionCMode::FLD)
                          || (ginj_cmode == Opm::Group::InjectionCMode::NONE))) {
                        if (!(higher_lev_ginj_ctrl == higher_lev_ginj_cmode)) {

                            auto result = findInVector<std::size_t>(group_parent_list, higher_lev_ginj_ctrl);
                            if (result.first) {
                                ginj_control_ind = result.second;
                            } else {
                                ginj_control_ind = 99999;
                            }

                            result = findInVector<std::size_t>(group_parent_list, higher_lev_ginj_cmode);
                            if (result.first) {
                                inj_cmode_ind = result.second;
                            } else {
                                inj_cmode_ind = 99999;
                            }

                            if (ginj_control_ind < inj_cmode_ind) {
                                iGrp[nwgmax + 22] = higher_lev_ginj_ctrl;
                            } else {
                                iGrp[nwgmax + 22] = higher_lev_ginj_cmode;
                            }
                            iGrp[nwgmax + 22] = higher_lev_ginj_ctrl;
                        } else {
                            iGrp[nwgmax + 22] = higher_lev_ginj_ctrl;
                        }
                    } else {
                        iGrp[nwgmax + 22] = 1;
                    }
                } else {
                    iGrp[nwgmax + 22] = -1;
                }
            }

            // item[nwgmax + 21] - mode for operation for gas injection
            // 1 - RATE
            // 2 - RESV
            // 3 - REIN
            // 4 - VREP
            // 0 - ellers
            const auto& inj_mode = (group.hasInjectionControl(Opm::Phase::GAS))
                ? group.injectionControls(Opm::Phase::GAS, sumState).cmode
                : Opm::Group::InjectionCMode::NONE;
            const auto it = cmodeToNum.find(inj_mode);
            if (it != cmodeToNum.end()) {
                iGrp[nwgmax + IGroup::GInjCMode] = it->second;
                iGrp[nwgmax + 23] = iGrp[nwgmax + IGroup::GInjCMode];
                iGrp[nwgmax + 24] = iGrp[nwgmax + IGroup::GInjCMode];
            }
        }
    }
}

template <class IGrpArray>
void storeGroupTree(const Opm::Schedule& sched,
                    const Opm::Group& group,
                    const int nwgmax,
                    const int ngmaxz,
                    const std::size_t simStep,
                    IGrpArray& iGrp) {

    namespace Value = ::Opm::RestartIO::Helpers::VectorItems::IGroup::Value;
    using IGroup = ::Opm::RestartIO::Helpers::VectorItems::IGroup::index;
    const bool is_field = group.name() == "FIELD";

    // Store index of all child wells or child groups.
    if (group.wellgroup()) {
        int igrpCount = 0;
        for (const auto& well_name : group.wells()) {
            const auto& well = sched.getWell(well_name, simStep);
            iGrp[igrpCount] = well.seqIndex() + 1;
            igrpCount += 1;
        }
        iGrp[nwgmax] = group.wells().size();
        iGrp[nwgmax + IGroup::GroupType] = Value::GroupType::WellGroup;
    } else  {
        int igrpCount = 0;
        for (const auto& group_name : group.groups()) {
            const auto& child_group = sched.getGroup(group_name, simStep);
            iGrp[igrpCount] = child_group.insert_index();
            igrpCount += 1;
        }
        iGrp[nwgmax] = group.groups().size();
        iGrp[nwgmax + IGroup::GroupType] = Value::GroupType::TreeGroup;
    }


    // Store index of parent group
    if (is_field)
        iGrp[nwgmax + IGroup::ParentGroup] = 0;
    else {
        const auto& parent_group = sched.getGroup(group.parent(), simStep);
        if (parent_group.name() == "FIELD")
            iGrp[nwgmax + IGroup::ParentGroup] = ngmaxz;
        else
            iGrp[nwgmax + IGroup::ParentGroup] = parent_group.insert_index();
    }

    iGrp[nwgmax + IGroup::GroupLevel] = currentGroupLevel(sched, group, simStep);
}


template <class IGrpArray>
void storeFlowingWells(const Opm::Group&        group,
                       const int                nwgmax,
                       const Opm::SummaryState& sumState,
                       IGrpArray&               iGrp) {
    using IGroup = ::Opm::RestartIO::Helpers::VectorItems::IGroup::index;
    const bool is_field = group.name() == "FIELD";
    const double g_act_pwells = is_field ? sumState.get("FMWPR", 0) : sumState.get_group_var(group.name(), "GMWPR", 0);
    const double g_act_iwells = is_field ? sumState.get("FMWIN", 0) : sumState.get_group_var(group.name(), "GMWIN", 0);
    iGrp[nwgmax + IGroup::FlowingWells] = static_cast<int>(g_act_pwells) + static_cast<int>(g_act_iwells);
}


template <class IGrpArray>
void staticContrib(const Opm::Schedule&     sched,
                   const Opm::Group&        group,
                   const int                nwgmax,
                   const int                ngmaxz,
                   const std::size_t        simStep,
                   const Opm::SummaryState& sumState,
                   const std::map<int, Opm::Group::ProductionCMode>& pCtrlToPCmode,
                   const std::map<Opm::Group::InjectionCMode, int>& cmodeToNum,
                   IGrpArray&               iGrp)
{
    const bool is_field = group.name() == "FIELD";

    storeGroupTree(sched, group, nwgmax, ngmaxz, simStep, iGrp);
    storeFlowingWells(group, nwgmax, sumState, iGrp);

    iGrp[nwgmax + 17] = -1;
    iGrp[nwgmax + 22] = -1;

    // Treat al groups which are *not* pure injection groups.
    if (group.getGroupType() != Opm::Group::GroupType::INJECTION)
        productionGroup(sched, group, nwgmax, simStep, sumState, pCtrlToPCmode, iGrp);

    // Treat al groups which are *not* pure production groups.
    if (group.getGroupType() != Opm::Group::GroupType::PRODUCTION)
        injectionGroup(sched, group, nwgmax, simStep, sumState, cmodeToNum, iGrp);

    if (is_field)
    {
        //the maximum number of groups in the model
        iGrp[nwgmax+88] = ngmaxz;
        iGrp[nwgmax+89] = ngmaxz;
        iGrp[nwgmax+95] = ngmaxz;
        iGrp[nwgmax+96] = ngmaxz;
    }
    else
    {
        //parameters connected to oil injection - not implemented in flow yet
        iGrp[nwgmax+11] = 0;
        iGrp[nwgmax+12] = -1;

        //assign values to group number (according to group sequence)
        iGrp[nwgmax+88] = group.insert_index();
        iGrp[nwgmax+89] = group.insert_index();
        iGrp[nwgmax+95] = group.insert_index();
        iGrp[nwgmax+96] = group.insert_index();
    }
}
} // Igrp

namespace SGrp {
std::size_t entriesPerGroup(const std::vector<int>& inteHead)
{
    return inteHead[Opm::RestartIO::Helpers::VectorItems::NSGRPZ];
}

Opm::RestartIO::Helpers::WindowedArray<float>
allocate(const std::vector<int>& inteHead)
{
    using WV = Opm::RestartIO::Helpers::WindowedArray<float>;

    return WV {
        WV::NumWindows{ ngmaxz(inteHead) },
            WV::WindowSize{ entriesPerGroup(inteHead) }
    };
}

template <class SGrpArray>
void staticContrib(const Opm::Group&        group,
                   const Opm::SummaryState& sumState,
                   const Opm::UnitSystem& units,
                   SGrpArray&               sGrp)
{
    using Isp = ::Opm::RestartIO::Helpers::VectorItems::SGroup::prod_index;
    using Isi = ::Opm::RestartIO::Helpers::VectorItems::SGroup::inj_index;
    using M = ::Opm::UnitSystem::measure;
    
    const auto dflt   = -1.0e+20f;
    const auto dflt_2 = -2.0e+20f;
    const auto infty  =  1.0e+20f;
    const auto zero   =  0.0f;
    const auto one    =  1.0f;

    const auto init = std::vector<float> { // 112 Items (0..111)
                                          // 0     1      2      3      4
                                          infty, infty, dflt , infty,  zero ,     //   0..  4  ( 0)
                                          zero , infty, infty, infty , infty,     //   5..  9  ( 1)
                                          infty, infty, infty, infty , dflt ,     //  10.. 14  ( 2)
                                          infty, infty, infty, infty , dflt ,     //  15.. 19  ( 3)
                                          infty, infty, infty, infty , dflt ,     //  20.. 24  ( 4)
                                          zero , zero , zero , dflt_2, zero ,     //  24.. 29  ( 5)
                                          zero , zero , zero , zero  , zero ,     //  30.. 34  ( 6)
                                          infty ,zero , zero , zero  , infty,     //  35.. 39  ( 7)
                                          zero , zero , zero , zero  , zero ,     //  40.. 44  ( 8)
                                          zero , zero , zero , zero  , zero ,     //  45.. 49  ( 9)
                                          zero , infty, infty, infty , infty,     //  50.. 54  (10)
                                          infty, infty, infty, infty , infty,     //  55.. 59  (11)
                                          infty, infty, infty, infty , infty,     //  60.. 64  (12)
                                          infty, infty, infty, infty , zero ,     //  65.. 69  (13)
                                          zero , zero , zero , zero  , zero ,     //  70.. 74  (14)
                                          zero , zero , zero , zero  , infty,     //  75.. 79  (15)
                                          infty, zero , infty, zero  , zero ,     //  80.. 84  (16)
                                          zero , zero , zero , zero  , zero ,     //  85.. 89  (17)
                                          zero , zero , one  , zero  , zero ,     //  90.. 94  (18)
                                          zero , zero , zero , zero  , zero ,     //  95.. 99  (19)
                                          zero , zero , zero , zero  , zero ,     // 100..104  (20)
                                          zero , zero , zero , zero  , zero ,     // 105..109  (21)
                                          zero , zero                             // 110..111  (22)
    };

    const auto sz = static_cast<
        decltype(init.size())>(sGrp.size());

    auto b = std::begin(init);
    auto e = b + std::min(init.size(), sz);

    std::copy(b, e, std::begin(sGrp));
    
    auto sgprop = [&units](const M u, const double x) -> float
    {
        return static_cast<float>(units.from_si(u, x));
    };
    
    if (group.isProductionGroup()) {
        const auto& prod_cntl = group.productionControls(sumState);
        if (prod_cntl.oil_target > 0.) {
            sGrp[Isp::OilRateLimit] = sgprop(M::liquid_surface_rate, prod_cntl.oil_target);
            sGrp[52] = sGrp[Isp::OilRateLimit];  // "ORAT" control
            printf("Writing OILRATE: %lg -> %lg \n", prod_cntl.oil_target, sGrp[52]);
        }
        if (prod_cntl.water_target > 0.) {
            sGrp[Isp::WatRateLimit] = sgprop(M::liquid_surface_rate, prod_cntl.water_target);
            sGrp[53] = sGrp[Isp::WatRateLimit];  //"WRAT" control
        }
        if (prod_cntl.gas_target > 0.) {
            sGrp[Isp::GasRateLimit] = sgprop(M::gas_surface_rate, prod_cntl.gas_target);
            sGrp[39] = sGrp[Isp::GasRateLimit];
        }
        if (prod_cntl.liquid_target > 0.) {
            sGrp[Isp::LiqRateLimit] = sgprop(M::liquid_surface_rate, prod_cntl.liquid_target);
            sGrp[54] = sGrp[Isp::LiqRateLimit];  //"LRAT" control
        }
    }

    if ((group.name() == "FIELD") && (group.getGroupType() == Opm::Group::GroupType::NONE)) {
          sGrp[Isp::GuideRate] = 0.;
          sGrp[14] = 0.;
          sGrp[19] = 0.;
          sGrp[24] = 0.;
    }
    
    if (group.isInjectionGroup()) {
        if (group.hasInjectionControl(Opm::Phase::GAS)) {
            const auto& inj_cntl = group.injectionControls(Opm::Phase::GAS, sumState);
            if (inj_cntl.surface_max_rate > 0.) {
                sGrp[Isi::gasSurfRateLimit] = sgprop(M::gas_surface_rate, inj_cntl.surface_max_rate);
                sGrp[65] =  sGrp[Isi::gasSurfRateLimit];
            }
            if (inj_cntl.resv_max_rate > 0.) {
                sGrp[Isi::gasResRateLimit] = sgprop(M::rate, inj_cntl.resv_max_rate);
                sGrp[66] =  sGrp[Isi::gasResRateLimit];
            }
            if (inj_cntl.target_reinj_fraction > 0.) {
                sGrp[Isi::gasReinjectionLimit] = inj_cntl.target_reinj_fraction;
                sGrp[67] =  sGrp[Isi::gasReinjectionLimit];
            }
            if (inj_cntl.target_void_fraction > 0.) {
                sGrp[Isi::gasVoidageLimit] = inj_cntl.target_void_fraction;
                sGrp[68] =  sGrp[Isi::gasVoidageLimit];
            }
        }

        if (group.hasInjectionControl(Opm::Phase::WATER)) {
            const auto& inj_cntl = group.injectionControls(Opm::Phase::WATER, sumState);
            if (inj_cntl.surface_max_rate > 0.) {
                sGrp[Isi::waterSurfRateLimit] = sgprop(M::liquid_surface_rate, inj_cntl.surface_max_rate);
                sGrp[61] =  sGrp[Isi::waterSurfRateLimit];
            }
            if (inj_cntl.resv_max_rate > 0.) {
                sGrp[Isi::waterResRateLimit] = sgprop(M::rate, inj_cntl.resv_max_rate);
                sGrp[62] =  sGrp[Isi::waterResRateLimit];
            }
            if (inj_cntl.target_reinj_fraction > 0.) {
                sGrp[Isi::waterReinjectionLimit] = inj_cntl.target_reinj_fraction;
                sGrp[63] =  sGrp[Isi::waterReinjectionLimit];
            }
            if (inj_cntl.target_void_fraction > 0.) {
                sGrp[Isi::waterVoidageLimit] = inj_cntl.target_void_fraction;
                sGrp[64] =  sGrp[Isi::waterVoidageLimit];
            }
        }

        if (group.hasInjectionControl(Opm::Phase::OIL)) {
            const auto& inj_cntl = group.injectionControls(Opm::Phase::OIL, sumState);
            if (inj_cntl.surface_max_rate > 0.) {
                sGrp[Isi::oilSurfRateLimit] = sgprop(M::liquid_surface_rate, inj_cntl.surface_max_rate);
                sGrp[57] =  sGrp[Isi::oilSurfRateLimit];
            }
            if (inj_cntl.resv_max_rate > 0.) {
                sGrp[Isi::oilResRateLimit] = sgprop(M::rate, inj_cntl.resv_max_rate);
                sGrp[58] =  sGrp[Isi::oilResRateLimit];
            }
            if (inj_cntl.target_reinj_fraction > 0.) {
                sGrp[Isi::oilReinjectionLimit] = inj_cntl.target_reinj_fraction;
                sGrp[59] =  sGrp[Isi::oilReinjectionLimit];
            }
            if (inj_cntl.target_void_fraction > 0.) {
                sGrp[Isi::oilVoidageLimit] = inj_cntl.target_void_fraction;
                sGrp[60] =  sGrp[Isi::oilVoidageLimit];
            }
        }

    }
}
} // SGrp

namespace XGrp {
std::size_t entriesPerGroup(const std::vector<int>& inteHead)
{
    return inteHead[Opm::RestartIO::Helpers::VectorItems::NXGRPZ];
}

Opm::RestartIO::Helpers::WindowedArray<double>
allocate(const std::vector<int>& inteHead)
{
    using WV = Opm::RestartIO::Helpers::WindowedArray<double>;

    return WV {
        WV::NumWindows{ ngmaxz(inteHead) },
            WV::WindowSize{ entriesPerGroup(inteHead) }
    };
}

// here define the dynamic group quantities to be written to the restart file
template <class XGrpArray>
void dynamicContrib(const std::vector<std::string>&      restart_group_keys,
                    const std::vector<std::string>&      restart_field_keys,
                    const std::map<std::string, size_t>& groupKeyToIndex,
                    const std::map<std::string, size_t>& fieldKeyToIndex,
                    const Opm::Group&                    group,
                    const Opm::SummaryState&             sumState,
                    XGrpArray&                           xGrp)
{
    using Ix = ::Opm::RestartIO::Helpers::VectorItems::XGroup::index;

    std::string groupName = group.name();
    const std::vector<std::string>& keys = (groupName == "FIELD")
        ? restart_field_keys : restart_group_keys;
    const std::map<std::string, size_t>& keyToIndex = (groupName == "FIELD")
        ? fieldKeyToIndex : groupKeyToIndex;

    for (const auto& key : keys) {
        std::string compKey = (groupName == "FIELD")
            ? key : key + ":" + groupName;

        if (sumState.has(compKey)) {
            double keyValue = sumState.get(compKey);
            const auto itr = keyToIndex.find(key);
            xGrp[itr->second] = keyValue;
        }
    }

    xGrp[Ix::OilPrGuideRate_2]  = xGrp[Ix::OilPrGuideRate];
    xGrp[Ix::WatPrGuideRate_2]  = xGrp[Ix::WatPrGuideRate];
    xGrp[Ix::GasPrGuideRate_2]  = xGrp[Ix::GasPrGuideRate];
    xGrp[Ix::VoidPrGuideRate_2] = xGrp[Ix::VoidPrGuideRate];

    xGrp[Ix::WatInjGuideRate_2] = xGrp[Ix::WatInjGuideRate];
}
} // XGrp

namespace ZGrp {
std::size_t entriesPerGroup(const std::vector<int>& inteHead)
{
    return inteHead[Opm::RestartIO::Helpers::VectorItems::NZGRPZ];
}

Opm::RestartIO::Helpers::WindowedArray<
    Opm::EclIO::PaddedOutputString<8>
    >
allocate(const std::vector<int>& inteHead)
{
    using WV = Opm::RestartIO::Helpers::WindowedArray<
        Opm::EclIO::PaddedOutputString<8>
        >;

    return WV {
        WV::NumWindows{ ngmaxz(inteHead) },
            WV::WindowSize{ entriesPerGroup(inteHead) }
    };
}

template <class ZGroupArray>
void staticContrib(const Opm::Group& group, ZGroupArray& zGroup)
{
    zGroup[0] = group.name();
}
} // ZGrp
} // Anonymous



// =====================================================================

Opm::RestartIO::Helpers::AggregateGroupData::
AggregateGroupData(const std::vector<int>& inteHead)
    : iGroup_ (IGrp::allocate(inteHead))
    , sGroup_ (SGrp::allocate(inteHead))
    , xGroup_ (XGrp::allocate(inteHead))
    , zGroup_ (ZGrp::allocate(inteHead))
    , nWGMax_ (nwgmax(inteHead))
    , nGMaxz_ (ngmaxz(inteHead))
{}

// ---------------------------------------------------------------------

void
Opm::RestartIO::Helpers::AggregateGroupData::
captureDeclaredGroupData(const Opm::Schedule&                 sched,
                         const Opm::UnitSystem&               units,
                         const std::size_t                    simStep,
                         const Opm::SummaryState&             sumState,
                         const std::vector<int>&              inteHead)
{
    const auto& curGroups = sched.restart_groups(simStep);

    groupLoop(curGroups, [&sched, simStep, sumState, this]
              (const Group& group, const std::size_t groupID) -> void
                         {
                             auto ig = this->iGroup_[groupID];

                             IGrp::staticContrib(sched, group, this->nWGMax_, this->nGMaxz_,
                                                 simStep, sumState, this->PCntlModeToPCMode, this->cmodeToNum, ig);
                         });

    // Define Static Contributions to SGrp Array.
    groupLoop(curGroups,
              [&sumState, &units, this](const Group& group , const std::size_t groupID) -> void
              {
                  auto sw = this->sGroup_[groupID];
                  SGrp::staticContrib(group, sumState, units, sw);
              });

    // Define Dynamic Contributions to XGrp Array.
    groupLoop(curGroups, [&sumState, this]
              (const Group& group, const std::size_t groupID) -> void
                         {
                             auto xg = this->xGroup_[groupID];

                             XGrp::dynamicContrib(this->restart_group_keys, this->restart_field_keys,
                                                  this->groupKeyToIndex, this->fieldKeyToIndex, group,
                                                  sumState, xg);
                         });

    // Define Static Contributions to ZGrp Array.
    groupLoop(curGroups, [this, &inteHead]
              (const Group& group, const std::size_t /* groupID */) -> void
                         {
                             std::size_t group_index = group.insert_index() - 1;
                             if (group.name() == "FIELD")
                                 group_index = ngmaxz(inteHead) - 1;
                             auto zg = this->zGroup_[ group_index ];

                             ZGrp::staticContrib(group, zg);
                         });
}
