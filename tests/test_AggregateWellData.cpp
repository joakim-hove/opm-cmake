/*
  Copyright 2019 Equinor
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

#define BOOST_TEST_MODULE Aggregate_Well_Data

#include <boost/test/unit_test.hpp>

#include <opm/output/eclipse/AggregateWellData.hpp>

#include <opm/parser/eclipse/EclipseState/Schedule/SummaryState.hpp>
#include <opm/output/eclipse/VectorItems/intehead.hpp>
#include <opm/output/eclipse/VectorItems/well.hpp>

#include <opm/output/data/Wells.hpp>

#include <opm/parser/eclipse/Deck/Deck.hpp>
#include <opm/parser/eclipse/Parser/Parser.hpp>
#include <opm/parser/eclipse/EclipseState/EclipseState.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Schedule.hpp>

#include <exception>
#include <stdexcept>
#include <utility>
#include <vector>

struct MockIH
{
    MockIH(const int numWells,
           const int iwelPerWell = 155,  // E100
           const int swelPerWell = 122,  // E100
           const int xwelPerWell = 130,  // E100
           const int zwelPerWell =   3); // E100

    std::vector<int> value;

    using Sz = std::vector<int>::size_type;

    Sz nwells;
    Sz niwelz;
    Sz nswelz;
    Sz nxwelz;
    Sz nzwelz;
};

MockIH::MockIH(const int numWells,
               const int iwelPerWell,
               const int swelPerWell,
               const int xwelPerWell,
               const int zwelPerWell)
    : value(411, 0)
{
    using Ix = ::Opm::RestartIO::Helpers::VectorItems::intehead;

    this->nwells = this->value[Ix::NWELLS] = numWells;
    this->niwelz = this->value[Ix::NIWELZ] = iwelPerWell;
    this->nswelz = this->value[Ix::NSWELZ] = swelPerWell;
    this->nxwelz = this->value[Ix::NXWELZ] = xwelPerWell;
    this->nzwelz = this->value[Ix::NZWELZ] = zwelPerWell;
}

namespace {
    Opm::Deck first_sim()
    {
        // Mostly copy of tests/FIRST_SIM.DATA
        const auto input = std::string {
            R"~(
RUNSPEC
OIL
GAS
WATER
DISGAS
VAPOIL
UNIFOUT
UNIFIN
DIMENS
 10 10 10 /

GRID
DXV
10*0.25 /
DYV
10*0.25 /
DZV
10*0.25 /
TOPS
100*0.25 /

PORO
1000*0.2 /

SOLUTION
RESTART
FIRST_SIM 1/


START             -- 0
1 NOV 1979 /

SCHEDULE
SKIPREST
RPTRST
BASIC=1
/
DATES             -- 1
 10  OKT 2008 /
/
WELSPECS
      'OP_1'       'OP'   9   9 1*     'OIL' 1*      1*  1*   1*  1*   1*  1*  /
      'OP_2'       'OP'   9   9 1*     'OIL' 1*      1*  1*   1*  1*   1*  1*  /
/
COMPDAT
      'OP_1'  9  9   1   1 'OPEN' 1*   32.948   0.311  3047.839 1*  1*  'X'  22.100 /
      'OP_2'  9  9   2   2 'OPEN' 1*   46.825   0.311  4332.346 1*  1*  'X'  22.123 /
      'OP_1'  9  9   3   3 'OPEN' 1*   32.948   0.311  3047.839 1*  1*  'X'  22.100 /
/
WCONPROD
      'OP_1' 'OPEN' 'ORAT' 20000  4* 1000 /
/
WCONINJE
      'OP_2' 'GAS' 'OPEN' 'RATE' 100 200 400 /
/

DATES             -- 2
 20  JAN 2011 /
/
WELSPECS
      'OP_3'       'OP'   9   9 1*     'OIL' 1*      1*  1*   1*  1*   1*  1*  /
/
COMPDAT
      'OP_3'  9  9   1   1 'OPEN' 1*   32.948   0.311  3047.839 1*  1*  'X'  22.100 /
/
WCONPROD
      'OP_3' 'OPEN' 'ORAT' 20000  4* 1000 /
/
WCONINJE
      'OP_2' 'WATER' 'OPEN' 'RATE' 100 200 400 /
/

DATES             -- 3
 15  JUN 2013 /
/
COMPDAT
      'OP_2'  9  9   3  9 'OPEN' 1*   32.948   0.311  3047.839 1*  1*  'X'  22.100 /
      'OP_1'  9  9   7  7 'SHUT' 1*   32.948   0.311  3047.839 1*  1*  'X'  22.100 /
/

DATES             -- 4
 22  APR 2014 /
/
WELSPECS
      'OP_4'       'OP'   9   9 1*     'OIL' 1*      1*  1*   1*  1*   1*  1*  /
/
COMPDAT
      'OP_4'  9  9   3  9 'OPEN' 1*   32.948   0.311  3047.839 1*  1*  'X'  22.100 /
      'OP_3'  9  9   3  9 'OPEN' 1*   32.948   0.311  3047.839 1*  1*  'X'  22.100 /
/
WCONPROD
      'OP_4' 'OPEN' 'ORAT' 20000  4* 1000 /
/

DATES             -- 5
 30  AUG 2014 /
/
WELSPECS
      'OP_5'       'OP'   9   9 1*     'OIL' 1*      1*  1*   1*  1*   1*  1*  /
/
COMPDAT
      'OP_5'  9  9   3  9 'OPEN' 1*   32.948   0.311  3047.839 1*  1*  'X'  22.100 /
/
WCONPROD
      'OP_5' 'OPEN' 'ORAT' 20000  4* 1000 /
/

DATES             -- 6
 15  SEP 2014 /
/
WCONPROD
      'OP_3' 'SHUT' 'ORAT' 20000  4* 1000 /
/

DATES             -- 7
 9  OCT 2014 /
/
WELSPECS
      'OP_6'       'OP'   9   9 1*     'OIL' 1*      1*  1*   1*  1*   1*  1*  /
/
COMPDAT
      'OP_6'  9  9   3  9 'OPEN' 1*   32.948   0.311  3047.839 1*  1*  'X'  22.100 /
/
WCONPROD
      'OP_6' 'OPEN' 'ORAT' 20000  4* 1000 /
/
TSTEP            -- 8
10 /
)~" };

        return Opm::Parser{}.parseString(input);
    }

    Opm::SummaryState sim_state()
    {
        auto state = Opm::SummaryState{};

        state.add("WOPR:OP_1" ,    1.0);
        state.add("WWPR:OP_1" ,    2.0);
        state.add("WGPR:OP_1" ,    3.0);
        state.add("WVPR:OP_1" ,    4.0);
        state.add("WOPT:OP_1" ,   10.0);
        state.add("WWPT:OP_1" ,   20.0);
        state.add("WGPT:OP_1" ,   30.0);
        state.add("WVPT:OP_1" ,   40.0);
        state.add("WWIR:OP_1" ,    0.0);
        state.add("WGIR:OP_1" ,    0.0);
        state.add("WWIT:OP_1" ,    0.0);
        state.add("WGIT:OP_1" ,    0.0);
        state.add("WWCT:OP_1" ,    0.625);
        state.add("WGOR:OP_1" ,  234.5);
        state.add("WBHP:OP_1" ,  314.15);
        state.add("WOPTH:OP_1",  345.6);
        state.add("WWPTH:OP_1",  456.7);
        state.add("WGPTH:OP_1",  567.8);
        state.add("WWITH:OP_1",    0.0);
        state.add("WGITH:OP_1",    0.0);
        state.add("WGVIR:OP_1",    0.0);
        state.add("WWVIR:OP_1",    0.0);

        state.add("WOPR:OP_2" ,    0.0);
        state.add("WWPR:OP_2" ,    0.0);
        state.add("WGPR:OP_2" ,    0.0);
        state.add("WVPR:OP_2" ,    0.0);
        state.add("WOPT:OP_2" ,    0.0);
        state.add("WWPT:OP_2" ,    0.0);
        state.add("WGPT:OP_2" ,    0.0);
        state.add("WVPT:OP_2" ,    0.0);
        state.add("WWIR:OP_2" ,  100.0);
        state.add("WGIR:OP_2" ,  200.0);
        state.add("WWIT:OP_2" , 1000.0);
        state.add("WGIT:OP_2" , 2000.0);
        state.add("WWCT:OP_2" ,    0.0);
        state.add("WGOR:OP_2" ,    0.0);
        state.add("WBHP:OP_2" ,  400.6);
        state.add("WOPTH:OP_2",    0.0);
        state.add("WWPTH:OP_2",    0.0);
        state.add("WGPTH:OP_2",    0.0);
        state.add("WWITH:OP_2", 1515.0);
        state.add("WGITH:OP_2", 3030.0);
        state.add("WGVIR:OP_2", 1234.0);
        state.add("WWVIR:OP_2", 4321.0);

        state.add("WOPR:OP_3" ,   11.0);
        state.add("WWPR:OP_3" ,   12.0);
        state.add("WGPR:OP_3" ,   13.0);
        state.add("WVPR:OP_3" ,   14.0);
        state.add("WOPT:OP_3" ,  110.0);
        state.add("WWPT:OP_3" ,  120.0);
        state.add("WGPT:OP_3" ,  130.0);
        state.add("WVPT:OP_3" ,  140.0);
        state.add("WWIR:OP_3" ,    0.0);
        state.add("WGIR:OP_3" ,    0.0);
        state.add("WWIT:OP_3" ,    0.0);
        state.add("WGIT:OP_3" ,    0.0);
        state.add("WWCT:OP_3" ,    0.0625);
        state.add("WGOR:OP_3" , 1234.5);
        state.add("WBHP:OP_3" ,  314.15);
        state.add("WOPTH:OP_3", 2345.6);
        state.add("WWPTH:OP_3", 3456.7);
        state.add("WGPTH:OP_3", 4567.8);
        state.add("WWITH:OP_3",    0.0);
        state.add("WGITH:OP_3",    0.0);
        state.add("WGVIR:OP_3",    0.0);
        state.add("WWVIR:OP_3",   43.21);

        return state;
    }

    Opm::data::WellRates well_rates_1()
    {
        using o = ::Opm::data::Rates::opt;

        auto xw = ::Opm::data::WellRates{};

        {
            xw["OP_1"].rates
                .set(o::wat, 1.0)
                .set(o::oil, 2.0)
                .set(o::gas, 3.0);

            xw["OP_1"].connections.emplace_back();
            auto& c = xw["OP_1"].connections.back();

            c.rates.set(o::wat, 1.0)
                   .set(o::oil, 2.0)
                   .set(o::gas, 3.0);
        }

        {
            xw["OP_2"].bhp = 234.0;

            xw["OP_2"].rates.set(o::gas, 5.0);
            xw["OP_2"].connections.emplace_back();
        }

        return xw;
    }

    Opm::data::WellRates well_rates_2()
    {
        using o = ::Opm::data::Rates::opt;

        auto xw = ::Opm::data::WellRates{};

        {
            xw["OP_1"].bhp = 150.0;  // Closed
        }

        {
            xw["OP_2"].bhp = 234.0;

            xw["OP_2"].rates.set(o::wat, 5.0);
            xw["OP_2"].connections.emplace_back();

            auto& c = xw["OP_2"].connections.back();
            c.rates.set(o::wat, 5.0);
        }

        return xw;
    }
}

struct SimulationCase
{
    explicit SimulationCase(const Opm::Deck& deck)
        : es   { deck }
        , sched{ deck, es }
    {}

    // Order requirement: 'es' must be declared/initialised before 'sched'.
    Opm::EclipseState es;
    Opm::Schedule     sched;
};

// =====================================================================

BOOST_AUTO_TEST_SUITE(Aggregate_WD)

BOOST_AUTO_TEST_CASE (Constructor)
{
    const auto ih = MockIH{ 5 };

    const auto awd = Opm::RestartIO::Helpers::AggregateWellData{ ih.value };

    BOOST_CHECK_EQUAL(awd.getIWell().size(), ih.nwells * ih.niwelz);
    BOOST_CHECK_EQUAL(awd.getSWell().size(), ih.nwells * ih.nswelz);
    BOOST_CHECK_EQUAL(awd.getXWell().size(), ih.nwells * ih.nxwelz);
    BOOST_CHECK_EQUAL(awd.getZWell().size(), ih.nwells * ih.nzwelz);
}

BOOST_AUTO_TEST_CASE (Declared_Well_Data)
{
    const auto simCase = SimulationCase{first_sim()};

    // Report Step 1: 2008-10-10 --> 2011-01-20
    const auto rptStep = std::size_t{1};

    const auto ih = MockIH {
        static_cast<int>(simCase.sched.getWells2(rptStep).size())
    };

    BOOST_CHECK_EQUAL(ih.nwells, MockIH::Sz{2});

    const auto smry = sim_state();
    auto awd = Opm::RestartIO::Helpers::AggregateWellData{ih.value};
    awd.captureDeclaredWellData(simCase.sched,
                                simCase.es.getUnits(), rptStep, smry, ih.value);

    // IWEL (OP_1)
    {
        using Ix = ::Opm::RestartIO::Helpers::VectorItems::IWell::index;

        const auto start = 0*ih.niwelz;

        const auto& iwell = awd.getIWell();
        BOOST_CHECK_EQUAL(iwell[start + Ix::IHead] , 9); // OP_1 -> I
        BOOST_CHECK_EQUAL(iwell[start + Ix::JHead] , 9); // OP_1 -> J
        BOOST_CHECK_EQUAL(iwell[start + Ix::FirstK], 1); // OP_1/Head -> K
        BOOST_CHECK_EQUAL(iwell[start + Ix::NConn] , 2); // OP_1 #Compl
        BOOST_CHECK_EQUAL(iwell[start + Ix::WType] , 1); // OP_1 -> Producer
        BOOST_CHECK_EQUAL(iwell[start + Ix::VFPTab], 0); // VFP defaulted -> 0

        // Completion order
        BOOST_CHECK_EQUAL(iwell[start + Ix::CompOrd], 0); // Track ordering (default)

        BOOST_CHECK_EQUAL(iwell[start + Ix::item18], -100); // M2 Magic
        BOOST_CHECK_EQUAL(iwell[start + Ix::item25], -  1); // M2 Magic
        BOOST_CHECK_EQUAL(iwell[start + Ix::item48], -  1); // M2 Magic
        BOOST_CHECK_EQUAL(iwell[start + Ix::item32],    7); // M2 Magic
    }

    // IWEL (OP_2)
    {
        using Ix = ::Opm::RestartIO::Helpers::VectorItems::IWell::index;

        const auto start = 1*ih.niwelz;

        const auto& iwell = awd.getIWell();
        BOOST_CHECK_EQUAL(iwell[start + Ix::IHead] , 9); // OP_2 -> I
        BOOST_CHECK_EQUAL(iwell[start + Ix::JHead] , 9); // OP_2 -> J
        BOOST_CHECK_EQUAL(iwell[start + Ix::FirstK], 2); // OP_2/Head -> K
        BOOST_CHECK_EQUAL(iwell[start + Ix::NConn] , 1); // OP_2 #Compl
        BOOST_CHECK_EQUAL(iwell[start + Ix::WType] , 4); // OP_2 -> Gas Inj.
        BOOST_CHECK_EQUAL(iwell[start + Ix::VFPTab], 0); // VFP defaulted -> 0

        // Completion order
        BOOST_CHECK_EQUAL(iwell[start + Ix::CompOrd], 0); // Track ordering (default)

        BOOST_CHECK_EQUAL(iwell[start + Ix::item18], -100); // M2 Magic
        BOOST_CHECK_EQUAL(iwell[start + Ix::item25], -  1); // M2 Magic
        BOOST_CHECK_EQUAL(iwell[start + Ix::item48], -  1); // M2 Magic
        BOOST_CHECK_EQUAL(iwell[start + Ix::item32],    7); // M2 Magic
    }

    // SWEL (OP_1)
    {
        using Ix = ::Opm::RestartIO::Helpers::VectorItems::SWell::index;

        const auto i0 = 0*ih.nswelz;

        const auto& swell = awd.getSWell();
        BOOST_CHECK_CLOSE(swell[i0 + Ix::OilRateTarget], 20.0e3f, 1.0e-7f);

        // No WRAT limit
        BOOST_CHECK_CLOSE(swell[i0 + Ix::WatRateTarget], 1.0e20f, 1.0e-7f);

        // No GRAT limit
        BOOST_CHECK_CLOSE(swell[i0 + Ix::GasRateTarget], 1.0e20f, 1.0e-7f);

        // LRAT limit derived from ORAT + WRAT (= ORAT + 0.0)
        BOOST_CHECK_CLOSE(swell[i0 + Ix::LiqRateTarget], 20.0e3f, 1.0e-7f);

        // No direct limit, extract value from 'smry' (WVPR:OP_1)
        BOOST_CHECK_CLOSE(swell[i0 + Ix::ResVRateTarget], 1.0e20f, 1.0e-7f);

        // No THP limit
        BOOST_CHECK_CLOSE(swell[i0 + Ix::THPTarget] ,    0.0f, 1.0e-7f);
        BOOST_CHECK_CLOSE(swell[i0 + Ix::BHPTarget] , 1000.0f, 1.0e-7f);
        BOOST_CHECK_CLOSE(swell[i0 + Ix::DatumDepth],  0.375f, 1.0e-7f);
    }

    // SWEL (OP_2)
    {
        using Ix = ::Opm::RestartIO::Helpers::VectorItems::SWell::index;

        const auto i1 = 1*ih.nswelz;

        const auto& swell = awd.getSWell();
        BOOST_CHECK_CLOSE(swell[i1 + Ix::THPTarget], 1.0e20f, 1.0e-7f);
        BOOST_CHECK_CLOSE(swell[i1 + Ix::BHPTarget],  400.0f, 1.0e-7f);

        BOOST_CHECK_CLOSE(swell[i1 + Ix::DatumDepth], 0.625f, 1.0e-7f);
    }

    // XWEL (OP_1)
    {
        using Ix = ::Opm::RestartIO::Helpers::VectorItems::XWell::index;

        const auto i0 = 0*ih.nxwelz;

        const auto& xwell = awd.getXWell();

        BOOST_CHECK_CLOSE(xwell[i0 + Ix::BHPTarget], 1000.0, 1.0e-10);
    }

    // XWEL (OP_2)
    {
        using Ix = ::Opm::RestartIO::Helpers::VectorItems::XWell::index;

        const auto i1 = 1*ih.nxwelz;

        const auto& xwell = awd.getXWell();

        BOOST_CHECK_CLOSE(xwell[i1 + Ix::BHPTarget], 400.0, 1.0e-10);
    }

    // ZWEL (OP_1)
    {
        using Ix = ::Opm::RestartIO::Helpers::VectorItems::ZWell::index;

        const auto i0 = 0*ih.nzwelz;

        const auto& zwell = awd.getZWell();

        BOOST_CHECK_EQUAL(zwell[i0 + Ix::WellName].c_str(), "OP_1    ");
    }

    // ZWEL (OP_2)
    {
        using Ix = ::Opm::RestartIO::Helpers::VectorItems::ZWell::index;

        const auto i1 = 1*ih.nzwelz;

        const auto& zwell = awd.getZWell();

        BOOST_CHECK_EQUAL(zwell[i1 + Ix::WellName].c_str(), "OP_2    ");
    }
}

// --------------------------------------------------------------------

BOOST_AUTO_TEST_CASE (Dynamic_Well_Data_Step1)
{
    const auto simCase = SimulationCase{first_sim()};

    // Report Step 1: 2008-10-10 --> 2011-01-20
    const auto rptStep = std::size_t{1};

    const auto ih = MockIH {
        static_cast<int>(simCase.sched.getWells2(rptStep).size())
    };

    const auto xw   = well_rates_1();
    const auto smry = sim_state();
    auto awd = Opm::RestartIO::Helpers::AggregateWellData{ih.value};

    awd.captureDynamicWellData(simCase.sched, rptStep, xw, smry);

    // IWEL (OP_1)
    {
        using Ix = ::Opm::RestartIO::Helpers::VectorItems::IWell::index;

        const auto i0 = 0*ih.niwelz;

        const auto& iwell = awd.getIWell();

        BOOST_CHECK_EQUAL(iwell[i0 + Ix::item9 ], iwell[i0 + Ix::ActWCtrl]);
        BOOST_CHECK_EQUAL(iwell[i0 + Ix::item11], 1);
    }

    // IWEL (OP_2)
    {
        using Ix = ::Opm::RestartIO::Helpers::VectorItems::IWell::index;

        const auto i1 = 1*ih.niwelz;

        const auto& iwell = awd.getIWell();

        BOOST_CHECK_EQUAL(iwell[i1 + Ix::item9 ], -1); // No flowing conns.
        BOOST_CHECK_EQUAL(iwell[i1 + Ix::item11],  1); // Well open/shut flag
    }

    // XWEL (OP_1)
    {
        using Ix = ::Opm::RestartIO::Helpers::VectorItems::XWell::index;

        const auto  i0 = 0*ih.nxwelz;
        const auto& xwell = awd.getXWell();

        BOOST_CHECK_CLOSE(xwell[i0 + Ix::OilPrRate], 1.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::WatPrRate], 2.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::GasPrRate], 3.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::LiqPrRate], 1.0 + 2.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::VoidPrRate], 4.0, 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i0 + Ix::FlowBHP], 314.15 , 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::WatCut] ,   0.625, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::GORatio], 234.5  , 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i0 + Ix::OilPrTotal], 10.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::WatPrTotal], 20.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::GasPrTotal], 30.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::VoidPrTotal], 40.0, 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i0 + Ix::item37],
                          xwell[i0 + Ix::WatPrRate], 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i0 + Ix::item38],
                          xwell[i0 + Ix::GasPrRate], 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i0 + Ix::HistOilPrTotal], 345.6, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::HistWatPrTotal], 456.7, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::HistGasPrTotal], 567.8, 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i0 + Ix::HistWatInjTotal], 0.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::HistGasInjTotal], 0.0, 1.0e-10);
    }

    // XWEL (OP_2)
    {
        using Ix = ::Opm::RestartIO::Helpers::VectorItems::XWell::index;

        const auto  i1 = 1*ih.nxwelz;
        const auto& xwell = awd.getXWell();

        BOOST_CHECK_CLOSE(xwell[i1 + Ix::GasPrRate], -200.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i1 + Ix::VoidPrRate], -1234.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i1 + Ix::FlowBHP], 400.6, 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i1 + Ix::WatInjTotal], 1000.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i1 + Ix::GasInjTotal], 2000.0, 1.0e-10);

        // Bg = VGIR / GIR = 1234.0 / 200.0
        BOOST_CHECK_CLOSE(xwell[i1 + Ix::GasFVF], 6.17, 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i1 + Ix::item38],
                          xwell[i1 + Ix::GasPrRate], 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i1 + Ix::HistOilPrTotal] ,    0.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i1 + Ix::HistWatPrTotal] ,    0.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i1 + Ix::HistGasPrTotal] ,    0.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i1 + Ix::HistWatInjTotal], 1515.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i1 + Ix::HistGasInjTotal], 3030.0, 1.0e-10);
    }
}

// --------------------------------------------------------------------

BOOST_AUTO_TEST_CASE (Dynamic_Well_Data_Step2)
{
    const auto simCase = SimulationCase{first_sim()};

    // Report Step 2: 2011-01-20 --> 2013-06-15
    const auto rptStep = std::size_t{2};

    const auto ih = MockIH {
        static_cast<int>(simCase.sched.getWells2(rptStep).size())
    };

    const auto xw   = well_rates_2();
    const auto smry = sim_state();
    auto awd = Opm::RestartIO::Helpers::AggregateWellData{ih.value};

    awd.captureDynamicWellData(simCase.sched, rptStep, xw, smry);

    // IWEL (OP_1) -- closed producer
    {
        using Ix = ::Opm::RestartIO::Helpers::VectorItems::IWell::index;

        const auto i0 = 0*ih.niwelz;

        const auto& iwell = awd.getIWell();

        BOOST_CHECK_EQUAL(iwell[i0 + Ix::item9] , -1000);
        BOOST_CHECK_EQUAL(iwell[i0 + Ix::item11], -1000);
    }

    // IWEL (OP_2) -- water injector
    {
        using Ix = ::Opm::RestartIO::Helpers::VectorItems::IWell::index;

        const auto i1 = 1*ih.niwelz;

        const auto& iwell = awd.getIWell();

        BOOST_CHECK_EQUAL(iwell[i1 + Ix::item9],
                          iwell[i1 + Ix::ActWCtrl]);
        BOOST_CHECK_EQUAL(iwell[i1 + Ix::item11], 1);
    }

    // XWEL (OP_1) -- closed producer
    {
        using Ix = ::Opm::RestartIO::Helpers::VectorItems::XWell::index;

        const auto  i0 = 0*ih.nxwelz;
        const auto& xwell = awd.getXWell();

        BOOST_CHECK_CLOSE(xwell[i0 + Ix::OilPrRate], 1.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::WatPrRate], 2.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::GasPrRate], 3.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::LiqPrRate], 1.0 + 2.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::VoidPrRate], 4.0, 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i0 + Ix::FlowBHP], 314.15, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::WatCut] , 0.625, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::GORatio], 234.5, 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i0 + Ix::OilPrTotal], 10.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::WatPrTotal], 20.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::GasPrTotal], 30.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::VoidPrTotal], 40.0, 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i0 + Ix::item37],
                          xwell[i0 + Ix::WatPrRate], 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i0 + Ix::item38],
                          xwell[i0 + Ix::GasPrRate], 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i0 + Ix::HistOilPrTotal], 345.6, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::HistWatPrTotal], 456.7, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i0 + Ix::HistGasPrTotal], 567.8, 1.0e-10);
    }

    // XWEL (OP_2) -- water injector
    {
        using Ix = ::Opm::RestartIO::Helpers::VectorItems::XWell::index;

        const auto  i1 = 1*ih.nxwelz;
        const auto& xwell = awd.getXWell();

        BOOST_CHECK_CLOSE(xwell[i1 + Ix::WatPrRate], -100.0, 1.0e-10);

        // Copy of WWIR
        BOOST_CHECK_CLOSE(xwell[i1 + Ix::LiqPrRate],
                          xwell[i1 + Ix::WatPrRate], 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i1 + Ix::FlowBHP], 400.6, 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i1 + Ix::WatInjTotal], 1000.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i1 + Ix::GasInjTotal], 2000.0, 1.0e-10);

        // Copy of WWIR
        BOOST_CHECK_CLOSE(xwell[i1 + Ix::item37],
                          xwell[i1 + Ix::WatPrRate], 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i1 + Ix::HistOilPrTotal] ,    0.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i1 + Ix::HistWatPrTotal] ,    0.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i1 + Ix::HistGasPrTotal] ,    0.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i1 + Ix::HistWatInjTotal], 1515.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i1 + Ix::HistGasInjTotal], 3030.0, 1.0e-10);

        // WWVIR
        BOOST_CHECK_CLOSE(xwell[i1 + Ix::WatVoidPrRate],
                          -4321.0, 1.0e-10);
    }

    // XWEL (OP_3) -- producer
    {
        using Ix = ::Opm::RestartIO::Helpers::VectorItems::XWell::index;

        const auto  i2 = 2*ih.nxwelz;
        const auto& xwell = awd.getXWell();

        BOOST_CHECK_CLOSE(xwell[i2 + Ix::OilPrRate], 11.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i2 + Ix::WatPrRate], 12.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i2 + Ix::GasPrRate], 13.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i2 + Ix::LiqPrRate], 11.0 + 12.0, 1.0e-10); // LPR
        BOOST_CHECK_CLOSE(xwell[i2 + Ix::VoidPrRate], 14.0, 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i2 + Ix::FlowBHP], 314.15, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i2 + Ix::WatCut] , 0.0625, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i2 + Ix::GORatio], 1234.5, 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i2 + Ix::OilPrTotal], 110.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i2 + Ix::WatPrTotal], 120.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i2 + Ix::GasPrTotal], 130.0, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i2 + Ix::VoidPrTotal], 140.0, 1.0e-10);

        // Copy of WWPR
        BOOST_CHECK_CLOSE(xwell[i2 + Ix::item37],
                          xwell[i2 + Ix::WatPrRate], 1.0e-10);

        // Copy of WGPR
        BOOST_CHECK_CLOSE(xwell[i2 + Ix::item38],
                          xwell[i2 + Ix::GasPrRate], 1.0e-10);

        BOOST_CHECK_CLOSE(xwell[i2 + Ix::HistOilPrTotal], 2345.6, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i2 + Ix::HistWatPrTotal], 3456.7, 1.0e-10);
        BOOST_CHECK_CLOSE(xwell[i2 + Ix::HistGasPrTotal], 4567.8, 1.0e-10);
    }
}

BOOST_AUTO_TEST_SUITE_END()
