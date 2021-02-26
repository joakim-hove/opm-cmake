/*
  Copyright 2021 Equinor ASA.

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
#include <fmt/format.h>

#include <opm/parser/eclipse/EclipseState/Schedule/ScheduleState.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/WellTestConfig.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Group/GConSump.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Group/GConSale.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/VFPProdTable.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/VFPInjTable.hpp>


namespace Opm {

namespace {

/*
  This is to ensure that only time_points which can be represented with
  std::time_t are used. The reason for clamping to std::time_t resolution is
  that the serialization code in
  opm-simulators:opm/simulators/utils/ParallelRestart.cpp goes via std::time_t.
*/
time_point clamp_time(time_point t) {
    return TimeService::from_time_t( TimeService::to_time_t( t ) );
}

}



ScheduleState::ScheduleState(const time_point& t1):
    m_start_time(clamp_time(t1))
{
}

ScheduleState::ScheduleState(const time_point& start_time, const time_point& end_time) :
    ScheduleState(start_time)
{
    this->m_end_time = clamp_time(end_time);
}

ScheduleState::ScheduleState(const ScheduleState& src, const time_point& start_time) :
    ScheduleState(src)
{
    this->m_start_time = clamp_time(start_time);
    this->m_end_time = std::nullopt;

    this->m_events.reset();
    this->m_wellgroup_events.reset();
    this->m_geo_keywords.clear();
    this->target_wellpi.clear();

    auto next_rft = this->rft_config().next();
    if (next_rft.has_value())
        this->rft_config.update( std::move(*next_rft) );
}

ScheduleState::ScheduleState(const ScheduleState& src, const time_point& start_time, const time_point& end_time) :
    ScheduleState(src, start_time)
{
    this->m_end_time = end_time;
}


time_point ScheduleState::start_time() const {
    return this->m_start_time;
}

time_point ScheduleState::end_time() const {
    return this->m_end_time.value();
}

void ScheduleState::update_nupcol(int nupcol) {
    this->m_nupcol = nupcol;
}

int ScheduleState::nupcol() const {
    return this->m_nupcol;
}

void ScheduleState::update_oilvap(OilVaporizationProperties oilvap) {
    this->m_oilvap = std::move(oilvap);
}

const OilVaporizationProperties& ScheduleState::oilvap() const {
    return this->m_oilvap;
}

OilVaporizationProperties& ScheduleState::oilvap() {
    return this->m_oilvap;
}

void ScheduleState::update_geo_keywords(std::vector<DeckKeyword> geo_keywords) {
    this->m_geo_keywords = std::move(geo_keywords);
}

std::vector<DeckKeyword>& ScheduleState::geo_keywords() {
    return this->m_geo_keywords;
}

const std::vector<DeckKeyword>& ScheduleState::geo_keywords() const {
    return this->m_geo_keywords;
}

void ScheduleState::update_message_limits(MessageLimits message_limits) {
    this->m_message_limits = std::move(message_limits);
}

const MessageLimits& ScheduleState::message_limits() const {
    return this->m_message_limits;
}

MessageLimits& ScheduleState::message_limits() {
    return this->m_message_limits;
}

Well::ProducerCMode ScheduleState::whistctl() const {
    return this->m_whistctl_mode;
}

void ScheduleState::update_whistctl(Well::ProducerCMode whistctl) {
    this->m_whistctl_mode = whistctl;
}

bool ScheduleState::operator==(const ScheduleState& other) const {

    return this->m_start_time == other.m_start_time &&
           this->m_oilvap == other.m_oilvap &&
           this->target_wellpi == other.target_wellpi &&
           this->m_tuning == other.m_tuning &&
           this->m_end_time == other.m_end_time &&
           this->m_events == other.m_events &&
           this->m_wellgroup_events == other.m_wellgroup_events &&
           this->m_geo_keywords == other.m_geo_keywords &&
           this->m_message_limits == other.m_message_limits &&
           this->m_whistctl_mode == other.m_whistctl_mode &&
           this->m_nupcol == other.m_nupcol &&
           this->wtest_config.get() == other.wtest_config.get() &&
           this->well_order.get() == other.well_order.get() &&
           this->group_order.get() == other.group_order.get() &&
           this->gconsale.get() == other.gconsale.get() &&
           this->gconsump.get() == other.gconsump.get() &&
           this->wlist_manager.get() == other.wlist_manager.get() &&
           this->rpt_config.get() == other.rpt_config.get() &&
           this->udq_active.get() == other.udq_active.get() &&
           this->glo.get() == other.glo.get() &&
           this->guide_rate.get() == other.guide_rate.get() &&
           this->rft_config.get() == other.rft_config.get() &&
           this->udq.get() == other.udq.get() &&
           this->wells == other.wells &&
           this->groups == other.groups &&
           this->vfpprod == other.vfpprod &&
           this->vfpinj == other.vfpinj;
}



ScheduleState ScheduleState::serializeObject() {
    auto t1 = TimeService::now();
    auto t2 = t1 + std::chrono::hours(48);
    ScheduleState ts(t1, t2);
    ts.vfpprod = map_member<int, VFPProdTable>::serializeObject();
    ts.vfpinj = map_member<int, VFPInjTable>::serializeObject();
    ts.groups = map_member<std::string, Group>::serializeObject();
    ts.m_events = Events::serializeObject();
    ts.update_nupcol(77);
    ts.update_oilvap( Opm::OilVaporizationProperties::serializeObject() );
    ts.m_message_limits = MessageLimits::serializeObject();
    ts.m_whistctl_mode = Well::ProducerCMode::THP;
    ts.target_wellpi = {{"WELL1", 1000}, {"WELL2", 2000}};

    ts.pavg.update( PAvg::serializeObject() );
    ts.wtest_config.update( WellTestConfig::serializeObject() );
    ts.gconsump.update( GConSump::serializeObject() );
    ts.gconsale.update( GConSale::serializeObject() );
    ts.wlist_manager.update( WListManager::serializeObject() );
    ts.rpt_config.update( RPTConfig::serializeObject() );
    ts.actions.update( Action::Actions::serializeObject() );
    ts.udq_active.update( UDQActive::serializeObject() );
    ts.network.update( Network::ExtNetwork::serializeObject() );
    ts.well_order.update( NameOrder::serializeObject() );
    ts.group_order.update( GroupOrder::serializeObject() );
    ts.udq.update( UDQConfig::serializeObject() );
    ts.guide_rate.update( GuideRateConfig::serializeObject() );
    ts.glo.update( GasLiftOpt::serializeObject() );
    ts.rft_config.update( RFTConfig::serializeObject() );

    return ts;
}

void ScheduleState::update_tuning(Tuning tuning) {
    this->m_tuning = std::move(tuning);
}

const Tuning& ScheduleState::tuning() const {
    return this->m_tuning;
}

Tuning& ScheduleState::tuning() {
    return this->m_tuning;
}

void ScheduleState::update_events(Events events) {
    this->m_events = events;
}

Events& ScheduleState::events() {
    return this->m_events;
}


const Events& ScheduleState::events() const {
    return this->m_events;
}

void ScheduleState::update_wellgroup_events(WellGroupEvents wgevents) {
    this->m_wellgroup_events = std::move(wgevents);
}

WellGroupEvents& ScheduleState::wellgroup_events() {
    return this->m_wellgroup_events;
}

const WellGroupEvents& ScheduleState::wellgroup_events() const {
    return this->m_wellgroup_events;
}

}
