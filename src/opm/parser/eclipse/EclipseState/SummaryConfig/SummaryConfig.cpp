/*
  Copyright 2016 Statoil ASA.

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
#include <algorithm>
#include <array>

#include <ert/ecl/ecl_smspec.hpp>

#include <opm/parser/eclipse/Parser/ParseContext.hpp>

#include <opm/parser/eclipse/Deck/Deck.hpp>
#include <opm/parser/eclipse/Deck/DeckItem.hpp>
#include <opm/parser/eclipse/Deck/DeckKeyword.hpp>
#include <opm/parser/eclipse/Deck/DeckRecord.hpp>
#include <opm/parser/eclipse/Deck/Section.hpp>

#include <opm/parser/eclipse/EclipseState/EclipseState.hpp>
#include <opm/parser/eclipse/EclipseState/Tables/TableManager.hpp>
#include <opm/parser/eclipse/EclipseState/Grid/EclipseGrid.hpp>
#include <opm/parser/eclipse/EclipseState/Grid/GridDims.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/UDQ/UDQConfig.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Group/Group.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Schedule.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/TimeMap.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/Connection.hpp>
#include <opm/parser/eclipse/EclipseState/Schedule/Well/WellConnections.hpp>
#include <opm/parser/eclipse/EclipseState/SummaryConfig/SummaryConfig.hpp>


namespace Opm {

namespace {
    /*
      Small dummy decks which contain a list of keywords; observe that
      these dummy decks will be used as proper decks and MUST START
      WITH SUMMARY.
    */

    const Deck ALL_keywords = {
        "SUMMARY",
        "FAQR",  "FAQRG", "FAQT", "FAQTG", "FGIP", "FGIPG", "FGIPL",
        "FGIR",  "FGIT",  "FGOR", "FGPR",  "FGPT", "FOIP",  "FOIPG",
        "FOIPL", "FOIR",  "FOIT", "FOPR",  "FOPT", "FPR",   "FVIR",
        "FVIT",  "FVPR",  "FVPT", "FWCT",  "FWGR", "FWIP",  "FWIR",
        "FWIT",  "FWPR",  "FWPT", "FWPP",  "FOPP", "FGPP",  "FWPI",
	"FOPI",  "FGPI",
        "GGIR",  "GGIT",  "GGOR", "GGPR",  "GGPT", "GOIR",  "GOIT",
        "GOPR",  "GOPT",  "GVIR", "GVIT",  "GVPR", "GVPT",  "GWCT",
        "GWGR",  "GWIR",  "GWIT", "GWPR",  "GWPT", "GWPP",  "GOPP",
	"GGPP",  "GWPI",  "GOPI", "GGPI",
        "WBHP",  "WGIR",  "WGIT", "WGOR",  "WGPR", "WGPT",  "WOIR",
        "WOIT",  "WOPR",  "WOPT", "WPI",   "WTHP", "WVIR",  "WVIT",
        "WVPR",  "WVPT",  "WWCT", "WWGR",  "WWIR", "WWIT",  "WWPR",
        "WWPT",  "WWPP",  "WOPP", "WGPP",  "WWPI", "WGPI",  "WOPI",
        // ALL will not expand to these keywords yet
        "AAQR",  "AAQRG", "AAQT", "AAQTG"
    };

    const Deck GMWSET_keywords = {
        "SUMMARY",
        "GMCTG", "GMWPT", "GMWPR", "GMWPA", "GMWPU", "GMWPG", "GMWPO", "GMWPS",
        "GMWPV", "GMWPP", "GMWPL", "GMWIT", "GMWIN", "GMWIA", "GMWIU", "GMWIG",
        "GMWIS", "GMWIV", "GMWIP", "GMWDR", "GMWDT", "GMWWO", "GMWWT"
    };

    const Deck FMWSET_keywords = {
        "SUMMARY",
        "FMCTF", "FMWPT", "FMWPR", "FMWPA", "FMWPU", "FMWPF", "FMWPO", "FMWPS",
        "FMWPV", "FMWPP", "FMWPL", "FMWIT", "FMWIN", "FMWIA", "FMWIU", "FMWIF",
        "FMWIS", "FMWIV", "FMWIP", "FMWDR", "FMWDT", "FMWWO", "FMWWT"
    };


    const Deck PERFORMA_keywords = {
        "SUMMARY",
        "TCPU", "ELAPSED","NEWTON","NLINERS","NLINSMIN", "NLINSMAX","MLINEARS",
        "MSUMLINS","MSUMNEWT","TIMESTEP","TCPUTS","TCPUDAY","STEPTYPE","TELAPLIN"
    };


    /*
      The variable type 'ECL_SMSPEC_MISC_TYPE' is a catch-all variable
      type, and will by default internalize keywords like 'ALL' and
      'PERFORMA', where only the keywords in the expanded list should
      be included.
    */
    const std::set<std::string> meta_keywords = {"PERFORMA" , "ALL" , "FMWSET", "GMWSET"};

    /*
      This is a hardcoded mapping between 3D field keywords,
      e.g. 'PRESSURE' and 'SWAT' and summary keywords like 'RPR' and
      'BPR'. The purpose of this mapping is to maintain an overview of
      which 3D field keywords are needed by the Summary calculation
      machinery, based on which summary keywords are requested. The
      Summary calculations are implemented in the opm-output
      repository.
    */
    const std::map<std::string , std::set<std::string>> required_fields =  {
         {"PRESSURE", {"FPR" , "RPR" , "BPR"}},
         {"OIP"  , {"ROIP" , "FOIP" , "FOE"}},
         {"OIPL" , {"ROIPL" ,"FOIPL" }},
         {"OIPG" , {"ROIPG" ,"FOIPG"}},
         {"GIP"  , {"RGIP" , "FGIP"}},
         {"GIPL" , {"RGIPL" , "FGIPL"}},
         {"GIPG" , {"RGIPG", "FGIPG"}},
         {"WIP"  , {"RWIP" , "FWIP"}},
         {"SWAT" , {"BSWAT"}},
         {"SGAS" , {"BSGAS"}}
    };

    bool is_udq(const std::string& keyword) {
        return (keyword.size() > 1 && keyword[1] == 'U' && keyword != "SUMTHIN");
    }


