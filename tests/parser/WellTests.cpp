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

#include <stdexcept>
#include <iostream>
#include <boost/filesystem.hpp>

#define BOOST_TEST_MODULE WellTest
#include <boost/test/unit_test.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <opm/parser/eclipse/Units/Units.hpp>
#include <opm/parser/eclipse/Units/UnitSystem.hpp>
#include <opm/parser/eclipse/Deck/Deck.hpp>
#include <opm/parser/eclipse/Deck/DeckItem.hpp>
#include <opm/parser/eclipse/Deck/DeckRecord.hpp>

#include <opm/parser/eclipse/EclipseState/EclipseState.hpp>
#include <opm/parser/eclipse/EclipseState/Grid/EclipseGrid.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Schedule.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/SummaryState.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/UDQ/UDQActive.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/ScheduleEnums.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/Connection.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/WellConnections.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/TimeMap.hpp>
#include <opm/parser/eclipse/Parser/ParseContext.hpp>
#include <opm/parser/eclipse/Parser/Parser.hpp>

#include "src/opm/parser/eclipse/EclipseState/Schedule/Well/WellProductionProperties.hpp"
#include "src/opm/parser/eclipse/EclipseState/Schedule/Well/WellInjectionProperties.hpp"

using namespace Opm;


namespace Opm {
inline std::ostream& operator<<( std::ostream& stream, const Connection& c ) {
    return stream << "(" << c.getI() << "," << c.getJ() << "," << c.getK() << ")";
}
inline std::ostream& operator<<( std::ostream& stream, const Well2& well ) {
    return stream << "(" << well.name() << ")";
}
}


BOOST_AUTO_TEST_CASE(WellCOMPDATtestTRACK) {
    Opm::Parser parser;
    std::string input =
                "START             -- 0 \n"
                "19 JUN 2007 / \n"
                "SCHEDULE\n"
                "DATES             -- 1\n"
                " 10  OKT 2008 / \n"
                "/\n"
                "WELSPECS\n"
                "    'OP_1'       'OP'   9   9 1*     'OIL' 1*      1*  1*   1*  1*   1*  1*  / \n"
                "/\n"
                "COMPORD\n"
                " OP_1 TRACK / \n"
                "/\n"
                "COMPDAT\n"
                " 'OP_1'  9  9   1   1 'OPEN' 1*   32.948   0.311  3047.839 1*  1*  'X'  22.100 / \n"
                " 'OP_1'  9  9   3   9 'OPEN' 1*   32.948   0.311  3047.839 1*  1*  'X'  22.100 / \n"
                " 'OP_1'  9  9   2   2 'OPEN' 1*   46.825   0.311  4332.346 1*  1*  'X'  22.123 / \n"
                "/\n"
                "DATES             -- 2\n"
                " 20  JAN 2010 / \n"
                "/\n";


    auto deck = parser.parseString(input);
    Opm::EclipseGrid grid(10,10,10);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Opm::Runspec runspec (deck);
    Opm::Schedule schedule(deck, grid , eclipseProperties, runspec);
    const auto& op_1 = schedule.getWell2("OP_1", 2);

    const auto& completions = op_1.getConnections();
    BOOST_CHECK_EQUAL(9U, completions.size());

    //Verify TRACK completion ordering
    for (size_t k = 0; k < completions.size(); ++k) {
        BOOST_CHECK_EQUAL(completions.get( k ).getK(), k);
    }
}


BOOST_AUTO_TEST_CASE(WellCOMPDATtestDefaultTRACK) {
    Opm::Parser parser;
    std::string input =
                "START             -- 0 \n"
                "19 JUN 2007 / \n"
                "SCHEDULE\n"
                "DATES             -- 1\n"
                " 10  OKT 2008 / \n"
                "/\n"
                "WELSPECS\n"
                "    'OP_1'       'OP'   9   9 1*     'OIL' 1*      1*  1*   1*  1*   1*  1*  / \n"
                "/\n"
                "COMPDAT\n"
                " 'OP_1'  9  9   1   1 'OPEN' 1*   32.948   0.311  3047.839 1*  1*  'X'  22.100 / \n"
                " 'OP_1'  9  9   3   9 'OPEN' 1*   32.948   0.311  3047.839 1*  1*  'X'  22.100 / \n"
                " 'OP_1'  9  9   2   2 'OPEN' 1*   46.825   0.311  4332.346 1*  1*  'X'  22.123 / \n"
                "/\n"
                "DATES             -- 2\n"
                " 20  JAN 2010 / \n"
                "/\n";


    auto deck = parser.parseString(input);
    Opm::EclipseGrid grid(10,10,10);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Opm::Runspec runspec (deck);
    Opm::Schedule schedule(deck, grid , eclipseProperties, runspec);
    const auto& op_1 = schedule.getWell2("OP_1", 2);

    const auto& completions = op_1.getConnections();
    BOOST_CHECK_EQUAL(9U, completions.size());

    //Verify TRACK completion ordering
    for (size_t k = 0; k < completions.size(); ++k) {
        BOOST_CHECK_EQUAL(completions.get( k ).getK(), k);
    }
}

