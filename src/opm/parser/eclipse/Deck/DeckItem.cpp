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

#include <opm/parser/eclipse/Deck/DeckOutput.hpp>
#include <opm/parser/eclipse/Deck/DeckItem.hpp>
#include <opm/parser/eclipse/Units/Dimension.hpp>

#include <boost/algorithm/string.hpp>

#include <algorithm>
#include <string>
#include <iostream>
#include <stdexcept>
#include <cmath>

namespace Opm {

template< typename T >
std::vector< T >& DeckItem::value_ref() {
    return const_cast< std::vector< T >& >(
            const_cast< const DeckItem& >( *this ).value_ref< T >()
         );
}

template<>
const std::vector< int >& DeckItem::value_ref< int >() const {
    if( this->type != get_type< int >() )
        throw std::invalid_argument( "DeckItem::value_ref<int> Item of wrong type. this->type: " + tag_name(this->type) + " " + this->name());

    return this->ival;
}

template<>
const std::vector< double >& DeckItem::value_ref< double >() const {
    if (this->type == get_type<double>())
        return this->dval;

    throw std::invalid_argument( "DeckItem::value_ref<double> Item of wrong type. this->type: " + tag_name(this->type) + " " + this->name());
}

template<>
const std::vector< std::string >& DeckItem::value_ref< std::string >() const {
    if( this->type != get_type< std::string >() )
        throw std::invalid_argument( "DeckItem::value_ref<std::string> Item of wrong type. this->type: " + tag_name(this->type) + " " + this->name());

    return this->sval;
}

template<>
const std::vector< UDAValue >& DeckItem::value_ref< UDAValue >() const {
    if( this->type != get_type< UDAValue >() )
        throw std::invalid_argument( "DeckItem::value_ref<UDAValue> Item of wrong type. this->type: " + tag_name(this->type) + " " + this->name());

    return this->uval;
}


std::vector<bool> DeckItem::defaulted() const {
    std::vector<bool> def(this->value_status.size(), true);
    for (std::size_t i=0; i < this->value_status.size(); i++)
        def[i] = value::defaulted(this->value_status[i]);

    return def;
}


DeckItem::DeckItem( const std::string& nm, int) :
    type( get_type< int >() ),
    item_name( nm )
{
}

DeckItem::DeckItem( const std::string& nm, std::string) :
    type( get_type< std::string >() ),
    item_name( nm )
{
}

DeckItem::DeckItem( const std::string& nm, double, const std::vector<Dimension>& active_dim, const std::vector<Dimension>& default_dim) :
    type( get_type< double >() ),
    item_name( nm ),
    active_dimensions(active_dim),
    default_dimensions(default_dim)
{
}

DeckItem::DeckItem( const std::string& nm, UDAValue, const std::vector<Dimension>& active_dim, const std::vector<Dimension>& default_dim) :
    type( get_type< UDAValue >() ),
    item_name( nm ),
    active_dimensions(active_dim),
    default_dimensions(default_dim)
{
}


const std::string& DeckItem::name() const {
    return this->item_name;
}

bool DeckItem::defaultApplied( size_t index ) const {
    return value::defaulted(this->value_status.at(index));
}

bool DeckItem::hasValue( size_t index ) const {
    /*switch( this->type ) {
        case type_tag::integer: return this->ival.size() > index;
        case type_tag::fdouble: return this->dval.size() > index;
        case type_tag::string:  return this->sval.size() > index;
        case type_tag::uda:     return this->uval.size() > index;
        default: throw std::logic_error( "DeckItem::hasValue: Type not set." );
    }
    */
    if (index >= this->value_status.size())
        return false;
    return value::has_value(this->value_status[index]);
}

size_t DeckItem::data_size() const {
    /*switch( this->type ) {
        case type_tag::integer: return this->ival.size();
        case type_tag::fdouble: return this->dval.size();
        case type_tag::string:  return this->sval.size();
        case type_tag::uda:     return this->uval.size();
        default: throw std::logic_error( "DeckItem::size: Type not set." );
    }
    */
    std::size_t s = this->value_status.size();
    /*if (s == 0)
        return 0;

    if (this->value_status.back() == status::invalid)
        s -= 1;
    */
    return s;
}


size_t DeckItem::out_size() const {
    size_t data_size = this->data_size();
    return std::max( data_size , this->value_status.size() );
}


template< typename T >
T DeckItem::get( size_t index ) const {
    this->assert_index(index);
    return this->value_ref< T >().at( index );
}


void DeckItem::assert_index(std::size_t index) const {
    if (!value::has_value(this->value_status.at(index)))
        throw std::invalid_argument("Tried to access invalid deck value for item: " + this->name());
}


template<>
UDAValue DeckItem::get( size_t index ) const {
    this->assert_index(index);
    auto value = this->value_ref<UDAValue>().at(index);
    if (this->active_dimensions.empty())
        return value;

    std::size_t dim_index = index % this->active_dimensions.size();
    if (value::defaulted(this->value_status[index]))
        return UDAValue( value, this->default_dimensions[dim_index]);
    else
        return UDAValue( value, this->active_dimensions[dim_index]);
}


template< typename T >
const std::vector< T >& DeckItem::getData() const {
    return this->value_ref< T >();
}


template< typename T >
void DeckItem::push( T x ) {
    auto& val = this->value_ref< T >();

    val.push_back( std::move( x ) );
    this->value_status.push_back( value::status::deck_value );
}

void DeckItem::push_back( int x ) {
    this->push( x );
}

void DeckItem::push_back( double x ) {
    this->push( x );
}

void DeckItem::push_back( std::string x ) {
    this->push( std::move( x ) );
}

void DeckItem::push_back( UDAValue x ) {
    this->push( std::move( x ) );
}

template< typename T >
void DeckItem::push( T x, size_t n ) {
    auto& val = this->value_ref< T >();

    val.insert( val.end(), n, x );
    this->value_status.insert( this->value_status.end(), n, value::status::deck_value );
}

void DeckItem::push_back( int x, size_t n ) {
    this->push( x, n );
}

void DeckItem::push_back( double x, size_t n ) {
    this->push( x, n );
}

void DeckItem::push_back( std::string x, size_t n ) {
    this->push( std::move( x ), n );
}

void DeckItem::push_back( UDAValue x, size_t n ) {
    this->push( std::move( x ), n );
}

template< typename T >
void DeckItem::push_default( T x ) {
    auto& val = this->value_ref< T >();
    val.push_back( std::move( x ) );
    this->value_status.push_back( value::status::valid_default );
}

template< typename T >
void DeckItem::push_backDummyDefault( ) {
    auto& val = this->value_ref< T >();
    val.push_back( T( ) );
    this->value_status.push_back( value::status::empty_default );
}

void DeckItem::push_backDefault( int x ) {
    this->push_default( x );
}

void DeckItem::push_backDefault( double x ) {
    this->push_default( x );
}

void DeckItem::push_backDefault( std::string x ) {
    this->push_default( std::move( x ) );
}

void DeckItem::push_backDefault( UDAValue x ) {
    this->push_default( std::move( x ) );
}


std::string DeckItem::getTrimmedString( size_t index ) const {
    this->assert_index(index);
    return boost::algorithm::trim_copy(
               this->value_ref< std::string >().at( index )
           );
}

double DeckItem::getSIDouble( size_t index ) const {
    this->assert_index(index);
    return this->getSIDoubleData().at( index );
}

template<>
const std::vector<double>& DeckItem::getData() const {
    auto& data = (const_cast<DeckItem*>(this))->value_ref< double >();
    if (this->raw_data)
        return data;

    const auto dim_size = this->active_dimensions.size();
    for( size_t index = 0; index < data.size(); index++ ) {
        const auto dimIndex = index % dim_size;
        if (value::defaulted(this->value_status[index])) {
            const auto& dim = this->default_dimensions[dimIndex];
            data[index] = dim.convertSiToRaw(data[index]);
        } else {
            const auto& dim = this->active_dimensions[dimIndex];
            data[index] = dim.convertSiToRaw(data[index]);
        }
    }
    this->raw_data = true;
    return data;
}

const std::vector< double >& DeckItem::getSIDoubleData() const {
    auto& data = (const_cast<DeckItem*>(this))->value_ref< double >();
    if (!this->raw_data)
        return data;


    if( this->active_dimensions.empty() )
        throw std::invalid_argument("No dimension has been set for item'"
                                    + this->name()
                                    + "'; can not ask for SI data");

    /*
     * This is an unobservable state change - SIData is lazily converted to
     * SI units, so externally the object still behaves as const
     */

    const auto dim_size = this->active_dimensions.size();
    const auto sz = data.size();
    for( size_t index = 0; index < sz; index++ ) {
        const auto dimIndex = index % dim_size;
        if (value::defaulted(this->value_status[index])) {
            const auto& dim = this->default_dimensions[dimIndex];
            data[index] = dim.convertRawToSi(data[index]);
        } else {
            const auto& dim = this->active_dimensions[dimIndex];
            data[index] = dim.convertRawToSi(data[index]);
        }
    }
    this->raw_data = false;
    return data;
}


type_tag DeckItem::getType() const {
    return this->type;
}



template< typename T >
void DeckItem::write_vector(DeckOutput& stream, const std::vector<T>& data) const {
    for (size_t index = 0; index < this->out_size(); index++) {
        if (this->defaultApplied(index))
            stream.stash_default( );
        else
            stream.write( data[index] );
    }
}


void DeckItem::write(DeckOutput& stream) const {
    switch( this->type ) {
    case type_tag::integer:
        this->write_vector( stream, this->ival );
        break;
    case type_tag::fdouble:
        {
            const auto& data = this->getData<double>();
            this->write_vector( stream,  data );
            break;
        }
    case type_tag::string:
        this->write_vector( stream,  this->sval );
        break;
    case type_tag::uda:
        this->write_vector( stream,  this->uval );
        break;
    default:
        throw std::logic_error( "DeckItem::write: Type not set." );
    }
}

std::ostream& operator<<(std::ostream& os, const DeckItem& item) {
    DeckOutput stream(os);
    item.write( stream );
    return os;
}

namespace {
bool double_equal(double value1, double value2, double abs_eps , double rel_eps) {

    bool equal = true;
    double diff = std::fabs(value1 - value2);
    if (diff > abs_eps) {
        double scale = std::max(std::fabs(value1), std::fabs(value2));

        if (diff > scale * rel_eps) {
            equal = false;
        }
    }
    return equal;
}
}


bool DeckItem::equal(const DeckItem& other, bool cmp_default, bool cmp_numeric) const {
    double rel_eps = 1e-4;
    double abs_eps = 1e-4;

    if (this->type != other.type)
        return false;

    if (this->data_size() != other.data_size())
        return false;

    if (this->item_name != other.item_name)
        return false;

    if (cmp_default)
        if (this->value_status != other.value_status)
            return false;

    switch( this->type ) {
    case type_tag::integer:
        if (this->ival != other.ival)
            return false;
        break;
    case type_tag::string:
        if (this->sval != other.sval)
            return false;
        break;
    case type_tag::fdouble:
        if (cmp_numeric) {
            const auto& this_data = this->getData<double>();
            const auto& other_data = other.getData<double>();
            for (size_t i=0; i < this_data.size(); i++) {
                if (!double_equal( this_data[i] , other_data[i], rel_eps, abs_eps))
                    return false;
            }
        } else {
            if (this->raw_data == other.raw_data)
                return (this->dval == other.dval);
            else {
                const auto& this_data = this->getData<double>();
                const auto& other_data = other.getData<double>();
                return (this_data == other_data);
            }
        }
        break;
    default:
        break;
    }

    return true;
}

bool DeckItem::operator==(const DeckItem& other) const {
    bool cmp_default = false;
    bool cmp_numeric = true;
    return this->equal( other , cmp_default, cmp_numeric);
}

bool DeckItem::operator!=(const DeckItem& other) const {
    return !(*this == other);
}


bool DeckItem::to_bool(std::string string_value) {
    std::transform(string_value.begin(), string_value.end(), string_value.begin(), toupper);

    if (string_value == "TRUE")
        return true;

    if (string_value == "YES")
        return true;

    if (string_value == "T")
        return true;

    if (string_value == "Y")
        return true;

    if (string_value == "1")
        return true;

    if (string_value == "FALSE")
        return false;

    if (string_value == "NO")
        return false;

    if (string_value == "F")
        return false;

    if (string_value == "N")
        return false;

    if (string_value == "0")
        return false;

    throw std::invalid_argument("Could not convert string " + string_value + " to bool ");
}



/*
 * Explicit template instantiations. These must be manually maintained and
 * updated with changes in DeckItem so that code is emitted.
 */

template int DeckItem::get< int >( size_t ) const;
template double DeckItem::get< double >( size_t ) const;
template std::string DeckItem::get< std::string >( size_t ) const;
template UDAValue DeckItem::get< UDAValue >( size_t ) const;

template const std::vector< int >& DeckItem::getData< int >() const;
template const std::vector< UDAValue >& DeckItem::getData< UDAValue >() const;
template const std::vector< std::string >& DeckItem::getData< std::string >() const;

template void DeckItem::push_backDummyDefault<int>( );
template void DeckItem::push_backDummyDefault<double>( );
template void DeckItem::push_backDummyDefault<std::string>( );
template void DeckItem::push_backDummyDefault<UDAValue>( );
}
