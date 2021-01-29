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

#ifndef SCHEDULE_TSTEP_HPP
#define SCHEDULE_TSTEP_HPP

#include <chrono>
#include <memory>
#include <optional>

#include <opm/parser/eclipse/Deck/DeckKeyword.hpp>

#include <opm/parser/eclipse/EclipseState/Schedule/RPTConfig.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/PAvg.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Tuning.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/OilVaporizationProperties.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Events.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/Well.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/NameOrder.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/WListManager.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/MessageLimits.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Group/GConSump.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Group/GConSale.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Network/ExtNetwork.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/VFPProdTable.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/VFPInjTable.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Action/Actions.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/UDQ/UDQActive.hpp>

namespace Opm {

    /*
      The purpose of the ScheduleState class is to hold the entire Schedule
      information, i.e. wells and groups and so on, at exactly one point in
      time. The ScheduleState class itself has no dynamic behavior, the dynamics
      is handled by the Schedule instance owning the ScheduleState instance.
    */

    class WellTestConfig;



    class ScheduleState {
    public:
        /*
          In the SCHEDULE section typically all information is a function of
          time, and the ScheduleState class is used to manage a snapshot of
          state at one point in time. Typically a large part of the
          configuration does not change between timesteps and consecutive
          ScheduleState instances are very similar, to handle this many of the
          ScheduleState members are implemented as std::shared_ptr<>s.

          The ptr_member<T> class is a small wrapper around the
          std::shared_ptr<T>. The ptr_member<T> class is meant to be internal to
          the Schedule implementation and downstream should only access this
          indirectly like e.g.

             const auto& gconsum = sched_state.gconsump();

          The remaining details of the ptr_member<T> class are heavily
          influenced by the code used to serialize the Schedule information.
         */



        template <typename T>
        class ptr_member {
        public:
            const T& get() const {
                return *this->m_data;
            }

            /*
              This will allocate new storage and assign @object to the new
              storage.
            */
            void update(T object)
            {
                this->m_data = std::make_shared<T>( std::move(object) );
            }

            /*
              Will reassign the pointer to point to existing shared instance
              @other.
            */
            void update(const ptr_member<T>& other)
            {
                this->m_data = other.m_data;
            }

            const T& operator()() const {
                return *this->m_data;
            }

        private:
            std::shared_ptr<T> m_data;
        };

        ScheduleState() = default;
        explicit ScheduleState(const std::chrono::system_clock::time_point& start_time);
        ScheduleState(const std::chrono::system_clock::time_point& start_time, const std::chrono::system_clock::time_point& end_time);
        ScheduleState(const ScheduleState& src, const std::chrono::system_clock::time_point& start_time);
        ScheduleState(const ScheduleState& src, const std::chrono::system_clock::time_point& start_time, const std::chrono::system_clock::time_point& end_time);


        std::chrono::system_clock::time_point start_time() const;
        std::chrono::system_clock::time_point end_time() const;
        ScheduleState next(const std::chrono::system_clock::time_point& next_start);

        bool operator==(const ScheduleState& other) const;
        static ScheduleState serializeObject();

        void update_tuning(Tuning tuning);
        Tuning& tuning();
        const Tuning& tuning() const;

        void update_nupcol(int nupcol);
        int nupcol() const;

        void update_oilvap(OilVaporizationProperties oilvap);
        const OilVaporizationProperties& oilvap() const;
        OilVaporizationProperties& oilvap();

        void update_events(Events events);
        Events& events();
        const Events& events() const;

        void update_wellgroup_events(WellGroupEvents wgevents);
        WellGroupEvents& wellgroup_events();
        const WellGroupEvents& wellgroup_events() const;

        void update_geo_keywords(std::vector<DeckKeyword> geo_keywords);
        std::vector<DeckKeyword>& geo_keywords();
        const std::vector<DeckKeyword>& geo_keywords() const;

        void update_message_limits(MessageLimits message_limits);
        MessageLimits& message_limits();
        const MessageLimits& message_limits() const;