BOOST_AUTO_TEST_CASE(WellCOMPDATtestINPUT) {
    Opm::Parser parser;
    std::string input =
                "START             -- 0 \n"
                "19 JUN 2007 / \n"
                "SCHEDULE\n"
                "DATES             -- 1\n"
                " 10  OKT 2008 / \n"
                "/\n"
                "WELSPECS\n"
                "    'OP_1'       'OP'   9   9 1*     'OIL' 1*      1*  1*   1*  1*   1*  1*  / \n"
                "/\n"
                "COMPORD\n"
                " OP_1 INPUT / \n"
                "/\n"
                "COMPDAT\n"
                " 'OP_1'  9  9   1   1 'OPEN' 1*   32.948   0.311  3047.839 1*  1*  'X'  22.100 / \n"
                " 'OP_1'  9  9   3   9 'OPEN' 1*   32.948   0.311  3047.839 1*  1*  'X'  22.100 / \n"
                " 'OP_1'  9  9   2   2 'OPEN' 1*   46.825   0.311  4332.346 1*  1*  'X'  22.123 / \n"
                "/\n"
                "DATES             -- 2\n"
                " 20  JAN 2010 / \n"
                "/\n";


    auto deck = parser.parseString(input);
    Opm::EclipseGrid grid(10,10,10);
    Opm::ErrorGuard errors;
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Opm::Runspec runspec (deck);
    Opm::Schedule schedule(deck, grid , eclipseProperties, runspec, Opm::ParseContext(), errors);
    const auto& op_1 = schedule.getWell2("OP_1", 2);

    const auto& completions = op_1.getConnections();
    BOOST_CHECK_EQUAL(9U, completions.size());

    //Verify INPUT completion ordering
    BOOST_CHECK_EQUAL(completions.get( 0 ).getK(), 0);
    BOOST_CHECK_EQUAL(completions.get( 1 ).getK(), 2);
    BOOST_CHECK_EQUAL(completions.get( 2 ).getK(), 3);
    BOOST_CHECK_EQUAL(completions.get( 3 ).getK(), 4);
    BOOST_CHECK_EQUAL(completions.get( 4 ).getK(), 5);
    BOOST_CHECK_EQUAL(completions.get( 5 ).getK(), 6);
    BOOST_CHECK_EQUAL(completions.get( 6 ).getK(), 7);
    BOOST_CHECK_EQUAL(completions.get( 7 ).getK(), 8);
    BOOST_CHECK_EQUAL(completions.get( 8 ).getK(), 1);
}

BOOST_AUTO_TEST_CASE(NewWellZeroCompletions) {
    Opm::Well2 well("WELL1", "GROUP", 0, 1, 0, 0, 0.0, Opm::Phase::OIL, Opm::WellProducer::CMODE_UNDEFINED,  WellCompletion::DEPTH, UnitSystem::newMETRIC(), 0);
    BOOST_CHECK_EQUAL( 0U , well.getConnections( ).size() );
}