void handleMissingWell( const ParseContext& parseContext, ErrorGuard& errors, const std::string& keyword, const std::string& well) {
    std::string msg = std::string("Error in keyword:") + keyword + std::string(" No such well: ") + well;
    if (parseContext.get( ParseContext::SUMMARY_UNKNOWN_WELL) == InputError::WARN)
        std::cerr << "ERROR: " << msg << std::endl;

    parseContext.handleError( ParseContext::SUMMARY_UNKNOWN_WELL , msg, errors );
}


void handleMissingGroup( const ParseContext& parseContext , ErrorGuard& errors, const std::string& keyword, const std::string& group) {
    std::string msg = std::string("Error in keyword:") + keyword + std::string(" No such group: ") + group;
    if (parseContext.get( ParseContext::SUMMARY_UNKNOWN_GROUP) == InputError::WARN)
        std::cerr << "ERROR: " << msg << std::endl;

    parseContext.handleError( ParseContext::SUMMARY_UNKNOWN_GROUP , msg, errors );
}

inline void keywordW( SummaryConfig::keyword_list& list,
                      const ParseContext& parseContext,
                      ErrorGuard& errors,
                      const DeckKeyword& keyword,
                      const Schedule& schedule ) {

    /*
      Here is a two step check whether this keyword should be discarded as not
      supported:

        1. Well keywords ending with 'L' represent completions, they are not
           supported.

        2. If the keyword is a UDQ keyword there is no convention enforced to
           the last character, and in that case it is treated as a normal well
           keyword anyways.
    */
    if (keyword.name().back() == 'L') {
        if (!is_udq(keyword.name())) {
            std::string msg = std::string("The completion keywords like: " + keyword.name() + " are not supported");
            parseContext.handleError( ParseContext::SUMMARY_UNHANDLED_KEYWORD, msg, errors);
            return;
        }
    }

    if (keyword.size() && keyword.getDataRecord().getDataItem().hasValue(0)) {
        for( const std::string& pattern : keyword.getStringData()) {
          auto well_names = schedule.wellNames( pattern, schedule.size() - 1 );

            if( well_names.empty() )
                handleMissingWell( parseContext, errors, keyword.name(), pattern );

            for( const auto& well_name : well_names)
                list.push_back( SummaryConfig::keyword_type( keyword.name(), well_name ));
        }
    } else
        for (const auto& wname : schedule.wellNames())
            list.push_back( SummaryConfig::keyword_type( keyword.name(),  wname));
  }


