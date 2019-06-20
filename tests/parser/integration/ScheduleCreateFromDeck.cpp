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

#define BOOST_TEST_MODULE ScheduleIntegrationTests
#include <math.h>

#include <boost/test/unit_test.hpp>
#include <boost/test/test_tools.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <opm/parser/eclipse/Deck/Deck.hpp>
#include <opm/parser/eclipse/Parser/Parser.hpp>
#include <opm/parser/eclipse/EclipseState/Grid/EclipseGrid.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Schedule.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/TimeMap.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Group.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/GroupTree.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/ScheduleEnums.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/WellConnections.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Events.hpp>
#include <opm/parser/eclipse/Units/Units.hpp>

#include "src/opm/parser/eclipse/EclipseState/Schedule/Well/WellProductionProperties.hpp"
#include "src/opm/parser/eclipse/EclipseState/Schedule/Well/WellInjectionProperties.hpp"

using namespace Opm;


inline std::string pathprefix() {
    return boost::unit_test::framework::master_test_suite().argv[1];
}

BOOST_AUTO_TEST_CASE(CreateSchedule) {
    Parser parser;
    EclipseGrid grid(10,10,10);
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE1");
    auto deck1 =  parser.parseFile(scheduleFile);
    std::stringstream ss;
    ss << deck1;
    auto deck2 = parser.parseString( ss.str());
    for (const auto& deck : {deck1 , deck2}) {
        TableManager table ( deck );
        Eclipse3DProperties eclipseProperties ( deck , table, grid);
        Runspec runspec (deck);
        Schedule sched(deck,  grid , eclipseProperties, runspec);
        const auto& timeMap = sched.getTimeMap();
        BOOST_CHECK_EQUAL(TimeMap::mkdate(2007 , 5 , 10), sched.getStartTime());
        BOOST_CHECK_EQUAL(9U, timeMap.size());
        BOOST_CHECK( deck.hasKeyword("NETBALAN") );
    }
}


BOOST_AUTO_TEST_CASE(CreateSchedule_Comments_After_Keywords) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_COMMENTS_AFTER_KEYWORDS");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(10,10,10);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    Schedule sched(deck,  grid , eclipseProperties, runspec);
    const auto& timeMap = sched.getTimeMap();
    BOOST_CHECK_EQUAL(TimeMap::mkdate(2007, 5 , 10) , sched.getStartTime());
    BOOST_CHECK_EQUAL(9U, timeMap.size());
}


BOOST_AUTO_TEST_CASE(WCONPROD_MissingCmode) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_MISSING_CMODE");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(10,10,3);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    BOOST_CHECK_NO_THROW( Schedule(deck, grid , eclipseProperties, runspec) );
}


BOOST_AUTO_TEST_CASE(WCONPROD_Missing_DATA) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_CMODE_MISSING_DATA");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(10,10,3);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    BOOST_CHECK_THROW( Schedule(deck, grid , eclipseProperties, runspec) , std::invalid_argument );
}


BOOST_AUTO_TEST_CASE(WellTestRefDepth) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_WELLS2");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(40,60,30);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    BOOST_CHECK_EQUAL(3, 3);
    Runspec runspec (deck);
    Schedule sched(deck , grid , eclipseProperties, runspec);
    BOOST_CHECK_EQUAL(4, 4);

    const auto& well1 = sched.getWell2atEnd("W_1");
    const auto& well2 = sched.getWell2atEnd("W_2");
    const auto& well4 = sched.getWell2atEnd("W_4");
    BOOST_CHECK_EQUAL( well1.getRefDepth() , grid.getCellDepth( 29 , 36 , 0 ));
    BOOST_CHECK_EQUAL( well2.getRefDepth() , 100 );
    BOOST_CHECK_THROW( well4.getRefDepth() , std::invalid_argument );
}