BOOST_AUTO_TEST_CASE(isProducerCorrectlySet) {
    // HACK: This test checks correctly setting of isProducer/isInjector. This property depends on which of
    //       WellProductionProperties/WellInjectionProperties is set last, independent of actual values.
    {
        Opm::Well2 well("WELL1" , "GROUP", 0, 1, 0, 0, 0.0, Opm::Phase::OIL, Opm::WellProducer::CMODE_UNDEFINED, WellCompletion::DEPTH, UnitSystem::newMETRIC(), 0);


        /* 1: Well is created as producer */
        BOOST_CHECK_EQUAL( false , well.isInjector());
        BOOST_CHECK_EQUAL( true , well.isProducer());

        /* Set a surface injection rate => Well becomes an Injector */
        auto injectionProps1 = std::make_shared<Opm::WellInjectionProperties>(well.getInjectionProperties());
        injectionProps1->surfaceInjectionRate.reset(100);
        well.updateInjection(injectionProps1);
        BOOST_CHECK_EQUAL( true  , well.isInjector());
        BOOST_CHECK_EQUAL( false , well.isProducer());
        BOOST_CHECK_EQUAL( 100 , well.getInjectionProperties().surfaceInjectionRate.get<double>());
    }


    {
        Opm::Well2 well("WELL1" , "GROUP", 0, 1, 0, 0, 0.0, Opm::Phase::OIL, Opm::WellProducer::CMODE_UNDEFINED, WellCompletion::DEPTH, UnitSystem::newMETRIC(), 0);

        /* Set a reservoir injection rate => Well becomes an Injector */
        auto injectionProps2 = std::make_shared<Opm::WellInjectionProperties>(well.getInjectionProperties());
        injectionProps2->reservoirInjectionRate.reset(200);
        well.updateInjection(injectionProps2);
        BOOST_CHECK_EQUAL( false , well.isProducer());
        BOOST_CHECK_EQUAL( 200 , well.getInjectionProperties().reservoirInjectionRate.get<double>());
    }

    {
        Opm::Well2 well("WELL1" , "GROUP", 0, 1, 0, 0, 0.0, Opm::Phase::OIL, Opm::WellProducer::CMODE_UNDEFINED, WellCompletion::DEPTH, UnitSystem::newMETRIC(), 0);

        /* Set rates => Well becomes a producer; injection rate should be set to 0. */
        auto injectionProps3 = std::make_shared<Opm::WellInjectionProperties>(well.getInjectionProperties());
        well.updateInjection(injectionProps3);

        auto properties = std::make_shared<Opm::WellProductionProperties>( well.getProductionProperties() );
        properties->OilRate.reset(100);
        properties->GasRate.reset(200);
        properties->WaterRate.reset(300);
        well.updateProduction(properties);

        BOOST_CHECK_EQUAL( false , well.isInjector());
        BOOST_CHECK_EQUAL( true , well.isProducer());
        BOOST_CHECK_EQUAL( 0 , well.getInjectionProperties().surfaceInjectionRate.get<double>());
        BOOST_CHECK_EQUAL( 0 , well.getInjectionProperties().reservoirInjectionRate.get<double>());
        BOOST_CHECK_EQUAL( 100 , well.getProductionProperties().OilRate.get<double>());
        BOOST_CHECK_EQUAL( 200 , well.getProductionProperties().GasRate.get<double>());
        BOOST_CHECK_EQUAL( 300 , well.getProductionProperties().WaterRate.get<double>());
    }
}

BOOST_AUTO_TEST_CASE(GroupnameCorretlySet) {
    Opm::Well2 well("WELL1" , "G1", 0, 1, 0, 0, 0.0, Opm::Phase::OIL, Opm::WellProducer::CMODE_UNDEFINED, WellCompletion::DEPTH, UnitSystem::newMETRIC(), 0);

    BOOST_CHECK_EQUAL("G1" , well.groupName());
    well.updateGroup( "GROUP2");
    BOOST_CHECK_EQUAL("GROUP2" , well.groupName());
}


BOOST_AUTO_TEST_CASE(addWELSPECS_setData_dataSet) {
    Opm::Well2 well("WELL1", "GROUP", 0, 1, 23, 42, 2334.32, Opm::Phase::WATER, WellProducer::CMODE_UNDEFINED, WellCompletion::DEPTH, UnitSystem::newMETRIC(), 0);

    BOOST_CHECK_EQUAL(23, well.getHeadI());
    BOOST_CHECK_EQUAL(42, well.getHeadJ());
    BOOST_CHECK_EQUAL(2334.32, well.getRefDepth());
    BOOST_CHECK_EQUAL(Opm::Phase::WATER, well.getPreferredPhase());
}


BOOST_AUTO_TEST_CASE(XHPLimitDefault) {
    Opm::Well2 well("WELL1", "GROUP", 0, 1, 23, 42, 2334.32, Opm::Phase::WATER, WellProducer::CMODE_UNDEFINED, WellCompletion::DEPTH, UnitSystem::newMETRIC(), 0);


    auto productionProps = std::make_shared<Opm::WellProductionProperties>(well.getProductionProperties());
    productionProps->BHPLimit.reset(100);
    productionProps->addProductionControl(Opm::WellProducer::BHP);
    well.updateProduction(productionProps);
    BOOST_CHECK_EQUAL( 100 , well.getProductionProperties().BHPLimit.get<double>());
    BOOST_CHECK_EQUAL( true, well.getProductionProperties().hasProductionControl( Opm::WellProducer::BHP ));

    auto injProps = std::make_shared<Opm::WellInjectionProperties>(well.getInjectionProperties());
    injProps->THPLimit.reset(200);
    well.updateInjection(injProps);
    BOOST_CHECK_EQUAL( 200 , well.getInjectionProperties().THPLimit.get<double>());
    BOOST_CHECK( !well.getInjectionProperties().hasInjectionControl( Opm::WellInjector::THP ));
}