inline void keywordG( SummaryConfig::keyword_list& list,
                      const ParseContext& parseContext,
                      ErrorGuard& errors,
                      const DeckKeyword& keyword,
                      const Schedule& schedule ) {

    if( keyword.name() == "GMWSET" ) return;

    if( keyword.size() == 0 ||
        !keyword.getDataRecord().getDataItem().hasValue( 0 ) ) {

        for( const auto& group : schedule.groupNames() ) {
            if( group == "FIELD" ) continue;
            list.push_back( SummaryConfig::keyword_type(keyword.name(), group ));
        }
        return;
    }

    const auto& item = keyword.getDataRecord().getDataItem();

    for( const std::string& group : item.getData< std::string >() ) {
        if( schedule.hasGroup( group ) )
            list.push_back( SummaryConfig::keyword_type(keyword.name(), group ));
        else
            handleMissingGroup( parseContext, errors, keyword.name(), group );
    }
}

inline void keywordF( SummaryConfig::keyword_list& list,
                      const DeckKeyword& keyword ) {
    if( keyword.name() == "FMWSET" ) return;
    list.push_back( SummaryConfig::keyword_type( keyword.name() ));
}

inline std::array< int, 3 > getijk( const DeckRecord& record,
                                    int offset = 0 ) {
    return {{
        record.getItem( offset + 0 ).get< int >( 0 ) - 1,
        record.getItem( offset + 1 ).get< int >( 0 ) - 1,
        record.getItem( offset + 2 ).get< int >( 0 ) - 1
    }};
}

inline std::array< int, 3 > getijk( const Connection& completion ) {
    return { { completion.getI(), completion.getJ(), completion.getK() }};
}


inline void keywordB( SummaryConfig::keyword_list& list,
                      const DeckKeyword& keyword,
                      const GridDims& dims) {
  for( const auto& record : keyword ) {
      auto ijk = getijk( record );
      int global_index = 1 + dims.getGlobalIndex(ijk[0], ijk[1], ijk[2]);
      list.push_back( SummaryConfig::keyword_type( keyword.name(), global_index, dims.getNXYZ().data() ));
  }
}

inline void keywordR2R( SummaryConfig::keyword_list& /* list */,
                        const ParseContext& parseContext,
                        ErrorGuard& errors,
                        const DeckKeyword& keyword)
{
    std::string msg = "OPM/flow does not support region to region summary keywords - " + keyword.name() + " is ignored.";
    parseContext.handleError(ParseContext::SUMMARY_UNHANDLED_KEYWORD, msg, errors);
}


  inline void keywordR( SummaryConfig::keyword_list& list,
                      const DeckKeyword& keyword,
                      const TableManager& tables) {

    /* RUNSUM is not a region keyword but a directive for how to format and
     * print output. Unfortunately its *recognised* as a region keyword
     * because of its structure and position. Hence the special handling of ignoring it.
     */
    if( keyword.name() == "RUNSUM" ) return;
    if( keyword.name() == "RPTONLY" ) return;

    const size_t numfip = tables.numFIPRegions( );
    const auto& item = keyword.getDataRecord().getDataItem();
    std::vector<int> regions;

    if (item.size() > 0)
        regions = item.getData< int >();
    else {
        for (size_t region=1; region <= numfip; region++)
            regions.push_back( region );
    }

    for( const int region : regions ) {
        if (region >= 1 && region <= static_cast<int>(numfip))
            list.push_back( SummaryConfig::keyword_type( keyword.name(), region ));
        else
            throw std::invalid_argument("Illegal region value: " + std::to_string( region ));
    }
}


