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


#include <opm/output/eclipse/SummaryState.hpp>

namespace Opm{

    namespace {
        std::string make_key(const std::string& keyword, const std::string& wgname) {
            return keyword + ":" + wgname;
        }

        std::string make_key(const std::string& keyword, const std::string& wgname, int num) {
            return keyword + ":" + wgname + ":" + std::to_string(num);
        }

        std::string make_key(const std::string& keyword, int num) {
            return keyword + ":" + std::to_string(num);
        }

    }


    void SummaryState::add(const std::string& keyword, double value) {
        this->values[keyword] = value;
    }


    bool SummaryState::has(const std::string& keyword) const {
        return (this->values.find(keyword) != this->values.end());
    }


    double SummaryState::get(const std::string& keyword) const {
        const auto iter = this->values.find(keyword);
        if (iter == this->values.end())
            throw std::invalid_argument("XX No such keyword: " + keyword);

        return iter->second;
    }

    void SummaryState::add(const std::string& keyword, const std::string& wgname, double value) {
        std::string key = make_key(keyword, wgname);
        this->values[key] = value;
    }

    bool SummaryState::has(const std::string& keyword, const std::string& wgname) const {
        std::string key = make_key(keyword, wgname);
        return (this->values.find(key) != this->values.end());
    }

    double SummaryState::get(const std::string& keyword, const std::string& wgname) const {
        std::string key = make_key(keyword, wgname);
        const auto iter = this->values.find(key);
        if (iter == this->values.end())
            throw std::invalid_argument("XX No such keyword: " + key);

        return iter->second;
    }

    void SummaryState::add(const std::string& keyword, int num, double value) {
        std::string key = make_key(keyword, num);
        this->values[key] = value;
    }

    bool SummaryState::has(const std::string& keyword, int num) const {
        std::string key = make_key(keyword, num);
        return (this->values.find(key) != this->values.end());
    }

    double SummaryState::get(const std::string& keyword, int num) const {
        std::string key = make_key(keyword, num);
        const auto iter = this->values.find(key);
        if (iter == this->values.end())
            throw std::invalid_argument("XX No such keyword: " + key);

        return iter->second;
    }


    void SummaryState::add(const std::string& keyword, const std::string& wgname, int num, double value) {
        std::string key = make_key(keyword, wgname, num);
        this->values[key] = value;
    }

    bool SummaryState::has(const std::string& keyword, const std::string& wgname, int num) const {
        std::string key = make_key(keyword, wgname, num);
        return (this->values.find(key) != this->values.end());
    }

    double SummaryState::get(const std::string& keyword, const std::string& wgname, int num) const {
        std::string key = make_key(keyword, wgname, num);
        const auto iter = this->values.find(key);
        if (iter == this->values.end())
            throw std::invalid_argument("XX No such keyword: " + key);

        return iter->second;
    }

    SummaryState::const_iterator SummaryState::begin() const {
        return this->values.begin();
    }


    SummaryState::const_iterator SummaryState::end() const {
        return this->values.end();
    }

}