BOOST_AUTO_TEST_CASE(InjectorType) {
    Opm::Well2 well("WELL1", "GROUP", 0, 1, 23, 42, 2334.32, Opm::Phase::WATER, WellProducer::CMODE_UNDEFINED, WellCompletion::DEPTH, UnitSystem::newMETRIC(), 0);

    auto injectionProps = std::make_shared<Opm::WellInjectionProperties>(well.getInjectionProperties());
    injectionProps->injectorType = Opm::WellInjector::WATER;
    well.updateInjection(injectionProps);
    // TODO: Should we test for something other than wate here, as long as
    //       the default value for InjectorType is WellInjector::WATER?
    BOOST_CHECK_EQUAL( Opm::WellInjector::WATER , well.getInjectionProperties().injectorType);
}



/*****************************************************************/


BOOST_AUTO_TEST_CASE(WellHaveProductionControlLimit) {
    Opm::Well2 well("WELL1", "GROUP", 0, 1, 23, 42, 2334.32, Opm::Phase::WATER, WellProducer::CMODE_UNDEFINED, WellCompletion::DEPTH, UnitSystem::newMETRIC(), 0);


    BOOST_CHECK( !well.getProductionProperties().hasProductionControl( Opm::WellProducer::ORAT ));
    BOOST_CHECK( !well.getProductionProperties().hasProductionControl( Opm::WellProducer::RESV ));

    auto properties1 = std::make_shared<Opm::WellProductionProperties>(well.getProductionProperties());
    properties1->OilRate.reset(100);
    properties1->addProductionControl(Opm::WellProducer::ORAT);
    well.updateProduction(properties1);
    BOOST_CHECK(  well.getProductionProperties().hasProductionControl( Opm::WellProducer::ORAT ));
    BOOST_CHECK( !well.getProductionProperties().hasProductionControl( Opm::WellProducer::RESV ));

    auto properties2 = std::make_shared<Opm::WellProductionProperties>(well.getProductionProperties());
    properties2->ResVRate.reset( 100 );
    properties2->addProductionControl(Opm::WellProducer::RESV);
    well.updateProduction(properties2);
    BOOST_CHECK( well.getProductionProperties().hasProductionControl( Opm::WellProducer::RESV ));

    auto properties3 = std::make_shared<Opm::WellProductionProperties>(well.getProductionProperties());
    properties3->OilRate.reset(100);
    properties3->WaterRate.reset(100);
    properties3->GasRate.reset(100);
    properties3->LiquidRate.reset(100);
    properties3->ResVRate.reset(100);
    properties3->BHPLimit.reset(100);
    properties3->THPLimit.reset(100);
    properties3->addProductionControl(Opm::WellProducer::ORAT);
    properties3->addProductionControl(Opm::WellProducer::LRAT);
    properties3->addProductionControl(Opm::WellProducer::BHP);
    well.updateProduction(properties3);

    BOOST_CHECK( well.getProductionProperties().hasProductionControl( Opm::WellProducer::ORAT ));
    BOOST_CHECK( well.getProductionProperties().hasProductionControl( Opm::WellProducer::LRAT ));
    BOOST_CHECK( well.getProductionProperties().hasProductionControl( Opm::WellProducer::BHP ));

    auto properties4 = std::make_shared<Opm::WellProductionProperties>(well.getProductionProperties());
    properties4->dropProductionControl( Opm::WellProducer::LRAT );
    well.updateProduction(properties4);

    BOOST_CHECK( well.getProductionProperties().hasProductionControl( Opm::WellProducer::ORAT ));
    BOOST_CHECK(!well.getProductionProperties().hasProductionControl( Opm::WellProducer::LRAT ));
    BOOST_CHECK( well.getProductionProperties().hasProductionControl( Opm::WellProducer::BHP ));
}