inline void keywordMISC( SummaryConfig::keyword_list& list,
                           const DeckKeyword& keyword)
{
    if (meta_keywords.count( keyword.name() ) == 0)
        list.push_back( SummaryConfig::keyword_type( keyword.name() ));
}


  inline void keywordC( SummaryConfig::keyword_list& list,
                        const ParseContext& parseContext,
                        ErrorGuard& errors,
                        const DeckKeyword& keyword,
                        const Schedule& schedule,
                        const GridDims& dims) {

    const auto& keywordstring = keyword.name();
    for( const auto& record : keyword ) {

        const auto& wellitem = record.getItem( 0 );

        const auto well_names = wellitem.defaultApplied( 0 )
                              ? schedule.wellNames()
                              : schedule.wellNames( wellitem.getTrimmedString( 0 ) );

        if( well_names.empty() )
            handleMissingWell( parseContext, errors, keyword.name(), wellitem.getTrimmedString( 0 ) );

        for(const auto& name : well_names) {
            const auto& well = schedule.getWell2atEnd(name);
            /*
             * we don't want to add completions that don't exist, so we iterate
             * over a well's completions regardless of the desired block is
             * defaulted or not
             */
            for( const auto& connection : well.getConnections() ) {
                /* block coordinates defaulted */
                auto cijk = getijk( connection );

                if( record.getItem( 1 ).defaultApplied( 0 ) ) {
                    int global_index = 1 + dims.getGlobalIndex(cijk[0], cijk[1], cijk[2]);
                    list.push_back( SummaryConfig::keyword_type( keywordstring, name.c_str(), global_index, dims.getNXYZ().data()));
                } else {
                    /* block coordinates specified */
                    auto recijk = getijk( record, 1 );
                    if( std::equal( recijk.begin(), recijk.end(), cijk.begin() ) ) {
                        int global_index = 1 + dims.getGlobalIndex(recijk[0], recijk[1], recijk[2]);
                        list.push_back(SummaryConfig::keyword_type( keywordstring, name.c_str(), global_index, dims.getNXYZ().data()));
                    }
                }
            }
        }
    }
}

    bool isKnownSegmentKeyword(const DeckKeyword& keyword)
    {
        const auto& kw = keyword.name();

        if (kw.size() > 4) {
            // Easy check first--handles SUMMARY and SUMTHIN &c.
            return false;
        }

        const auto kw_whitelist = std::vector<const char*> {
            "SOFR", "SGFR", "SWFR", "SPR",
        };

        return std::any_of(kw_whitelist.begin(), kw_whitelist.end(),
                           [&kw](const char* known) -> bool
                           {
                               return kw == known;
                           });
    }


    int maxNumWellSegments(const std::size_t last_timestep,
                           const Well2&       well)
    {
        auto numSeg = 0;

        for (auto step  = 0*last_timestep;
                  step <=   last_timestep; ++step)
        {
            if (! well.isMultiSegment())
                continue;

            const auto nseg =
                well.getSegments().size();

            if (nseg > numSeg) {
                numSeg = nseg;
            }
        }

        return numSeg;
    }

    void makeSegmentNodes(const std::size_t               last_timestep,
                          const int                       segID,
                          const DeckKeyword&              keyword,
                          const Well2&                    well,
                          SummaryConfig::keyword_list&    list)
    {
        // Modifies 'list' in place.
        auto makeNode = [&keyword, &list]
            (const std::string& well_name, const int segNumber)
        {
            list.push_back(SummaryConfig::keyword_type( keyword.name(), well_name, segNumber ));
        };

        if (!well.isMultiSegment())
            // Not an MSW.  Don't create summary vectors for segments.
            return;

        const auto& wname = well.name();
        if (segID < 1) {
            // Segment number defaulted.  Allocate a summary
            // vector for each segment.
            const auto nSeg = maxNumWellSegments(last_timestep, well);

            for (auto segNumber = 0*nSeg;
                 segNumber <   nSeg; ++segNumber)
                {
                    // One-based segment number.
                    makeNode(wname, segNumber + 1);
                }
        }
        else {
            // Segment number specified.  Allocate single
            // summary vector for that segment number.
            makeNode(wname, segID);
        }
    }

    void keywordSNoRecords(const std::size_t            last_timestep,
                           const DeckKeyword&           keyword,
                           const Schedule&              schedule,
                           SummaryConfig::keyword_list& list)
    {
        // No keyword records.  Allocate summary vectors for all
        // segments in all wells at all times.
        //
        // Expected format:
        //
        //   SGFR
        //   / -- All segments in all MS wells at all times.

        const auto segID = -1;

        for (const auto& well : schedule.getWells2atEnd())
            makeSegmentNodes(last_timestep, segID, keyword,
                             well, list);
    }

    void keywordSWithRecords(const std::size_t            last_timestep,
                             const ParseContext&          parseContext,
                             ErrorGuard& errors,
                             const DeckKeyword&           keyword,
                             const Schedule&              schedule,
                             SummaryConfig::keyword_list& list)
    {
        // Keyword has explicit records.  Process those and create
        // segment-related summary vectors for those wells/segments
        // that match the description.
        //
        // Expected formats:
        //
        //   SOFR
        //     'W1'   1 /
        //     'W1'  10 /
        //     'W3'     / -- All segments
        //   /
        //
        //   SPR
        //     1*   2 / -- Segment 2 in all multi-segmented wells
        //   /

        for (const auto& record : keyword) {
            const auto& wellitem = record.getItem(0);
            const auto& well_names = wellitem.defaultApplied(0)
                ? schedule.wellNames()
                : schedule.wellNames(wellitem.getTrimmedString(0));

            if (well_names.empty())
                handleMissingWell(parseContext, errors, keyword.name(),
                                  wellitem.getTrimmedString(0));

            // Negative 1 (< 0) if segment ID defaulted.  Defaulted
            // segment number in record implies all segments.
            const auto segID = record.getItem(1).defaultApplied(0)
                ? -1 : record.getItem(1).get<int>(0);

            for (const auto& well_name : well_names)
                makeSegmentNodes(last_timestep, segID, keyword, schedule.getWell2atEnd(well_name), list);
        }
    }

    inline void keywordS(SummaryConfig::keyword_list& list,
                         const ParseContext&          parseContext,
                         ErrorGuard& errors,
                         const DeckKeyword&           keyword,
                         const Schedule&              schedule)
    {
        // Generate SMSPEC nodes for SUMMARY keywords of the form
        //
        //   SOFR
        //     'W1'   1 /
        //     'W1'  10 /
        //     'W3'     / -- All segments
        //   /
        //
        //   SPR
        //     1*   2 / -- Segment 2 in all multi-segmented wells
        //   /
        //
        //   SGFR
        //   / -- All segments in all MS wells at all times.

        if (! isKnownSegmentKeyword(keyword)) {
            // Ignore keywords that have not been explicitly white-listed
            // for treatment as segment summary vectors.
            return;
        }

        const auto last_timestep = schedule.getTimeMap().last();

        if (keyword.size() > 0) {
            // Keyword with explicit records.
            // Handle as alternatives SOFR and SPR above
            keywordSWithRecords(last_timestep, parseContext, errors,
                                keyword, schedule, list);
        }
        else {
            // Keyword with no explicit records.
            // Handle as alternative SGFR above.
            keywordSNoRecords(last_timestep, keyword, schedule, list);
        }
    }

  inline void handleKW( SummaryConfig::keyword_list& list,
                        const DeckKeyword& keyword,
                        const Schedule& schedule,
                        const TableManager& tables,
                        const ParseContext& parseContext,
                        ErrorGuard& errors,
                        const GridDims& dims) {
    const auto var_type = ecl_smspec_identify_var_type( keyword.name().c_str() );
    const auto& name = keyword.name();
    if (is_udq(name)) {
        const auto& udq = schedule.getUDQConfig(schedule.size() - 1);

        if (!udq.has_keyword(name)) {
            std::string msg{"Summary output has been requested for UDQ keyword: " + name + " but it has not been configured"};
            parseContext.handleError(ParseContext::SUMMARY_UNDEFINED_UDQ, msg, errors);
            return;
        }

        if (!udq.has_unit(name)) {
            std::string msg{"Summary output has been requested for UDQ keyword: " + name + " but no unit has not been configured"};
            parseContext.handleError(ParseContext::SUMMARY_UDQ_MISSING_UNIT, msg, errors);
        }
    }

    switch( var_type ) {
        case ECL_SMSPEC_WELL_VAR: return keywordW( list, parseContext, errors, keyword, schedule );
        case ECL_SMSPEC_GROUP_VAR: return keywordG( list, parseContext, errors, keyword, schedule );
        case ECL_SMSPEC_FIELD_VAR: return keywordF( list, keyword );
        case ECL_SMSPEC_BLOCK_VAR: return keywordB( list, keyword, dims );
        case ECL_SMSPEC_REGION_VAR: return keywordR( list, keyword, tables );
        case ECL_SMSPEC_REGION_2_REGION_VAR: return keywordR2R(list, parseContext, errors, keyword);
        case ECL_SMSPEC_COMPLETION_VAR: return keywordC( list, parseContext, errors, keyword, schedule, dims);
        case ECL_SMSPEC_SEGMENT_VAR: return keywordS( list, parseContext, errors, keyword, schedule );
        case ECL_SMSPEC_MISC_VAR: return keywordMISC( list, keyword );

        default:
            std::string msg = "Summary keywords of type: " + std::string(ecl_smspec_get_var_type_name( var_type )) + " is not supported. Keyword: " + keyword.name() + " is ignored";
            parseContext.handleError(ParseContext::SUMMARY_UNHANDLED_KEYWORD, msg, errors);
            return;
    }
}

  inline void uniq( SummaryConfig::keyword_list& vec ) {
    const auto lt = []( const SummaryConfig::keyword_type& lhs,
                        const SummaryConfig::keyword_type& rhs ) {
        return lhs.cmp(rhs) < 0;
    };

    const auto eq = []( const SummaryConfig::keyword_type& lhs,
                        const SummaryConfig::keyword_type& rhs ) {
        return lhs.cmp(rhs) == 0;
    };

    std::sort( vec.begin(), vec.end(), lt );
    auto logical_end = std::unique( vec.begin(), vec.end(), eq );
    vec.erase( logical_end, vec.end() );
  }
}