BOOST_AUTO_TEST_CASE(WellTesting) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_WELLS2");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(40,60,30);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    Schedule sched(deck,  grid , eclipseProperties, runspec);

    BOOST_CHECK_EQUAL(4U, sched.numWells());
    BOOST_CHECK(sched.hasWell("W_1"));
    BOOST_CHECK(sched.hasWell("W_2"));
    BOOST_CHECK(sched.hasWell("W_3"));

    BOOST_CHECK_CLOSE( 777/Metric::Time , sched.getWell2("W_2", 7).getProductionProperties().ResVRate.get<double>() , 0.0001);
    BOOST_CHECK_EQUAL( 0 ,                sched.getWell2("W_2", 8).getProductionProperties().ResVRate.get<double>());

    BOOST_CHECK_EQUAL( WellCommon::SHUT , sched.getWell2("W_2", 3).getStatus());

    {
        const auto& rft_config = sched.rftConfig();
        BOOST_CHECK( !rft_config.rft("W_2", 2));
        BOOST_CHECK( rft_config.rft("W_2", 3));
        BOOST_CHECK( rft_config.rft("W_2", 4));
        BOOST_CHECK( !rft_config.rft("W_2", 5));
        BOOST_CHECK( rft_config.rft("W_1", 3));
    }
    {
        const WellProductionProperties& prop3 = sched.getWell2("W_2", 3).getProductionProperties();
        BOOST_CHECK_EQUAL( WellProducer::ORAT , prop3.controlMode);
        BOOST_CHECK(  prop3.hasProductionControl(WellProducer::ORAT));
        BOOST_CHECK( !prop3.hasProductionControl(WellProducer::GRAT));
        BOOST_CHECK( !prop3.hasProductionControl(WellProducer::WRAT));
    }


    BOOST_CHECK_EQUAL( WellCommon::AUTO, sched.getWell2("W_3", 3).getStatus());
    {
        const WellProductionProperties& prop7 = sched.getWell2("W_3", 7).getProductionProperties();
        BOOST_CHECK_CLOSE( 999/Metric::Time , prop7.LiquidRate.get<double>() , 0.001);
        BOOST_CHECK_EQUAL( WellProducer::RESV, prop7.controlMode);
    }
    BOOST_CHECK_CLOSE( 8000./Metric::Time , sched.getWell2("W_3", 3).getProductionProperties().LiquidRate.get<double>(), 1.e-12);
    BOOST_CHECK_CLOSE( 18000./Metric::Time, sched.getWell2("W_3", 8).getProductionProperties().LiquidRate.get<double>(), 1.e-12);


    {
        BOOST_CHECK_EQUAL(sched.getWell2("W_1", 3).getProductionProperties().predictionMode, false);
        BOOST_CHECK_CLOSE(sched.getWell2("W_1", 3).getProductionProperties().WaterRate.get<double>() , 4/Metric::Time, 0.001);
        BOOST_CHECK_CLOSE(sched.getWell2("W_1", 3).getProductionProperties().GasRate.get<double>()   , 12345/Metric::Time, 0.001);
        BOOST_CHECK_CLOSE(sched.getWell2("W_1", 3).getProductionProperties().OilRate.get<double>() , 4000/Metric::Time, 0.001);

        BOOST_CHECK_CLOSE(sched.getWell2("W_1", 4).getProductionProperties().OilRate.get<double>() , 4000/Metric::Time, 0.001);
        BOOST_CHECK_CLOSE(sched.getWell2("W_1", 4).getProductionProperties().WaterRate.get<double>() , 4/Metric::Time, 0.001);
        BOOST_CHECK_CLOSE(sched.getWell2("W_1", 4).getProductionProperties().GasRate.get<double>()   , 12345/Metric::Time,0.001);

        BOOST_CHECK_CLOSE(sched.getWell2("W_1", 5).getProductionProperties().WaterRate.get<double>(), 4/Metric::Time, 0.001);
        BOOST_CHECK_CLOSE(sched.getWell2("W_1", 5).getProductionProperties().GasRate.get<double>() , 12345/Metric::Time, 0.001);
        BOOST_CHECK_CLOSE(sched.getWell2("W_1", 5).getProductionProperties().OilRate.get<double>() , 4000/Metric::Time, 0.001);


        BOOST_CHECK_EQUAL(sched.getWell2("W_1", 6).getProductionProperties().predictionMode, false);
        BOOST_CHECK_CLOSE(sched.getWell2("W_1", 6).getProductionProperties().OilRate.get<double>() , 14000/Metric::Time, 0.001);

        BOOST_CHECK_EQUAL(sched.getWell2("W_1", 7).getProductionProperties().predictionMode, true);
        BOOST_CHECK_CLOSE(sched.getWell2("W_1", 7).getProductionProperties().OilRate.get<double>() , 11000/Metric::Time, 0.001);
        BOOST_CHECK_CLOSE(sched.getWell2("W_1", 7).getProductionProperties().WaterRate.get<double>() , 44/Metric::Time, 0.001);


        BOOST_CHECK_EQUAL(sched.getWell2("W_1", 8).getProductionProperties().predictionMode, false);
        BOOST_CHECK_CLOSE(sched.getWell2("W_1", 8).getProductionProperties().OilRate.get<double>() , 13000/Metric::Time , 0.001);

        BOOST_CHECK_CLOSE(sched.getWell2("W_1", 10).getInjectionProperties().BHPLimit.get<double>(), 123.00 * Metric::Pressure , 0.001);
        BOOST_CHECK_CLOSE(sched.getWell2("W_1", 10).getInjectionProperties().THPLimit.get<double>(), 678.00 * Metric::Pressure , 0.001);



        BOOST_CHECK( sched.getWell2("W_1", 9).isInjector());
        {
            SummaryState st;
            const auto controls = sched.getWell2("W_1", 9).injectionControls(st);
            BOOST_CHECK_CLOSE(20000/Metric::Time ,  controls.surface_rate  , 0.001);
            BOOST_CHECK_CLOSE(200000/Metric::Time , controls.reservoir_rate, 0.001);
            BOOST_CHECK_CLOSE(6895 * Metric::Pressure , controls.bhp_limit, 0.001);
            BOOST_CHECK_CLOSE(0 , controls.thp_limit , 0.001);
            BOOST_CHECK_EQUAL( WellInjector::RESV  , controls.cmode);
            BOOST_CHECK(  controls.hasControl(WellInjector::RATE ));
            BOOST_CHECK(  controls.hasControl(WellInjector::RESV ));
            BOOST_CHECK( !controls.hasControl(WellInjector::THP));
            BOOST_CHECK(  controls.hasControl(WellInjector::BHP));
        }


        BOOST_CHECK_EQUAL( WellCommon::OPEN, sched.getWell2("W_1", 11).getStatus( ));
        BOOST_CHECK_EQUAL( WellCommon::OPEN, sched.getWell2("W_1", 12).getStatus( ));
        BOOST_CHECK_EQUAL( WellCommon::SHUT, sched.getWell2("W_1", 13).getStatus( ));
        BOOST_CHECK_EQUAL( WellCommon::OPEN, sched.getWell2("W_1", 14).getStatus( ));
        {
            SummaryState st;
            const auto controls = sched.getWell2("W_1", 12).injectionControls(st);
            BOOST_CHECK(  controls.hasControl(WellInjector::RATE ));
            BOOST_CHECK( !controls.hasControl(WellInjector::RESV));
            BOOST_CHECK(  controls.hasControl(WellInjector::THP ));
            BOOST_CHECK(  controls.hasControl(WellInjector::BHP ));
        }
    }
}


