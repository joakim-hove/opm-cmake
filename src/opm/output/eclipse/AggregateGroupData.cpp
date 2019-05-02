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

#include <opm/output/eclipse/AggregateGroupData.hpp>
#include <opm/output/eclipse/WriteRestartHelpers.hpp>

#include <opm/parser/eclipse/EclipseState/EclipseState.hpp>
#include <opm/parser/eclipse/EclipseState/Runspec.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/SummaryState.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Schedule.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/ScheduleEnums.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/GroupTree.hpp>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>
#include <iostream>

// #####################################################################
// Class Opm::RestartIO::Helpers::AggregateGroupData
// ---------------------------------------------------------------------


namespace {

    // maximum number of groups
    std::size_t ngmaxz(const std::vector<int>& inteHead)
    {
        return inteHead[20];
    }

    // maximum number of wells in any group
    int nwgmax(const std::vector<int>& inteHead)
    {
        return inteHead[19];
    }

    int groupType(const Opm::Schedule& sched,
		      const Opm::Group& group,
                      const std::size_t simStep)
    {
	const std::string& groupName = group.name();
	if (!sched.hasGroup(groupName))
            throw std::invalid_argument("No such group: " + groupName);
        {
            if (group.hasBeenDefined( simStep )) {
                const auto& groupTree = sched.getGroupTree( simStep );
                const auto& childGroups = groupTree.children( groupName );

                if (childGroups.size()) {
		    return 1;
                }
                else {
		    return 0;
		}
            }
            return 0;
        }
    }

    template <typename GroupOp>
    void groupLoop(const std::vector<const Opm::Group*>& groups,
                  GroupOp&&                             groupOp)
    {
        auto groupID = std::size_t{0};
        for (const auto* group : groups) {
            groupID += 1;

            if (group == nullptr) { continue; }

            groupOp(*group, groupID - 1);
        }
    }

    std::map <size_t, const Opm::Group*>  currentGroupMapIndexGroup(const Opm::Schedule& sched, const size_t simStep, const std::vector<int>& inteHead)
    {
	const auto& groups = sched.getGroups(simStep);
	// make group index for current report step
	std::map <size_t, const Opm::Group*> indexGroupMap;
	for (const auto* group : groups) {
	    int ind = (group->name() == "FIELD")
	    ? ngmaxz(inteHead)-1 : group->seqIndex()-1;
	    const std::pair<size_t, const Opm::Group*> groupPair = std::make_pair(static_cast<size_t>(ind), group);
	    indexGroupMap.insert(groupPair);
	}
	return indexGroupMap;
    }

    std::map <const std::string, size_t>  currentGroupMapNameIndex(const Opm::Schedule& sched, const size_t simStep, const std::vector<int>& inteHead)
    {
	const auto& groups = sched.getGroups(simStep);
	// make group name to index map for the current time step
	std::map <const std::string, size_t> groupIndexMap;
	for (const auto* group : groups) {
	    int ind = (group->name() == "FIELD")
                    ? ngmaxz(inteHead)-1 : group->seqIndex()-1;
	    std::pair<const std::string, size_t> groupPair = std::make_pair(group->name(), ind);
	    groupIndexMap.insert(groupPair);
	}
	return groupIndexMap;
    }

    int currentGroupLevel(const Opm::Schedule& sched, const Opm::Group& group, const size_t simStep)
    {
	int level = 0;
	const std::vector< const Opm::Group* >  groups = sched.getGroups(simStep);
      	const std::string& groupName = group.name();
	if (!sched.hasGroup(groupName))
            throw std::invalid_argument("No such group: " + groupName);
        {
            if (group.hasBeenDefined( simStep )) {
                const auto& groupTree = sched.getGroupTree( simStep );
		//find group level - field level is 0
		std::string tstGrpName = groupName;
		while (((tstGrpName.size())>0) && (!(tstGrpName=="FIELD"))) {
		    std::string curParent = groupTree.parent(tstGrpName);
		    level+=1;
		    tstGrpName = curParent;
		}
		return level;
	    }
	    else {
		std::stringstream str;
		str << "actual group has not been defined at report time: " << simStep;
		throw std::invalid_argument(str.str());
	    }
	}
	return level;
    }