SummaryConfig::SummaryConfig( const Deck& deck,
                              const Schedule& schedule,
                              const TableManager& tables,
                              const ParseContext& parseContext,
                              ErrorGuard& errors,
                              const GridDims& dims) {
    SUMMARYSection section( deck );

    // The kw_iter++ hoops is to skip the initial 'SUMMARY' keyword.
    auto kw_iter = section.begin();
    if (kw_iter != section.end())
        kw_iter++;

    for(; kw_iter != section.end(); ++kw_iter) {
        const auto& kw = *kw_iter;
        handleKW( this->keywords, kw, schedule, tables, parseContext, errors, dims);
    }

    if( section.hasKeyword( "ALL" ) )
        this->merge( { ALL_keywords, schedule, tables, parseContext, errors, dims} );

    if( section.hasKeyword( "GMWSET" ) )
        this->merge( { GMWSET_keywords, schedule, tables, parseContext, errors, dims} );

    if( section.hasKeyword( "FMWSET" ) )
        this->merge( { FMWSET_keywords, schedule, tables, parseContext, errors, dims} );

    if (section.hasKeyword( "PERFORMA" ) )
        this->merge( { PERFORMA_keywords, schedule, tables, parseContext, errors, dims} );

    uniq( this->keywords );
    for (const auto& kw: this->keywords) {
        this->short_keywords.insert( kw.keyword() );
        this->summary_keywords.insert( kw.gen_key() );
    }

}