BOOST_AUTO_TEST_CASE(WellTestCOMPDAT_DEFAULTED_ITEMS) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_COMPDAT1");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(10,10,10);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    Schedule sched(deck,  grid , eclipseProperties, runspec);
}


BOOST_AUTO_TEST_CASE(WellTestCOMPDAT) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_WELLS2");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(40,60,30);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    Schedule sched(deck,  grid , eclipseProperties, runspec);

    BOOST_CHECK_EQUAL(4U, sched.numWells());
    BOOST_CHECK(sched.hasWell("W_1"));
    BOOST_CHECK(sched.hasWell("W_2"));
    BOOST_CHECK(sched.hasWell("W_3"));
    {
        BOOST_CHECK_CLOSE(13000/Metric::Time , sched.getWell2("W_1", 8).getProductionProperties().OilRate.get<double>() , 0.0001);
        {
            const auto& connections = sched.getWell2("W_1", 3).getConnections();
            BOOST_CHECK_EQUAL(4U, connections.size());

            BOOST_CHECK_EQUAL(WellCompletion::OPEN, connections.get(3).state());
            BOOST_CHECK_EQUAL(2.2836805555555556e-12 , connections.get(3).CF());
        }
        {
            const auto& connections = sched.getWell2("W_1", 7).getConnections();
            BOOST_CHECK_EQUAL(4U, connections.size() );
            BOOST_CHECK_EQUAL(WellCompletion::SHUT, connections.get( 3 ).state() );
        }
    }
}

BOOST_AUTO_TEST_CASE(GroupTreeTest_GRUPTREE_with_explicit_L0_parenting) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_GRUPTREE_EXPLICIT_PARENTING");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(10,10,3);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    Schedule sched(deck,  grid , eclipseProperties, runspec);

    const auto& grouptree = sched.getGroupTree( 0 );

    BOOST_CHECK( grouptree.exists( "FIRST_LEVEL1" ) );
    BOOST_CHECK( grouptree.exists( "FIRST_LEVEL2" ) );
    BOOST_CHECK( grouptree.exists( "SECOND_LEVEL1" ) );
    BOOST_CHECK( grouptree.exists( "SECOND_LEVEL2" ) );
    BOOST_CHECK( grouptree.exists( "THIRD_LEVEL1" ) );

    BOOST_CHECK_EQUAL( "FIELD", grouptree.parent( "FIRST_LEVEL1" ) );
    BOOST_CHECK_EQUAL( "FIELD", grouptree.parent( "FIRST_LEVEL2" ) );
    BOOST_CHECK_EQUAL( "FIRST_LEVEL1", grouptree.parent( "SECOND_LEVEL1" ) );
    BOOST_CHECK_EQUAL( "FIRST_LEVEL2", grouptree.parent( "SECOND_LEVEL2" ) );
    BOOST_CHECK_EQUAL( "SECOND_LEVEL1", grouptree.parent( "THIRD_LEVEL1" ) );
}


BOOST_AUTO_TEST_CASE(GroupTreeTest_GRUPTREE_correct) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_WELSPECS_GRUPTREE");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(10,10,3);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    Schedule schedule(deck,  grid , eclipseProperties, runspec);

    BOOST_CHECK( schedule.hasGroup( "FIELD" ));
    BOOST_CHECK( schedule.hasGroup( "PROD" ));
    BOOST_CHECK( schedule.hasGroup( "INJE" ));
    BOOST_CHECK( schedule.hasGroup( "MANI-PROD" ));
    BOOST_CHECK( schedule.hasGroup( "MANI-INJ" ));
    BOOST_CHECK( schedule.hasGroup( "DUMMY-PROD" ));
    BOOST_CHECK( schedule.hasGroup( "DUMMY-INJ" ));
}



BOOST_AUTO_TEST_CASE(GroupTreeTest_WELSPECS_AND_GRUPTREE_correct_size ) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_WELSPECS_GROUPS");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(10,10,3);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    Schedule schedule(deck,  grid , eclipseProperties,runspec);

    // Time 0, only from WELSPECS
    BOOST_CHECK_EQUAL( 2U, schedule.getGroupTree(0).children("FIELD").size() );

    // Time 1, a new group added in tree
    BOOST_CHECK_EQUAL( 3U, schedule.getGroupTree(1).children("FIELD").size() );
}