BOOST_AUTO_TEST_CASE(WellHaveInjectionControlLimit) {
    Opm::Well2 well("WELL1", "GROUP", 0, 1, 23, 42, 2334.32, Opm::Phase::WATER, WellProducer::CMODE_UNDEFINED, WellCompletion::DEPTH, UnitSystem::newMETRIC(), 0);

    BOOST_CHECK( !well.getInjectionProperties().hasInjectionControl( Opm::WellInjector::RATE ));
    BOOST_CHECK( !well.getInjectionProperties().hasInjectionControl( Opm::WellInjector::RESV ));

    auto injProps1 = std::make_shared<Opm::WellInjectionProperties>(well.getInjectionProperties());
    injProps1->surfaceInjectionRate.reset(100);
    injProps1->addInjectionControl(Opm::WellInjector::RATE);
    well.updateInjection(injProps1);
    BOOST_CHECK(  well.getInjectionProperties().hasInjectionControl( Opm::WellInjector::RATE ));
    BOOST_CHECK( !well.getInjectionProperties().hasInjectionControl( Opm::WellInjector::RESV ));

    auto injProps2 = std::make_shared<Opm::WellInjectionProperties>(well.getInjectionProperties());
    injProps2->reservoirInjectionRate.reset(100);
    injProps2->addInjectionControl(Opm::WellInjector::RESV);
    well.updateInjection(injProps2);
    BOOST_CHECK( well.getInjectionProperties().hasInjectionControl( Opm::WellInjector::RESV ));

    auto injProps3 = std::make_shared<Opm::WellInjectionProperties>(well.getInjectionProperties());
    injProps3->BHPLimit.reset(100);
    injProps3->addInjectionControl(Opm::WellInjector::BHP);
    injProps3->THPLimit.reset(100);
    injProps3->addInjectionControl(Opm::WellInjector::THP);
    well.updateInjection(injProps3);

    BOOST_CHECK( well.getInjectionProperties().hasInjectionControl( Opm::WellInjector::RATE ));
    BOOST_CHECK( well.getInjectionProperties().hasInjectionControl( Opm::WellInjector::RESV ));
    BOOST_CHECK( well.getInjectionProperties().hasInjectionControl( Opm::WellInjector::THP ));
    BOOST_CHECK( well.getInjectionProperties().hasInjectionControl( Opm::WellInjector::BHP ));


    auto injProps4 = std::make_shared<Opm::WellInjectionProperties>(well.getInjectionProperties());
    injProps4->dropInjectionControl( Opm::WellInjector::RESV );
    well.updateInjection(injProps4);
    BOOST_CHECK(  well.getInjectionProperties().hasInjectionControl( Opm::WellInjector::RATE ));
    BOOST_CHECK( !well.getInjectionProperties().hasInjectionControl( Opm::WellInjector::RESV ));
    BOOST_CHECK(  well.getInjectionProperties().hasInjectionControl( Opm::WellInjector::THP ));
    BOOST_CHECK(  well.getInjectionProperties().hasInjectionControl( Opm::WellInjector::BHP ));
}
/*********************************************************************/

BOOST_AUTO_TEST_CASE(WellGuideRatePhase_GuideRatePhaseSet) {
    Opm::Well2 well("WELL1" , "GROUP", 0, 1, 0, 0, 0.0, Opm::Phase::OIL, Opm::WellProducer::CMODE_UNDEFINED, WellCompletion::DEPTH, UnitSystem::newMETRIC(), 0);

    BOOST_CHECK_EQUAL(Opm::GuideRate::UNDEFINED, well.getGuideRatePhase());

    BOOST_CHECK(well.updateWellGuideRate(true, 100, Opm::GuideRate::RAT, 66.0));
    BOOST_CHECK_EQUAL(Opm::GuideRate::RAT, well.getGuideRatePhase());
    BOOST_CHECK_EQUAL(100, well.getGuideRate());
    BOOST_CHECK_EQUAL(66.0, well.getGuideRateScalingFactor());
}

BOOST_AUTO_TEST_CASE(WellEfficiencyFactorSet) {
    Opm::Well2 well("WELL1" , "GROUP", 0, 1, 0, 0, 0.0, Opm::Phase::OIL, Opm::WellProducer::CMODE_UNDEFINED, WellCompletion::DEPTH, UnitSystem::newMETRIC(), 0);

    BOOST_CHECK_EQUAL(1.0, well.getEfficiencyFactor());
    BOOST_CHECK( well.updateEfficiencyFactor(0.9));
    BOOST_CHECK_EQUAL(0.9, well.getEfficiencyFactor());
}


namespace {
    namespace WCONHIST {
        std::string all_specified_CMODE_THP() {
            const std::string input =
                "WCONHIST\n"
                "'P' 'OPEN' 'THP' 1 2 3/\n/\n";

            return input;
        }



        std::string all_specified() {
            const std::string input =
                "WCONHIST\n"
                "'P' 'OPEN' 'ORAT' 1 2 3/\n/\n";

            return input;
        }

        std::string orat_defaulted() {
            const std::string input =
                "WCONHIST\n"
                "'P' 'OPEN' 'WRAT' 1* 2 3/\n/\n";

            return input;
        }

        std::string owrat_defaulted() {
            const std::string input =
                "WCONHIST\n"
                "'P' 'OPEN' 'GRAT' 1* 1* 3/\n/\n";

            return input;
        }

