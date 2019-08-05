#include <opm/parser/eclipse/EclipseState/Schedule/Well/Well2.hpp>
#include <pybind11/stl.h>
#include "sunbeam.hpp"


namespace {

    std::vector<Connection> connections( const Well2& w ) {
        const auto& well_connections = w.getConnections();
        return std::vector<Connection>(well_connections.begin(), well_connections.end());
    }

    std::string status( const Well2& w )  {
        return WellCommon::Status2String( w.getStatus() );
    }

    std::string preferred_phase( const Well2& w ) {
        switch( w.getPreferredPhase() ) {
            case Phase::OIL:   return "OIL";
            case Phase::GAS:   return "GAS";
            case Phase::WATER: return "WATER";
            default: throw std::logic_error( "Unhandled enum value" );
        }
    }

    int    (Well2::*headI)() const = &Well2::getHeadI;
    int    (Well2::*headJ)() const = &Well2::getHeadI;
    double (Well2::*refD)()  const = &Well2::getRefDepth;

    int    (Well2::*headI_at)() const = &Well2::getHeadI;
    int    (Well2::*headJ_at)() const = &Well2::getHeadI;
    double (Well2::*refD_at)()  const = &Well2::getRefDepth;

}

void sunbeam::export_Well(py::module& module) {

    py::class_< Well2 >( module, "Well")
        .def_property_readonly( "name", &Well2::name )
        .def_property_readonly( "preferred_phase", &preferred_phase )
        .def( "I",               headI )
        .def( "I",               headI_at )
        .def( "J",               headJ )
        .def( "J",               headJ_at )
        .def( "ref",             refD )
        .def( "ref",             refD_at )
        .def( "status",          &status )
        .def( "isdefined",       &Well2::hasBeenDefined )
        .def( "isinjector",      &Well2::isInjector )
        .def( "isproducer",      &Well2::isProducer )
        .def( "group",           &Well2::groupName )
        .def( "guide_rate",      &Well2::getGuideRate )
        .def( "available_gctrl", &Well2::isAvailableForGroupControl )
        .def( "_connections",    &connections );

}