    namespace IGrp {
        std::size_t entriesPerGroup(const std::vector<int>& inteHead)
        {
            // INTEHEAD[36] = NIGRPZ
            return inteHead[36];
        }

        Opm::RestartIO::Helpers::WindowedArray<int>
        allocate(const std::vector<int>& inteHead)
        {
            using WV = Opm::RestartIO::Helpers::WindowedArray<int>;

            return WV {
                WV::NumWindows{ ngmaxz(inteHead) },
                WV::WindowSize{ entriesPerGroup(inteHead) }
            };
        }

        template <class IGrpArray>
        void staticContrib(const Opm::Schedule&    sched,
			   const Opm::Group&       group,
			   const int               nwgmax,
			   const int               ngmaxz,
			   const std::size_t       simStep,
			   IGrpArray&              iGrp,
			   const std::vector<int>& inteHead)
        {
            // find the number of wells or child groups belonging to a group and store in
            // location nwgmax +1 in the iGrp array

            const auto childGroups = sched.getChildGroups(group.name(), simStep);
            const auto childWells  = sched.getChildWells2(group.name(), simStep, Opm::GroupWellQueryMode::Immediate);
            const auto groupMapNameIndex =  currentGroupMapNameIndex(sched, simStep, inteHead);
            const auto mapIndexGroup = currentGroupMapIndexGroup(sched, simStep, inteHead);
            if ((childGroups.size() != 0) && (childWells.size()!=0))
                throw std::invalid_argument("group has both wells and child groups" + group.name());
            int igrpCount = 0;
            if (childWells.size() != 0) {
                //group has child wells
                //store the well number (sequence index) in iGrp according to the sequence they are defined
                for ( const auto& well : childWells) {
                    iGrp[igrpCount] = well.seqIndex()+1;
                    igrpCount+=1;
                }
            }
            else if (childGroups.size() != 0) {
                //group has child groups
                //The field group always has seqIndex = 0 because it is always defined first
                //Hence the all groups except the Field group uses the seqIndex assigned
                //iGrp contains the child groups in ascending group sequence index
                std::vector<size_t> childGroupIndex;
                Opm::RestartIO::Helpers::groupMaps groupMap;
                groupMap.currentGrpTreeNameSeqIndMap(sched, simStep, groupMapNameIndex,mapIndexGroup);
                const auto indGroupMap = groupMap.indexGroupMap();
                const auto gNameIndMap = groupMap.groupNameIndexMap();
                for (auto* grp : childGroups) {
                    auto groupName = grp->name();
                    auto searchGTName = gNameIndMap.find(groupName);
                    if (searchGTName != gNameIndMap.end()) {
                        childGroupIndex.push_back(searchGTName->second);
                    }
                    else {
                        throw std::invalid_argument( "Invalid group name" );
                    }
                }
                std::sort(childGroupIndex.begin(), childGroupIndex.end());
                for (auto groupTreeIndex : childGroupIndex) {
                    auto searchGTIterator = indGroupMap.find(groupTreeIndex);
                    if (searchGTIterator != indGroupMap.end()) {
                        auto gname = (searchGTIterator->second)->name();
                        auto gSeqNoItr = groupMapNameIndex.find(gname);
                        if (gSeqNoItr != groupMapNameIndex.end()) {
                            iGrp[igrpCount] = (gSeqNoItr->second) + 1;
                            igrpCount+=1;
                        }
                        else {
                            std::cout << "AggregateGroupData, ChildGroups - Group name: groupMapNameIndex: " << gSeqNoItr->first << std::endl;
                            throw std::invalid_argument( "Invalid group name" );
                        }
                    }
                    else {
                        std::cout << "AggregateGroupData, ChildGroups - GroupTreeIndex: " << groupTreeIndex << std::endl;
                        throw std::invalid_argument( "Invalid GroupTree index" );
                    }
                }
            }

	    //assign the number of child wells or child groups to
	    // location nwgmax
	    iGrp[nwgmax] =  (childGroups.size() == 0)
                    ? childWells.size() : childGroups.size();

	    // find the group type (well group (type 0) or node group (type 1) and store the type in
	    // location nwgmax + 26
	    const auto grpType = groupType(sched, group, simStep);
	    iGrp[nwgmax+26] = grpType;

	    //find group level ("FIELD" is level 0) and store the level in
	    //location nwgmax + 27
	    const auto grpLevel = currentGroupLevel(sched, group, simStep);
	    iGrp[nwgmax+27] = grpLevel;

	    // set values for group probably connected to GCONPROD settings
	    //
	    if (group.name() != "FIELD")
	    {
		iGrp[nwgmax+ 5] = -1;
		iGrp[nwgmax+12] = -1;
		iGrp[nwgmax+17] = -1;
		iGrp[nwgmax+22] = -1;

		//assign values to group number (according to group sequence)
		iGrp[nwgmax+88] = group.seqIndex();
		iGrp[nwgmax+89] = group.seqIndex();
		iGrp[nwgmax+95] = group.seqIndex();
		iGrp[nwgmax+96] = group.seqIndex();
	    }
	    else
	    {
		//assign values to group number (according to group sequence)
		iGrp[nwgmax+88] = ngmaxz;
		iGrp[nwgmax+89] = ngmaxz;
		iGrp[nwgmax+95] = ngmaxz;
		iGrp[nwgmax+96] = ngmaxz;
	    }
	    //find parent group and store index of parent group in
	    //location nwgmax + 28
	    const auto& groupTree = sched.getGroupTree( simStep );
	    const std::string& parent = groupTree.parent(group.name());
	    if (group.name() == "FIELD")
	      iGrp[nwgmax+28] = 0;
	    else {
		if (parent.size() == 0)
		    throw std::invalid_argument("parent group does not exist" + group.name());
		const auto itr = groupMapNameIndex.find(parent);
		iGrp[nwgmax+28] = (itr->second)+1;
	    }
        }
    } // Igrp