        std::string all_defaulted() {
            const std::string input =
                "WCONHIST\n"
                "'P' 'OPEN' 'LRAT'/\n/\n";

            return input;
        }

        std::string all_defaulted_with_bhp() {
            const std::string input =
                "WCONHIST\n"
                "-- 1    2     3      4-9 10\n"
                "   'P' 'OPEN' 'RESV' 6*  500/\n/\n";

            return input;
        }

        std::string bhp_defaulted() {
            const std::string input =
                "WCONHIST\n"
                "-- 1    2     3    4-9 10\n"
                "  'P' 'OPEN' 'BHP' 6*  500/\n/\n";

            return input;
        }

        std::string all_defaulted_with_bhp_vfp_table() {
            const std::string input =
                "WCONHIST\n"
                "-- 1    2     3    4-6  7  8  9  10\n"
                "  'P' 'OPEN' 'RESV' 3*  3 10. 1* 500/\n/\n";

            return input;
        }


        Opm::WellProductionProperties properties(const std::string& input) {
            Opm::Parser parser;

            auto deck = parser.parseString(input);
            const auto& record = deck.getKeyword("WCONHIST").getRecord(0);
            Opm::WellProductionProperties hist("W");
            hist.handleWCONHIST(record);


            return hist;
        }
    } // namespace WCONHIST

    namespace WCONPROD {
        std::string
        all_specified_CMODE_BHP()
        {
            const std::string input =
                "WCONHIST\n"
                "'P' 'OPEN' 'BHP' 1 2 3/\n/\n";

            return input;
        }

        std::string orat_CMODE_other_defaulted()
        {
            const std::string input =
                "WCONPROD\n"
                "'P' 'OPEN' 'ORAT' 1 2 3/\n/\n";

            return input;
        }

        std::string thp_CMODE()
        {
            const std::string input =
                "WCONPROD\n"
                "'P' 'OPEN' 'THP' 1 2 3 3* 10. 8 13./\n/\n";

            return input;
        }

        std::string bhp_CMODE()
        {
            const std::string input =
                "WCONPROD\n"
                "'P' 'OPEN' 'BHP' 1 2 3 2* 20. 10. 8 13./\n/\n";

            return input;
        }


        Opm::WellProductionProperties properties(const std::string& input)
        {
            Opm::Parser parser;
            auto deck = parser.parseString(input);
            const auto& kwd     = deck.getKeyword("WCONPROD");
            const auto&  record = kwd.getRecord(0);
            Opm::WellProductionProperties pred("W");
            pred.handleWCONPROD("WELL", record);

            return pred;
        }
    }

} // namespace anonymous

BOOST_AUTO_TEST_CASE(WCH_All_Specified_BHP_Defaulted)
{
    const Opm::WellProductionProperties& p =
        WCONHIST::properties(WCONHIST::all_specified());

    BOOST_CHECK(p.hasProductionControl(Opm::WellProducer::ORAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::WRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::GRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::LRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::RESV));

    BOOST_CHECK_EQUAL(p.controlMode , Opm::WellProducer::ORAT);

    BOOST_CHECK(p.hasProductionControl(Opm::WellProducer::BHP));
    BOOST_CHECK_EQUAL(p.BHPLimit.get<double>(), 101325.);
}

BOOST_AUTO_TEST_CASE(WCH_ORAT_Defaulted_BHP_Defaulted)
{
    const Opm::WellProductionProperties& p =
        WCONHIST::properties(WCONHIST::orat_defaulted());

    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::ORAT));
    BOOST_CHECK(p.hasProductionControl(Opm::WellProducer::WRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::GRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::LRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::RESV));
    BOOST_CHECK_EQUAL(p.controlMode , Opm::WellProducer::WRAT);

    BOOST_CHECK(p.hasProductionControl(Opm::WellProducer::BHP));
    BOOST_CHECK_EQUAL(p.BHPLimit.get<double>(), 101325.);
}

BOOST_AUTO_TEST_CASE(WCH_OWRAT_Defaulted_BHP_Defaulted)
{
    const Opm::WellProductionProperties& p =
        WCONHIST::properties(WCONHIST::owrat_defaulted());

    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::ORAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::WRAT));
    BOOST_CHECK(p.hasProductionControl(Opm::WellProducer::GRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::LRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::RESV));
    BOOST_CHECK_EQUAL(p.controlMode , Opm::WellProducer::GRAT);

    BOOST_CHECK(p.hasProductionControl(Opm::WellProducer::BHP));
    BOOST_CHECK_EQUAL(p.BHPLimit.get<double>(), 101325.);
}

