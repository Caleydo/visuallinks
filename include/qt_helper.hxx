/*
 * qt_helper.hxx
 *
 *  Created on: 12.02.2013
 *      Author: tom
 */

#ifndef QT_HELPER_HXX_
#define QT_HELPER_HXX_

#include <iosfwd>
#include <QtCore/QString>

/**
 * Helper to convert QString to std::string.
 *
 * @note Use this instead of QString::toStdString() as otherwise with Visual
 *       Studio it can easily lead to crashes.
 * @param str   String to convert
 * @return      Converted String
 */
std::string to_string(const QString& str);

/**
 * Print QString to std streams
 *
 * @param strm
 * @param str
 * @return
 */
std::ostream& operator<<(std::ostream& strm, const QString& str);

#endif /* QT_HELPER_HXX_ */
