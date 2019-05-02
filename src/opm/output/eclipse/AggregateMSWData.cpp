/*
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

#include <opm/output/eclipse/AggregateMSWData.hpp>

#include <opm/parser/eclipse/EclipseState/EclipseState.hpp>

#include <opm/parser/eclipse/EclipseState/Runspec.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/SummaryState.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Schedule.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/ScheduleEnums.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/Connection.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/WellConnections.hpp>
#include <opm/parser/eclipse/Units/UnitSystem.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <iostream>
#include <limits>

// #####################################################################
// Class Opm::RestartIO::Helpers::AggregateMSWData
// ---------------------------------------------------------------------

namespace {

    std::size_t nswlmx(const std::vector<int>& inteHead)
    {
        // inteHead(175) = NSWLMX
        return inteHead[175];
    }

    std::size_t nisegz(const std::vector<int>& inteHead)
    {
        // inteHead(178) = NISEGZ
        return inteHead[178];
    }

    std::size_t nrsegz(const std::vector<int>& inteHead)
    {
        // inteHead(179) = NRSEGZ
        return inteHead[179];
    }

    std::size_t nilbrz(const std::vector<int>& inteHead)
    {
        // inteHead(180) = NILBRZ
        return inteHead[180];
    }

    std::vector<std::size_t>
    inflowSegmentsIndex(const Opm::WellSegments& segSet, const std::size_t& segIndex) {
        const auto& segNumber  = segSet[segIndex].segmentNumber();
        std::vector<std::size_t> inFlowSegInd;
        for (int ind = 0; ind < segSet.size(); ind++) {
            const auto& i_outletSeg = segSet[ind].outletSegment();
            if (segNumber == i_outletSeg) {
                inFlowSegInd.push_back(ind);
            }
        }
        return inFlowSegInd;
    }

    Opm::RestartIO::Helpers::BranchSegmentPar
    getBranchSegmentParam(const Opm::WellSegments& segSet, const int branch)
    {
        int noSegInBranch = 0;
        int firstSeg = -1;
        int lastSeg  = -1;
        int outletS = 0;
        for (int segInd = 0; segInd < segSet.size(); segInd++) {
            const auto& segNo = segSet[segInd].segmentNumber();
            const auto& i_branch = segSet[segInd].branchNumber();
            const auto& i_outS = segSet[segInd].outletSegment();
            if (i_branch == branch) {
                noSegInBranch +=1;
                if (firstSeg < 0) {
                    firstSeg = segNo;
                    outletS = (branch > 1) ? i_outS : 0;
                }
                lastSeg = segNo;
            }
        }

        return {
            outletS,
                noSegInBranch,
                firstSeg,
                lastSeg,
                branch
                };
    }

    std::vector <std::size_t> segmentIndFromOrderedSegmentInd(const Opm::WellSegments& segSet, const std::vector<std::size_t>& ordSegNo) {
        std::vector <std::size_t> sNFOSN (segSet.size(),0);
        for (int segInd = 0; segInd < segSet.size(); segInd++) {
            sNFOSN[ordSegNo[segInd]] = segInd;
        }
        return sNFOSN;
    }
    std::vector<std::size_t> segmentOrder(const Opm::WellSegments& segSet, const std::size_t segIndex) {
        std::vector<std::size_t> ordSegNumber;
        std::vector<std::size_t> tempOrdVect;
        std::vector<std::size_t> segIndCB;
        // Store "heel" segment since that will not always be at the end of the list
        segIndCB.push_back(segIndex);
        int newSInd = segIndex;
        const auto& origBranchNo = segSet[segIndex].branchNumber();
        bool endOrigBranch = true;
        // loop down branch to find all segments in branch and number from "toe" to "heel"
        while (newSInd < segSet.size()) {
            endOrigBranch = true;
            const auto& iSInd = inflowSegmentsIndex(segSet, newSInd);
            for (auto& isi : iSInd )  {
                const auto& inflowBranch = segSet[isi].branchNumber();
                if (origBranchNo == inflowBranch) {
                    endOrigBranch = false;
                }
            }
            if (iSInd.size() > 0) {
                for (const auto& ind : iSInd) {
                    auto inflowBranch = segSet[ind].branchNumber();
                    if (origBranchNo == inflowBranch) {
                        // if inflow segment belongs to same branch add contribution
                        segIndCB.insert(segIndCB.begin(), ind);
                        // search recursively down this branch to find more inflow branches
                        newSInd = ind;
                    }
                    else {
                        // if inflow segment belongs to different branch, start new search
                        const auto& nSOrd = segmentOrder(segSet, ind);
                        // copy the segments already found and indexed into the total ordered segment vector
                        for (std::size_t indOS = 0; indOS < nSOrd.size(); indOS++) {
                            ordSegNumber.push_back(nSOrd[indOS]);
                        }
                        if (endOrigBranch) {
                            newSInd = segSet.size();
                        }
                        // increment the local branch sequence number counter
                    }
                }
            }
            if (endOrigBranch || (iSInd.size()==0)) {
                // have come to toe of current branch - store segment indicies of current branch
                for (std::size_t indOS = 0; indOS < segIndCB.size(); indOS++) {
                    ordSegNumber.push_back(segIndCB[indOS]);
                }
                // set new index to exit while loop
                newSInd = segSet.size();
            }
        }

        if (origBranchNo == 1) {
            // make the vector of ordered segments - by segment index (zero-based)
            tempOrdVect.resize(ordSegNumber.size());
            for (std::size_t ov_ind = 0; ov_ind < ordSegNumber.size(); ov_ind++) {
                tempOrdVect[ordSegNumber[ov_ind]] = ov_ind;
            }
            return tempOrdVect;
        } else {
            return ordSegNumber;
        }
    }


    Opm::RestartIO::Helpers::SegmentSetSourceSinkTerms
    getSegmentSetSSTerms(const Opm::WellSegments& segSet, const std::vector<Opm::data::Connection>& rateConns,
                         const Opm::WellConnections& welConns, const Opm::UnitSystem& units)
    {
      std::vector<double> qosc (segSet.size(), 0.);
      std::vector<double> qwsc (segSet.size(), 0.);
      std::vector<double> qgsc (segSet.size(), 0.);
      std::vector<const Opm::Connection* > openConnections;
      using M  = ::Opm::UnitSystem::measure;
      using R  = ::Opm::data::Rates::opt;
      for (auto nConn = welConns.size(), connID = 0*nConn; connID < nConn; connID++) {
        if (welConns[connID].state() == Opm::WellCompletion::StateEnum::OPEN) openConnections.push_back(&welConns[connID]);
      }
      if (openConnections.size() != rateConns.size()) {
        throw std::invalid_argument {
          "Inconsistent number of open connections I in Opm::WellConnections (" +
            std::to_string(welConns.size()) + ") and vector<Opm::data::Connection> (" +
            std::to_string(rateConns.size()) + ") in Well " + segSet.wellName()
            };
      }
      for (auto nConn = openConnections.size(), connID = 0*nConn; connID < nConn; connID++) {
        const auto& segNo = openConnections[connID]->segment();
        const auto& segInd = segSet.segmentNumberToIndex(segNo);
        const auto& Q = rateConns[connID].rates;

        auto get = [&units, &Q](const M u, const R q) -> double
                   {
                     const auto val = Q.has(q) ? Q.get(q) : 0.0;

                     return - units.from_si(u, val);
                   };

        qosc[segInd] += get(M::liquid_surface_rate, R::oil);
        qwsc[segInd] += get(M::liquid_surface_rate, R::wat);
        qgsc[segInd] += get(M::gas_surface_rate,    R::gas);

      }

      return {
              qosc,
              qwsc,
              qgsc
      };
    }

    Opm::RestartIO::Helpers::SegmentSetFlowRates
    getSegmentSetFlowRates(const Opm::WellSegments& segSet, const std::vector<Opm::data::Connection>& rateConns,
                           const Opm::WellConnections& welConns, const Opm::UnitSystem& units)
    {
        std::vector<double> sofr (segSet.size(), 0.);
        std::vector<double> swfr (segSet.size(), 0.);
        std::vector<double> sgfr (segSet.size(), 0.);
        //
        //call function to calculate the individual segment source/sink terms
        auto sSSST = getSegmentSetSSTerms(segSet, rateConns, welConns, units);

        // find an ordered list of segments
        std::size_t segmentInd = 0;
        auto orderedSegmentInd = segmentOrder(segSet, segmentInd);
        auto sNFOSN = segmentIndFromOrderedSegmentInd(segSet, orderedSegmentInd);
        // loop over segments according to the ordered segments sequence which ensures that the segments alway are traversed in the from
        // inflow to outflow direction (a branch toe is the innermost inflow end)
        for (std::size_t indOSN = 0; indOSN < sNFOSN.size(); indOSN++) {
            const auto& segInd = sNFOSN[indOSN];
            // the segment flow rates is the sum of the the source/sink terms for each segment plus the flow rates from the inflow segments
            // add source sink terms
            sofr[segInd] += sSSST.qosc[segInd];
            swfr[segInd] += sSSST.qwsc[segInd];
            sgfr[segInd] += sSSST.qgsc[segInd];
            // add flow from all inflow segments
            for (const auto& segNo : segSet[segInd].inletSegments()) {
                const auto & ifSegInd = segSet.segmentNumberToIndex(segNo);
                sofr[segInd] += sofr[ifSegInd];
                swfr[segInd] += swfr[ifSegInd];
                sgfr[segInd] += sgfr[ifSegInd];
            }
        }
        return {
            sofr,
                swfr,
                sgfr
                };
    }


    std::vector<std::size_t> SegmentSetBranches(const Opm::WellSegments& segSet) {
        std::vector<std::size_t> branches;
        for (int segInd = 0; segInd < segSet.size(); segInd++) {
            const auto& i_branch = segSet[segInd].branchNumber();
            if (std::find(branches.begin(), branches.end(), i_branch) == branches.end()) {
                branches.push_back(i_branch);
            }
        }
        return branches;
    }

    int firstSegmentInBranch(const Opm::WellSegments& segSet, const int branch) {
        int firstSegInd = 0;
        int segInd = 0;
        while ((segInd < segSet.size()) && (firstSegInd == 0)) {
            const auto& i_branch = segSet[segInd].branchNumber();
            if (branch == i_branch) {
                firstSegInd = segInd;
            }
            segInd+=1;
        }
        return firstSegInd;
    }

    int noConnectionsSegment(const Opm::WellConnections& compSet,
                             const Opm::WellSegments&    segSet,
                             const std::size_t           segIndex)
    {
        const auto& segNumber  = segSet[segIndex].segmentNumber();
        int noConnections = 0;
        for (const auto& it : compSet) {
            auto cSegment = it.segment();
            if (segNumber == cSegment) {
                noConnections+=1;
            }
        }

        return noConnections;
    }

    int sumConnectionsSegment(const Opm::WellConnections& compSet,
                              const Opm::WellSegments&    segSet,
                              const std::size_t           segIndex)
    {
        // This function returns (for a given segment) the sum of number of connections for each segment
        // with lower segment index than the currnet segment
        // If the segment contains no connections, the number returned is zero.
        int sumConn = 0;
        if (noConnectionsSegment(compSet, segSet, segIndex) > 0) {
            // add up the number of connections for å segments with lower segment index than current segment
            for (size_t ind = 0; ind <= segIndex; ind++) {
                size_t addCon = (ind == segIndex) ? 1 : noConnectionsSegment(compSet, segSet, ind);
                sumConn += addCon;
            }
        }
        return sumConn;
    }

    int noInFlowBranches(const Opm::WellSegments& segSet, std::size_t segIndex) {
        const auto& segNumber  = segSet[segIndex].segmentNumber();
        const auto& branch     = segSet[segIndex].branchNumber();
        int noIFBr = 0;
        for (int ind = 0; ind < segSet.size(); ind++) {
            const auto& o_segNum = segSet[ind].outletSegment();
            const auto& i_branch = segSet[ind].branchNumber();
            if ((segNumber == o_segNum) && (branch != i_branch)){
                noIFBr+=1;
            }
        }
        return noIFBr;
    }
    //find the number of inflow branch-segments (segments that has a branch) from the
    // first segment to the current segment for segments that has at least one inflow branch
    // Segments with no inflow branches get the value zero
    int sumNoInFlowBranches(const Opm::WellSegments& segSet, const std::size_t& segIndex) {
        int sumIFB = 0;
        //auto segInd = segIndex;
        for (int segInd = static_cast<int>(segIndex); segInd >= 0; segInd--) {
            const auto& curBranch = segSet[segInd].branchNumber();
            const auto& iSInd = inflowSegmentsIndex(segSet, segInd);
            for (auto inFlowInd : iSInd) {
                const auto& inFlowBranch = segSet[inFlowInd].branchNumber();
                // if inflow segment belongs to different branch add contribution
                if (curBranch != inFlowBranch) {
                    sumIFB+=1;
                }
            }
        }
        // check if the segment has inflow branches - if yes return sumIFB else return zero
        return  (noInFlowBranches(segSet, segIndex) >= 1)
            ? sumIFB : 0;
    }


    int inflowSegmentCurBranch(const Opm::WellSegments& segSet, std::size_t segIndex) {
        const auto& branch = segSet[segIndex].branchNumber();
        const auto& segNumber  = segSet[segIndex].segmentNumber();
        int inFlowSegInd = -1;
        for (int ind = 0; ind < segSet.size(); ind++) {
            const auto& i_segNum = segSet[ind].segmentNumber();
            const auto& i_branch = segSet[ind].branchNumber();
            const auto& i_outFlowSeg = segSet[ind].outletSegment();
            if ((branch == i_branch) && (segNumber == i_outFlowSeg)) {
                if (inFlowSegInd == -1) {
                    inFlowSegInd = segSet.segmentNumberToIndex(i_segNum);
                }
                else {
                    std::cout << "Non-unique inflow segment in same branch, Well: " << segSet.wellName() << std::endl;
                    std::cout <<  "Segment number: " << segNumber << std::endl;
                    std::cout <<  "Branch number: " << branch << std::endl;
                    std::cout <<  "Inflow segment number 1: " << segSet[inFlowSegInd].segmentNumber() << std::endl;
                    std::cout <<  "Inflow segment number 2: " << segSet[ind].segmentNumber() << std::endl;
                    throw std::invalid_argument("Non-unique inflow segment in same branch, Well " + segSet.wellName());
                }
            }
        }
        return (inFlowSegInd == -1) ? 0 : inFlowSegInd;
    }

    template <typename MSWOp>
    void MSWLoop(const std::vector<const Opm::Well2*>& wells,
                 MSWOp&&                               mswOp)
    {
        auto mswID = std::size_t{0};
        for (const auto* well : wells) {
            mswID += 1;

            if (well == nullptr) { continue; }

            mswOp(*well, mswID - 1);
        }
    }

    namespace ISeg {
        std::size_t entriesPerMSW(const std::vector<int>& inteHead)
        {
            // inteHead(176) = NSEGMX
            // inteHead(178) = NISEGZ
            return inteHead[176] * inteHead[178];
        }

        Opm::RestartIO::Helpers::WindowedArray<int>
        allocate(const std::vector<int>& inteHead)
        {
            using WV = Opm::RestartIO::Helpers::WindowedArray<int>;

            return WV {
                WV::NumWindows{ nswlmx(inteHead) },
                    WV::WindowSize{ entriesPerMSW(inteHead) }
            };
        }

        template <class ISegArray>
        void staticContrib(const Opm::Well2&       well,
                           const std::size_t       rptStep,
                           const std::vector<int>& inteHead,
                           const Opm::EclipseGrid&  /* grid */,
                           ISegArray&              iSeg
                           )
        {
            if (well.isMultiSegment()) {
                //loop over segment set and print out information
                const auto& welSegSet     = well.getSegments();
                const auto& completionSet = well.getConnections();
                const auto& noElmSeg      = nisegz(inteHead);
                std::size_t segmentInd = 0;
                auto orderedSegmentNo = segmentOrder(welSegSet, segmentInd);
                for (int ind = 0; ind < welSegSet.size(); ind++) {
                    auto segNumber = welSegSet[ind].segmentNumber();
                    auto iS = (segNumber-1)*noElmSeg;
                    iSeg[iS + 0] = welSegSet[orderedSegmentNo[ind]].segmentNumber();
                    iSeg[iS + 1] = welSegSet[ind].outletSegment();
                    iSeg[iS + 2] = (inflowSegmentCurBranch(welSegSet, ind) == 0) ? 0 : welSegSet[inflowSegmentCurBranch(welSegSet, ind)].segmentNumber();
                    iSeg[iS + 3] = welSegSet[ind].branchNumber();
                    iSeg[iS + 4] = noInFlowBranches(welSegSet, ind);
                    iSeg[iS + 5] = sumNoInFlowBranches(welSegSet, ind);
                    iSeg[iS + 6] = noConnectionsSegment(completionSet, welSegSet, ind);
                    iSeg[iS + 7] = sumConnectionsSegment(completionSet, welSegSet, ind);
                    iSeg[iS + 8] = iSeg[iS+0];
                }
            }
            else {
                throw std::invalid_argument("No such multisegment well: " + well.name());
            }
        }
    } // ISeg

    namespace RSeg {
        std::size_t entriesPerMSW(const std::vector<int>& inteHead)
        {
            // inteHead(176) = NSEGMX
            // inteHead(179) = NRSEGZ
            return inteHead[176] * inteHead[179];
        }

        Opm::RestartIO::Helpers::WindowedArray<double>
        allocate(const std::vector<int>& inteHead)
        {
            using WV = Opm::RestartIO::Helpers::WindowedArray<double>;

            return WV {
                WV::NumWindows{ nswlmx(inteHead) },
                    WV::WindowSize{ entriesPerMSW(inteHead) }
            };
        }

        template <class RSegArray>
        void staticContrib_useMSW(const Opm::Well2&  	well,
                                  const std::size_t          		rptStep,
                                  const std::vector<int>&    		inteHead,
                                  const Opm::EclipseGrid&    		grid,
                                  const Opm::UnitSystem&     		units,
                                  const ::Opm::SummaryState& 		smry,
                                  const Opm::data::WellRates&  	wr,
                                  RSegArray&                 		rSeg)
        {
            if (well.isMultiSegment()) {
                // use segment index as counter  - zero-based
                int segIndex = 0;
                using M = ::Opm::UnitSystem::measure;
                const auto gfactor = (units.getType() == Opm::UnitSystem::UnitType::UNIT_TYPE_FIELD)
                    ? 0.1781076 : 0.001;

                //loop over segment set and print out information
                const auto&  noElmSeg  = nrsegz(inteHead);
                const auto& welSegSet = well.getSegments();
                const auto& segNumber  = welSegSet[segIndex].segmentNumber();

                // 'stringSegNum' is one-based (1 .. #segments inclusive)
                std::string stringSegNum = std::to_string(segNumber);

                const auto& conn0 = well.getConnections();
                const auto& welConns = Opm::WellConnections(conn0, grid);
                const auto& wname     = well.name();
                const auto wPKey = "WBHP:"  + wname;
                const auto& wRatesIt =  wr.find(wname);
                bool haveWellRes = wRatesIt != wr.end();
                const auto volFromLengthUnitConv = units.from_si(M::length, units.from_si(M::length, units.from_si(M::length, 1.)));
                const auto areaFromLengthUnitConv =  units.from_si(M::length, units.from_si(M::length, 1.));
                //
                //Initialize temporary variables
                double temp_o = 0.;
                double temp_w = 0.;
                double temp_g = 0.;

                // find well connections and calculate segment rates based on well connection production/injection terms
                auto sSFR = Opm::RestartIO::Helpers::SegmentSetFlowRates{};
                if (haveWellRes) {
                    sSFR = getSegmentSetFlowRates(welSegSet, wRatesIt->second.connections, welConns, units);

                }
                auto get = [&smry, &wname, &stringSegNum](const std::string& vector)
                    {
                        // 'stringSegNum' is one-based (1 .. #segments inclusive)
                        const auto key = vector + ':' + wname + ':' + stringSegNum;
                        return smry.has(key) ? smry.get(key) : 0.0;
                    };

                // Treat the top segment individually
                rSeg[0] = units.from_si(M::length, welSegSet.lengthTopSegment());
                rSeg[1] = units.from_si(M::length, welSegSet.depthTopSegment());
                rSeg[5] = volFromLengthUnitConv*welSegSet.volumeTopSegment();
                rSeg[6] = rSeg[0];
                rSeg[7] = rSeg[1];
                //
                // branch according to whether multisegment well calculations are switched on or not


                if (haveWellRes && wRatesIt->second.segments.size() < 2) {
                    // Note: Segment flow rates and pressure from 'smry' have correct
                    // output units and sign conventions.
                    temp_o = sSFR.sofr[segIndex];
                    temp_w = sSFR.swfr[segIndex]*0.1;
                    temp_g = sSFR.sgfr[segIndex]*gfactor;
                    //Item 12 Segment pressure - use well flow bhp
                    rSeg[11] = (smry.has(wPKey)) ? smry.get(wPKey) :0.0;
                }
                else {
                    // Note: Segment flow rates and pressure from 'smry' have correct
                    // output units and sign conventions.
                    temp_o = get("SOFR");
                    temp_w = get("SWFR")*0.1;
                    temp_g = get("SGFR")*gfactor;
                    //Item 12 Segment pressure
                    rSeg[11] = get("SPR");
                }

                rSeg[ 8] = temp_o + temp_w + temp_g;
                rSeg[ 9] = (std::abs(temp_w) > 0) ? temp_w / rSeg[8] : 0.;
                rSeg[10] = (std::abs(temp_g) > 0) ? temp_g / rSeg[8] : 0.;

                //  value is 1. based on tests on several data sets
                rSeg[ 39] = 1.;

                rSeg[105] = 1.0;
                rSeg[106] = 1.0;
                rSeg[107] = 1.0;
                rSeg[108] = 1.0;
                rSeg[109] = 1.0;
                rSeg[110] = 1.0;

                //Treat subsequent segments
                for (segIndex = 1; segIndex < welSegSet.size(); segIndex++) {

                    const auto& segNumber = welSegSet[segIndex].segmentNumber();
                    // 'stringSegNum' is one-based (1 .. #segments inclusive)
                    stringSegNum = std::to_string(segNumber);

                    // set the elements of the rSeg array
                    const auto& outSeg = welSegSet[segIndex].outletSegment();
                    const auto& ind_ofs = welSegSet.segmentNumberToIndex(outSeg);
                    auto iS = (segNumber-1)*noElmSeg;
                    rSeg[iS +   0] = units.from_si(M::length, (welSegSet[segIndex].totalLength() - welSegSet[ind_ofs].totalLength()));
                    rSeg[iS +   1] = units.from_si(M::length, (welSegSet[segIndex].depth() - welSegSet[ind_ofs].depth()));
                    rSeg[iS +   2] = units.from_si(M::length, (welSegSet[segIndex].internalDiameter()));
                    rSeg[iS +   3] = units.from_si(M::length, (welSegSet[segIndex].roughness()));
                    rSeg[iS +   4] = areaFromLengthUnitConv *  welSegSet[segIndex].crossArea();
                    rSeg[iS +   5] = volFromLengthUnitConv  *  welSegSet[segIndex].volume();
                    rSeg[iS +   6] = units.from_si(M::length, (welSegSet[segIndex].totalLength()));
                    rSeg[iS +   7] = units.from_si(M::length, (welSegSet[segIndex].depth()));

                    //see section above for explanation of values
                    // branch according to whether multisegment well calculations are switched on or not
                    if (haveWellRes && wRatesIt->second.segments.size() < 2) {
                        // Note: Segment flow rates and pressure from 'smry' have correct
                        // output units and sign conventions.
                        temp_o = sSFR.sofr[segIndex];
                        temp_w = sSFR.swfr[segIndex]*0.1;
                        temp_g = sSFR.sgfr[segIndex]*gfactor;
                        //Item 12 Segment pressure - use well flow bhp
                        rSeg[iS +  11] = (smry.has(wPKey)) ? smry.get(wPKey) :0.0;
                    }
                    else {
                        // Note: Segment flow rates and pressure from 'smry' have correct
                        // output units and sign conventions.
                        temp_o = get("SOFR");
                        temp_w = get("SWFR")*0.1;
                        temp_g = get("SGFR")*gfactor;
                        //Item 12 Segment pressure
                        rSeg[iS +  11] = get("SPR");
                    }

                    rSeg[iS +  8] = temp_o + temp_w + temp_g;
                    rSeg[iS +  9] = (std::abs(temp_w) > 0) ? temp_w / rSeg[iS + 8] : 0.;
                    rSeg[iS + 10] = (std::abs(temp_g) > 0) ? temp_g / rSeg[iS + 8] : 0.;

                    rSeg[iS +  39] = 1.;

                    rSeg[iS + 105] = 1.0;
                    rSeg[iS + 106] = 1.0;
                    rSeg[iS + 107] = 1.0;
                    rSeg[iS + 108] = 1.0;
                    rSeg[iS + 109] = 1.0;
                    rSeg[iS + 110] = 1.0;
                }
            }
            else {
                throw std::invalid_argument("No such multisegment well: " + well.name());
            }
        }
    } // RSeg


    namespace ILBS {
        std::size_t entriesPerMSW(const std::vector<int>& inteHead)
        {
            // inteHead(177) = NLBRMX
            return inteHead[177];
        }

        Opm::RestartIO::Helpers::WindowedArray<int>
        allocate(const std::vector<int>& inteHead)
        {
            using WV = Opm::RestartIO::Helpers::WindowedArray<int>;

            return WV {
                WV::NumWindows{ nswlmx(inteHead) },
                    WV::WindowSize{ entriesPerMSW(inteHead) }
            };
        }

        template <class ILBSArray>
        void staticContrib(const Opm::Well2& well,
                           const std::size_t rptStep,
                           ILBSArray&        iLBS)
        {
            if (well.isMultiSegment()) {
                //
                // Store the segment number of the first segment in branch for branch number
                // 2 and upwards
                const auto& welSegSet = well.getSegments();
                const auto& branches = SegmentSetBranches(welSegSet);
                for (auto it = branches.begin()+1; it != branches.end(); it++){
                    iLBS[*it-2] = welSegSet[firstSegmentInBranch(welSegSet, *it)].segmentNumber();
                }
            }
            else {
                throw std::invalid_argument("No such multisegment well: " + well.name());
            }
        }
    } // ILBS

    namespace ILBR {
        std::size_t entriesPerMSW(const std::vector<int>& inteHead)
        {
            // inteHead(177) = NLBRMX
            // inteHead(180) = NILBRZ
            return inteHead[177] * inteHead[180];
        }

        Opm::RestartIO::Helpers::WindowedArray<int>
        allocate(const std::vector<int>& inteHead)
        {
            using WV = Opm::RestartIO::Helpers::WindowedArray<int>;

            return WV {
                WV::NumWindows{ nswlmx(inteHead) },
                    WV::WindowSize{ entriesPerMSW(inteHead) }
            };
        }

        template <class ILBRArray>
        void staticContrib(const Opm::Well2&  well,
                           const std::size_t rptStep,
                           const std::vector<int>& inteHead,
                           ILBRArray&        iLBR)
        {
            if (well.isMultiSegment()) {
                //
                const auto& welSegSet = well.getSegments();
                const auto& branches = SegmentSetBranches(welSegSet);
                const auto& noElmBranch = nilbrz(inteHead);
                for (auto it = branches.begin(); it != branches.end(); it++){
                    const auto iB = (*it-1)*noElmBranch;
                    const auto& branchParam = getBranchSegmentParam(welSegSet, *it);
                    iLBR[iB  ] = branchParam.outletS;
                    iLBR[iB+1] = branchParam.noSegInBranch;
                    iLBR[iB+2] = branchParam.firstSeg;
                    iLBR[iB+3] = branchParam.lastSeg;
                    iLBR[iB+4] = branchParam.branch - 1;
                }
            }
            else {
                throw std::invalid_argument("No such multisegment well: " + well.name());
            }
        }
    } // ILBR

} // Anonymous