BOOST_AUTO_TEST_CASE(WCH_Rates_Defaulted_BHP_Defaulted)
{
    const Opm::WellProductionProperties& p =
        WCONHIST::properties(WCONHIST::all_defaulted());

    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::ORAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::WRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::GRAT));
    BOOST_CHECK(p.hasProductionControl(Opm::WellProducer::LRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::RESV));
    BOOST_CHECK_EQUAL(p.controlMode , Opm::WellProducer::LRAT);

    BOOST_CHECK(p.hasProductionControl(Opm::WellProducer::BHP));
    BOOST_CHECK_EQUAL(p.BHPLimit.get<double>(), 101325.);
}

BOOST_AUTO_TEST_CASE(WCH_Rates_Defaulted_BHP_Specified)
{
    const Opm::WellProductionProperties& p =
        WCONHIST::properties(WCONHIST::all_defaulted_with_bhp());

    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::ORAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::WRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::GRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::LRAT));
    BOOST_CHECK(p.hasProductionControl(Opm::WellProducer::RESV));

    BOOST_CHECK_EQUAL(p.controlMode , Opm::WellProducer::RESV);

    BOOST_CHECK_EQUAL(true, p.hasProductionControl(Opm::WellProducer::BHP));
    BOOST_CHECK_EQUAL(p.BHPLimit.get<double>(), 101325.);
}

BOOST_AUTO_TEST_CASE(WCH_Rates_NON_Defaulted_VFP)
{
    const Opm::WellProductionProperties& p =
        WCONHIST::properties(WCONHIST::all_defaulted_with_bhp_vfp_table());

    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::ORAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::WRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::GRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::LRAT));
    BOOST_CHECK(p.hasProductionControl(Opm::WellProducer::RESV));

    BOOST_CHECK_EQUAL(p.controlMode , Opm::WellProducer::RESV);

    BOOST_CHECK_EQUAL(true, p.hasProductionControl(Opm::WellProducer::BHP));
    BOOST_CHECK_EQUAL(p.VFPTableNumber, 3);
    BOOST_CHECK_EQUAL(p.ALQValue, 10.);
    BOOST_CHECK_EQUAL(p.BHPLimit.get<double>(), 101325.);
}

BOOST_AUTO_TEST_CASE(WCH_BHP_Specified)
{
    const Opm::WellProductionProperties& p =
        WCONHIST::properties(WCONHIST::bhp_defaulted());

    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::ORAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::WRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::GRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::LRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::RESV));

    BOOST_CHECK_EQUAL(p.controlMode , Opm::WellProducer::BHP);

    BOOST_CHECK_EQUAL(true, p.hasProductionControl(Opm::WellProducer::BHP));

    BOOST_CHECK_EQUAL(p.BHPLimit.get<double>(), 5.e7); // 500 barsa
}

BOOST_AUTO_TEST_CASE(WCONPROD_ORAT_CMode)
{
    const Opm::WellProductionProperties& p = WCONPROD::properties(WCONPROD::orat_CMODE_other_defaulted());

    BOOST_CHECK( p.hasProductionControl(Opm::WellProducer::ORAT));
    BOOST_CHECK( p.hasProductionControl(Opm::WellProducer::WRAT));
    BOOST_CHECK( p.hasProductionControl(Opm::WellProducer::GRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::LRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::RESV));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::THP));

    BOOST_CHECK_EQUAL(p.controlMode , Opm::WellProducer::ORAT);

    BOOST_CHECK_EQUAL(true, p.hasProductionControl(Opm::WellProducer::BHP));

    BOOST_CHECK_EQUAL(p.VFPTableNumber, 0);
    BOOST_CHECK_EQUAL(p.ALQValue, 0.);
}

BOOST_AUTO_TEST_CASE(WCONPROD_THP_CMode)
{
    const Opm::WellProductionProperties& p =
        WCONPROD::properties(WCONPROD::thp_CMODE());

    BOOST_CHECK( p.hasProductionControl(Opm::WellProducer::ORAT));
    BOOST_CHECK( p.hasProductionControl(Opm::WellProducer::WRAT));
    BOOST_CHECK( p.hasProductionControl(Opm::WellProducer::GRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::LRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::RESV));
    BOOST_CHECK( p.hasProductionControl(Opm::WellProducer::THP));

    BOOST_CHECK_EQUAL(p.controlMode , Opm::WellProducer::THP);

    BOOST_CHECK_EQUAL(true, p.hasProductionControl(Opm::WellProducer::BHP));

    BOOST_CHECK_EQUAL(p.VFPTableNumber, 8);
    BOOST_CHECK_EQUAL(p.ALQValue, 13.);
    BOOST_CHECK_EQUAL(p.THPLimit.get<double>(), 1000000.); // 10 barsa
    BOOST_CHECK_EQUAL(p.BHPLimit.get<double>(), 101325.); // 1 atm.
}