SummaryConfig::SummaryConfig( const Deck& deck,
                              const Schedule& schedule,
                              const TableManager& tables,
                              const ParseContext& parseContext,
                              ErrorGuard& errors) :
    SummaryConfig( deck , schedule, tables, parseContext, errors, GridDims( deck ))
{ }


template <typename T>
SummaryConfig::SummaryConfig( const Deck& deck,
                              const Schedule& schedule,
                              const TableManager& tables,
                              const ParseContext& parseContext,
                              T&& errors) :
    SummaryConfig(deck, schedule, tables, parseContext, errors)
{}


SummaryConfig::SummaryConfig( const Deck& deck,
               const Schedule& schedule,
               const TableManager& tables) :
    SummaryConfig(deck, schedule, tables, ParseContext(), ErrorGuard())
{}



SummaryConfig::const_iterator SummaryConfig::begin() const {
    return this->keywords.cbegin();
}

SummaryConfig::const_iterator SummaryConfig::end() const {
    return this->keywords.cend();
}

SummaryConfig& SummaryConfig::merge( const SummaryConfig& other ) {
    this->keywords.insert( this->keywords.end(),
                            other.keywords.begin(),
                            other.keywords.end() );

    uniq( this->keywords );
    return *this;
}

SummaryConfig& SummaryConfig::merge( SummaryConfig&& other ) {
    auto fst = std::make_move_iterator( other.keywords.begin() );
    auto lst = std::make_move_iterator( other.keywords.end() );
    this->keywords.insert( this->keywords.end(), fst, lst );
    other.keywords.clear();

    uniq( this->keywords );
    return *this;
}