    namespace SGrp {
        std::size_t entriesPerGroup(const std::vector<int>& inteHead)
        {
            // INTEHEAD[37] = NSGRPZ
            return inteHead[37];
        }

        Opm::RestartIO::Helpers::WindowedArray<float>
        allocate(const std::vector<int>& inteHead)
        {
            using WV = Opm::RestartIO::Helpers::WindowedArray<float>;

            return WV {
                WV::NumWindows{ ngmaxz(inteHead) },
                WV::WindowSize{ entriesPerGroup(inteHead) }
            };
        }

	template <class SGrpArray>
	void staticContrib(SGrpArray& sGrp)
	{
		const auto dflt   = -1.0e+20f;
		const auto dflt_2 = -2.0e+20f;
		const auto infty  =  1.0e+20f;
		const auto zero   =  0.0f;
		const auto one    =  1.0f;

		const auto init = std::vector<float> { // 112 Items (0..111)
		    // 0     1      2      3      4
		    infty, infty, dflt , infty,  zero ,     //   0..  4  ( 0)
		    zero , infty, infty, infty , infty,     //   5..  9  ( 1)
		    infty, infty, infty, infty , dflt ,     //  10.. 14  ( 2)
		    infty, infty, infty, infty , dflt ,     //  15.. 19  ( 3)
		    infty, infty, infty, infty , dflt ,     //  20.. 24  ( 4)
		    zero , zero , zero , dflt_2, zero ,     //  24.. 29  ( 5)
		    zero , zero , zero , zero  , zero ,     //  30.. 34  ( 6)
		    infty ,zero , zero , zero  , infty,     //  35.. 39  ( 7)
		    zero , zero , zero , zero  , zero ,     //  40.. 44  ( 8)
		    zero , zero , zero , zero  , zero ,     //  45.. 49  ( 9)
		    zero , infty, infty, infty , infty,     //  50.. 54  (10)
		    infty, infty, infty, infty , infty,     //  55.. 59  (11)
		    infty, infty, infty, infty , infty,     //  60.. 64  (12)
		    infty, infty, infty, infty , zero ,     //  65.. 69  (13)
		    zero , zero , zero , zero  , zero ,     //  70.. 74  (14)
		    zero , zero , zero , zero  , infty,     //  75.. 79  (15)
		    infty, zero , infty, zero  , zero ,     //  80.. 84  (16)
		    zero , zero , zero , zero  , zero ,     //  85.. 89  (17)
		    zero , zero , one  , zero  , zero ,     //  90.. 94  (18)
		    zero , zero , zero , zero  , zero ,     //  95.. 99  (19)
		    zero , zero , zero , zero  , zero ,     // 100..104  (20)
		    zero , zero , zero , zero  , zero ,     // 105..109  (21)
		    zero , zero                             // 110..111  (22)
		};

		const auto sz = static_cast<
		    decltype(init.size())>(sGrp.size());

		auto b = std::begin(init);
		auto e = b + std::min(init.size(), sz);

		std::copy(b, e, std::begin(sGrp));
	}
    } // SGrp

