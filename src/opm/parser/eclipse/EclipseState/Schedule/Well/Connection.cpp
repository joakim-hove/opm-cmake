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

#include <algorithm>
#include <cassert>
#include <vector>
#include <sstream>
#include <iostream>

#include <opm/parser/eclipse/Deck/DeckItem.hpp>
#include <opm/parser/eclipse/Deck/DeckKeyword.hpp>
#include <opm/parser/eclipse/Deck/DeckRecord.hpp>
#include <opm/parser/eclipse/Parser/ParseContext.hpp>
#include <opm/parser/eclipse/EclipseState/Eclipse3DProperties.hpp>
#include <opm/parser/eclipse/EclipseState/Grid/EclipseGrid.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/ScheduleEnums.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/Connection.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Schedule.hpp>
#include <opm/parser/eclipse/EclipseState/Util/Value.hpp>

namespace Opm {


    Connection::Connection(int i, int j , int k ,
                           int compnum,
                           double depth,
                           WellCompletion::StateEnum stateArg ,
                           double CF,
                           double Kh,
                           double rw,
                           double r0,
                           double skin_factor,
                           const int satTableId,
                           const WellCompletion::DirectionEnum directionArg,
			   const std::size_t seqIndex,
			   const double segDistStart,
			   const double segDistEnd,
			   const bool defaultSatTabId
			  )
        : direction(directionArg),
          center_depth(depth),
          open_state(stateArg),
          sat_tableId(satTableId),
          m_complnum( compnum ),
          m_CF(CF),
          m_Kh(Kh),
          m_rw(rw),
          m_r0(r0),
          m_skin_factor(skin_factor),
          ijk({i,j,k}),
          m_seqIndex(seqIndex),
          m_segDistStart(segDistStart),
          m_segDistEnd(segDistEnd),
          m_defaultSatTabId(defaultSatTabId)
    {}

    /*bool Connection::sameCoordinate(const Connection& other) const {
        if ((m_i == other.m_i) &&
            (m_j == other.m_j) &&
            (m_k == other.m_k))
            return true;
        else
            return false;
    }*/

    bool Connection::sameCoordinate(const int i, const int j, const int k) const {
        if ((ijk[0] == i) && (ijk[1] == j) && (ijk[2] == k)) {
            return true;
        } else {
            return false;
        }
    }



    int Connection::getI() const {
        return ijk[0];
    }

    int Connection::getJ() const {
        return ijk[1];
    }

    int Connection::getK() const {
        return ijk[2];
    }

    bool Connection::attachedToSegment() const {
        return (segment_number > 0);
    }

    const std::size_t& Connection::getSeqIndex() const {
        return m_seqIndex;
    }

    const bool& Connection::getDefaultSatTabId() const {
        return m_defaultSatTabId;
    }

    const std::size_t& Connection::getCompSegSeqIndex() const {
        return m_compSeg_seqIndex;
    }

    WellCompletion::DirectionEnum Connection::dir() const {
        return this->direction;
    }

    const double& Connection::getSegDistStart() const {
        return m_segDistStart;
    }

    const double& Connection::getSegDistEnd() const {
        return m_segDistEnd;
    }


    void Connection::setCompSegSeqIndex(std::size_t index) {
        m_compSeg_seqIndex = index;
    }

    void Connection::setDefaultSatTabId(bool id) {
        m_defaultSatTabId = id;
    }

    void Connection::setSegDistStart(const double& distStart) {
        m_segDistStart = distStart;
    }

    void Connection::setSegDistEnd(const double& distEnd) {
        m_segDistEnd = distEnd;
    }

    double Connection::depth() const {
        return this->center_depth;
    }

    WellCompletion::StateEnum Connection::state() const {
        return this->open_state;
    }

    int Connection::satTableId() const {
        return this->sat_tableId;
    }

    int Connection::complnum() const {
        return this->m_complnum;
    }

    void Connection::setComplnum(int complnum) {
        this->m_complnum = complnum;
    }

    double Connection::CF() const {
        return this->m_CF;
    }

    double Connection::Kh() const {
        return this->m_Kh;
    }

    double Connection::rw() const {
        return this->m_rw;
    }

    double Connection::r0() const {
        return this->m_r0;
    }

    double Connection::skinFactor() const {
        return this->m_skin_factor;
    }

    void Connection::setState(WellCompletion::StateEnum state) {
        this->open_state = state;
    }

    void Connection::updateSegment(int segment_number_arg, double center_depth_arg, std::size_t seqIndex) {
        this->segment_number = segment_number_arg;
        this->center_depth = center_depth_arg;
        this->m_seqIndex = seqIndex;
    }

    int Connection::segment() const {
        return this->segment_number;
    }

    void Connection::scaleWellPi(double wellPi) {
        this->wPi *= wellPi;
    }

    double Connection::wellPi() const {
        return this->wPi;
    }



    std::string Connection::str() const {
        std::stringstream ss;
        ss << "ijk: " << this->ijk[0] << ","  << this->ijk[1] << "," << this->ijk[2] << std::endl;
        ss << "COMPLNUM " << this->m_complnum << std::endl;
        ss << "CF " << this->m_CF << std::endl;
        ss << "RW " << this->m_rw << std::endl;
        ss << "R0 " << this->m_r0 << std::endl;
        ss << "skinf " << this->m_skin_factor << std::endl;
        ss << "wPi " << this->wPi << std::endl;
        ss << "kh " << this->m_Kh << std::endl;
        ss << "sat_tableId " << this->sat_tableId << std::endl;
        ss << "open_state " << this->open_state << std::endl;
        ss << "direction " << this->direction << std::endl;
        ss << "segment_nr " << this->segment_number << std::endl;
        ss << "center_depth " << this->center_depth << std::endl;
        ss << "seqIndex " << this->m_seqIndex << std::endl;

        return ss.str();
}

    bool Connection::operator==( const Connection& rhs ) const {
        bool eq = this->ijk == rhs.ijk
            && this->m_complnum == rhs.m_complnum
            && this->m_CF == rhs.m_CF
            && this->m_rw == rhs.m_rw
            && this->m_r0 == rhs.m_r0
            && this->m_skin_factor == rhs.m_skin_factor
            && this->wPi == rhs.wPi
            && this->m_Kh == rhs.m_Kh
            && this->sat_tableId == rhs.sat_tableId
            && this->open_state == rhs.open_state
            && this->direction == rhs.direction
            && this->segment_number == rhs.segment_number
            && this->center_depth == rhs.center_depth
            && this->m_seqIndex == rhs.m_seqIndex;
        if (!eq) {
            //std::cout << this->str() << rhs.str() << std::endl;
        }
        return eq;
    }

    bool Connection::operator!=( const Connection& rhs ) const {
        return !( *this == rhs );
    }
}
