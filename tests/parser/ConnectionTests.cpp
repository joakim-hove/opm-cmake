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

#define BOOST_TEST_MODULE CompletionTests
#include <boost/test/unit_test.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>




#include <opm/parser/eclipse/Deck/DeckItem.hpp>
#include <opm/parser/eclipse/Deck/DeckRecord.hpp>
#include <opm/parser/eclipse/Deck/DeckKeyword.hpp>

#include <opm/parser/eclipse/EclipseState/Schedule/Connection.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/WellConnections.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/ScheduleEnums.hpp>
#include <opm/parser/eclipse/EclipseState/Grid/EclipseGrid.hpp>

namespace Opm {

inline std::ostream& operator<<( std::ostream& stream, const Connection& c ) {
    return stream << "(" << c.getI() << "," << c.getJ() << "," << c.getK() << ")";
}

inline std::ostream& operator<<( std::ostream& stream, const WellConnections& cs ) {
    stream << "{ ";
    for( const auto& c : cs ) stream << c << " ";
    return stream << "}";
}

}



BOOST_AUTO_TEST_CASE(CreateCompletionOK) {
    Opm::Connection completion(10,10,10, 1, 0.0,Opm::WellCompletion::OPEN,Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);
}


BOOST_AUTO_TEST_CASE(testGetFunctions) {
    Opm::Connection completion(10,11,12, 1, 0.0, Opm::WellCompletion::OPEN,Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);
    BOOST_CHECK_EQUAL( 10 , completion.getI() );
    BOOST_CHECK_EQUAL( 11 , completion.getJ() );
    BOOST_CHECK_EQUAL( 12 , completion.getK() );

    BOOST_CHECK_EQUAL( Opm::WellCompletion::OPEN , completion.getState());
    BOOST_CHECK_EQUAL( 99.88 , completion.getConnectionTransmissibilityFactor());
    BOOST_CHECK_EQUAL( 22.33 , completion.getDiameter());
    BOOST_CHECK_EQUAL( 33.22 , completion.getSkinFactor());
    BOOST_CHECK_EQUAL( 0 , completion.getSatTableId());
}


BOOST_AUTO_TEST_CASE(CompletionTestssameCoordinate) {
    Opm::Connection completion1(10,10,10, 1, 0.0, Opm::WellCompletion::OPEN, Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);
    Opm::Connection completion2(10,10,10, 1, 0.0, Opm::WellCompletion::OPEN, Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);
    Opm::Connection completion3(11,10,10, 1, 0.0, Opm::WellCompletion::OPEN, Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);
    Opm::Connection completion4(10,11,10, 1, 0.0, Opm::WellCompletion::OPEN, Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);
    Opm::Connection completion5(10,10,11, 1, 0.0, Opm::WellCompletion::OPEN, Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);

    BOOST_CHECK( completion1.sameCoordinate( completion2 ));
    BOOST_CHECK_EQUAL( false , completion1.sameCoordinate( completion3 ));
    BOOST_CHECK_EQUAL( false , completion1.sameCoordinate( completion4 ));
    BOOST_CHECK_EQUAL( false , completion1.sameCoordinate( completion5 ));
}

BOOST_AUTO_TEST_CASE(CreateWellConnectionsOK) {
    Opm::WellConnections completionSet;
    BOOST_CHECK_EQUAL( 0U , completionSet.size() );
}



BOOST_AUTO_TEST_CASE(AddCompletionSizeCorrect) {
    Opm::WellConnections completionSet;
    Opm::Connection completion1( 10,10,10, 1, 0.0,Opm::WellCompletion::OPEN , Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);
    Opm::Connection completion2( 11,10,10, 1, 0.0,Opm::WellCompletion::OPEN , Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);
    completionSet.add( completion1 );
    BOOST_CHECK_EQUAL( 1U , completionSet.size() );

    completionSet.add( completion2 );
    BOOST_CHECK_EQUAL( 2U , completionSet.size() );

    BOOST_CHECK_EQUAL( completion1 , completionSet.get(0) );
}


BOOST_AUTO_TEST_CASE(WellConnectionsGetOutOfRangeThrows) {
    Opm::WellConnections completionSet;
    Opm::Connection completion1( 10,10,10,1, 0.0,Opm::WellCompletion::OPEN , Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);
    Opm::Connection completion2( 11,10,10,1, 0.0,Opm::WellCompletion::OPEN , Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);
    completionSet.add( completion1 );
    BOOST_CHECK_EQUAL( 1U , completionSet.size() );

    completionSet.add( completion2 );
    BOOST_CHECK_EQUAL( 2U , completionSet.size() );

    BOOST_CHECK_THROW( completionSet.get(10) , std::out_of_range );
}




BOOST_AUTO_TEST_CASE(AddCompletionSameCellUpdates) {
    Opm::WellConnections completionSet;
    Opm::Connection completion1( 10,10,10, 1, 0.0, Opm::WellCompletion::OPEN , Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);
    Opm::Connection completion2( 10,10,10, 1, 0.0,Opm::WellCompletion::SHUT , Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);


    completionSet.add( completion1 );
    BOOST_CHECK_EQUAL( 1U , completionSet.size() );

    completionSet.add( completion2 );
    BOOST_CHECK_EQUAL( 1U , completionSet.size() );
}



BOOST_AUTO_TEST_CASE(AddCompletionCopy) {
    Opm::WellConnections completionSet;

    Opm::Connection completion1( 10,10,10, 1, 0.0, Opm::WellCompletion::OPEN , Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);
    Opm::Connection completion2( 10,10,11, 1, 0.0, Opm::WellCompletion::SHUT , Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);
    Opm::Connection completion3( 10,10,12, 1, 0.0, Opm::WellCompletion::SHUT , Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);

    completionSet.add( completion1 );
    completionSet.add( completion2 );
    completionSet.add( completion3 );
    BOOST_CHECK_EQUAL( 3U , completionSet.size() );

    auto copy = completionSet;
    BOOST_CHECK_EQUAL( 3U , copy.size() );

    BOOST_CHECK_EQUAL( completion1 , copy.get(0));
    BOOST_CHECK_EQUAL( completion2 , copy.get(1));
    BOOST_CHECK_EQUAL( completion3 , copy.get(2));
}


BOOST_AUTO_TEST_CASE(ActiveCompletions) {
    Opm::EclipseGrid grid(10,10,10);
    Opm::WellConnections completions;
    Opm::Connection completion1( 0,0,0, 1, 0.0, Opm::WellCompletion::OPEN , Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);
    Opm::Connection completion2( 0,0,1, 1, 0.0, Opm::WellCompletion::SHUT , Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);
    Opm::Connection completion3( 0,0,2, 1, 0.0, Opm::WellCompletion::SHUT , Opm::Value<double>("ConnectionTransmissibilityFactor",99.88), Opm::Value<double>("D",22.33), Opm::Value<double>("SKIN",33.22), 0);

    completions.add( completion1 );
    completions.add( completion2 );
    completions.add( completion3 );

    std::vector<int> actnum(1000,1);
    actnum[0] = 0;
    grid.resetACTNUM( actnum.data() );

    Opm::WellConnections active_completions(completions, grid);
    BOOST_CHECK_EQUAL( active_completions.size() , 2);
    BOOST_CHECK_EQUAL( completion2, active_completions.get(0));
    BOOST_CHECK_EQUAL( completion3, active_completions.get(1));
}

