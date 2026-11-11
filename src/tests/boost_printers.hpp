#pragma once

#include <boost/test/unit_test.hpp>

#include <QString>

#include <ostream>

// readable QString values when a BOOST_TEST comparison fails
namespace boost::test_tools::tt_detail
{
    template <>
    struct print_log_value<QString>
    {
        void operator()(std::ostream &os, const QString &value)
        {
            os << value.toStdString();
        }
    };
} // namespace boost::test_tools::tt_detail