BOOST_AUTO_TEST_CASE(GroupTreeTest_WELSPECS_AND_GRUPTREE_correct_tree) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_WELSPECS_GROUPS");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(10,10,3);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    Schedule schedule(deck,  grid , eclipseProperties, runspec);

    // Time 0, only from WELSPECS
    const auto& tree0 = schedule.getGroupTree( 0 );
    BOOST_CHECK( tree0.exists( "FIELD" ) );
    BOOST_CHECK_EQUAL( "FIELD", tree0.parent( "GROUP_BJARNE" ) );
    BOOST_CHECK( tree0.exists("GROUP_ODD") );

    // Time 1, now also from GRUPTREE
    const auto& tree1 = schedule.getGroupTree( 1 );
    BOOST_CHECK( tree1.exists( "FIELD" ) );
    BOOST_CHECK_EQUAL( "FIELD", tree1.parent( "GROUP_BJARNE" ) );
    BOOST_CHECK( tree1.exists("GROUP_ODD"));

    // - from GRUPTREE
    BOOST_CHECK( tree1.exists( "GROUP_BIRGER" ) );
    BOOST_CHECK_EQUAL( "GROUP_BJARNE", tree1.parent( "GROUP_BIRGER" ) );

    BOOST_CHECK( tree1.exists( "GROUP_NEW" ) );
    BOOST_CHECK_EQUAL( "FIELD", tree1.parent( "GROUP_NEW" ) );

    BOOST_CHECK( tree1.exists( "GROUP_NILS" ) );
    BOOST_CHECK_EQUAL( "GROUP_NEW", tree1.parent( "GROUP_NILS" ) );
}

BOOST_AUTO_TEST_CASE(GroupTreeTest_GRUPTREE_WITH_REPARENT_correct_tree) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_GROUPS_REPARENT");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(10,10,3);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    Schedule sched(deck,  grid , eclipseProperties, runspec);


    const auto& tree0 = sched.getGroupTree( 0 );

    BOOST_CHECK( tree0.exists( "GROUP_BJARNE" ) );
    BOOST_CHECK( tree0.exists( "GROUP_NILS" ) );
    BOOST_CHECK( tree0.exists( "GROUP_NEW" ) );
    BOOST_CHECK_EQUAL( "FIELD", tree0.parent( "GROUP_BJARNE" ) );
    BOOST_CHECK_EQUAL( "GROUP_BJARNE", tree0.parent( "GROUP_BIRGER" ) );
    BOOST_CHECK_EQUAL( "GROUP_NEW", tree0.parent( "GROUP_NILS" ) );
}

BOOST_AUTO_TEST_CASE( WellTestGroups ) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_GROUPS");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(10,10,3);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    Schedule sched(deck,  grid , eclipseProperties, runspec);

    BOOST_CHECK_EQUAL( 3U , sched.numGroups() );
    BOOST_CHECK( sched.hasGroup( "INJ" ));
    BOOST_CHECK( sched.hasGroup( "OP" ));

    {
        auto& group = sched.getGroup("INJ");
        BOOST_CHECK_EQUAL( Phase::WATER , group.getInjectionPhase( 3 ));
        BOOST_CHECK_EQUAL( GroupInjection::VREP , group.getInjectionControlMode( 3 ));
        BOOST_CHECK_CLOSE( 10/Metric::Time , group.getSurfaceMaxRate( 3 ) , 0.001);
        BOOST_CHECK_CLOSE( 20/Metric::Time , group.getReservoirMaxRate( 3 ) , 0.001);
        BOOST_CHECK_EQUAL( 0.75 , group.getTargetReinjectFraction( 3 ));
        BOOST_CHECK_EQUAL( 0.95 , group.getTargetVoidReplacementFraction( 3 ));

        BOOST_CHECK_EQUAL( Phase::OIL , group.getInjectionPhase( 6 ));
        BOOST_CHECK_EQUAL( GroupInjection::RATE , group.getInjectionControlMode( 6 ));
        BOOST_CHECK_CLOSE( 1000/Metric::Time , group.getSurfaceMaxRate( 6 ) , 0.0001);

        BOOST_CHECK(group.isInjectionGroup(3));
    }

    {
        auto& group = sched.getGroup("OP");
        BOOST_CHECK_EQUAL( GroupProduction::ORAT , group.getProductionControlMode(3));
        BOOST_CHECK_CLOSE( 10/Metric::Time , group.getOilTargetRate(3) , 0.001);
        BOOST_CHECK_CLOSE( 20/Metric::Time , group.getWaterTargetRate(3) , 0.001);
        BOOST_CHECK_CLOSE( 30/Metric::Time , group.getGasTargetRate(3) , 0.001);
        BOOST_CHECK_CLOSE( 40/Metric::Time , group.getLiquidTargetRate(3) , 0.001);

        BOOST_CHECK(group.isProductionGroup(3));
    }

}


BOOST_AUTO_TEST_CASE( WellTestGroupAndWellRelation ) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_WELLS_AND_GROUPS");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(10,10,3);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    Schedule sched(deck,  grid , eclipseProperties, runspec);

    auto& group1 = sched.getGroup("GROUP1");
    auto& group2 = sched.getGroup("GROUP2");

    BOOST_CHECK(  group1.hasBeenDefined(0) );
    BOOST_CHECK( !group2.hasBeenDefined(0));
    BOOST_CHECK(  group2.hasBeenDefined(1));

    BOOST_CHECK(  group1.hasWell("W_1" , 0));
    BOOST_CHECK(  group1.hasWell("W_2" , 0));
    BOOST_CHECK( !group2.hasWell("W_1" , 0));
    BOOST_CHECK( !group2.hasWell("W_2" , 0));

    BOOST_CHECK(  group1.hasWell("W_1" , 1));
    BOOST_CHECK( !group1.hasWell("W_2" , 1));
    BOOST_CHECK( !group2.hasWell("W_1" , 1));
    BOOST_CHECK(  group2.hasWell("W_2" , 1));
}