    namespace XGrp {
        std::size_t entriesPerGroup(const std::vector<int>& inteHead)
        {
            // INTEHEAD[38] = NXGRPZ
            return inteHead[38];
        }

        Opm::RestartIO::Helpers::WindowedArray<double>
        allocate(const std::vector<int>& inteHead)
        {
            using WV = Opm::RestartIO::Helpers::WindowedArray<double>;

            return WV {
                WV::NumWindows{ ngmaxz(inteHead) },
                WV::WindowSize{ entriesPerGroup(inteHead) }
            };
        }

        // here define the dynamic group quantities to be written to the restart file
        template <class XGrpArray>
        void dynamicContrib(const std::vector<std::string>&      restart_group_keys,
			    const std::vector<std::string>&      restart_field_keys,
			    const std::map<std::string, size_t>& groupKeyToIndex,
			    const std::map<std::string, size_t>& fieldKeyToIndex,
			    const Opm::Group&                    group,
			    const Opm::SummaryState&             sumState,
			    XGrpArray&                           xGrp)
        {
	  std::string groupName = group.name();
	  const std::vector<std::string>& keys = (groupName == "FIELD")
	  ? restart_field_keys : restart_group_keys;
	  const std::map<std::string, size_t>& keyToIndex = (groupName == "FIELD")
	  ? fieldKeyToIndex : groupKeyToIndex;

	  for (const auto& key : keys) {
	      std::string compKey = (groupName == "FIELD")
	      ? key : key + ":" + groupName;

	      if (sumState.has(compKey)) {
		  double keyValue = sumState.get(compKey);
		  const auto itr = keyToIndex.find(key);
		  xGrp[itr->second] = keyValue;
	      }
	      /*else {
		  std::cout << "AggregateGroupData_compkey: " << compKey << std::endl;
		  std::cout << "AggregateGroupData, empty " << std::endl;
		  //throw std::invalid_argument("No such keyword: " + compKey);
	    }*/
	  }
	}
    } // XGrp

    namespace ZGrp {
        std::size_t entriesPerGroup(const std::vector<int>& inteHead)
        {
            // INTEHEAD[39] = NZGRPZ
            return inteHead[39];
        }

        Opm::RestartIO::Helpers::WindowedArray<
            Opm::RestartIO::Helpers::CharArrayNullTerm<8>
        >
        allocate(const std::vector<int>& inteHead)
        {
            using WV = Opm::RestartIO::Helpers::WindowedArray<
                Opm::RestartIO::Helpers::CharArrayNullTerm<8>
            >;

            return WV {
                WV::NumWindows{ ngmaxz(inteHead) },
                WV::WindowSize{ entriesPerGroup(inteHead) }
            };
        }

        template <class ZGroupArray>
        void staticContrib(const Opm::Group& group, ZGroupArray& zGroup)
        {
            zGroup[0] = group.name();
        }
    } // ZGrp
} // Anonymous