// =====================================================================

Opm::RestartIO::Helpers::AggregateMSWData::
AggregateMSWData(const std::vector<int>& inteHead)
    : iSeg_ (ISeg::allocate(inteHead))
    , rSeg_ (RSeg::allocate(inteHead))
    , iLBS_ (ILBS::allocate(inteHead))
    , iLBR_ (ILBR::allocate(inteHead))
{}

// ---------------------------------------------------------------------

void
Opm::RestartIO::Helpers::AggregateMSWData::
captureDeclaredMSWData(const Schedule&         sched,
                       const std::size_t       rptStep,
                       const Opm::UnitSystem& units,
                       const std::vector<int>& inteHead,
                       const Opm::EclipseGrid&  grid,
                       const Opm::SummaryState& smry,
                       const Opm::data::WellRates&  wr
                       )
{
    const auto& wells = sched.getWells2(rptStep);
    auto msw = std::vector<const Opm::Well2*>{};

    //msw.reserve(wells.size());
    for (const auto& well : wells) {
        if (well.isMultiSegment())
            msw.push_back(&well);
    }
    // Extract Contributions to ISeg Array
    {
        MSWLoop(msw, [rptStep, inteHead, &grid, this]
                (const Well2& well, const std::size_t mswID) -> void
                {
                    auto imsw = this->iSeg_[mswID];

                    ISeg::staticContrib(well, rptStep, inteHead, grid, imsw);
                });
    }
    // Extract Contributions to RSeg Array
    {
        MSWLoop(msw, [&units, rptStep, inteHead, &grid, &smry, this, &wr]
                (const Well2& well, const std::size_t mswID) -> void
                {
                    auto rmsw = this->rSeg_[mswID];

                    RSeg::staticContrib_useMSW(well, rptStep, inteHead, grid, units, smry, wr, rmsw);
                });
    }
    // Extract Contributions to ILBS Array
    {
        MSWLoop(msw, [rptStep, this]
                (const Well2& well, const std::size_t mswID) -> void
                {
                    auto ilbs_msw = this->iLBS_[mswID];

                    ILBS::staticContrib(well, rptStep, ilbs_msw);
                });
    }
    // Extract Contributions to ILBR Array
    {
        MSWLoop(msw, [rptStep, inteHead, this]
                (const Well2& well, const std::size_t mswID) -> void
                {
                    auto ilbr_msw = this->iLBR_[mswID];

                    ILBR::staticContrib(well, rptStep, inteHead, ilbr_msw);
                });
    }
}