bool SummaryConfig::hasKeyword( const std::string& keyword ) const {
    return (this->short_keywords.count( keyword ) == 1);
}


bool SummaryConfig::hasSummaryKey(const std::string& keyword ) const {
    return (this->summary_keywords.count( keyword ) == 1);
}


size_t SummaryConfig::size() const {
    return this->keywords.size();
}

/*
  Can be used to query if a certain 3D field, e.g. PRESSURE, is
  required to calculate the summary variables.

  The implementation is based on the hardcoded datastructure
  required_fields defined in a anonymous namespaces at the top of this
  file; the content of this datastructure again is based on the
  implementation of the Summary calculations in the opm-output
  repository: opm/output/eclipse/Summary.cpp.
*/

bool SummaryConfig::require3DField( const std::string& keyword ) const {
    const auto iter = required_fields.find( keyword );
    if (iter == required_fields.end())
        return false;

    for (const auto& kw : iter->second) {
        if (this->hasKeyword( kw ))
            return true;
    }

    return false;
}


bool SummaryConfig::requireFIPNUM( ) const {
    return this->hasKeyword("ROIP")  ||
           this->hasKeyword("ROIPL") ||
           this->hasKeyword("RGIP")  ||
           this->hasKeyword("RGIPL") ||
           this->hasKeyword("RGIPG") ||
           this->hasKeyword("RWIP")  ||
           this->hasKeyword("RPR");
}

}