/*
BOOST_AUTO_TEST_CASE(WellTestWELOPEN_ConfigWithIndexes_Throws) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_WELOPEN_INVALID");
    auto deck =  parser.parseFile(scheduleFile);
    std::shared_ptr<const EclipseGrid> grid = std::make_shared<const EclipseGrid>(10,10,3);
    BOOST_CHECK_THROW(Schedule(grid , deck), std::logic_error);
}


BOOST_AUTO_TEST_CASE(WellTestWELOPENControlsSet) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_WELOPEN");
    auto deck =  parser.parseFile(scheduleFile);
    std::shared_ptr<const EclipseGrid> grid = std::make_shared<const EclipseGrid>( 10,10,10 );
    Schedule sched(grid , deck);

    const auto* well1 = sched.getWell("W_1");
    BOOST_CHECK_EQUAL(WellCommon::StatusEnum::OPEN, sched.getWell("W_1")->getStatus(0));
    BOOST_CHECK_EQUAL(WellCommon::StatusEnum::SHUT, sched.getWell("W_1")->getStatus(1));
    BOOST_CHECK_EQUAL(WellCommon::StatusEnum::OPEN, sched.getWell("W_1")->getStatus(2));
    BOOST_CHECK_EQUAL(WellCommon::StatusEnum::STOP, sched.getWell("W_1")->getStatus(3));
    BOOST_CHECK_EQUAL(WellCommon::StatusEnum::AUTO, sched.getWell("W_1")->getStatus(4));
    BOOST_CHECK_EQUAL(WellCommon::StatusEnum::STOP, sched.getWell("W_1")->getStatus(5));
}
*/



BOOST_AUTO_TEST_CASE(WellTestWGRUPCONWellPropertiesSet) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_WGRUPCON");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(10,10,10);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    Schedule sched(deck,  grid , eclipseProperties, runspec);

    const auto& well1 = sched.getWell2("W_1", 0);
    BOOST_CHECK(well1.isAvailableForGroupControl( ));
    BOOST_CHECK_EQUAL(-1, well1.getGuideRate( ));
    BOOST_CHECK_EQUAL(GuideRate::OIL, well1.getGuideRatePhase( ));
    BOOST_CHECK_EQUAL(1.0, well1.getGuideRateScalingFactor( ));

    const auto& well2 = sched.getWell2("W_2", 0);
    BOOST_CHECK(!well2.isAvailableForGroupControl( ));
    BOOST_CHECK_EQUAL(-1, well2.getGuideRate( ));
    BOOST_CHECK_EQUAL(GuideRate::UNDEFINED, well2.getGuideRatePhase( ));
    BOOST_CHECK_EQUAL(1.0, well2.getGuideRateScalingFactor( ));

    const auto& well3 = sched.getWell2("W_3", 0);
    BOOST_CHECK(well3.isAvailableForGroupControl( ));
    BOOST_CHECK_EQUAL(100, well3.getGuideRate( ));
    BOOST_CHECK_EQUAL(GuideRate::RAT, well3.getGuideRatePhase( ));
    BOOST_CHECK_EQUAL(0.5, well3.getGuideRateScalingFactor( ));
}


BOOST_AUTO_TEST_CASE(TestDefaultedCOMPDATIJ) {
    Parser parser;
    const char * deckString = "\n\
START\n\
\n\
10 MAI 2007 /\n\
\n\
GRID\n\
PERMX\n\
   9000*0.25 /\n\
COPY \n\
   PERMX PERMY /\n\
   PERMX PERMZ /\n\
/\n\
SCHEDULE\n\
WELSPECS \n\
     'W1'        'OP'   11   21  3.33       'OIL'  7* /   \n\
/\n\
COMPDAT \n\
     'W1'   2*    1    1      'OPEN'  1*     32.948      0.311   3047.839  2*         'X'     22.100 /\n\
/\n";
    auto deck =  parser.parseString(deckString);
    EclipseGrid grid(30,30,10);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    Schedule sched(deck,  grid , eclipseProperties, runspec);
    const auto& connections = sched.getWell2("W1", 0).getConnections();
    BOOST_CHECK_EQUAL( 10 , connections.get(0).getI() );
    BOOST_CHECK_EQUAL( 20 , connections.get(0).getJ() );
}


/**
   This is a deck used in the opm-core wellsManager testing; just be
   certain we can parse it.
*/
BOOST_AUTO_TEST_CASE(OpmCode) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/wells_group.data");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(10,10,5);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    BOOST_CHECK_NO_THROW( Schedule(deck , grid , eclipseProperties, runspec) );
}



