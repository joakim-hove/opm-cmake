/*
  Copyright 2013 Statoil ASA.

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

#include <opm/common/OpmLog/OpmLog.hpp>

#include <opm/parser/eclipse/Deck/DeckRecord.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Connection.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/WellConnections.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/DynamicState.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/MSW/SegmentSet.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/TimeMap.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/MSW/updatingConnectionsWithSegments.hpp>

#include <ert/ecl/ecl_grid.h>


namespace Opm {

    Well::Well(const std::string& name_, int headI,
               int headJ, double refDepth , Phase preferredPhase,
               const TimeMap& timeMap, size_t creationTimeStep,
               WellCompletion::CompletionOrderEnum completionOrdering,
               bool allowCrossFlow, bool automaticShutIn)
        : m_creationTimeStep( creationTimeStep ),
          m_name( name_ ),
          m_status( timeMap, WellCommon::SHUT ),
          m_isAvailableForGroupControl( timeMap, true ),
          m_guideRate( timeMap, -1.0 ),
          m_guideRatePhase( timeMap, GuideRate::UNDEFINED ),
          m_guideRateScalingFactor( timeMap, 1.0 ),
          m_efficiencyFactors (timeMap, 1.0 ),
          m_isProducer( timeMap, true ) ,
          connections( timeMap, std::make_shared<WellConnections>(headI, headJ) ),
          m_productionProperties( timeMap, WellProductionProperties() ),
          m_injectionProperties( timeMap, WellInjectionProperties() ),
          m_polymerProperties( timeMap, WellPolymerProperties() ),
          m_econproductionlimits( timeMap, WellEconProductionLimits() ),
          m_solventFraction( timeMap, 0.0 ),
          m_groupName( timeMap, "" ),
          m_rft( timeMap, false ),
          m_plt( timeMap, false ),
          m_headI( timeMap, headI ),
          m_headJ( timeMap, headJ ),
          m_refDepth( timeMap, refDepth ),
          m_preferredPhase(preferredPhase),
          m_comporder(completionOrdering),
          m_allowCrossFlow(allowCrossFlow),
          m_automaticShutIn(automaticShutIn),
          m_segmentset( timeMap, SegmentSet{} ),
          timesteps( timeMap.numTimesteps() ),
          events( timeMap )
    {
        addEvent( ScheduleEvents::NEW_WELL , creationTimeStep );
    }

    const std::string& Well::name() const {
        return m_name;
    }


    void Well::switchToProducer( size_t timeStep) {
        WellInjectionProperties p = getInjectionPropertiesCopy(timeStep);

        p.BHPLimit = 0;
        p.dropInjectionControl( Opm::WellInjector::BHP );
        setInjectionProperties( timeStep , p );
    }


    void Well::switchToInjector( size_t timeStep) {
        WellProductionProperties p = getProductionPropertiesCopy(timeStep);

        p.BHPLimit = 0;
        p.dropProductionControl( Opm::WellProducer::BHP );
        setProductionProperties( timeStep , p );
    }


    bool Well::operator==(const Well& other) const {
        return this->m_creationTimeStep == other.m_creationTimeStep
            && this->m_name == other.m_name
            && this->m_preferredPhase == other.m_preferredPhase
            && this->timesteps == other.timesteps;
    }

    bool Well::operator!=(const Well& other) const {
        return !(*this == other);
    }

    double Well::production_rate( Phase phase, size_t timestep ) const {
        if( !this->isProducer( timestep ) ) return 0.0;

        const auto& p = this->getProductionProperties( timestep );

        switch( phase ) {
            case Phase::WATER: return p.WaterRate;
            case Phase::OIL:   return p.OilRate;
            case Phase::GAS:   return p.GasRate;
            case Phase::SOLVENT:
                throw std::invalid_argument( "Production of 'SOLVENT' requested." );
            case Phase::POLYMER:
                throw std::invalid_argument( "Production of 'POLYMER' requested." );
            case Phase::ENERGY:
                throw std::invalid_argument( "Production of 'ENERGY' requested." );
            case Phase::POLYMW:
                throw std::invalid_argument( "Production of 'POLYMW' requested.");
        }

        throw std::logic_error( "Unreachable state. Invalid Phase value. "
                                "This is likely a programming error." );
    }

    double Well::injection_rate( Phase phase, size_t timestep ) const {
        if( !this->isInjector( timestep ) ) return 0.0;

        const auto& i = this->getInjectionProperties( timestep );
        const auto type = i.injectorType;

        if( phase == Phase::WATER && type != WellInjector::WATER ) return 0.0;
        if( phase == Phase::OIL   && type != WellInjector::OIL   ) return 0.0;
        if( phase == Phase::GAS   && type != WellInjector::GAS   ) return 0.0;

        return i.surfaceInjectionRate;
    }

    bool Well::setProductionProperties(size_t timeStep , const WellProductionProperties& newProperties) {
        if (isInjector(timeStep))
            switchToProducer( timeStep );

        m_isProducer.update(timeStep , true);
        bool update = m_productionProperties.update(timeStep, newProperties);
        if (update)
            addEvent( ScheduleEvents::PRODUCTION_UPDATE, timeStep );

        return update;
    }

    WellProductionProperties Well::getProductionPropertiesCopy(size_t timeStep) const {
        return m_productionProperties.get(timeStep);
    }

    const WellProductionProperties& Well::getProductionProperties(size_t timeStep) const {
        return m_productionProperties.at(timeStep);
    }

    bool Well::setInjectionProperties(size_t timeStep , const WellInjectionProperties& newProperties) {
        if (isProducer(timeStep))
            switchToInjector( timeStep );

        m_isProducer.update(timeStep , false);
        bool update = m_injectionProperties.update(timeStep, newProperties);
        if (update)
            addEvent( ScheduleEvents::INJECTION_UPDATE, timeStep );

        return update;
    }

    WellInjectionProperties Well::getInjectionPropertiesCopy(size_t timeStep) const {
        return m_injectionProperties.get(timeStep);
    }

    const WellInjectionProperties& Well::getInjectionProperties(size_t timeStep) const {
        return m_injectionProperties.at(timeStep);
    }

    bool Well::setPolymerProperties(size_t timeStep , const WellPolymerProperties& newProperties) {
        m_isProducer.update(timeStep , false);
        bool update = m_polymerProperties.update(timeStep, newProperties);
        if (update)
            addEvent( ScheduleEvents::WELL_POLYMER_UPDATE, timeStep );

        return update;
    }

    WellPolymerProperties Well::getPolymerPropertiesCopy(size_t timeStep) const {
        return m_polymerProperties.get(timeStep);
    }

    const WellPolymerProperties& Well::getPolymerProperties(size_t timeStep) const {
        return m_polymerProperties.at(timeStep);
    }

    bool Well::setSolventFraction(size_t timeStep , const double fraction) {
        m_isProducer.update(timeStep , false);
        return m_solventFraction.update(timeStep, fraction);
    }

    bool Well::setEconProductionLimits(const size_t timeStep, const WellEconProductionLimits& productionlimits) {
        // not sure if this keyword turning a well to be producer.
        // not sure what will happen if we use this keyword to a injector.
        return m_econproductionlimits.update(timeStep, productionlimits);
    }

    const WellEconProductionLimits& Well::getEconProductionLimits(const size_t timeStep) const {
        return m_econproductionlimits.at(timeStep);
    }

    const double& Well::getSolventFraction(size_t timeStep) const {
        return m_solventFraction.at(timeStep);
    }



    bool Well::hasBeenDefined(size_t timeStep) const {
        if (timeStep < m_creationTimeStep)
            return false;
        else
            return true;
    }

    WellCommon::StatusEnum Well::getStatus(size_t timeStep) const {
        return m_status.get( timeStep );
    }

    bool Well::setStatus(size_t timeStep, WellCommon::StatusEnum status) {
        if ((WellCommon::StatusEnum::OPEN == status) && getConnections(timeStep).allConnectionsShut()) {
            OpmLog::note("When handling keyword for well " + name() + ": Cannot open a well where all completions are shut" );
            return false;
        } else {
            bool update = m_status.update( timeStep , status );
            if (update)
                addEvent( ScheduleEvents::WELL_STATUS_CHANGE , timeStep );

            return update;
        }
    }

    bool Well::isProducer(size_t timeStep) const {
        return bool( m_isProducer.get(timeStep) );
    }

    bool Well::isInjector(size_t timeStep) const {
        return !bool( isProducer(timeStep) );
    }

    bool Well::isAvailableForGroupControl(size_t timeStep) const {
        return m_isAvailableForGroupControl.get(timeStep);
    }

    void Well::setAvailableForGroupControl(size_t timeStep, bool isAvailableForGroupControl_) {
        m_isAvailableForGroupControl.update(timeStep, isAvailableForGroupControl_);
    }

    double Well::getGuideRate(size_t timeStep) const {
        return m_guideRate.get(timeStep);
    }

    void Well::setGuideRate(size_t timeStep, double guideRate) {
        m_guideRate.update(timeStep, guideRate);
    }

    GuideRate::GuideRatePhaseEnum Well::getGuideRatePhase(size_t timeStep) const {
        return m_guideRatePhase.get(timeStep);
    }

    void Well::setGuideRatePhase(size_t timeStep, GuideRate::GuideRatePhaseEnum phase) {
        m_guideRatePhase.update(timeStep, phase);
    }

    double Well::getGuideRateScalingFactor(size_t timeStep) const {
        return m_guideRateScalingFactor.get(timeStep);
    }

    void Well::setGuideRateScalingFactor(size_t timeStep, double scalingFactor) {
        m_guideRateScalingFactor.update(timeStep, scalingFactor);
    }

    double Well::getEfficiencyFactor (size_t timeStep) const {
        return m_efficiencyFactors.get(timeStep);
    }

    void Well::setEfficiencyFactor(size_t timeStep, double scalingFactor) {
        m_efficiencyFactors.update(timeStep, scalingFactor);
    }

    /*****************************************************************/

    // WELSPECS

    int Well::getHeadI() const {
        return m_headI.back();
    }

    int Well::getHeadJ() const {
        return m_headJ.back();
    }

    int Well::getHeadI( size_t timestep ) const {
        return this->m_headI.get( timestep );
    }

    int Well::getHeadJ( size_t timestep ) const {
        return this->m_headJ.get( timestep );
    }

    void Well::setHeadI( size_t timestep, int I ) {
        this->m_headI.update( timestep, I );
    }

    void Well::setHeadJ( size_t timestep, int J ) {
        this->m_headJ.update( timestep, J );
    }

    double Well::getRefDepth() const {
        return this->getRefDepth( this->timesteps );
    }

    double Well::getRefDepth( size_t timestep ) const {
        auto depth = this->m_refDepth.get( timestep );

        if( depth >= 0.0 ) return depth;

        // ref depth was defaulted and we get the depth of the first completion
        const auto& completions = this->getConnections( timestep );
        if( completions.size() == 0 ) {
            throw std::invalid_argument( "No completions defined for well: "
                                         + name()
                                         + ". Can not infer reference depth" );
        }

        return completions.get( 0 ).getCenterDepth();
    }

    void Well::setRefDepth( size_t timestep, double depth ) {
        this->m_refDepth.update( timestep, depth );
    }

    Phase Well::getPreferredPhase() const {
        return m_preferredPhase;
    }

    const WellConnections& Well::getConnections(size_t timeStep) const {
        return *connections.get( timeStep );
    }

    WellConnections Well::getActiveConnections(size_t timeStep, const EclipseGrid& grid) const {
        return WellConnections(this->getConnections(timeStep), grid);
    }

    const WellConnections& Well::getConnections() const {
        return *connections.back();
    }

    void Well::addConnections(size_t time_step, const std::vector< Connection >& newConnections ) {
        WellConnections * new_set = new WellConnections(this->getConnections(time_step));
        int complnum_shift = new_set->size();

        const auto headI = this->m_headI[ time_step ];
        const auto headJ = this->m_headJ[ time_step ];

        auto prev_size = new_set->size();
        for( auto completion : newConnections ) {
            completion.fixDefaultIJ( headI , headJ );
            completion.shift_complnum( complnum_shift );

            new_set->add( completion );
            const auto new_size = new_set->size();

            /* Connections can be "re-added", i.e. same coordinates but with a
             * different set of properties. In this case they also inherit the
             * completion number (which must otherwise be shifted because
             * every COMPDAT keyword thinks it's the only one.
             */
            if( new_size == prev_size ) --complnum_shift;
            else ++prev_size;
        }

        this->updateWellConnections( time_step, new_set );
    }


    WellConnections * Well::newWellConnections(size_t time_step) {
        return new WellConnections( this->m_headI[time_step], this->m_headJ[time_step]);
    }

    void Well::updateWellConnections(size_t time_step, WellConnections * new_set ){
        if( getWellConnectionOrdering() == WellCompletion::TRACK) {
            const auto headI = this->m_headI[ time_step ];
            const auto headJ = this->m_headJ[ time_step ];
            new_set->orderConnections( headI, headJ );
        }

        connections.update( time_step, std::shared_ptr<WellConnections>( new_set ));
        addEvent( ScheduleEvents::COMPLETION_CHANGE , time_step );
    }

    namespace {

        bool defaulted( int x ) { return x < 0; }

        int maybe( const DeckRecord& rec, const std::string& s ) {
            const auto& item = rec.getItem( s );
            return item.defaultApplied( 0 ) ? -1 : item.get< int >( 0 ) - 1;
        }

        bool match_le(int value, int limit) {
            if (defaulted(limit))
                return true;

            return value <= limit;
        }

        bool match_ge(int value, int limit) {
            if (defaulted(limit))
                return true;

            return value >= limit;
        }

        bool match_eq(int value, int limit) {
            if (defaulted(limit))
                return true;

            return (limit == value);
        }
    }

    void Well::handleCOMPSEGS(const DeckKeyword& keyword, size_t time_step) {
        const auto& segment_set = this->getSegmentSet(time_step);
        const auto& completion_set = this->getConnections( time_step );
        WellConnections * new_connection_set = newConnectionsWithSegments(keyword, completion_set, segment_set);
        this->updateWellConnections(time_step, new_connection_set);
    }

    void Well::handleCOMPLUMP(const DeckRecord& record, size_t time_step)  {
        const int I  = maybe( record, "I" );
        const int J  = maybe( record, "J" );
        const int K1 = maybe( record, "K1" );
        const int K2 = maybe( record, "K2" );

        auto match = [=]( const Connection& c ) -> bool {
            if (!match_eq(c.getI(), I))  return false;
            if (!match_eq(c.getJ(), J))  return false;
            if (!match_le(c.getK(), K1)) return false;
            if (!match_ge(c.getK(), K2)) return false;

            return true;
        };

        WellConnections * new_connections = this->newWellConnections(time_step);
        const int complnum = record.getItem("N").get<int>(0);
        for (const auto& c : this->getConnections(time_step)) {
            if (match(c))
                new_connections->add(Connection(c,complnum));
            else
                new_connections->add(c);
        }
        this->updateWellConnections(time_step, new_connections);
    }


    void Well::handleWPIMULT(const DeckRecord& record, size_t time_step) {
        const int I  = maybe(record, "I");
        const int J  = maybe(record, "J");
        const int K  = maybe(record, "K");
        const int C1 = maybe(record, "FIRST");
        const int C2 = maybe(record, "LAST");

        auto match = [=]( const Connection &c) -> bool {
            if (!match_le(c.complnum(), C1)) return false;
            if (!match_ge(c.complnum(), C2)) return false;
            if (!match_eq(c.getI(), I))      return false;
            if (!match_eq(c.getJ(), J))      return false;
            if (!match_eq(c.getK(), K))      return false;

            return true;
        };

        WellConnections * new_connections = this->newWellConnections(time_step);
        double wellPi = record.getItem("WELLPI").get< double >(0);

        for (const auto& c : this->getConnections(time_step)) {
            if (match(c))
                new_connections->add(Connection(c,wellPi));
            else
                new_connections->add(c);
        }

        this->updateWellConnections(time_step, new_connections);
    }


    void Well::handleWELOPEN(const DeckRecord& record, size_t time_step, WellCompletion::StateEnum status) {
        const int I  = maybe(record, "I");
        const int J  = maybe(record, "J");
        const int K  = maybe(record, "K");
        const int C1 = maybe(record, "C1");
        const int C2 = maybe(record, "C2");

        auto match = [=]( const Connection &c) -> bool {
            if (!match_le(c.complnum(), C1)) return false;
            if (!match_ge(c.complnum(), C2)) return false;
            if (!match_eq(c.getI(), I))      return false;
            if (!match_eq(c.getJ(), J))      return false;
            if (!match_eq(c.getK(), K))      return false;

            return true;
        };

        WellConnections * new_connections = this->newWellConnections(time_step);

        for (const auto& c : this->getConnections(time_step)) {
            if (match(c))
                new_connections->add(Connection(c,status));
            else
                new_connections->add(c);
        }

        this->updateWellConnections(time_step, new_connections);
    }



    void Well::handleCOMPDAT(size_t time_step, const DeckRecord& record, const EclipseGrid& grid, const Eclipse3DProperties& eclipseProperties) {
        WellConnections * connections = new WellConnections(this->getConnections(time_step));
        connections->loadCOMPDAT(record, grid, eclipseProperties);
        this->updateWellConnections(time_step, connections);
    }

    const std::string Well::getGroupName(size_t time_step) const {
        return m_groupName.get(time_step);
    }


    void Well::setGroupName(size_t time_step, const std::string& groupName ) {
        m_groupName.update(time_step , groupName);
    }



    void Well::updateRFTActive(size_t time_step, RFTConnections::RFTEnum mode) {
        switch(mode) {
        case RFTConnections::RFTEnum::YES:
            m_rft.update_elm(time_step, true);
            break;
        case RFTConnections::RFTEnum::TIMESTEP:
            m_rft.update_elm(time_step, true);
            break;
        case RFTConnections::RFTEnum::REPT:
            m_rft.update(time_step, true);
            break;
        case RFTConnections::RFTEnum::FOPN:
            setRFTForWellWhenFirstOpen(time_step);
            break;
        case RFTConnections::RFTEnum::NO:
            m_rft.update(time_step, false);
            break;
        default:
            break;
        }
    }

    void Well::updatePLTActive(size_t time_step, PLTConnections::PLTEnum mode){
        switch(mode) {
        case PLTConnections::PLTEnum::YES:
            m_plt.update_elm(time_step, true);
            break;
        case PLTConnections::PLTEnum::REPT:
            m_plt.update(time_step, true);
            break;
        case PLTConnections::PLTEnum::NO:
            m_plt.update(time_step, false);
            break;
        default:
            break;
        }
    }

    bool Well::getRFTActive(size_t time_step) const{
        return bool( m_rft.get(time_step) );
    }

    bool Well::getPLTActive(size_t time_step) const{
     return bool( m_plt.get(time_step) );
    }

    /*
      The first report step where *either* RFT or PLT output is active.
    */
    int Well::firstRFTOutput( ) const {
        int rft_output = m_rft.find( true );
        int plt_output = m_plt.find( true );

        if (rft_output < plt_output) {
            if (rft_output >= 0)
                return rft_output;
            else
                return plt_output;
        } else {
            if (plt_output >= 0)
                return plt_output;
            else
                return rft_output;
        }
    }


    size_t Well::firstTimeStep( ) const {
        return m_creationTimeStep;
    }


    int Well::findWellFirstOpen(int startTimeStep) const{
        for( size_t i = startTimeStep; i < this->timesteps ;i++){
            if(getStatus(i)==WellCommon::StatusEnum::OPEN){
                return i;
            }
        }
        return -1;
    }

    void Well::setRFTForWellWhenFirstOpen(size_t currentStep){
        int time;
        if(getStatus(currentStep)==WellCommon::StatusEnum::OPEN ){
            time = currentStep;
        }else {
            time = findWellFirstOpen(currentStep);
        }
        if (time > -1)
            updateRFTActive(time, RFTConnections::RFTEnum::YES);
    }

    WellCompletion::CompletionOrderEnum Well::getWellConnectionOrdering() const {
        return m_comporder;
    }


    bool Well::wellNameInWellNamePattern(const std::string& wellName, const std::string& wellNamePattern) {
        bool wellNameInPattern = false;
        if (util_fnmatch( wellNamePattern.c_str() , wellName.c_str()) == 0) {
            wellNameInPattern = true;
        }
        return wellNameInPattern;
    }

    bool Well::getAllowCrossFlow() const {
        return m_allowCrossFlow;
    }

    bool Well::getAutomaticShutIn() const {
        return m_automaticShutIn;
    }

    bool Well::canOpen(size_t currentStep) const {
        if( getAllowCrossFlow() ) return true;

        if( isInjector( currentStep ) )
            return getInjectionProperties( currentStep ).surfaceInjectionRate != 0;

        const auto& prod = getProductionProperties( currentStep );
        return (prod.WaterRate + prod.OilRate + prod.GasRate) != 0;
    }


    const SegmentSet& Well::getSegmentSet(size_t time_step) const {
        return m_segmentset.get(time_step);
    }

    bool Well::isMultiSegment(size_t time_step) const {
        return (getSegmentSet(time_step).numberSegment() > 0);
    }

    void Well::addSegmentSet(size_t time_step, SegmentSet new_segmentset ) {
        // to see if it is the first time entering WELSEGS input to this well.
        // if the well is not multi-segment well, it will be the first time
        // not sure if a well can switch between mutli-segment well and other
        // type of well
        // Here, we assume not
        const bool first_time = !isMultiSegment(time_step);

        if( !first_time ) {
            // checking the consistency of the input WELSEGS information
            throw std::logic_error("re-entering WELSEGS for a well is not supported yet!!.");
        }

        // overwrite the BHP reference depth with the one from WELSEGS keyword
        const double ref_depth = new_segmentset.depthTopSegment();
        m_refDepth.update( time_step, ref_depth );

        if (new_segmentset.lengthDepthType() == WellSegment::ABS) {
            new_segmentset.processABS();
        } else if (new_segmentset.lengthDepthType() == WellSegment::INC) {
            new_segmentset.processINC(first_time);
        } else {
            throw std::logic_error(" unknown length_depth_type in the new_segmentset");
        }
        m_segmentset.update(time_step, new_segmentset);
    }




    void Well::addEvent(ScheduleEvents::Events event, size_t reportStep) {
        this->events.addEvent( event , reportStep );
    }


    bool Well::hasEvent(uint64_t eventMask, size_t reportStep) const {
        return this->events.hasEvent( eventMask , reportStep );
    }


    void Well::filterConnections(const EclipseGrid& grid) {
        /*
          The connections member variable is DynamicState<WellConnections>
          instance, hence this for loop is over all timesteps.
        */
        for (auto& conn_set : connections)
            conn_set->filter(grid);
    }
}