BOOST_AUTO_TEST_CASE(WCONPROD_BHP_CMode)
{
    const Opm::WellProductionProperties& p =
        WCONPROD::properties(WCONPROD::bhp_CMODE());

    BOOST_CHECK( p.hasProductionControl(Opm::WellProducer::ORAT));
    BOOST_CHECK( p.hasProductionControl(Opm::WellProducer::WRAT));
    BOOST_CHECK( p.hasProductionControl(Opm::WellProducer::GRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::LRAT));
    BOOST_CHECK( !p.hasProductionControl(Opm::WellProducer::RESV));
    BOOST_CHECK( p.hasProductionControl(Opm::WellProducer::THP));

    BOOST_CHECK_EQUAL(p.controlMode , Opm::WellProducer::BHP);

    BOOST_CHECK_EQUAL(true, p.hasProductionControl(Opm::WellProducer::BHP));

    BOOST_CHECK_EQUAL(p.VFPTableNumber, 8);
    BOOST_CHECK_EQUAL(p.ALQValue, 13.);
    BOOST_CHECK_EQUAL(p.THPLimit.get<double>(), 1000000.); // 10 barsa
    BOOST_CHECK_EQUAL(p.BHPLimit.get<double>(), 2000000.); // 20 barsa
}


BOOST_AUTO_TEST_CASE(BHP_CMODE)
{
    BOOST_CHECK_THROW( WCONHIST::properties(WCONHIST::all_specified_CMODE_THP()) , std::invalid_argument);
    BOOST_CHECK_THROW( WCONPROD::properties(WCONPROD::all_specified_CMODE_BHP()) , std::invalid_argument);
}




BOOST_AUTO_TEST_CASE(CMODE_DEFAULT) {
    const Opm::WellProductionProperties Pproperties("W");
    const Opm::WellInjectionProperties Iproperties("W");

    BOOST_CHECK_EQUAL( Pproperties.controlMode , Opm::WellProducer::CMODE_UNDEFINED );
    BOOST_CHECK_EQUAL( Iproperties.controlMode , Opm::WellInjector::CMODE_UNDEFINED );
}




BOOST_AUTO_TEST_CASE(WELL_CONTROLS) {
    Opm::Well2 well("WELL", "GROUP", 0, 0, 0, 0, 1000, Opm::Phase::OIL, Opm::WellProducer::CMODE_UNDEFINED, Opm::WellCompletion::DEPTH, UnitSystem::newMETRIC(), 0);
    Opm::WellProductionProperties prod("OP1");
    Opm::SummaryState st;
    well.productionControls(st);

    // Use a scalar FIELD variable - that should work; although it is a bit weird.
    st.update("FUX", 1);
    prod.OilRate = UDAValue("FUX");
    BOOST_CHECK_EQUAL(1, prod.controls(st, 0).oil_rate);


    // Use the wellrate WUX for well OP1; the well is now added with
    // SummaryState::update_well_var() and we should automatically fetch the
    // correct well value.
    prod.OilRate = UDAValue("WUX");
    st.update_well_var("OP1", "WUX", 10);
    BOOST_CHECK_EQUAL(10, prod.controls(st, 0).oil_rate);
}


BOOST_AUTO_TEST_CASE(ExtraAccessors) {
    Opm::Well2 inj("WELL1" , "GROUP", 0, 1, 0, 0, 0.0, Opm::Phase::OIL, Opm::WellProducer::CMODE_UNDEFINED, WellCompletion::DEPTH, UnitSystem::newMETRIC(), 0);
    Opm::Well2 prod("WELL1" , "GROUP", 0, 1, 0, 0, 0.0, Opm::Phase::OIL, Opm::WellProducer::CMODE_UNDEFINED, WellCompletion::DEPTH, UnitSystem::newMETRIC(), 0);

    auto inj_props= std::make_shared<Opm::WellInjectionProperties>(inj.getInjectionProperties());
    inj_props->VFPTableNumber = 100;
    inj.updateInjection(inj_props);

    auto prod_props = std::make_shared<Opm::WellProductionProperties>( prod.getProductionProperties() );
    prod_props->VFPTableNumber = 200;
    prod.updateProduction(prod_props);

    BOOST_CHECK_THROW(inj.alq_value(), std::runtime_error);
    BOOST_CHECK_THROW(prod.temperature(), std::runtime_error);
    BOOST_CHECK_EQUAL(inj.vfp_table_number(), 100);
    BOOST_CHECK_EQUAL(prod.vfp_table_number(), 200);
}