BOOST_AUTO_TEST_CASE(WELLS_SHUT) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_SHUT_WELL");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(20,40,1);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    Schedule sched(deck,  grid , eclipseProperties, runspec);


    {
        const auto& well1 = sched.getWell2("W1", 1);
        const auto& well2 = sched.getWell2("W2", 1);
        const auto& well3 = sched.getWell2("W3", 1);
        BOOST_CHECK_EQUAL( WellCommon::StatusEnum::OPEN , well1.getStatus());
        BOOST_CHECK_EQUAL( WellCommon::StatusEnum::OPEN , well2.getStatus());
        BOOST_CHECK_EQUAL( WellCommon::StatusEnum::OPEN , well3.getStatus());
    }
    {
        const auto& well1 = sched.getWell2("W1", 2);
        const auto& well2 = sched.getWell2("W2", 2);
        const auto& well3 = sched.getWell2("W3", 2);
        BOOST_CHECK_EQUAL( WellCommon::StatusEnum::SHUT , well1.getStatus());
        BOOST_CHECK_EQUAL( WellCommon::StatusEnum::SHUT , well2.getStatus());
        BOOST_CHECK_EQUAL( WellCommon::StatusEnum::SHUT , well3.getStatus());
    }
}


BOOST_AUTO_TEST_CASE(WellTestWPOLYMER) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_POLYMER");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(30,30,30);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    Schedule sched(deck,  grid , eclipseProperties, runspec);


    BOOST_CHECK_EQUAL(4U, sched.numWells());
    BOOST_CHECK(sched.hasWell("INJE01"));
    BOOST_CHECK(sched.hasWell("PROD01"));

    {
        const auto& well1 = sched.getWell2("INJE01", 0);
        BOOST_CHECK( well1.isInjector());
        const WellPolymerProperties& props_well10 = well1.getPolymerProperties();
        BOOST_CHECK_CLOSE(1.5*Metric::PolymerDensity, props_well10.m_polymerConcentration, 0.0001);
    }
    {
        const auto& well1 = sched.getWell2("INJE01", 1);
        const WellPolymerProperties& props_well11 = well1.getPolymerProperties();
        BOOST_CHECK_CLOSE(1.0*Metric::PolymerDensity, props_well11.m_polymerConcentration, 0.0001);
    }
    {
        const auto& well1 = sched.getWell2("INJE01", 2);
        const WellPolymerProperties& props_well12 = well1.getPolymerProperties();
        BOOST_CHECK_CLOSE(0.1*Metric::PolymerDensity, props_well12.m_polymerConcentration, 0.0001);
    }

    {
        const auto& well2 = sched.getWell2("INJE02", 0);
        BOOST_CHECK( well2.isInjector());
        const WellPolymerProperties& props_well20 = well2.getPolymerProperties();
        BOOST_CHECK_CLOSE(2.0*Metric::PolymerDensity, props_well20.m_polymerConcentration, 0.0001);
    }
    {
        const auto& well2 = sched.getWell2("INJE02", 1);
        const WellPolymerProperties& props_well21 = well2.getPolymerProperties();
        BOOST_CHECK_CLOSE(1.5*Metric::PolymerDensity, props_well21.m_polymerConcentration, 0.0001);
    }
    {
        const auto& well2 = sched.getWell2("INJE02", 2);
        const WellPolymerProperties& props_well22 = well2.getPolymerProperties();
        BOOST_CHECK_CLOSE(0.2*Metric::PolymerDensity, props_well22.m_polymerConcentration, 0.0001);
    }
    {
        const auto& well3 = sched.getWell2("INJE03", 0);
        BOOST_CHECK( well3.isInjector());
        const WellPolymerProperties& props_well30 = well3.getPolymerProperties();
        BOOST_CHECK_CLOSE(2.5*Metric::PolymerDensity, props_well30.m_polymerConcentration, 0.0001);
    }
    {
        const auto& well3 = sched.getWell2("INJE03", 1);
        const WellPolymerProperties& props_well31 = well3.getPolymerProperties();
        BOOST_CHECK_CLOSE(2.0*Metric::PolymerDensity, props_well31.m_polymerConcentration, 0.0001);
    }
    {
        const auto& well3 = sched.getWell2("INJE03", 2);
        const WellPolymerProperties& props_well32 = well3.getPolymerProperties();
        BOOST_CHECK_CLOSE(0.3*Metric::PolymerDensity, props_well32.m_polymerConcentration, 0.0001);
    }
}


