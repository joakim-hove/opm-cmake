/*
  Copyright (c) 2018 Equinor ASA
  Copyright (c) 2016 Statoil ASA
  Copyright (c) 2013-2015 Andreas Lauser
  Copyright (c) 2013 SINTEF ICT, Applied Mathematics.
  Copyright (c) 2013 Uni Research AS
  Copyright (c) 2015 IRIS AS

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

#include <opm/output/eclipse/RestartIO.hpp>

#include <opm/output/eclipse/AggregateGroupData.hpp>
#include <opm/output/eclipse/AggregateWellData.hpp>
#include <opm/output/eclipse/AggregateConnectionData.hpp>
#include <opm/output/eclipse/AggregateMSWData.hpp>
#include <opm/output/eclipse/AggregateUDQData.hpp>
#include <opm/output/eclipse/WriteRestartHelpers.hpp>

#include <opm/io/eclipse/OutputStream.hpp>
#include <opm/io/eclipse/PaddedOutputString.hpp>

#include <opm/parser/eclipse/EclipseState/Schedule/SummaryState.hpp>
#include <opm/parser/eclipse/EclipseState/Grid/EclipseGrid.hpp>
#include <opm/parser/eclipse/EclipseState/EclipseState.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Schedule.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Tuning.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/Well2.hpp>
#include <opm/parser/eclipse/EclipseState/Tables/Eqldims.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <fstream>
#include <initializer_list>
#include <iterator>
#include <string>
#include <unordered_set>
#include <vector>

namespace Opm { namespace RestartIO {

namespace {
    /*
      The RestartValue structure has an 'extra' container which can be used to
      add extra fields to the restart file. The extra field is used both to add
      OPM specific fields like 'OPMEXTRA', and eclipse standard fields like
      THRESHPR. In the case of e.g. THRESHPR this should - if present - be added
      in the SOLUTION section of the restart file. The extra_solution object
      identifies the keys which should be output in the solution section.
    */

    bool extraInSolution(const std::string& vector)
    {
        static const auto extra_solution =
            std::unordered_set<std::string>
        {
            "THRESHPR",
        };

        return extra_solution.count(vector) > 0;
    }

    double nextStepSize(const Opm::RestartValue& rst_value)
    {
        return rst_value.hasExtra("OPMEXTRA")
            ? rst_value.getExtra("OPMEXTRA")[0]
            : 0.0;
    }

    std::vector<int>
    serialize_OPM_IWEL(const data::Wells&              wells,
                       const std::vector<std::string>& sched_wells)
    {
      const auto getctrl = [&]( const std::string& wname ) {
            const auto itr = wells.find( wname );
            return itr == wells.end() ? 0 : itr->second.control;
        };

        std::vector<int> iwel(sched_wells.size(), 0.0);
        std::transform(sched_wells.begin(), sched_wells.end(), iwel.begin(), getctrl);

        return iwel;
    }

    std::vector<double>
    serialize_OPM_XWEL(const data::Wells&             wells,
                       const std::vector<Opm::Well2>& sched_wells,
                       const Phases&                  phase_spec,
                       const EclipseGrid&             grid)
    {
        using rt = data::Rates::opt;

        std::vector<rt> phases;
        if (phase_spec.active(Phase::WATER)) phases.push_back(rt::wat);
        if (phase_spec.active(Phase::OIL))   phases.push_back(rt::oil);
        if (phase_spec.active(Phase::GAS))   phases.push_back(rt::gas);

        std::vector< double > xwel;
        for (const auto& sched_well : sched_wells) {
            if (wells.count(sched_well.name()) == 0 ||
                sched_well.getStatus() == Opm::WellCommon::SHUT)
            {
                const auto elems = (sched_well.getConnections().size()
                                    * (phases.size() + data::Connection::restart_size))
                    + 2 /* bhp, temperature */
                    + phases.size();

                // write zeros if no well data is provided
                xwel.insert( xwel.end(), elems, 0.0 );
                continue;
            }

            const auto& well = wells.at( sched_well.name() );

            xwel.push_back( well.bhp );
            xwel.push_back( well.temperature );

            for (auto phase : phases)
                xwel.push_back(well.rates.get(phase));

            for (const auto& sc : sched_well.getConnections()) {
                const auto i = sc.getI(), j = sc.getJ(), k = sc.getK();

                const auto rs_size = phases.size() + data::Connection::restart_size;
                if (!grid.cellActive(i, j, k) || sc.state() == WellCompletion::SHUT) {
                    xwel.insert(xwel.end(), rs_size, 0.0);
                    continue;
                }

                const auto global_index = grid.getGlobalIndex(i, j, k);

                const auto& connection =
                    std::find_if(well.connections.begin(),
                                 well.connections.end(),
                        [global_index](const data::Connection& c)
                    {
                        return c.index == global_index;
                    });

                if (connection == well.connections.end()) {
                    xwel.insert( xwel.end(), rs_size, 0.0 );
                    continue;
                }

                xwel.push_back(connection->pressure);
                xwel.push_back(connection->reservoir_rate);

                for (auto phase : phases)
                    xwel.push_back(connection->rates.get(phase));
            }
        }

        return xwel;
    }

    void checkSaveArguments(const EclipseState& es,
                            const RestartValue& restart_value,
                            const EclipseGrid&  grid)
    {
        for (const auto& elm: restart_value.solution)
            if (elm.second.data.size() != grid.getNumActive())
                throw std::runtime_error("Wrong size on solution vector: " + elm.first);

        if (es.getSimulationConfig().getThresholdPressure().size() > 0) {
            // If the the THPRES option is active the restart_value should have a
            // THPRES field. This is not enforced here because not all the opm
            // simulators have been updated to include the THPRES values.
            if (!restart_value.hasExtra("THRESHPR")) {
                OpmLog::warning("This model has THPRES active - should have THPRES as part of restart data.");
                return;
            }

            const auto num_regions = es.getTableManager().getEqldims().getNumEquilRegions();
            const auto& thpres = restart_value.getExtra("THRESHPR");

            if (thpres.size() != num_regions * num_regions)
                throw std::runtime_error("THPRES vector has invalid size - should have num_region * num_regions.");
        }
    }

    std::vector<int>
    writeHeader(const int                     sim_step,
                const double                  next_step_size,
                const double                  simTime,
                const Schedule&               schedule,
                const EclipseGrid&            grid,
                const EclipseState&           es,
                EclIO::OutputStream::Restart& rstFile)
    {
        // write INTEHEAD to restart file
        const auto ih = Helpers::createInteHead(es, grid, schedule,
                                                simTime, sim_step, sim_step);
        rstFile.write("INTEHEAD", ih);

        // write LOGIHEAD to restart file
        rstFile.write("LOGIHEAD", Helpers::createLogiHead(es));

        // write DOUBHEAD to restart file
        const auto dh = Helpers::createDoubHead(es, schedule, sim_step,
                                                simTime, next_step_size);
        rstFile.write("DOUBHEAD", dh);

        // return the inteHead vector
        return ih;
    }

    void writeGroup(int                           sim_step,
                    const Schedule&               schedule,
                    const Opm::SummaryState&      sumState,
                    const std::vector<int>&       ih,
                    EclIO::OutputStream::Restart& rstFile)
    {
        // write IGRP to restart file
        const size_t simStep = static_cast<size_t> (sim_step);

        auto  groupData = Helpers::AggregateGroupData(ih);

        groupData.captureDeclaredGroupData(schedule, simStep, sumState, ih);

        rstFile.write("IGRP", groupData.getIGroup());
        rstFile.write("SGRP", groupData.getSGroup());
        rstFile.write("XGRP", groupData.getXGroup());
        rstFile.write("ZGRP", groupData.getZGroup());
    }

    void writeMSWData(int                           sim_step,
                      const UnitSystem&             units,
                      const Schedule&               schedule,
                      const EclipseGrid&            grid,
                      const Opm::SummaryState&      sumState,
                      const Opm::data::Wells&       wells,
                      const std::vector<int>&       ih,
                      EclIO::OutputStream::Restart& rstFile)
    {
        // write ISEG, RSEG, ILBS and ILBR to restart file
        const auto simStep = static_cast<std::size_t> (sim_step);

        auto  MSWData = Helpers::AggregateMSWData(ih);
        MSWData.captureDeclaredMSWData(schedule, simStep, units,
                                       ih, grid, sumState, wells);

        rstFile.write("ISEG", MSWData.getISeg());
        rstFile.write("ILBS", MSWData.getILBs());
        rstFile.write("ILBR", MSWData.getILBr());
        rstFile.write("RSEG", MSWData.getRSeg());
    }

    void writeUDQ(int                           sim_step,
                  const Schedule&               schedule,
		  const std::vector<int>&      	ih,
		  EclIO::OutputStream::Restart& rstFile)
    {
        // write UDQ - data to restart file
        const std::size_t simStep = static_cast<size_t> (sim_step);

	const auto udqDims = Helpers::createUdqDims(schedule, simStep, ih);
	auto  udqData = Helpers::AggregateUDQData(udqDims);
        udqData.captureDeclaredUDQData(schedule, simStep);
	
	rstFile.write("IUDQ", udqData.getIUDQ());
	rstFile.write("IUAD", udqData.getIUAD());
	rstFile.write("ZUDN", udqData.getZUDN());
	rstFile.write("ZUDL", udqData.getZUDL());
    }
   
    void writeWell(int                           sim_step,
                   const bool                    ecl_compatible_rst,
                   const Phases&                 phases,
                   const UnitSystem&             units,
                   const EclipseGrid&            grid,
                   const Schedule&               schedule,
                   const data::Wells&            wells,
                   const Opm::SummaryState&      sumState,
                   const std::vector<int>&       ih,
                   EclIO::OutputStream::Restart& rstFile)
    {
        auto wellData = Helpers::AggregateWellData(ih);
        wellData.captureDeclaredWellData(schedule, units, sim_step, sumState, ih);
        wellData.captureDynamicWellData(schedule, sim_step,
                                        wells, sumState);

        rstFile.write("IWEL", wellData.getIWell());
        rstFile.write("SWEL", wellData.getSWell());
        rstFile.write("XWEL", wellData.getXWell());
        rstFile.write("ZWEL", wellData.getZWell());

        // Extended set of OPM well vectors
        if (!ecl_compatible_rst)
        {
            const auto sched_wells = schedule.getWells2(sim_step);
            const auto sched_well_names = schedule.wellNames(sim_step);

            const auto opm_xwel =
                serialize_OPM_XWEL(wells, sched_wells, phases, grid);

            const auto opm_iwel = serialize_OPM_IWEL(wells, sched_well_names);

            rstFile.write("OPM_IWEL", opm_iwel);
            rstFile.write("OPM_XWEL", opm_xwel);
        }

        auto connectionData = Helpers::AggregateConnectionData(ih);
        connectionData.captureDeclaredConnData(schedule, grid, units,
                                               wells, sim_step);

        rstFile.write("ICON", connectionData.getIConn());
        rstFile.write("SCON", connectionData.getSConn());
        rstFile.write("XCON", connectionData.getXConn());
    }

    bool haveHysteresis(const RestartValue& value)
    {
        for (const auto* key : { "KRNSW_OW", "PCSWM_OW",
                                 "KRNSW_GO", "PCSWM_GO", })
        {
            if (value.solution.has(key)) { return true; }
        }

        return false;
    }

    std::vector<double>
    convertedHysteresisSat(const RestartValue& value,
                           const std::string&  primary,
                           const std::string&  fallback)
    {
        auto smax = std::vector<double>{};

        if (value.solution.has(primary)) {
            smax = value.solution.data(primary);
        }
        else if (value.solution.has(fallback)) {
            smax = value.solution.data(fallback);
        }

        if (! smax.empty()) {
            std::transform(std::begin(smax), std::end(smax), std::begin(smax),
                           [](const double s) { return 1.0 - s; });
        }

        return smax;
    }

    template <class OutputVector>
    void writeEclipseCompatHysteresis(const RestartValue& value,
                                      const bool          write_double,
                                      OutputVector&&      writeVector)
    {
        // Convert Flow-specific vectors {KRNSW,PCSWM}_OW to ECLIPSE's
        // requisite SOMAX vector.  Only partially characterised.
        // Sufficient for Norne.
        {
            const auto somax =
                convertedHysteresisSat(value, "KRNSW_OW", "PCSWM_OW");

            if (! somax.empty()) {
                writeVector("SOMAX", somax, write_double);
            }
        }

        // Convert Flow-specific vectors {KRNSW,PCSWM}_GO to ECLIPSE's
        // requisite SGMAX vector.  Only partially characterised.
        // Sufficient for Norne.
        {
            const auto sgmax =
                convertedHysteresisSat(value, "KRNSW_GO", "PCSWM_GO");

            if (! sgmax.empty()) {
                writeVector("SGMAX", sgmax, write_double);
            }
        }
    }

    void writeSolution(const RestartValue&           value,
		       const Schedule& 	   	     schedule,
		       int 			     report_step,
                       const bool                    ecl_compatible_rst,
                       const bool                    write_double_arg,
		       const std::vector<int>&       inteHD,
                       EclIO::OutputStream::Restart& rstFile)
    {
        rstFile.message("STARTSOL");

        auto write = [&rstFile]
            (const std::string&         key,
             const std::vector<double>& data,
             const bool                 write_double) -> void
        {
            if (write_double) {
                rstFile.write(key, data);
            }
            else {
                rstFile.write(key, std::vector<float> {
                    data.begin(), data.end()
                });
            }
        };

        for (const auto& elm : value.solution) {
            if (elm.second.target == data::TargetType::RESTART_SOLUTION)
            {
                write(elm.first, elm.second.data, write_double_arg);
            }
        }

        writeUDQ(report_step, schedule, inteHD, rstFile);
        
        for (const auto& elm : value.extra) {
            const std::string& key = elm.first.key;
            if (extraInSolution(key)) {
                // Observe that the extra data is unconditionally
                // output as double precision.
                write(key, elm.second, true);
            }
        }

        if (ecl_compatible_rst && haveHysteresis(value)) {
            writeEclipseCompatHysteresis(value, write_double_arg, write);
        }

        rstFile.message("ENDSOL");

        if (ecl_compatible_rst) {
            return;
        }

        for (const auto& elm : value.solution) {
            if (elm.second.target == data::TargetType::RESTART_AUXILIARY) {
                write(elm.first, elm.second.data, write_double_arg);
            }
        }
    }

    void writeExtraData(const RestartValue::ExtraVector& extra_data,
                        EclIO::OutputStream::Restart&    rstFile)
    {
        for (const auto& extra_value : extra_data) {
            const std::string& key = extra_value.first.key;

            if (! extraInSolution(key)) {
                rstFile.write(key, extra_value.second);
            }
        }
    }

} // Anonymous namespace

