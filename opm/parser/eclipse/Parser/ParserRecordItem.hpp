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
#ifndef PARSER_RECORD_ITEM_H
#define PARSER_RECORD_ITEM_H

#include <string>
#include <iostream>

#include <boost/shared_ptr.hpp>

#include <opm/parser/eclipse/Parser/ItemSize.hpp>

namespace Opm {
  template<class T>
  class ParserRecordItem {
  private:
    std::string m_name;
    ItemSizeConstPtr m_itemSize;
    
  public:

    ParserRecordItem<T>(const std::string& itemName , ItemSizeConstPtr itemSize) {
      m_name.assign( itemName );
      m_itemSize = itemSize;
    }
    

    bool scanItem(const std::string& itemString , T& value) { 
      std::istringstream inputStream( itemString );
      T newValue;
      inputStream >> newValue;

      if (inputStream.fail())
        return false;
      else {
        char c;
        inputStream >> c;
        if (inputStream.eof() || c == ' ') {
          value = newValue;
          return true;
        }
        else
          return false;
      }
    };


    int scanItems(const std::string& itemString , size_t items , std::vector<T>& values) {
      std::istringstream inputStream( itemString );
      unsigned int itemsRead = 0;
      
      while (inputStream.good() && itemsRead < items) {
        T value;
        inputStream >> value;
        values.push_back( value );
        itemsRead++;
      }
      
      return itemsRead;
    }


    int scanItems(const std::string& itemString , std::vector<T>& values) {
      return scanItems( itemString , m_itemSize->sizeValue() , values);
    }
    

  };
}

#endif