BOOST_AUTO_TEST_CASE(WellTestWECON) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_WECON");
    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(30,30,30);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    Schedule sched(deck,  grid , eclipseProperties, runspec);

    BOOST_CHECK_EQUAL(3U, sched.numWells());
    BOOST_CHECK(sched.hasWell("INJE01"));
    BOOST_CHECK(sched.hasWell("PROD01"));
    BOOST_CHECK(sched.hasWell("PROD02"));

    {
        const WellEconProductionLimits& econ_limit1 = sched.getWell2("PROD01", 0).getEconLimits();
        BOOST_CHECK(econ_limit1.onMinOilRate());
        BOOST_CHECK(econ_limit1.onMaxWaterCut());
        BOOST_CHECK(!(econ_limit1.onMinGasRate()));
        BOOST_CHECK(!(econ_limit1.onMaxGasOilRatio()));
        BOOST_CHECK_EQUAL(econ_limit1.maxWaterCut(), 0.95);
        BOOST_CHECK_EQUAL(econ_limit1.minOilRate(), 50.0/86400.);
        BOOST_CHECK_EQUAL(econ_limit1.minGasRate(), 0.0);
        BOOST_CHECK_EQUAL(econ_limit1.maxGasOilRatio(), 0.0);
        BOOST_CHECK_EQUAL(econ_limit1.endRun(), false);
        BOOST_CHECK_EQUAL(econ_limit1.followonWell(), "'");
        BOOST_CHECK_EQUAL(econ_limit1.quantityLimit(), WellEcon::RATE);
        BOOST_CHECK_EQUAL(econ_limit1.workover(), WellEcon::CON);
        BOOST_CHECK_EQUAL(econ_limit1.workoverSecondary(), WellEcon::CON);
        BOOST_CHECK(econ_limit1.requireWorkover());
        BOOST_CHECK(econ_limit1.requireSecondaryWorkover());
        BOOST_CHECK(!(econ_limit1.validFollowonWell()));
        BOOST_CHECK(!(econ_limit1.endRun()));
        BOOST_CHECK(econ_limit1.onAnyRatioLimit());
        BOOST_CHECK(econ_limit1.onAnyRateLimit());
        BOOST_CHECK(econ_limit1.onAnyEffectiveLimit());

        const WellEconProductionLimits& econ_limit2 = sched.getWell2("PROD01", 1).getEconLimits();
        BOOST_CHECK(!(econ_limit2.onMinOilRate()));
        BOOST_CHECK(econ_limit2.onMaxWaterCut());
        BOOST_CHECK(econ_limit2.onMinGasRate());
        BOOST_CHECK(!(econ_limit2.onMaxGasOilRatio()));
        BOOST_CHECK_EQUAL(econ_limit2.maxWaterCut(), 0.95);
        BOOST_CHECK_EQUAL(econ_limit2.minOilRate(), 0.0);
        BOOST_CHECK_EQUAL(econ_limit2.minGasRate(), 1000./86400.);
        BOOST_CHECK_EQUAL(econ_limit2.maxGasOilRatio(), 0.0);
        BOOST_CHECK_EQUAL(econ_limit2.endRun(), false);
        BOOST_CHECK_EQUAL(econ_limit2.followonWell(), "'");
        BOOST_CHECK_EQUAL(econ_limit2.quantityLimit(), WellEcon::RATE);
        BOOST_CHECK_EQUAL(econ_limit2.workover(), WellEcon::CON);
        BOOST_CHECK_EQUAL(econ_limit2.workoverSecondary(), WellEcon::CON);
        BOOST_CHECK(econ_limit2.requireWorkover());
        BOOST_CHECK(econ_limit2.requireSecondaryWorkover());
        BOOST_CHECK(!(econ_limit2.validFollowonWell()));
        BOOST_CHECK(!(econ_limit2.endRun()));
        BOOST_CHECK(econ_limit2.onAnyRatioLimit());
        BOOST_CHECK(econ_limit2.onAnyRateLimit());
        BOOST_CHECK(econ_limit2.onAnyEffectiveLimit());
    }

    {
        const WellEconProductionLimits& econ_limit1 = sched.getWell2("PROD02", 0).getEconLimits();
        BOOST_CHECK(!(econ_limit1.onMinOilRate()));
        BOOST_CHECK(!(econ_limit1.onMaxWaterCut()));
        BOOST_CHECK(!(econ_limit1.onMinGasRate()));
        BOOST_CHECK(!(econ_limit1.onMaxGasOilRatio()));
        BOOST_CHECK_EQUAL(econ_limit1.maxWaterCut(), 0.0);
        BOOST_CHECK_EQUAL(econ_limit1.minOilRate(), 0.0);
        BOOST_CHECK_EQUAL(econ_limit1.minGasRate(), 0.0);
        BOOST_CHECK_EQUAL(econ_limit1.maxGasOilRatio(), 0.0);
        BOOST_CHECK_EQUAL(econ_limit1.endRun(), false);
        BOOST_CHECK_EQUAL(econ_limit1.followonWell(), "'");
        BOOST_CHECK_EQUAL(econ_limit1.quantityLimit(), WellEcon::RATE);
        BOOST_CHECK_EQUAL(econ_limit1.workover(), WellEcon::NONE);
        BOOST_CHECK_EQUAL(econ_limit1.workoverSecondary(), WellEcon::NONE);
        BOOST_CHECK(!(econ_limit1.requireWorkover()));
        BOOST_CHECK(!(econ_limit1.requireSecondaryWorkover()));
        BOOST_CHECK(!(econ_limit1.validFollowonWell()));
        BOOST_CHECK(!(econ_limit1.endRun()));
        BOOST_CHECK(!(econ_limit1.onAnyRatioLimit()));
        BOOST_CHECK(!(econ_limit1.onAnyRateLimit()));
        BOOST_CHECK(!(econ_limit1.onAnyEffectiveLimit()));

        const WellEconProductionLimits& econ_limit2 = sched.getWell2("PROD02", 1).getEconLimits();
        BOOST_CHECK(!(econ_limit2.onMinOilRate()));
        BOOST_CHECK(econ_limit2.onMaxWaterCut());
        BOOST_CHECK(econ_limit2.onMinGasRate());
        BOOST_CHECK(!(econ_limit2.onMaxGasOilRatio()));
        BOOST_CHECK_EQUAL(econ_limit2.maxWaterCut(), 0.95);
        BOOST_CHECK_EQUAL(econ_limit2.minOilRate(), 0.0);
        BOOST_CHECK_EQUAL(econ_limit2.minGasRate(), 1000.0/86400.);
        BOOST_CHECK_EQUAL(econ_limit2.maxGasOilRatio(), 0.0);
        BOOST_CHECK_EQUAL(econ_limit2.endRun(), false);
        BOOST_CHECK_EQUAL(econ_limit2.followonWell(), "'");
        BOOST_CHECK_EQUAL(econ_limit2.quantityLimit(), WellEcon::RATE);
        BOOST_CHECK_EQUAL(econ_limit2.workover(), WellEcon::CON);
        BOOST_CHECK_EQUAL(econ_limit2.workoverSecondary(), WellEcon::CON);
        BOOST_CHECK(econ_limit2.requireWorkover());
        BOOST_CHECK(econ_limit2.requireSecondaryWorkover());
        BOOST_CHECK(!(econ_limit2.validFollowonWell()));
        BOOST_CHECK(!(econ_limit2.endRun()));
        BOOST_CHECK(econ_limit2.onAnyRatioLimit());
        BOOST_CHECK(econ_limit2.onAnyRateLimit());
        BOOST_CHECK(econ_limit2.onAnyEffectiveLimit());
    }
}


