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

#include <opm/common/ErrorMacros.hpp>
#include <opm/common/util/numeric/cmp.hpp>
#include <opm/common/data/SimulationDataContainer.hpp>

namespace Opm {

    const std::string SimulationDataContainer::PRESSURE = "PRESSURE";
    const std::string SimulationDataContainer::TEMPERATURE = "TEMPERATURE";
    const std::string SimulationDataContainer::SATURATION = "SATURATION";
  
    const std::string SimulationDataContainer::FACEPRESSURE = "FACEPRESSURE";
    const std::string SimulationDataContainer::FACEFLUX = "FACEFLUX";


    SimulationDataContainer::SimulationDataContainer(size_t num_cells, size_t num_faces , size_t num_phases) :
        m_num_cells( num_cells),
        m_num_faces( num_faces),
        m_num_phases( num_phases )
    {
        addDefaultFields( );
    }


    size_t SimulationDataContainer::numPhases() const {
        return m_num_phases;
    }


    size_t SimulationDataContainer::numFaces() const {
        return m_num_faces;
    }

    size_t SimulationDataContainer::numCells() const {
        return m_num_cells;
    }


    bool SimulationDataContainer::hasCellData( const std::string& name ) const {
        return ( m_cell_data.find( name ) == m_cell_data.end() ? false : true );
    }


    std::vector<double>& SimulationDataContainer::getCellData( const std::string& name ) {
        auto iter = m_cell_data.find( name );
        if (iter == m_cell_data.end()) {
            throw std::invalid_argument("The cell data with name: " + name + " does not exist");
        } else
            return iter->second;
    }


    const std::vector<double>& SimulationDataContainer::getCellData( const std::string& name ) const {
        auto iter = m_cell_data.find( name );
        if (iter == m_cell_data.end()) {
            throw std::invalid_argument("The cell data with name: " + name + " does not exist");
        } else
            return iter->second;
    }


    void SimulationDataContainer::registerCellData( const std::string& name , size_t components , double initialValue) {
        if (!hasCellData( name )) {
            m_cell_data.insert( std::pair<std::string , std::vector<double>>( name , std::vector<double>(components * m_num_cells , initialValue )));
        }
    }


    void SimulationDataContainer::setCellDataComponent( const std::string& key ,
                                                        size_t component ,
                                                        const std::vector<int>& cells ,
                                                        const std::vector<double>& values) {
        auto& data = getCellData( key );
        if (component >= m_num_phases)
            OPM_THROW(std::invalid_argument, "The component number: " << component << " is invalid");


        if (cells.size() != values.size())
            OPM_THROW(std::invalid_argument, "size mismatch between cells and values");

        // This is currently quite broken; the setCellDataComponent
        // method assumes that the number of components in the field
        // we are currently focusing on has num_phases components in
        // total. This restriction should be lifted by allowing a per
        // field number of components.

        if (data.size() != m_num_phases * m_num_cells)
            OPM_THROW(std::invalid_argument , "Can currently only be used on fields with num_components == num_phases (i.e. saturation...) ");


        for (size_t i = 0; i < cells.size(); i++) {
            if (size_t(cells[i]) < m_num_cells) {
                auto field_index = cells[i] * m_num_phases + component;
                data[field_index] = values[i];
            } else {
                OPM_THROW(std::invalid_argument , "The cell number: " << cells[i] << " is invalid.");
            }
        }
    }


    bool SimulationDataContainer::hasFaceData( const std::string& name ) const {
        return ( m_face_data.find( name ) == m_face_data.end() ? false : true );
    }


    std::vector<double>& SimulationDataContainer::getFaceData( const std::string& name ) {
        auto iter = m_face_data.find( name );
        if (iter == m_face_data.end()) {
            throw std::invalid_argument("The face data with name: " + name + " does not exist");
        } else
            return iter->second;
    }

    const std::vector<double>& SimulationDataContainer::getFaceData( const std::string& name ) const {
        auto iter = m_face_data.find( name );
        if (iter == m_face_data.end()) {
            throw std::invalid_argument("The Face data with name: " + name + " does not exist");
        } else
            return iter->second;
    }


    void SimulationDataContainer::registerFaceData( const std::string& name , size_t components , double initialValue) {
        if (!hasFaceData( name )) {
            m_face_data.insert( std::pair<std::string , std::vector<double>>( name , std::vector<double>(components * m_num_faces , initialValue )));
        }
    }

    bool SimulationDataContainer::equal( const SimulationDataContainer& other ) const {
        if ((m_num_cells != other.m_num_cells) ||
            (m_num_phases != other.m_num_phases) ||
            (m_num_faces != other.m_num_faces))
            return false;

        if ((m_face_data.size() != other.m_face_data.size()) ||
            (m_cell_data.size() != other.m_cell_data.size()))
            return false;

        for (const auto& cell_data : m_cell_data) {
            const auto key = cell_data.first;
            const auto data = cell_data.second;

            if (other.hasCellData( key )) {
                const auto& other_data = other.getCellData( key );
                if (!cmp::vector_equal<double>( data , other_data ))
                    return false;
            } else
                return false;
        }

        for (const auto& face_data : m_face_data) {
            const auto key = face_data.first;
            const auto data = face_data.second;

            if (other.hasFaceData( key )) {
                const auto& other_data = other.getFaceData( key );
                if (!cmp::vector_equal<double>( data , other_data ))
                    return false;
            } else
                return false;
        }

        return true;
    }

    size_t SimulationDataContainer::numCellDataComponents( const std::string& name ) const {
        const auto& data = getCellData( name );
        return data.size() / m_num_cells;
    }


    const std::unordered_map<std::string, std::vector<double>>& SimulationDataContainer::cellData() const {
        return m_cell_data;
    }

    std::unordered_map<std::string, std::vector<double>>& SimulationDataContainer::cellData() {
        return m_cell_data;
    }

    /* This is very deprecated. */
    void SimulationDataContainer::addDefaultFields() {
        registerCellData(SimulationDataContainer::PRESSURE , 1 , 0.0);
        registerCellData(SimulationDataContainer::SATURATION , m_num_phases , 0.0);
        registerCellData(SimulationDataContainer::TEMPERATURE , 1 , 273.15 + 20);

        registerFaceData(SimulationDataContainer::FACEPRESSURE , 1 , 0.0 );
        registerFaceData(SimulationDataContainer::FACEFLUX , 1 , 0.0 );
    }


}