        Well::ProducerCMode whistctl() const;
        void update_whistctl(Well::ProducerCMode whistctl);

        /*********************************************************************/

        ptr_member<PAvg> pavg;
        ptr_member<WellTestConfig> wtest_config;
        ptr_member<GConSale> gconsale;
        ptr_member<GConSump> gconsump;
        ptr_member<WListManager> wlist_manager;
        ptr_member<Network::ExtNetwork> network;
        ptr_member<RPTConfig> rpt_config;
        ptr_member<Action::Actions> actions;
        ptr_member<UDQActive> udq_active;
        ptr_member<NameOrder> well_order;
        ptr_member<GroupOrder> group_order;

        template <typename T>
        ptr_member<T>& get() {
            if constexpr ( std::is_same_v<T, PAvg> )
                             return this->pavg;
            else if constexpr ( std::is_same_v<T, WellTestConfig> )
                                  return this->wtest_config;
            else if constexpr ( std::is_same_v<T, GConSale> )
                                  return this->gconsale;
            else if constexpr ( std::is_same_v<T, GConSump> )
                                  return this->gconsump;
            else if constexpr ( std::is_same_v<T, WListManager> )
                                  return this->wlist_manager;
            else if constexpr ( std::is_same_v<T, Network::ExtNetwork> )
                                  return this->network;
            else if constexpr ( std::is_same_v<T, RPTConfig> )
                                  return this->rpt_config;
            else if constexpr ( std::is_same_v<T, Action::Actions> )
                                  return this->actions;
            else if constexpr ( std::is_same_v<T, UDQActive> )
                                  return this->udq_active;
            else if constexpr ( std::is_same_v<T, NameOrder> )
                                  return this->well_order;
            else if constexpr ( std::is_same_v<T, GroupOrder> )
                                  return this->group_order;
        }

        template <typename T>
        void update_ptr(ScheduleState& other) {
            auto& member = this->get<T>();
            member.update( other.get<T>() );
        }


        std::vector<std::reference_wrapper<const VFPProdTable>> vfpprod() const;
        const VFPProdTable& vfpprod(int table_id) const;
        void update_vfpprod(VFPProdTable vfpprod);
        std::optional<std::reference_wrapper<const VFPProdTable>> try_vfpprod(int table_id) const;

        std::vector<std::reference_wrapper<const VFPInjTable>> vfpinj() const;
        const VFPInjTable& vfpinj(int table_id) const;
        void update_vfpinj(VFPInjTable vfpinj);
        std::optional<std::reference_wrapper<const VFPInjTable>> try_vfpinj(int table_id) const;

        template<class Serializer>
        void serializeOp(Serializer& serializer) {
            serializer(m_start_time);
            serializer(m_end_time);
            m_tuning.serializeOp(serializer);
            serializer(m_nupcol);
            m_oilvap.serializeOp(serializer);
            m_events.serializeOp(serializer);
            m_wellgroup_events.serializeOp(serializer);
            serializer.vector(m_geo_keywords);
            m_message_limits.serializeOp(serializer);
            serializer(m_whistctl_mode);
            serializer.map(m_vfpprod);
            serializer.map(m_vfpinj);
        }


    private:
        std::chrono::system_clock::time_point m_start_time;
        std::optional<std::chrono::system_clock::time_point> m_end_time;

        Tuning m_tuning;
        int m_nupcol;
        OilVaporizationProperties m_oilvap;
        Events m_events;
        WellGroupEvents m_wellgroup_events;
        std::vector<DeckKeyword> m_geo_keywords;
        MessageLimits m_message_limits;
        Well::ProducerCMode m_whistctl_mode = Well::ProducerCMode::CMODE_UNDEFINED;
        std::map<int, std::shared_ptr<VFPProdTable>> m_vfpprod;
        std::map<int, std::shared_ptr<VFPInjTable>> m_vfpinj;
    };
}

#endif