void save(EclIO::OutputStream::Restart& rstFile,
          int                           report_step,
          double                        seconds_elapsed,
          RestartValue                  value,
          const EclipseState&           es,
          const EclipseGrid&            grid,
          const Schedule&               schedule,
          const SummaryState&           sumState,
          bool                          write_double)
{
    ::Opm::RestartIO::checkSaveArguments(es, value, grid);

    const auto& ioCfg = es.getIOConfig();
    const auto ecl_compatible_rst = ioCfg.getEclCompatibleRST();

    const auto  sim_step = std::max(report_step - 1, 0);
    const auto& units    = es.getUnits();

    if (ecl_compatible_rst) {
        write_double = false;
    }

    // Convert solution fields and extra values from SI to user units.
    value.convertFromSI(units);

    const auto inteHD =
        writeHeader(sim_step, nextStepSize(value), seconds_elapsed,
                    schedule, grid, es, rstFile);

    writeGroup(sim_step, schedule, sumState, inteHD, rstFile);

    // Write well and MSW data only when applicable (i.e., when present)
    {
        const auto& wells = schedule.getWells2(sim_step);

        if (! wells.empty()) {
            const auto haveMSW =
                std::any_of(std::begin(wells), std::end(wells),
                    [](const Well2& well)
            {
                return well.isMultiSegment();
            });

            if (haveMSW) {
                writeMSWData(sim_step, units, schedule, grid, sumState,
                             value.wells, inteHD, rstFile);
            }

            writeWell(sim_step, ecl_compatible_rst,
                      es.runspec().phases(), units, grid, schedule,
                      value.wells, sumState, inteHD, rstFile);
        }
    }
    
    writeSolution(value, schedule, sim_step, ecl_compatible_rst, write_double, inteHD, rstFile);

    if (! ecl_compatible_rst) {
        writeExtraData(value.extra, rstFile);
    }
}

}} // Opm::RestartIO