void
Opm::RestartIO::Helpers::groupMaps::
currentGrpTreeNameSeqIndMap(const Opm::Schedule&                        sched,
                            const size_t                                      simStep,
                            const std::map<const std::string , size_t>& /* GnIMap */,
                            const std::map<size_t, const Opm::Group*>&  IGMap)
    {
	const auto& grpTreeNSIMap = (sched.getGroupTree(simStep)).nameSeqIndMap();
	// make group index for current report step
	for (auto itr = IGMap.begin(); itr != IGMap.end(); itr++) {
	    auto name = (itr->second)->name();
	    auto srchGN = grpTreeNSIMap.find(name);
	    if (srchGN != grpTreeNSIMap.end()) {
		//come here if group is in gruptree
		this->m_indexGroupMap.insert(std::make_pair(srchGN->second, itr->second));
		this->m_groupNameIndexMap.insert(std::make_pair(srchGN->first, srchGN->second));
	    }
	    else {
	    //come here if group is not in gruptree to put in from global group list
		this->m_indexGroupMap.insert(std::make_pair(itr->first, itr->second));
		this->m_groupNameIndexMap.insert(std::make_pair(name, itr->first));
	    }
	}
    }

const std::map <size_t, const Opm::Group*>&
Opm::RestartIO::Helpers::groupMaps::indexGroupMap() const
{
	return this->m_indexGroupMap;
}

const std::map <const std::string, size_t>&
Opm::RestartIO::Helpers::groupMaps::groupNameIndexMap() const
{
	return this->m_groupNameIndexMap;
}

// =====================================================================

Opm::RestartIO::Helpers::AggregateGroupData::
AggregateGroupData(const std::vector<int>& inteHead)
    : iGroup_ (IGrp::allocate(inteHead))
    , sGroup_ (SGrp::allocate(inteHead))
    , xGroup_ (XGrp::allocate(inteHead))
    , zGroup_ (ZGrp::allocate(inteHead))
    , nWGMax_ (nwgmax(inteHead))
    , nGMaxz_ (ngmaxz(inteHead))
{}

// ---------------------------------------------------------------------

void
Opm::RestartIO::Helpers::AggregateGroupData::
captureDeclaredGroupData(const Opm::Schedule&                 sched,
			 const std::vector<std::string>&      restart_group_keys,
			 const std::vector<std::string>&      restart_field_keys,
			 const std::map<std::string, size_t>& groupKeyToIndex,
			 const std::map<std::string, size_t>& fieldKeyToIndex,
			 const std::size_t                    simStep,
			 const Opm::SummaryState&             sumState,
			 const std::vector<int>&              inteHead)
{
    const auto indexGroupMap = currentGroupMapIndexGroup(sched, simStep, inteHead);
    const auto nameIndexMap = currentGroupMapNameIndex(sched, simStep, inteHead);

    std::vector<const Opm::Group*> curGroups(ngmaxz(inteHead), nullptr);

    auto it = indexGroupMap.begin();
    while (it != indexGroupMap.end())
    {
        curGroups[static_cast<int>(it->first)] = it->second;
        it++;
    }

    groupLoop(curGroups, [&sched, simStep, &inteHead, this]
        (const Group& group, const std::size_t groupID) -> void
    {
        auto ig = this->iGroup_[groupID];

        IGrp::staticContrib(sched, group, this->nWGMax_, this->nGMaxz_,
                            simStep, ig, inteHead);
    });

    // Define Static Contributions to SGrp Array.
    groupLoop(curGroups,
              [this](const Group& /* group */, const std::size_t groupID) -> void
    {
        auto sw = this->sGroup_[groupID];
        SGrp::staticContrib(sw);
    });

    // Define Dynamic Contributions to XGrp Array.
    groupLoop(curGroups, [&restart_group_keys, &restart_field_keys,
                          &groupKeyToIndex, &fieldKeyToIndex,
                          &sumState, this]
	(const Group& group, const std::size_t groupID) -> void
    {
        auto xg = this->xGroup_[groupID];

        XGrp::dynamicContrib(restart_group_keys, restart_field_keys,
                             groupKeyToIndex, fieldKeyToIndex, group,
                             sumState, xg);
    });

    // Define Static Contributions to ZGrp Array.
    groupLoop(curGroups, [this, &nameIndexMap]
        (const Group& group, const std::size_t /* groupID */) -> void
    {
        auto zg = this->zGroup_[ nameIndexMap.at(group.name()) ];

        ZGrp::staticContrib(group, zg);
    });
}
