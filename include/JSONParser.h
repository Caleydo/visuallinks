/*
 * JSONParser.h
 *
 *  Created on: 23.10.2012
 *      Author: tom
 */

#ifndef JSONPARSER_H_
#define JSONPARSER_H_

#include <QScriptEngine>
#include <QScriptValueIterator>

#include <cstdint>
#include <stdexcept>

#include "qt_helper.hxx"

/**
 * Helper for handling json messages
 */
class JSONParser
{
  public:

    JSONParser(QString data)
    {
      _json = _engine.evaluate("("+data+")");

      if( !_json.isValid() )
        throw std::runtime_error("Failed to evaluted received data.");

      if( _engine.hasUncaughtException() )
        throw std::runtime_error("Failed to evaluted received data: "
                                 + to_string(_json.toString()));

      if( _json.isArray() || !_json.isObject() )
        throw std::runtime_error("Received data is no JSON object.");
    }

    template <class T>
    T getValue(const QString& key) const
    {
      QScriptValue prop = _json.property(key);

      if( !prop.isValid() || prop.isUndefined() )
        throw std::runtime_error("No such property ("+to_string(key)+")");

      return extractValue<T>(prop);
    }

    template <class T>
    T getValue(const QString& key, const T& def) const
    {
      QScriptValue prop = _json.property(key);

      if( !prop.isValid() || prop.isUndefined() )
        return def;

      return extractValue<T>(prop);
    }

    bool isSet(const QString& key) const
    {
      QScriptValue prop = _json.property(key);
      return prop.isValid() && !prop.isUndefined();
    }

  private:

    template <class T> T extractValue(QScriptValue&) const;

    QScriptEngine _engine;
    QScriptValue  _json;
};

template<>
QString JSONParser::extractValue(QScriptValue& val) const
{
  if( !val.isString() )
    throw std::runtime_error("Not a string");
  return val.toString();
}

template<>
std::string JSONParser::extractValue(QScriptValue& val) const
{
  return to_string(extractValue<QString>(val));
}

template<>
uint32_t JSONParser::extractValue(QScriptValue& val) const
{
  if( !val.isNumber() )
    throw std::runtime_error("Not a number");
  return val.toUInt32();
}

template<>
QVariantList JSONParser::extractValue(QScriptValue& val) const
{
  if( !val.isArray() )
    throw std::runtime_error("Not an array");
  return val.toVariant().toList();
}

template<>
QRect JSONParser::extractValue(QScriptValue& val) const
{
  QVariantList region_list = extractValue<QVariantList>(val);
  if( region_list.length() != 4 )
    throw std::runtime_error("Invalid region (length != 4)");

  return QRect( region_list.at(0).toInt(),
                region_list.at(1).toInt(),
                region_list.at(2).toInt(),
                region_list.at(3).toInt() );
}

#endif /* JSONPARSER_H_ */