BOOST_AUTO_TEST_CASE(TestEvents) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_EVENTS");

    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(40,40,30);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec (deck);
    Schedule sched(deck , grid , eclipseProperties, runspec);
    const Events& events = sched.getEvents();

    BOOST_CHECK(  events.hasEvent(ScheduleEvents::NEW_WELL , 0 ) );
    BOOST_CHECK( !events.hasEvent(ScheduleEvents::NEW_WELL , 1 ) );
    BOOST_CHECK(  events.hasEvent(ScheduleEvents::NEW_WELL , 2 ) );
    BOOST_CHECK( !events.hasEvent(ScheduleEvents::NEW_WELL , 3 ) );

    BOOST_CHECK(  events.hasEvent(ScheduleEvents::COMPLETION_CHANGE , 0 ) );
    BOOST_CHECK( !events.hasEvent(ScheduleEvents::COMPLETION_CHANGE , 1) );
    BOOST_CHECK(  events.hasEvent(ScheduleEvents::COMPLETION_CHANGE , 5 ) );

    BOOST_CHECK(  events.hasEvent(ScheduleEvents::WELL_STATUS_CHANGE , 1 ));
    BOOST_CHECK( !events.hasEvent(ScheduleEvents::WELL_STATUS_CHANGE , 2 ));
    BOOST_CHECK( events.hasEvent(ScheduleEvents::WELL_STATUS_CHANGE , 3 ));
    BOOST_CHECK( events.hasEvent(ScheduleEvents::COMPLETION_CHANGE , 5) );

    BOOST_CHECK(  events.hasEvent(ScheduleEvents::GROUP_CHANGE , 0 ));
    BOOST_CHECK( !events.hasEvent(ScheduleEvents::GROUP_CHANGE , 1 ));
    BOOST_CHECK(  events.hasEvent(ScheduleEvents::GROUP_CHANGE , 3 ) );
    BOOST_CHECK( !events.hasEvent(ScheduleEvents::NEW_GROUP , 2 ) );
    BOOST_CHECK(  events.hasEvent(ScheduleEvents::NEW_GROUP , 3 ) );
}


BOOST_AUTO_TEST_CASE(TestWellEvents) {
    Parser parser;
    std::string scheduleFile(pathprefix() + "SCHEDULE/SCHEDULE_EVENTS");

    auto deck =  parser.parseFile(scheduleFile);
    EclipseGrid grid(40,40,30);
    TableManager table ( deck );
    Eclipse3DProperties eclipseProperties ( deck , table, grid);
    Runspec runspec(deck);
    Schedule sched(deck , grid , eclipseProperties, runspec);

    BOOST_CHECK(  sched.hasWellEvent( "W_1", ScheduleEvents::NEW_WELL , 0 ));
    BOOST_CHECK(  sched.hasWellEvent( "W_2", ScheduleEvents::NEW_WELL , 2 ));
    BOOST_CHECK( !sched.hasWellEvent( "W_2", ScheduleEvents::NEW_WELL , 3 ));
    BOOST_CHECK(  sched.hasWellEvent( "W_2", ScheduleEvents::WELL_WELSPECS_UPDATE , 3 ));

    BOOST_CHECK( sched.hasWellEvent( "W_1", ScheduleEvents::WELL_STATUS_CHANGE , 0 ));
    BOOST_CHECK( sched.hasWellEvent( "W_1", ScheduleEvents::WELL_STATUS_CHANGE , 1 ));
    BOOST_CHECK( sched.hasWellEvent( "W_1", ScheduleEvents::WELL_STATUS_CHANGE , 3 ));
    BOOST_CHECK( sched.hasWellEvent( "W_1", ScheduleEvents::WELL_STATUS_CHANGE , 4 ));
    BOOST_CHECK( !sched.hasWellEvent( "W_1", ScheduleEvents::WELL_STATUS_CHANGE , 5 ));

    BOOST_CHECK( sched.hasWellEvent( "W_1", ScheduleEvents::COMPLETION_CHANGE , 0 ));
    BOOST_CHECK( sched.hasWellEvent( "W_1", ScheduleEvents::COMPLETION_CHANGE , 5 ));
}
