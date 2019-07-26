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


#ifndef WELL2_HPP
#define WELL2_HPP

#include <string>

#include <opm/parser/eclipse/EclipseState/Runspec.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/ScheduleEnums.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/SummaryState.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/WellConnections.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/MSW/WellSegments.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/ProductionControls.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/InjectionControls.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/WellFoamProperties.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/WellTracerProperties.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/WellPolymerProperties.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/WellEconProductionLimits.hpp>
#include <opm/parser/eclipse/Units/Units.hpp>


namespace Opm {

class DeckRecord;
class EclipseGrid;
class DeckKeyword;
struct WellInjectionProperties;
class WellProductionProperties;
class UDQActive;

struct WellGuideRate {
    bool available;
    double guide_rate;
    GuideRate::GuideRatePhaseEnum guide_phase;
    double scale_factor;
};


class Well2 {
public:
    Well2(const std::string& wname,
          const std::string& gname,
          std::size_t init_step,
          std::size_t insert_index,
          int headI,
          int headJ,
          double ref_depth,
          Phase phase,
          WellProducer::ControlModeEnum whistctl_cmode,
          WellCompletion::CompletionOrderEnum ordering,
          const UnitSystem& unit_system,
          double udq_undefined);

    bool isMultiSegment() const;
    bool isAvailableForGroupControl() const;
    double getGuideRate() const;
    GuideRate::GuideRatePhaseEnum getGuideRatePhase() const;
    double getGuideRateScalingFactor() const;

    bool hasBeenDefined(size_t timeStep) const;
    std::size_t firstTimeStep() const;
    bool predictionMode() const;
    bool canOpen() const;
    bool isProducer() const;
    bool isInjector() const;
    WellInjector::TypeEnum injectorType() const;
    size_t seqIndex() const;
    bool getAutomaticShutIn() const;
    bool getAllowCrossFlow() const;
    const std::string& name() const;
    int getHeadI() const;
    int getHeadJ() const;
    double getRefDepth() const;
    double getDrainageRadius() const;
    double getEfficiencyFactor() const;
    WellCompletion::CompletionOrderEnum getWellConnectionOrdering() const;
    const WellProductionProperties& getProductionProperties() const;
    const WellInjectionProperties& getInjectionProperties() const;
    const WellEconProductionLimits& getEconLimits() const;
    const WellFoamProperties& getFoamProperties() const;
    const WellPolymerProperties& getPolymerProperties() const;
    const WellTracerProperties& getTracerProperties() const;
    const WellConnections& getConnections() const;
    const WellSegments& getSegments() const;
    double getSolventFraction() const;
    WellCommon::StatusEnum getStatus() const;
    const std::string& groupName() const;
    Phase getPreferredPhase() const;
    /* The rate of a given phase under the following assumptions:
     * * Returns zero if production is requested for an injector (and vice
     *   versa)
     * * If this is an injector and something else than the
     *   requested phase is injected, returns 0, i.e.
     *   water_injector.injection_rate( gas ) == 0
     * * Mixed injection is not supported and always returns 0.
     */
    double production_rate( const SummaryState& st, Phase phase) const;
    double injection_rate( const SummaryState& st,  Phase phase) const;
    static bool wellNameInWellNamePattern(const std::string& wellName, const std::string& wellNamePattern);

    /*
      The getCompletions() function will return a map:

      {
        1 : [Connection, Connection],
        2 : [Connection, Connection, Connecton],
        3 : [Connection],
        4 : [Connection]
      }

      The integer ID's correspond to the COMPLETION id given by the COMPLUMP
      keyword.
    */
    std::map<int, std::vector<Connection>> getCompletions() const;

    bool updatePrediction(bool prediction_mode);
    bool updateAutoShutin(bool auto_shutin);
    bool updateCrossFlow(bool allow_cross_flow);
    bool updateHead(int I, int J);
    bool updateRefDepth(double ref_dpeth);
    bool updateDrainageRadius(double drainage_radius);
    bool updateConnections(const std::shared_ptr<WellConnections> connections);
    bool updateStatus(WellCommon::StatusEnum status);
    bool updateGroup(const std::string& group);
    bool updateProducer(bool is_producer);
    bool updateWellGuideRate(bool available, double guide_rate, GuideRate::GuideRatePhaseEnum guide_phase, double scale_factor);
    bool updateWellGuideRate(double guide_rate);
    bool updateEfficiencyFactor(double efficiency_factor);
    bool updateSolventFraction(double solvent_fraction);
    bool updateTracer(std::shared_ptr<WellTracerProperties> tracer_properties);
    bool updateFoamProperties(std::shared_ptr<WellFoamProperties> foam_properties);
    bool updatePolymerProperties(std::shared_ptr<WellPolymerProperties> polymer_properties);
    bool updateEconLimits(std::shared_ptr<WellEconProductionLimits> econ_limits);
    bool updateProduction(std::shared_ptr<WellProductionProperties> production);
    bool updateInjection(std::shared_ptr<WellInjectionProperties> injection);

    bool handleWELSEGS(const DeckKeyword& keyword);
    bool handleCOMPSEGS(const DeckKeyword& keyword, const EclipseGrid& grid, const ParseContext& parseContext, ErrorGuard& errors);
    bool handleWELOPEN(const DeckRecord& record, WellCompletion::StateEnum status);
    bool handleCOMPLUMP(const DeckRecord& record);
    bool handleWPIMULT(const DeckRecord& record);

    void filterConnections(const EclipseGrid& grid);
    void switchToInjector();
    void switchToProducer();
    ProductionControls productionControls(const SummaryState& st) const;
    InjectionControls injectionControls(const SummaryState& st) const;
    int vfp_table_number() const;
    double alq_value() const;
    double temperature() const;
private:
    std::string wname;
    std::string group_name;
    std::size_t init_step;
    std::size_t insert_index;
    int headI;
    int headJ;
    double ref_depth;
    Phase phase;
    WellCompletion::CompletionOrderEnum ordering;
    UnitSystem unit_system;
    double udq_undefined;

    WellCommon::StatusEnum status;
    double drainage_radius;
    bool allow_cross_flow;
    bool automatic_shutin;
    bool producer;
    WellGuideRate guide_rate;
    double efficiency_factor;
    double solvent_fraction;
    bool prediction_mode = true;

    std::shared_ptr<const WellEconProductionLimits> econ_limits;
    std::shared_ptr<const WellFoamProperties> foam_properties;
    std::shared_ptr<const WellPolymerProperties> polymer_properties;
    std::shared_ptr<const WellTracerProperties> tracer_properties;
    std::shared_ptr<WellConnections> connections; // The WellConnections object can not be const because of the filterConnections method - would be beneficial to rewrite to enable const
    std::shared_ptr<const WellProductionProperties> production;
    std::shared_ptr<const WellInjectionProperties> injection;
    std::shared_ptr<const WellSegments> segments;
};


}

#endif
